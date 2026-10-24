#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

"""One release of a harbour-* app: version up, changelog, commit, tag.

    mise run release <patch|minor|major|X.Y.Z> [--dry-run] [--skip-checks] [--push]

The per-app options belong in the application's release task. Select
--runner mise for mise tasks; existing just recipes remain supported:

  1. check: on the release branch, clean tree, not behind origin, every
     --require-release submodule at a tagged release, the tag still free
  2. run required --preflight tasks, then optional --test tasks
  3. set the version -- the .spec's Version (the one source the build
     reads, see the app's CMakeLists.txt) and any --mirror-version file,
     then the --sync command that regenerates derived files
  4. CHANGELOG.md: "Unreleased" becomes the new version, built from the
     commits since the last tag when nobody wrote it by hand; the same
     lines go into rpm/<app>.changes, the package's changelog
  5. commit "(c) Release X.Y.Z" and an annotated tag vX.Y.Z

Pushes only with --push. A published tag is never moved or deleted.

What the app is, is read from the repository rather than configured: the
single rpm/*.spec names it, and CHANGELOG.md and rpm/<app>.changes sit
where they always sit.
"""

import argparse
import datetime
import os
import re
import subprocess
import sys

# Which changelog heading a commit prefix belongs under, in the order
# Keep a Changelog lists them. Set per app with --lang (the apps whose
# changelog is written in German) or, for a single wording, --heading.
HEADINGS = {
    "en": [("+", "Added"), ("c", "Changed"), ("f", "Fixed"), ("-", "Removed")],
    "de": [("+", "Neu"), ("c", "Geändert"), ("f", "Behoben"), ("-", "Entfernt")],
}

# Only the two strings that end up in a released file, so a German
# changelog stays German. Everything the script says to its operator is
# English, like the rest of the code in these projects.
WORDING = {
    "en": {"core": "Core", "first": "first release"},
    "de": {"core": "Kern", "first": "erstes Release"},
}

CHANGELOG_HEADER = {
    "en": """# Changelog

All notable changes to {name}, by version. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), the versions
follow [Semantic Versioning](https://semver.org/). How a release is made:
docs/versioning.md.

## [Unreleased]
""",
    "de": """# Changelog

Alle nennenswerten Änderungen an {name}, nach Version. Format nach
[Keep a Changelog](https://keepachangelog.com/de/1.1.0/), Versionen nach
[Semantic Versioning](https://semver.org/lang/de/). Wie ein Release
entsteht: docs/versioning.md.

## [Unreleased]
""",
}


def die(message):
    print(f"\n✗ {message}\n", file=sys.stderr)
    sys.exit(1)


class Repo:
    def __init__(self, root):
        self.root = root

    def git(self, *args, cwd=None, check=True):
        result = subprocess.run(["git", *args], cwd=cwd or self.root,
                                capture_output=True, text=True)
        if check and result.returncode != 0:
            die(f"git {' '.join(args)}: {result.stderr.strip()}")
        return result.stdout.rstrip("\n") if result.returncode == 0 else None

    def read(self, path):
        with open(os.path.join(self.root, path), encoding="utf-8") as f:
            return f.read()

    def write(self, path, text):
        with open(os.path.join(self.root, path), "w", encoding="utf-8") as f:
            f.write(text)

    def exists(self, path):
        return os.path.exists(os.path.join(self.root, path))


# --- versions -------------------------------------------------------------

SEMVER = re.compile(r"^(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?$")


def parse(version):
    match = SEMVER.match(version)
    if not match:
        die(f'"{version}" is not a version of the form X.Y.Z (or X.Y.Z-rc.N).')
    return int(match[1]), int(match[2]), int(match[3]), match[4]


def bump(current, how):
    major, minor, patch, pre = parse(current)
    if how == "major":
        return f"{major + 1}.0.0"
    if how == "minor":
        return f"{major}.{minor + 1}.0"
    if how == "patch":
        # From a pre-release, "patch" is the version itself: 1.0.0-rc.2 -> 1.0.0.
        return f"{major}.{minor}.{patch}" if pre else f"{major}.{minor}.{patch + 1}"
    parse(how)
    return how


def pre_key(pre):
    return [(0, int(part), "") if part.isdigit() else (1, 0, part) for part in pre.split(".")]


def is_newer(next_version, current):
    a, b = parse(next_version), parse(current)
    if a[:3] != b[:3]:
        return a[:3] > b[:3]
    if a[3] == b[3]:
        return False
    if a[3] is None:
        return True
    if b[3] is None:
        return False
    return pre_key(a[3]) > pre_key(b[3])


# The .spec is what the build reads (rpm passes it to CMake as
# APP_VERSION, and CMake falls back to reading the .spec itself when built
# without rpm). A --mirror-version file says the same version a second
# time, for a crate or package that carries its own.
SPEC_VERSION = re.compile(r"^(Version:\s*)(\S+)$", re.M)
SPEC_RELEASE = re.compile(r"^(Release:\s*)(\S+)$", re.M)
MIRROR_VERSION = re.compile(r'^(version = ")([^"]+)(")$', re.M)


def current_version(repo, spec, mirrors):
    found = SPEC_VERSION.search(repo.read(spec))
    if not found:
        die(f"No Version: line in {spec}.")
    for path in mirrors:
        if not MIRROR_VERSION.search(repo.read(path)):
            die(f'No \'version = "..."\' line in {path}.')
    return found[2]


def write_version(repo, spec, mirrors, next_version):
    text = SPEC_VERSION.sub(lambda m: m[1] + next_version, repo.read(spec), count=1)
    # A new version is the first package of it.
    text = SPEC_RELEASE.sub(lambda m: m[1] + "1", text, count=1)
    repo.write(spec, text)
    for path in mirrors:
        repo.write(path, MIRROR_VERSION.sub(
            lambda m: m[1] + next_version + m[3], repo.read(path), count=1))


# --- changelog ------------------------------------------------------------

def section_from_commits(repo, last_tag, headings, ignore):
    """The commits since the last release, by their prefix -- only when
    nobody wrote "Unreleased" by hand. Submodule moves and earlier
    releases are not entries."""
    range_args = [f"{last_tag}..HEAD"] if last_tag else []
    subjects = [s for s in (repo.git("log", "--no-merges", "--format=%s", *range_args) or "").split("\n") if s]
    # Not entries: an earlier release commit (whether it was named
    # "Release X.Y.Z" or just the bare version, as a hand-made release was),
    # and moving a submodule pointer.
    skip = re.compile(r"^\([+fc-]\)\s*(release\b|v?\d+\.\d+(\.\d+)?$|submodule update" +
                      "".join(f"|{re.escape(name)}" for name in ignore) + r")", re.I)
    groups = {heading: [] for _, heading in headings}
    for subject in subjects:
        if skip.match(subject):
            continue
        match = re.match(r"^\(([+fc-])\)\s*(.*)$", subject)
        default = next(h for p, h in headings if p == "c")
        heading = next((h for p, h in headings if match and p == match[1]), default)
        groups[heading].append(match[2] if match else subject)
    return "\n\n".join(
        f"### {heading}\n\n" + "\n".join(f"- {line}" for line in groups[heading])
        for _, heading in headings if groups[heading])


def changelog(repo, path, name, lang, next_version, last_tag, headings, ignore, extra_lines):
    text = repo.read(path) if repo.exists(path) else CHANGELOG_HEADER[lang].format(name=name)
    marker = "## [Unreleased]"
    at = text.find(marker)
    if at < 0:
        die(f'{path} has no "{marker}" section.')
    after = at + len(marker)
    following = text.find("\n## [", after)
    end = len(text) if following < 0 else following + 1
    written = text[after:end].strip()
    body = "\n\n".join(part for part in [
        "\n".join(extra_lines),
        written or section_from_commits(repo, last_tag, headings, ignore),
    ] if part)
    if not body:
        die("There is nothing to release since the last one.")
    # Whatever this file already puts between version and date - a plain
    # hyphen or an en dash - so the new section matches the ones above it.
    older = re.search(r"^## \[\d[^\]]*\]\s*(\S+)\s*\d{4}-\d{2}-\d{2}", text[end:], re.M)
    separator = older[1] if older else "-"
    date = datetime.date.today().isoformat()
    section = f"## [{next_version}] {separator} {date}\n\n{body}\n"
    updated = re.sub(r"\n{3,}", "\n\n", f"{text[:at]}{marker}\n\n{section}\n{text[end:]}")
    return section, updated


def rpm_changes_entry(repo, next_version, section):
    """The same lines as an rpm changelog entry, newest first in
    rpm/<app>.changes -- what `rpm -q --changelog` and the store show for
    the package."""
    name = repo.git("config", "user.name") or "unknown"
    email = repo.git("config", "user.email") or "unknown"
    # rpm wants English day and month names, whatever the locale.
    today = datetime.date.today()
    day = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"][today.weekday()]
    month = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"][today.month - 1]
    lines = [line[2:] for line in section.split("\n") if line.startswith("- ")]
    return f"* {day} {month} {today.day:02d} {today.year} {name} <{email}> {next_version}-1\n" + \
        "\n".join(f"- {line}" for line in lines) + "\n"


# --- the app ----------------------------------------------------------------

def find_app(root):
    """The app this repository is, from its single rpm/*.spec."""
    rpm_dir = os.path.join(root, "rpm")
    specs = sorted(f for f in os.listdir(rpm_dir) if f.endswith(".spec")) if os.path.isdir(rpm_dir) else []
    if len(specs) != 1:
        die(f"Expected exactly one rpm/*.spec in {root}, found {len(specs)}.\n"
            "  Run this from the app's own directory.")
    return specs[0][:-len(".spec")]


def options(argv):
    parser = argparse.ArgumentParser(
        prog="release.py", add_help=True,
        description="Cuts a release of a harbour-* app. The options below belong in the "
                    "app's release task; on the command line only the version and the flags.")
    parser.add_argument("version", metavar="patch|minor|major|X.Y.Z",
                        help="which version comes next")
    parser.add_argument("--dry-run", action="store_true",
                        help="show the new version and changelog section, write and test nothing")
    parser.add_argument("--skip-checks", action="store_true",
                        help="skip the tests - only when they just ran")
    parser.add_argument("--push", action="store_true",
                        help="push the branch and the tag when everything worked")
    parser.add_argument("--test", metavar="TASK", action="append", default=[],
                        help="a task to run before releasing (repeatable; default: test)")
    parser.add_argument("--runner", choices=("just", "mise"), default="just",
                        help="task runner (default: just for existing apps; use mise for mise.toml)")
    parser.add_argument("--preflight", metavar="TASK", action="append", default=[],
                        help="required task that also runs with --skip-checks, e.g. legal:check (repeatable)")
    parser.add_argument("--lang", choices=sorted(HEADINGS), default="en",
                        help="the language the changelog is written in (default: en)")
    parser.add_argument("--heading", metavar="PREFIX=TEXT", action="append", default=[],
                        help="override one changelog heading, e.g. +=Hinzugefügt (repeatable)")
    parser.add_argument("--carry", metavar="PATH", action="append", default=[],
                        help="a path that may be uncommitted and goes into the release commit, "
                             "e.g. a submodule pointer (repeatable)")
    parser.add_argument("--require-release", metavar="PATH", action="append", default=[],
                        help="a submodule that must be clean and on a v* tag; its tag becomes a "
                             "changelog line (repeatable, implies --carry)")
    parser.add_argument("--mirror-version", metavar="FILE", action="append", default=[],
                        help='a file with a \'version = "X.Y.Z"\' line to keep in sync (repeatable)')
    parser.add_argument("--sync", metavar="CMD",
                        help="shell command that regenerates files derived from the version "
                             "(a lock file); a failure rolls the release back")
    parser.add_argument("--sync-file", metavar="PATH", action="append", default=[],
                        help="a file --sync rewrites: restored if it fails, committed if it works "
                             "(repeatable)")
    parser.add_argument("--branch", default="master", help="the release branch (default: master)")
    parser.add_argument("--root", help="the app's directory (default: the current repository)")
    args = parser.parse_args(argv)

    args.test = args.test or ["test"]
    args.carry = sorted(set(args.carry) | set(args.require_release) | set(args.sync_file))
    headings = list(HEADINGS[args.lang])
    for override in args.heading:
        prefix, _, text = override.partition("=")
        if prefix not in dict(headings) or not text:
            die(f'--heading wants PREFIX=TEXT with PREFIX one of + c f -, not "{override}".')
        headings = [(p, text if p == prefix else h) for p, h in headings]
    args.headings = headings
    return args


# --- the run ----------------------------------------------------------------

def main(argv):
    args = options(argv)
    root = os.path.abspath(args.root) if args.root else None
    if root is None:
        found = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                               capture_output=True, text=True)
        if found.returncode != 0:
            die("Not inside a git repository.")
        root = found.stdout.strip()
    repo = Repo(root)
    name = find_app(root)
    words = WORDING[args.lang]

    spec = f"rpm/{name}.spec"
    changes = f"rpm/{name}.changes"
    changelog_path = "CHANGELOG.md"

    # 1. preconditions
    branch = repo.git("rev-parse", "--abbrev-ref", "HEAD")
    if branch != args.branch:
        die(f'Releases are made on {args.branch}, not on "{branch}".')
    status = subprocess.run(["git", "status", "--porcelain"], cwd=root,
                            capture_output=True, text=True).stdout
    dirty = [line[3:] for line in status.split("\n") if line and line[3:] not in args.carry]
    if dirty:
        die("The working tree is not clean:\n  " + "\n  ".join(dirty))

    extra_lines = []
    for path in args.require_release:
        full = os.path.join(root, path)
        if repo.git("status", "--porcelain", cwd=full):
            die(f"{path} has uncommitted changes. Clean up and release there first.")
        tag = repo.git("describe", "--tags", "--exact-match", "--match", "v*", "HEAD",
                       cwd=full, check=False)
        if not tag:
            die(f"{path} is not at a release.\n"
                f"  Release it first, move the submodule pointer to its tag, then release here.")
        extra_lines.append(f"{words['core']}: {os.path.basename(path)} {tag}")

    # --no-recurse-submodules: only this repository's own refs matter here, and
    # a submodule that is not checked out would otherwise fail the whole fetch.
    if repo.git("fetch", "--quiet", "--tags", "--no-recurse-submodules", "origin", check=False) is None:
        print(f"! origin unreachable -- whether {args.branch} is up to date stays unchecked.")
    else:
        behind = repo.git("rev-list", "--count", f"HEAD..origin/{args.branch}", check=False)
        if behind and behind != "0":
            die(f"{args.branch} is {behind} commit(s) behind origin/{args.branch}. Pull first.")

    spec_version = current_version(repo, spec, args.mirror_version)
    # A spec that still says "0.1" from the SDK template is no X.Y.Z -- read
    # as 0.1.0. From the first release on it is a real version.
    current = spec_version if SEMVER.match(spec_version) else f"{spec_version}.0"
    next_version = bump(current, args.version)
    if not is_newer(next_version, current):
        die(f"{next_version} is not newer than {current}.")
    tag = f"v{next_version}"
    if repo.git("rev-parse", "-q", "--verify", f"refs/tags/{tag}", check=False):
        die(f"The tag {tag} already exists.")
    last_tag = repo.git("describe", "--tags", "--abbrev=0", "--match", "v*", check=False)

    ignore = [os.path.basename(path) for path in args.carry]
    section, updated = changelog(repo, changelog_path, name, args.lang, next_version,
                                 last_tag, args.headings, ignore, extra_lines)
    since = f", since {last_tag}" if last_tag else f", {words['first']}"
    print(f"\n{name}: {current} → {next_version}  (tag {tag}{since})\n")
    print(section)
    if args.dry_run:
        print("--dry-run: nothing written, nothing tested.")
        return

    # Required publication checks are independent of optional test suites.
    runner = ["mise", "run"] if args.runner == "mise" else ["just"]
    for task in args.preflight:
        print(f"\n→ {' '.join(runner)} {task}")
        if subprocess.run([*runner, task], cwd=root).returncode != 0:
            die(f"Required preflight {task} failed -- no release.")

    # 2. tests
    if not args.skip_checks:
        for task in args.test:
            print(f"\n→ {' '.join(runner)} {task}")
            if subprocess.run([*runner, task], cwd=root).returncode != 0:
                die(f"{' '.join(runner)} {task} failed -- no release.")

    # 3. + 4. version and changelogs. Kept as they were until --sync is
    # through: a release that stops halfway must not leave the version set
    # and nothing committed.
    touched = [spec, *args.mirror_version, *args.sync_file]
    before = {path: repo.read(path) for path in touched if repo.exists(path)}
    write_version(repo, spec, args.mirror_version, next_version)
    if args.sync:
        print(f"\n→ {args.sync}")
        if subprocess.run(args.sync, cwd=root, shell=True, stdout=subprocess.DEVNULL).returncode != 0:
            for path, text in before.items():
                repo.write(path, text)
            die(f"{args.sync} failed -- nothing changed.")
    repo.write(changelog_path, updated)
    old_changes = repo.read(changes) if repo.exists(changes) else ""
    repo.write(changes, rpm_changes_entry(repo, next_version, section) +
               ("\n" + old_changes if old_changes else ""))

    # 5. commit and tag
    repo.git("add", spec, changelog_path, changes, *args.mirror_version, *args.carry)
    repo.git("commit", "--quiet", "-m", f"(c) Release {next_version}")
    # Annotated, never lightweight: a release tag says who, when and what.
    # Signed when git is set up for it (tag.gpgSign).
    notes = "\n".join(section.split("\n")[2:])
    repo.git("tag", "--annotate", tag, "--cleanup=verbatim", "-m", f"{name} {next_version}\n\n{notes}")
    print(f"\n✓ {name} {next_version}: commit and tag {tag} created.")

    commands = [["push", "origin", args.branch], ["push", "origin", tag]]
    if args.push:
        for command in commands:
            repo.git(*command)
        print(f"✓ pushed: {args.branch} and {tag}")
    else:
        print("\nTo publish (a pushed tag stays as it is):")
        for command in commands:
            print("  git " + " ".join(command))
        print("\nBuild the RPM with the application's SDK build task.")


if __name__ == "__main__":
    main(sys.argv[1:])
