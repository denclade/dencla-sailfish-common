# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
"""What app_release.py and cloud_release.py share: versions, tags, git checks, files.

See docs/releasing.md.
"""

import hashlib
import os
import re
import stat
import subprocess

# SemVer 2.0.0 without build metadata: d0b tags do not allow "+".
SEMVER = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-((?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*)"
    r"(?:\.(?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*))*))?$")
TAG = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")


class ReleaseError(Exception):
    pass


# --- versions and tags -------------------------------------------------

def parse_version(text):
    match = SEMVER.match(text)
    if not match:
        raise ReleaseError(f'"{text}" is not a SemVer version without build metadata, e.g. 1.2.0 or 1.2.0-beta.1')
    pre = match[4].split(".") if match[4] else []
    return int(match[1]), int(match[2]), int(match[3]), pre


def _pre_key(part):
    # Numeric identifiers compare numerically and sort before alphanumeric ones.
    return (0, int(part), "") if part.isdigit() else (1, 0, part)


def compare_versions(a, b):
    """-1, 0 or 1 by SemVer precedence."""
    va, vb = parse_version(a), parse_version(b)
    if va[:3] != vb[:3]:
        return -1 if va[:3] < vb[:3] else 1
    pa, pb = va[3], vb[3]
    if pa == pb:
        return 0
    # A release ranks above its pre-releases.
    if not pa:
        return 1
    if not pb:
        return -1
    for x, y in zip(pa, pb):
        if x != y:
            return -1 if _pre_key(x) < _pre_key(y) else 1
    return -1 if len(pa) < len(pb) else 1


def validate_tag(tag):
    # Same rule as d0b: cannot leave a path, fits a MANIFEST.
    if not TAG.match(tag):
        raise ReleaseError(f'invalid tag "{tag}": 1-64 characters [A-Za-z0-9._-], starting with a letter or digit')


def assert_tag_matches_version(tag, prefix, version, source, prepare_task):
    if tag != prefix + version:
        raise ReleaseError(
            f"tag {tag} does not match {source} ({version}, tag {prefix}{version}). "
            f"Set a new version with: mise run {prepare_task} -- <version>")


# --- git ---------------------------------------------------------------

def git(root, *args, check=True):
    result = subprocess.run(["git", "-C", root, *args], capture_output=True, text=True)
    if check and result.returncode != 0:
        raise ReleaseError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout.strip() if result.returncode == 0 else None


def assert_clean_worktree(root):
    if git(root, "status", "--porcelain", "--untracked-files=all"):
        raise ReleaseError("the working tree is not clean; commit first")


def tag_exists(root, tag):
    return git(root, "rev-parse", "-q", "--verify", f"refs/tags/{tag}", check=False) is not None


def assert_tag_on_head(root, tag, prepare_task):
    commit = git(root, "rev-parse", "-q", "--verify", f"refs/tags/{tag}^{{commit}}", check=False)
    if commit is None:
        raise ReleaseError(f"tag {tag} does not exist; create it with {prepare_task}")
    if commit != git(root, "rev-parse", "HEAD"):
        raise ReleaseError(f"HEAD is not {tag}; check out the tag first")


# --- files -------------------------------------------------------------

def regular_files(base):
    """Relative paths of all files below base; rejects links and special files.

    Checked before copying (a copy would resolve links) and again before
    archiving.
    """
    found = []
    for dirpath, dirnames, filenames in os.walk(base):
        for name in dirnames + filenames:
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, base)
            if any(c in name for c in "\r\n\\"):
                raise ReleaseError(f"{path}: name contains CR, LF or backslash")
            st = os.lstat(path)
            if stat.S_ISDIR(st.st_mode):
                continue
            if not stat.S_ISREG(st.st_mode):
                raise ReleaseError(f"{path}: not a regular file (link or special file)")
            if st.st_nlink != 1:
                raise ReleaseError(f"{path}: hardlink")
            found.append(rel)
    return sorted(found)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def write_sha256sums(base, files, target):
    """'<sha256>  ./<path>' for each file, sorted."""
    with open(target, "w", encoding="utf-8") as f:
        for rel in sorted(files):
            f.write(f"{sha256(os.path.join(base, rel))}  ./{rel}\n")
