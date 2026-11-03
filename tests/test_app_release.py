# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for scripts/app_release.py.

    python3 -m unittest discover -s tests -p 'test_*release.py'


The SDK build is replaced by a stand-in that writes the RPM; everything
else (tag, spec, git checks, source archive with submodule, dist/) is real.
"""

import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "scripts"))

import app_release as ar  # noqa: E402

SCRIPTS = os.path.join(os.path.dirname(HERE), "scripts")
TARGETS = ["SailfishOS-5.1.0.11-aarch64", "SailfishOS-5.1.0.11-i486"]
SPEC = "Name:       harbour-demo\nVersion:    1.1.0\nRelease:    1\n"


def git(root, *args):
    return subprocess.run(["git", "-c", "protocol.file.allow=always", "-C", root, *args],
                          check=True, capture_output=True, text=True).stdout.strip()


def init_repo(root):
    git(root, "init", "-q", "-b", "main")
    for key, value in (("user.name", "Test"), ("user.email", "test@example.invalid"),
                       ("commit.gpgSign", "false"), ("tag.gpgSign", "false")):
        git(root, "config", key, value)


def write(root, rel, text):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


class FakeSdk:
    """Stands in for mise and sdk-rpm.sh."""

    def __init__(self, leave_file=None):
        self.commands = []
        self.leave_file = leave_file

    def __call__(self, command, root):
        self.commands.append(command)
        if command[1:2] == ["build"]:
            arch = command[2].rsplit("-", 1)[1]
            write(root, f"RPMS/harbour-demo-1.1.0-1.{arch}.rpm", f"rpm {arch}")
            if self.leave_file:
                write(root, self.leave_file, "x")


class Spec(unittest.TestCase):
    def test_targets_follow_the_sdk_release(self):
        old = os.environ.get("SAILFISH_SDK_TARGET")
        os.environ["SAILFISH_SDK_TARGET"] = "SailfishOS-5.2.0.1-i486"
        try:
            self.assertEqual(ar.sdk_targets(), ["SailfishOS-5.2.0.1-aarch64",
                                                "SailfishOS-5.2.0.1-armv7hl",
                                                "SailfishOS-5.2.0.1-i486"])
        finally:
            if old is None:
                del os.environ["SAILFISH_SDK_TARGET"]
            else:
                os.environ["SAILFISH_SDK_TARGET"] = old

    def test_arches_can_be_narrowed(self):
        self.assertEqual([t.rsplit("-", 1)[1] for t in ar.sdk_targets(["aarch64"])], ["aarch64"])


class Deploy(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp)
        sub = os.path.join(tmp, "common")
        os.makedirs(sub)
        init_repo(sub)
        write(sub, "lib.txt", "shared")
        git(sub, "add", ".")
        git(sub, "commit", "-q", "-m", "sub")

        self.root = os.path.join(tmp, "app")
        os.makedirs(self.root)
        init_repo(self.root)
        write(self.root, "rpm/harbour-demo.spec", SPEC)
        write(self.root, ".gitignore", "/RPMS/\n/dist/\n")
        git(self.root, "submodule", "add", "-q", sub, "3rdparty/common")
        git(self.root, "add", ".")
        git(self.root, "commit", "-q", "-m", "app")
        git(self.root, "tag", "-a", "v1.1.0", "-m", "harbour-demo 1.1.0")

    def test_dist_holds_rpms_source_and_sums(self):
        sdk = FakeSdk()
        out = ar.deploy_release(self.root, "v1.1.0", preflight=["check:legal"], run=sdk, targets=TARGETS)
        self.assertEqual(out, os.path.join(self.root, "dist", "harbour-demo-v1.1.0"))
        self.assertEqual(ar.regular_files(out), [
            "SHA256SUMS",
            "harbour-demo-1.1.0-1.aarch64.rpm",
            "harbour-demo-1.1.0-1.i486.rpm",
            "harbour-demo-1.1.0-source.tar.bz2",
        ])
        self.assertEqual(sdk.commands[0], ["mise", "run", "check:legal"])
        self.assertIn([os.path.join(SCRIPTS, "sdk-rpm.sh"), "check", TARGETS[0],
                       "RPMS/harbour-demo-1.1.0-1.aarch64.rpm"], sdk.commands)

        with open(os.path.join(out, "SHA256SUMS")) as f:
            summed = sorted(line.split("  ./", 1)[1].rstrip("\n") for line in f)
        self.assertEqual(summed, [n for n in ar.regular_files(out) if n != "SHA256SUMS"])

        with tarfile.open(os.path.join(out, "harbour-demo-1.1.0-source.tar.bz2")) as t:
            names = t.getnames()
        self.assertIn("harbour-demo-1.1.0/rpm/harbour-demo.spec", names)
        self.assertIn("harbour-demo-1.1.0/3rdparty/common/lib.txt", names)

    def test_just_runner(self):
        sdk = FakeSdk()
        ar.deploy_release(self.root, "v1.1.0", preflight=["legal-check"], runner="just",
                          run=sdk, targets=TARGETS)
        self.assertEqual(sdk.commands[0], ["just", "legal-check"])

    def test_tag_must_match_spec(self):
        git(self.root, "tag", "-a", "v1.2.0", "-m", "x")
        with self.assertRaisesRegex(ar.ReleaseError, "does not match"):
            ar.deploy_release(self.root, "v1.2.0", run=FakeSdk(), targets=TARGETS)
        with self.assertRaisesRegex(ar.ReleaseError, "does not match"):
            ar.deploy_release(self.root, "cloud-v1.1.0", run=FakeSdk(), targets=TARGETS)

    def test_dirty_tree_fails_before_building(self):
        write(self.root, "README", "x")
        sdk = FakeSdk()
        with self.assertRaisesRegex(ar.ReleaseError, "not clean"):
            ar.deploy_release(self.root, "v1.1.0", run=sdk, targets=TARGETS)
        self.assertEqual(sdk.commands, [])

    def test_head_must_be_the_tag(self):
        write(self.root, "README", "x")
        git(self.root, "add", "README")
        git(self.root, "commit", "-q", "-m", "later")
        with self.assertRaisesRegex(ar.ReleaseError, "HEAD is not"):
            ar.deploy_release(self.root, "v1.1.0", run=FakeSdk(), targets=TARGETS)

    def test_build_leaving_files_fails(self):
        with self.assertRaisesRegex(ar.ReleaseError, "not clean"):
            ar.deploy_release(self.root, "v1.1.0", run=FakeSdk(leave_file="stray.txt"), targets=TARGETS)
        self.assertFalse(os.path.exists(os.path.join(self.root, "dist")))

    def test_stale_rpm_is_not_picked_up(self):
        write(self.root, "RPMS/harbour-demo-1.1.0-1.i486.rpm", "old")

        def no_rpm(command, root):
            pass
        with self.assertRaisesRegex(ar.ReleaseError, "did not produce"):
            ar.deploy_release(self.root, "v1.1.0", run=no_rpm, targets=TARGETS[1:])


if __name__ == "__main__":
    unittest.main()
