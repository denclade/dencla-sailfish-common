#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
"""Delivery of a harbour-* app release (tag v<version>), the release:deploy task.

    app_release.py deploy <tag> [--preflight TASK]... [--arch ARCH]... [--runner mise|just]

Checks that the tag is v<Version: of rpm/<app>.spec>, the tree is clean
and HEAD is the tag; runs the preflight tasks; builds the RPM and runs
Jolla's Harbour check for each architecture (sdk-rpm.sh); writes the RPMs,
a source archive with all submodules and SHA256SUMS to dist/<app>-<tag>/.
Version and tag come from release.py (release:prepare); nothing here sets a
version, pushes or uploads. See docs/releasing.md.
"""

import argparse
import io
import os
import re
import shutil
import subprocess
import sys
import tarfile

from release_lib import (ReleaseError, assert_clean_worktree, assert_tag_matches_version,
                         assert_tag_on_head, git, parse_version, regular_files,
                         validate_tag, write_sha256sums)

SCRIPTS = os.path.dirname(os.path.abspath(__file__))
TAG_PREFIX = "v"
PREPARE_TASK = "release:prepare"
# Harbour takes one RPM per architecture.
ARCHES = ("aarch64", "armv7hl", "i486")
DEFAULT_TARGET = "SailfishOS-5.1.0.11-i486"


def find_app(root):
    """The app this repository is, from its single rpm/*.spec."""
    rpm_dir = os.path.join(root, "rpm")
    specs = sorted(f for f in os.listdir(rpm_dir) if f.endswith(".spec")) if os.path.isdir(rpm_dir) else []
    if len(specs) != 1:
        raise ReleaseError(f"expected exactly one rpm/*.spec in {root}, found {len(specs)}")
    return specs[0][:-len(".spec")]


def read_spec(root, app):
    """(version, release) from the .spec, the app's one version source."""
    with open(os.path.join(root, "rpm", f"{app}.spec"), encoding="utf-8") as f:
        text = f.read()
    version = re.search(r"^Version:\s*(\S+)\s*$", text, re.M)
    release = re.search(r"^Release:\s*(\S+)\s*$", text, re.M)
    if not version or not release:
        raise ReleaseError(f"rpm/{app}.spec has no Version: or Release: line")
    parse_version(version[1])
    return version[1], release[1]


def sdk_targets(arches=ARCHES):
    """The given architectures of the SDK release SAILFISH_SDK_TARGET names."""
    current = os.environ.get("SAILFISH_SDK_TARGET") or DEFAULT_TARGET
    base = current.rsplit("-", 1)[0]
    return [f"{base}-{arch}" for arch in arches]


def rpm_name(app, version, release, arch):
    return f"{app}-{version}-{release}.{arch}.rpm"


def task_command(runner, task):
    return ["mise", "run", task] if runner == "mise" else ["just", task]


# --- source archive ----------------------------------------------------

def _add_tree(output, repo, revision, prefix):
    archive = subprocess.check_output(["git", "-C", repo, "archive", "--format=tar", revision])
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        for member in source:
            member.name = prefix + member.name
            if member.islnk():
                member.linkname = prefix + member.linkname
            output.addfile(member, source.extractfile(member) if member.isfile() else None)
    listing = subprocess.check_output(["git", "-C", repo, "ls-tree", "-rz", revision])
    for entry in listing.split(b"\0"):
        if not entry:
            continue
        metadata, name = entry.split(b"\t", 1)
        mode, _, commit = metadata.split()
        if mode != b"160000":
            continue
        relative = name.decode()
        child = os.path.join(repo, relative)
        if not os.path.exists(os.path.join(child, ".git")):
            raise ReleaseError(f"submodule not initialized: {relative}")
        _add_tree(output, child, commit.decode(), prefix + relative + "/")


def source_archive(root, app, version, destination):
    """HEAD with the exact recursive submodule sources; a GitHub archive lacks them."""
    temporary = destination + ".tmp"
    try:
        with tarfile.open(temporary, "w:bz2") as output:
            _add_tree(output, root, "HEAD", f"{app}-{version}/")
        os.replace(temporary, destination)
    finally:
        if os.path.exists(temporary):
            os.remove(temporary)
    return destination


# --- deploy ------------------------------------------------------------

def run(command, root):
    print("$ " + " ".join(command), flush=True)
    if subprocess.run(command, cwd=root).returncode != 0:
        raise ReleaseError(f"{' '.join(command)} failed")


def deploy_release(root, tag, preflight=(), runner="mise", targets=None, run=run):
    validate_tag(tag)
    app = find_app(root)
    version, release = read_spec(root, app)
    assert_tag_matches_version(tag, TAG_PREFIX, version, f"rpm/{app}.spec", PREPARE_TASK)
    assert_clean_worktree(root)
    assert_tag_on_head(root, tag, PREPARE_TASK)
    for task in preflight:
        run(task_command(runner, task), root)

    sdk_rpm = os.path.join(SCRIPTS, "sdk-rpm.sh")
    rpms = []
    for target in targets or sdk_targets():
        name = rpm_name(app, version, release, target.rsplit("-", 1)[1])
        path = os.path.join(root, "RPMS", name)
        # Never pick up an RPM of an earlier build.
        if os.path.exists(path):
            os.remove(path)
        run([sdk_rpm, "build", target], root)
        if not os.path.isfile(path):
            raise ReleaseError(f"the build for {target} did not produce RPMS/{name}")
        run([sdk_rpm, "check", target, f"RPMS/{name}"], root)
        rpms.append(path)
    # The build ran in the working tree; it must still be the tagged commit.
    assert_clean_worktree(root)

    out = os.path.join(root, "dist", f"{app}-{tag}")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    for path in rpms:
        shutil.copyfile(path, os.path.join(out, os.path.basename(path)))
    source_archive(root, app, version, os.path.join(out, f"{app}-{version}-source.tar.bz2"))
    write_sha256sums(out, regular_files(out), os.path.join(out, "SHA256SUMS"))
    return out


def options(argv):
    parser = argparse.ArgumentParser(
        prog="app_release.py",
        description="Builds the delivery of a tagged app release into dist/<app>-<tag>/. "
                    "The options belong in the app's release:deploy task.")
    parser.add_argument("command", choices=("deploy",))
    parser.add_argument("tag", help="v<Version: of the .spec>, e.g. v1.1.0")
    parser.add_argument("--preflight", metavar="TASK", action="append", default=[],
                        help="task that must pass before building, e.g. check:legal (repeatable)")
    parser.add_argument("--arch", action="append", choices=ARCHES,
                        help=f"architecture to build (repeatable; default: {' '.join(ARCHES)})")
    parser.add_argument("--runner", choices=("mise", "just"), default="mise",
                        help="task runner for --preflight (default: mise)")
    parser.add_argument("--root", help="the app's directory (default: the current repository)")
    return parser.parse_args(argv)


def main(argv):
    args = options(argv)
    try:
        root = os.path.abspath(args.root) if args.root else git(os.getcwd(), "rev-parse", "--show-toplevel")
        out = deploy_release(root, args.tag, args.preflight, args.runner,
                             sdk_targets(args.arch or ARCHES))
        print(f"-> {os.path.relpath(out, root)}/")
        for name in regular_files(out):
            print(f"   {name}")
    except ReleaseError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
