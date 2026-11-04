# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for scripts/app_release.py.

    python3 -m unittest discover -s tests -p 'test_*release.py'


And scripts/rpm-payload-check.py (check:rpm) on RPMs built here. The SDK
build is replaced by a stand-in that writes the RPM; everything
else (tag, spec, git checks, source archive with submodule, dist/) is real.
"""

import gzip
import importlib.util
import os
import shutil
import struct
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

    def test_rpm_checks_run_on_each_rpm(self):
        sdk = FakeSdk()
        ar.deploy_release(self.root, "v1.1.0", run=sdk, targets=TARGETS, rpm_checks=["check:rpm"])
        for arch in ("aarch64", "i486"):
            rpm = f"RPMS/harbour-demo-1.1.0-1.{arch}.rpm"
            harbour = sdk.commands.index([os.path.join(SCRIPTS, "sdk-rpm.sh"), "check",
                                          f"SailfishOS-5.1.0.11-{arch}", rpm])
            self.assertEqual(sdk.commands[harbour + 1], ["mise", "run", "check:rpm", rpm])

    def test_prebuilds_run_before_each_build_with_the_target(self):
        sdk = FakeSdk()
        ar.deploy_release(self.root, "v1.1.0", run=sdk, targets=TARGETS, prebuilds=["rust:build:release"])
        for target in TARGETS:
            build = sdk.commands.index([os.path.join(SCRIPTS, "sdk-rpm.sh"), "build", target])
            self.assertEqual(sdk.commands[build - 1], ["mise", "run", "rust:build:release", target])

    def test_prebuild_leaving_files_fails(self):
        def sdk(command, root):
            FakeSdk.__call__(fake, command, root)
            if command[:3] == ["mise", "run", "prepare:lib"]:
                write(root, "lib.a", "x")
        fake = FakeSdk()
        with self.assertRaisesRegex(ar.ReleaseError, "not clean"):
            ar.deploy_release(self.root, "v1.1.0", run=sdk, targets=TARGETS, prebuilds=["prepare:lib"])
        self.assertFalse(os.path.exists(os.path.join(self.root, "dist")))

    def test_failing_rpm_check_writes_no_dist(self):
        def sdk(command, root):
            FakeSdk.__call__(fake, command, root)
            if command[:3] == ["mise", "run", "check:rpm"]:
                raise ar.ReleaseError("check:rpm failed")
        fake = FakeSdk()
        with self.assertRaisesRegex(ar.ReleaseError, "check:rpm failed"):
            ar.deploy_release(self.root, "v1.1.0", run=sdk, targets=TARGETS, rpm_checks=["check:rpm"])
        self.assertFalse(os.path.exists(os.path.join(self.root, "dist")))

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


def load_payload_check():
    spec = importlib.util.spec_from_file_location(
        "rpm_payload_check", os.path.join(SCRIPTS, "rpm-payload-check.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


pc = load_payload_check()


def rpm_header(tags):
    """An rpm header with string tags, as rpm writes it."""
    store = b""
    index = b""
    for tag, value in tags.items():
        index += struct.pack(">IIII", tag, 6, len(store), 1)
        store += value.encode() + b"\0"
    return b"\x8e\xad\xe8\x01\0\0\0\0" + struct.pack(">II", len(tags), len(store)) + index + store


def cpio(files):
    """A newc cpio archive with ./-prefixed paths and a directory entry."""
    out = b""
    entries = [("./usr/share", 0o040755, b"")] + [("." + p, 0o100644, d) for p, d in files.items()]
    for name, mode, data in entries + [("TRAILER!!!", 0, b"")]:
        raw = name.encode() + b"\0"
        fields = [0, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(raw), 0]
        out += b"070701" + b"".join(b"%08X" % f for f in fields) + raw
        out += b"\0" * (-len(out) % 4) + data
        out += b"\0" * (-len(out) % 4)
    return out


def make_rpm(path, files, name="harbour-demo", compressor="zstd"):
    payload = cpio(files)
    if compressor == "gzip":
        payload = gzip.compress(payload)
    else:
        try:
            from compression import zstd  # Python 3.14+
            payload = zstd.compress(payload)
        except ImportError:
            payload = subprocess.run(["zstd", "-qc"], input=payload, capture_output=True,
                                     check=True).stdout
    signature = rpm_header({})
    signature += b"\0" * (-(96 + len(signature)) % 8)
    lead = b"\xed\xab\xee\xdb" + b"\0" * 92
    with open(path, "wb") as f:
        f.write(lead + signature + rpm_header({1000: name, 1125: compressor}) + payload)


GOOD = {
    "/usr/bin/harbour-demo": b"\x7fELF",
    "/usr/share/harbour-demo/legal/impressum.html": b"<h2>Anbieter</h2><p>Name</p>",
    "/usr/share/harbour-demo/legal/datenschutz.html": b"<h2>Datenschutz</h2>",
    "/usr/share/harbour-demo/licenses/index.json":
        b'{"components": [{"name": "common", "files": ["GPL-3.0-or-later.txt"]}]}',
    "/usr/share/harbour-demo/licenses/GPL-3.0-or-later.txt": b"GNU GENERAL PUBLIC LICENSE",
}


class PayloadCheck(unittest.TestCase):
    """scripts/rpm-payload-check.py (check:rpm) against RPMs built here."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.dir)

    def check(self, files, **kwargs):
        path = os.path.join(self.dir, "demo.rpm")
        make_rpm(path, files, **kwargs)
        name, payload = pc.read_rpm(path)
        return name, payload, pc.problems(name, payload)

    def test_complete_package_passes(self):
        for compressor in ("zstd", "gzip"):
            name, payload, found = self.check(GOOD, compressor=compressor)
            self.assertEqual(name, "harbour-demo")
            self.assertEqual(sorted(payload), sorted(GOOD))
            self.assertEqual(found, [], compressor)

    def test_development_legal_documents_fail(self):
        for marker in ("<p><b>Entwicklungsfassung: …</b></p>", "<!-- DENCLA-LEGAL-PRIVACY-DRAFT -->",
                       "@LEGAL_PROVIDER_NAME@"):
            files = dict(GOOD)
            files["/usr/share/harbour-demo/legal/datenschutz.html"] = marker.encode()
            found = self.check(files)[2]
            self.assertEqual(len(found), 1, marker)
            self.assertIn("datenschutz.html is a development version", found[0])

    def test_missing_documents_and_license_texts_fail(self):
        files = dict(GOOD)
        del files["/usr/share/harbour-demo/legal/impressum.html"]
        files["/usr/share/harbour-demo/licenses/GPL-3.0-or-later.txt"] = b""
        found = self.check(files)[2]
        self.assertTrue(any("impressum.html is missing" in m for m in found), found)
        self.assertTrue(any("GPL-3.0-or-later.txt (for common)" in m for m in found), found)

    def test_missing_index_fails(self):
        files = dict(GOOD)
        del files["/usr/share/harbour-demo/licenses/index.json"]
        self.assertIn("index.json is missing", self.check(files)[2][0])

    def test_not_an_rpm(self):
        path = os.path.join(self.dir, "x.rpm")
        write(self.dir, "x.rpm", "plain text")
        with self.assertRaises(pc.PayloadError):
            pc.read_rpm(path)

    def test_marker_matches_the_generated_preview_notice(self):
        with open(os.path.join(os.path.dirname(HERE), "cmake", "GenerateLegal.cmake"),
                  encoding="utf-8") as f:
            notice = next(line for line in f if line.lstrip().startswith("set(LEGAL_PREVIEW_NOTICE \"<"))
        self.assertIn(pc.LEGAL_DEV_MARKERS[0], notice)


class SdkRpm(unittest.TestCase):
    """scripts/sdk-rpm.sh against a stand-in sfdk that records its arguments."""

    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.root)
        self.log = os.path.join(self.root, "args")
        sfdk = os.path.join(self.root, "sdk", "bin", "sfdk")
        write(self.root, "sdk/bin/sfdk", f'#!/bin/sh\nprintf "%s\\n" "$@" >"{self.log}"\n')
        os.chmod(sfdk, 0o755)

    def build(self, defines=None):
        env = dict(os.environ, SAILFISH_SDK_PATH=os.path.join(self.root, "sdk"))
        env.pop("IN_SAILFISH_SANDBOX", None)
        env.pop("SDK_RPM_DEFINES", None)
        if defines is not None:
            env["SDK_RPM_DEFINES"] = defines
        return subprocess.run([os.path.join(SCRIPTS, "sdk-rpm.sh"), "build", TARGETS[1]],
                              cwd=self.root, env=env, capture_output=True, text=True)

    def args(self):
        with open(self.log) as f:
            return f.read().splitlines()

    def test_plain_build(self):
        self.assertEqual(self.build().returncode, 0)
        self.assertEqual(self.args(), ["-c", f"target={TARGETS[1]}", "build"])

    def test_defines_reach_rpmbuild(self):
        self.assertEqual(self.build("demo_distribution 1; ;other_flag x y").returncode, 0)
        self.assertEqual(self.args(), ["-c", f"target={TARGETS[1]}", "build", "--",
                                       "--define", "demo_distribution 1",
                                       "--define", "other_flag x y"])

    def test_bad_define_fails_before_building(self):
        for bad in ("novalue", "bad-name 1", "1x 1"):
            result = self.build(bad)
            self.assertEqual(result.returncode, 2, bad)
            self.assertIn("SDK_RPM_DEFINES", result.stderr)
        self.assertFalse(os.path.exists(self.log))


if __name__ == "__main__":
    unittest.main()
