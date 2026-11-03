#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
"""Releases of an app's server on d0b (tags cloud-v<version>), the cloud:release:* tasks.

    cloud_release.py prepare <version> --service NAME [options]
        set the version file, commit, tag cloud-v<version>
    cloud_release.py deploy <tag> --service NAME [options]
        check the tagged, clean tree, build deploy/ and dist/<service>-<tag>.tar.zst
    cloud_release.py build [tag] --service NAME [options]
        the same bundle without the release checks (PRs, trying things)

The standard server is PocketBase: the bundle holds containers/ and
server/pb_migrations, pb_hooks, pb_public under pb/. --profile and
--include cover other servers. The server has its own versions, apart
from the app's v* tags. Nothing here pushes or uploads. See
docs/releasing.md.
"""

import argparse
import os
import re
import shutil
import stat
import subprocess
import sys
from datetime import datetime, timezone

from release_lib import (ReleaseError, assert_clean_worktree, assert_tag_on_head,  # noqa: F401
                         compare_versions, git, parse_version, regular_files, sha256,
                         tag_exists, validate_tag, write_sha256sums)
import release_lib

TAG_PREFIX = "cloud-v"
PREPARE_TASK = "cloud:release:prepare"
# What goes into the bundle besides containers/, per profile, unless
# --include says otherwise. A missing folder becomes an empty one.
DEFAULT_INCLUDES = {
    "pocketbase": ["server/pb_migrations:pb/pb_migrations",
                   "server/pb_hooks:pb/pb_hooks",
                   "server/pb_public:pb/pb_public"],
}


class Config:
    """What differs per app; the options of its cloud:release:* tasks."""

    def __init__(self, service, profile="pocketbase", includes=(), version_file="server/VERSION",
                 containers="containers", preflight=(), runner="mise"):
        self.service = service
        self.profile = profile
        includes = list(includes) or DEFAULT_INCLUDES.get(profile, [])
        self.includes = [parse_include(i) for i in includes]
        self.version_file = version_file
        self.containers = containers
        self.preflight = list(preflight)
        self.runner = runner


def parse_include(text):
    src, _, dest = text.partition(":")
    dest = dest or src
    for part in (src, dest):
        if not part or part.startswith("/") or ".." in part.split("/"):
            raise ReleaseError(f'--include "{text}": relative paths inside the repo only')
    src, dest = src.rstrip("/"), dest.rstrip("/")
    if dest == "containers" or dest.startswith("containers/") or dest in ("MANIFEST", "SHA256SUMS"):
        raise ReleaseError(f'--include "{text}": {dest} is reserved in the bundle')
    return src, dest


# --- versions and tags -------------------------------------------------

def release_tag(version):
    parse_version(version)
    return TAG_PREFIX + version


def read_version(root, cfg):
    path = os.path.join(root, cfg.version_file)
    if not os.path.isfile(path):
        raise ReleaseError(f"{cfg.version_file} is missing; create it with the line 0.0.0")
    with open(path, encoding="utf-8") as f:
        version = f.read().strip()
    parse_version(version)
    return version


def write_version(root, cfg, version):
    with open(os.path.join(root, cfg.version_file), "w", encoding="utf-8") as f:
        f.write(version + "\n")


def assert_tag_matches_version(tag, cfg, version):
    release_lib.assert_tag_matches_version(tag, TAG_PREFIX, version, cfg.version_file, PREPARE_TASK)


# --- prepare -----------------------------------------------------------

def prepare_release(root, cfg, version):
    parse_version(version)
    current = read_version(root, cfg)
    if compare_versions(version, current) <= 0:
        raise ReleaseError(f"{version} is not greater than the current version {current}")
    assert_clean_worktree(root)
    tag = release_tag(version)
    if tag_exists(root, tag):
        raise ReleaseError(f"tag {tag} already exists")
    write_version(root, cfg, version)
    git(root, "add", cfg.version_file)
    git(root, "commit", "-q", "-m", f"(c) Cloud release {version}")
    git(root, "tag", "--annotate", tag, "-m", f"{cfg.service} {version}")
    return tag


# --- contract checks (d0b contract 1, see docs/releasing.md) ------------

BUILD_ENV_LINE = re.compile(r"^[A-Z][A-Z0-9_]*=[A-Za-z0-9._:/+@-]+$")
SMOKE_LINE = re.compile(r"^GET /\S* [1-5][0-9]{2}( \S.*)?$")
FORBIDDEN_KEYS = ("Network", "PodmanArgs", "AddCapability", "Sysctl")
REQUIRED_KEYS = ("ReadOnly=true", "NoNewPrivileges=true", "DropCapability=all", "Notify=healthy")


def roles(root, cfg):
    """The roles, from containers/quadlets/<service>-<role>.container."""
    quadlets = os.path.join(root, cfg.containers, "quadlets")
    prefix = f"{cfg.service}-"
    found = sorted(n[len(prefix):-len(".container")] for n in os.listdir(quadlets)
                   if n.startswith(prefix) and n.endswith(".container"))
    if not found:
        raise ReleaseError(f"no {cfg.containers}/quadlets/{prefix}<role>.container")
    return found


def check_contract(root, cfg):
    """Cheap static checks of containers/, before anything is built."""
    problems = []
    containers = os.path.join(root, cfg.containers)
    where = cfg.containers

    for name, pattern, hint in (("build.env", BUILD_ENV_LINE, "KEY=value with [A-Za-z0-9._:/+@-] (no comments)"),
                                ("smoke.txt", SMOKE_LINE, "'GET <path> <status> [header]'")):
        path = os.path.join(containers, name)
        if not os.path.isfile(path):
            problems.append(f"{where}/{name} is missing")
            continue
        with open(path, encoding="utf-8") as f:
            for n, line in enumerate(f.read().splitlines(), 1):
                if line and not pattern.match(line):
                    problems.append(f"{where}/{name}:{n}: not {hint}")

    quadlets = os.path.join(containers, "quadlets")
    names = sorted(os.listdir(quadlets))
    if f"{cfg.service}.pod" not in names:
        problems.append(f"{where}/quadlets/{cfg.service}.pod is missing")
    for role in roles(root, cfg):
        name = f"{cfg.service}-{role}.container"
        if not os.path.isfile(os.path.join(containers, f"{role}.Containerfile")):
            problems.append(f"{where}/{role}.Containerfile is missing")
        with open(os.path.join(quadlets, name), encoding="utf-8") as f:
            lines = f.read().splitlines()
        for key in REQUIRED_KEYS:
            if key not in lines:
                problems.append(f"{where}/quadlets/{name}: {key} is missing")
        if not any(line.startswith("HealthCmd=") for line in lines):
            problems.append(f"{where}/quadlets/{name}: HealthCmd= is missing")
        if f"Image=localhost/{cfg.service}-{role}:@RELEASE@" not in lines or "Pull=never" not in lines:
            problems.append(f"{where}/quadlets/{name}: needs Image=localhost/{cfg.service}-{role}:@RELEASE@ and Pull=never")
    for name in names:
        if not name.endswith((".pod", ".container")):
            continue
        with open(os.path.join(quadlets, name), encoding="utf-8") as f:
            for n, line in enumerate(f.read().splitlines(), 1):
                if line.endswith("\\"):
                    problems.append(f"{where}/quadlets/{name}:{n}: line continuation")
                if line.split("=", 1)[0] in FORBIDDEN_KEYS:
                    problems.append(f"{where}/quadlets/{name}:{n}: {line.split('=', 1)[0]}= is not allowed")
    if problems:
        raise ReleaseError(f"{where}/ breaks the d0b contract:\n  " + "\n  ".join(problems))


# --- bundle ------------------------------------------------------------

def tracked_files(root, src):
    """Versioned files below src (or src itself), checked like regular_files().

    Only what git tracks: ignored data next to the sources (caches, local
    databases, .env) never ends up in a bundle.
    """
    out = git(root, "ls-files", "-z", "--cached", "--", src)
    files = []
    for rel in sorted(p for p in out.split("\0") if p):
        path = os.path.join(root, rel)
        if any(c in rel for c in "\r\n\\"):
            raise ReleaseError(f"{rel}: name contains CR, LF or backslash")
        try:
            st = os.lstat(path)
        except FileNotFoundError:
            continue  # deleted in the working tree
        if not stat.S_ISREG(st.st_mode):
            raise ReleaseError(f"{rel}: not a regular file (link or special file)")
        if st.st_nlink != 1:
            raise ReleaseError(f"{rel}: hardlink")
        files.append(rel)
    return files


def _copy(root, src, dest, deploy, skip=lambda rel: False):
    files = tracked_files(root, src)
    if not files:
        # An empty folder the Containerfile can still COPY.
        os.makedirs(os.path.join(deploy, dest), exist_ok=True)
        return
    for rel in files:
        inner = os.path.relpath(rel, src)
        target = os.path.join(deploy, dest) if inner == "." else os.path.join(deploy, dest, inner)
        if skip(rel):
            continue
        os.makedirs(os.path.dirname(target), exist_ok=True)
        shutil.copyfile(os.path.join(root, rel), target)


def prepare_bundle(root, cfg, tag, commit):
    """deploy/ from containers/ and the --include list, rebuilt from scratch."""
    validate_tag(tag)
    if not cfg.includes:
        raise ReleaseError(f"profile {cfg.profile} has no default contents; name them with --include")
    deploy = os.path.join(root, "deploy")
    shutil.rmtree(deploy, ignore_errors=True)
    os.makedirs(deploy)
    # REUSE sidecars are for the repo, not for d0b.
    _copy(root, cfg.containers, "containers", deploy, skip=lambda rel: rel.endswith(".license"))
    for src, dest in cfg.includes:
        _copy(root, src, dest, deploy)
    built = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    with open(os.path.join(deploy, "MANIFEST"), "w", encoding="utf-8") as f:
        f.write(f"contract=1\nservice={cfg.service}\nrelease={tag}\ncommit={commit}\n"
                f"built={built}\nprofile={cfg.profile}\nroles={','.join(roles(root, cfg))}\n")
    write_sha256sums(deploy, regular_files(deploy), os.path.join(deploy, "SHA256SUMS"))
    return deploy


def archive_name(cfg, tag):
    return f"{cfg.service}-{tag}.tar.zst"


def archive_bundle(root, cfg, tag):
    validate_tag(tag)
    deploy = os.path.join(root, "deploy")
    regular_files(deploy)
    dist = os.path.join(root, "dist")
    os.makedirs(dist, exist_ok=True)
    name = archive_name(cfg, tag)
    archive = os.path.join(dist, name)
    subprocess.run(["tar", "--zstd", "-cf", archive, "-C", deploy, "."], check=True)
    with open(archive + ".sha256", "w", encoding="utf-8") as f:
        f.write(f"{sha256(archive)}  {name}\n")
    return archive


# --- commands ----------------------------------------------------------

def run(command, root):
    print("$ " + " ".join(command), flush=True)
    if subprocess.run(command, cwd=root).returncode != 0:
        raise ReleaseError(f"{' '.join(command)} failed")


def task_command(runner, task):
    return ["mise", "run", task] if runner == "mise" else ["just", task]


def deploy_release(root, cfg, tag, run=run):
    validate_tag(tag)
    assert_tag_matches_version(tag, cfg, read_version(root, cfg))
    assert_clean_worktree(root)
    assert_tag_on_head(root, tag, PREPARE_TASK)
    check_contract(root, cfg)
    for task in cfg.preflight:
        run(task_command(cfg.runner, task), root)
    prepare_bundle(root, cfg, tag, git(root, "rev-parse", "HEAD"))
    return archive_bundle(root, cfg, tag)


def build_bundle(root, cfg, tag="dev"):
    validate_tag(tag)
    check_contract(root, cfg)
    prepare_bundle(root, cfg, tag, git(root, "rev-parse", "HEAD"))
    return archive_bundle(root, cfg, tag)


def options(argv):
    parser = argparse.ArgumentParser(
        prog="cloud_release.py",
        description="Releases of an app's server on d0b. The options belong in the app's "
                    "cloud:release:* and cloud:build tasks; on the command line only the "
                    "command and the version or tag.")
    parser.add_argument("command", choices=("prepare", "deploy", "build"))
    parser.add_argument("arg", nargs="?", help="prepare: the version; deploy: the tag; build: a tag (default dev)")
    parser.add_argument("--service", required=True, help="d0b service = Linux user = pod, e.g. fitfishcloud")
    parser.add_argument("--profile", default="pocketbase", help="MANIFEST profile (default: pocketbase)")
    parser.add_argument("--include", metavar="SRC[:DEST]", action="append", default=[],
                        help="versioned files or folder for the bundle, at DEST (default SRC); "
                             "replaces the profile's defaults (repeatable)")
    parser.add_argument("--version-file", default="server/VERSION", help="default: server/VERSION")
    parser.add_argument("--containers", default="containers", help="default: containers")
    parser.add_argument("--preflight", metavar="TASK", action="append", default=[],
                        help="task that must pass before deploy builds, e.g. cloud:test (repeatable)")
    parser.add_argument("--runner", choices=("mise", "just"), default="mise")
    parser.add_argument("--root", help="the app's directory (default: the current repository)")
    args = parser.parse_args(argv)
    if args.command in ("prepare", "deploy") and not args.arg:
        parser.error(f"{args.command} needs a {'version' if args.command == 'prepare' else 'tag'}")
    return args


def main(argv):
    args = options(argv)
    try:
        root = os.path.abspath(args.root) if args.root else git(os.getcwd(), "rev-parse", "--show-toplevel")
        cfg = Config(args.service, args.profile, args.include, args.version_file,
                     args.containers, args.preflight, args.runner)
        if args.command == "prepare":
            tag = prepare_release(root, cfg, args.arg)
            print(f"commit and tag {tag} created. Next:\n"
                  f"  mise run cloud:release:deploy -- {tag}\n"
                  f"  mise run cloud:test:containers\n"
                  f"  git push origin <branch> && git push origin {tag}")
        elif args.command == "deploy":
            print(f"-> {os.path.relpath(deploy_release(root, cfg, args.arg), root)}")
        else:
            print(f"-> {os.path.relpath(build_bundle(root, cfg, args.arg or 'dev'), root)}")
    except ReleaseError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
