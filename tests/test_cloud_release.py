# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for scripts/cloud_release.py and release_lib.py.

    python3 -m unittest discover -s tests -p 'test_*release.py'
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "scripts"))

import cloud_release as cr  # noqa: E402

LICENSE = "SPDX-FileCopyrightText: 2026 Test\nSPDX-License-Identifier: GPL-3.0-or-later\n"
QUADLET = """# SPDX-FileCopyrightText: 2026 Test
# SPDX-License-Identifier: GPL-3.0-or-later

[Container]
ContainerName=demo-pb
Pod=demo.pod
Image=localhost/demo-pb:@RELEASE@
Pull=never
ReadOnly=true
NoNewPrivileges=true
DropCapability=all
HealthCmd=wget -q -O /dev/null http://127.0.0.1:8090/api/health
Notify=healthy
"""
FILES = {
    "containers/quadlets/demo.pod": "[Pod]\nPodName=demo\n",
    "containers/quadlets/demo-pb.container": QUADLET,
    "containers/pb.Containerfile": "FROM docker.io/library/alpine:3\n",
    "containers/build.env": "PB_VERSION=0.40.6\n",
    "containers/build.env.license": LICENSE,
    "containers/smoke.txt": "GET /api/health 200\nGET /_/ 403\n",
    "containers/smoke.txt.license": LICENSE,
    "containers/env/pb.env.example": "# comment\n",
    "server/pb_migrations/1_init.js": "migrate(() => {})\n",
    "server/pb_public/index.html": "<p>hi</p>\n",
    "server/VERSION": "1.0.0\n",
    ".gitignore": "/deploy/\n/dist/\n/server/pb_data/\n",
}
CFG = cr.Config("demo")


def git(root, *args):
    return subprocess.run(["git", "-C", root, *args], check=True,
                          capture_output=True, text=True).stdout.strip()


class Versions(unittest.TestCase):
    def test_valid(self):
        for v in ("0.0.0", "1.2.3", "1.2.0-beta.1", "1.0.0-rc.10", "1.0.0-0a", "1.0.0-x-y"):
            cr.parse_version(v)

    def test_invalid(self):
        for v in ("1.2", "v1.2.3", "01.2.3", "1.2.3-01", "1.2.3+build.1", "1.2.3-", "1.2.3-beta..1", ""):
            with self.assertRaises(cr.ReleaseError, msg=v):
                cr.parse_version(v)

    def test_precedence(self):
        ordered = ["1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta",
                   "1.0.0-beta.2", "1.0.0-beta.10", "1.0.0-rc.1", "1.0.0", "1.0.1", "1.1.0", "2.0.0"]
        for a, b in zip(ordered, ordered[1:]):
            self.assertEqual(cr.compare_versions(a, b), -1, f"{a} < {b}")
            self.assertEqual(cr.compare_versions(b, a), 1, f"{b} > {a}")
        self.assertEqual(cr.compare_versions("1.0.0-beta.2", "1.0.0-beta.2"), 0)

    def test_tag(self):
        self.assertEqual(cr.release_tag("1.1.0"), "cloud-v1.1.0")
        cr.assert_tag_matches_version("cloud-v1.1.0", CFG, "1.1.0")
        for tag in ("v1.1.0", "cloud-v1.1.1", "cloud-1.1.0", "cloud-v1.1.0-rc.1"):
            with self.assertRaises(cr.ReleaseError, msg=tag):
                cr.assert_tag_matches_version(tag, CFG, "1.1.0")

    def test_validate_tag(self):
        for tag in ("cloud-v1.1.0", "pr-12", "dev", "cloud-v1.0.0-beta.1"):
            cr.validate_tag(tag)
        for tag in ("", "-x", ".x", "../x", "a/b", "a+b", "x" * 65):
            with self.assertRaises(cr.ReleaseError, msg=tag):
                cr.validate_tag(tag)

    def test_includes(self):
        self.assertEqual(cr.Config("x").includes[0], ("server/pb_migrations", "pb/pb_migrations"))
        self.assertEqual(cr.Config("x", "go", ["server", "LICENSE"]).includes,
                         [("server", "server"), ("LICENSE", "LICENSE")])
        for bad in ("../x", "/etc", "x:containers/y", "x:MANIFEST", ":x"):
            with self.assertRaises(cr.ReleaseError, msg=bad):
                cr.parse_include(bad)


class Repo(unittest.TestCase):
    """A throwaway app repo with a PocketBase server."""

    def setUp(self):
        self.root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.root)
        for rel, text in FILES.items():
            self.write(rel, text)
        git(self.root, "init", "-q", "-b", "main")
        for key, value in (("user.name", "Test"), ("user.email", "test@example.invalid"),
                           ("commit.gpgSign", "false"), ("tag.gpgSign", "false")):
            git(self.root, "config", key, value)
        git(self.root, "add", ".")
        git(self.root, "commit", "-q", "-m", "init")

    def write(self, rel, text):
        path = os.path.join(self.root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)


class Prepare(Repo):
    def test_one_commit_and_annotated_tag(self):
        before = git(self.root, "rev-parse", "HEAD")
        tag = cr.prepare_release(self.root, CFG, "1.1.0")
        self.assertEqual(tag, "cloud-v1.1.0")
        self.assertEqual(git(self.root, "rev-list", "--count", f"{before}..HEAD"), "1")
        self.assertEqual(git(self.root, "diff", "--name-only", before, "HEAD"), "server/VERSION")
        self.assertEqual(cr.read_version(self.root, CFG), "1.1.0")
        self.assertEqual(git(self.root, "cat-file", "-t", tag), "tag")
        self.assertEqual(git(self.root, "rev-parse", f"{tag}^{{commit}}"), git(self.root, "rev-parse", "HEAD"))
        self.assertEqual(git(self.root, "log", "-1", "--format=%s"), "(c) Cloud release 1.1.0")

    def test_dirty_tree_fails(self):
        self.write("server/pb_public/x.html", "x")
        with self.assertRaisesRegex(cr.ReleaseError, "not clean"):
            cr.prepare_release(self.root, CFG, "1.1.0")

    def test_not_greater_fails(self):
        for v in ("1.0.0", "0.9.0", "1.0.0-rc.1"):
            with self.assertRaisesRegex(cr.ReleaseError, "not greater", msg=v):
                cr.prepare_release(self.root, CFG, v)

    def test_existing_tag_fails(self):
        git(self.root, "tag", "cloud-v1.1.0")
        with self.assertRaisesRegex(cr.ReleaseError, "already exists"):
            cr.prepare_release(self.root, CFG, "1.1.0")

    def test_missing_version_file(self):
        with self.assertRaisesRegex(cr.ReleaseError, "0.0.0"):
            cr.prepare_release(self.root, cr.Config("demo", version_file="cloud/VERSION"), "1.1.0")


class Deploy(Repo):
    def test_bundle_from_tag(self):
        tag = cr.prepare_release(self.root, CFG, "1.1.0")
        # Ignored data next to the sources must not end up in the bundle.
        self.write("server/pb_data/data.db", "secret")
        archive = cr.deploy_release(self.root, CFG, tag)
        self.assertEqual(os.path.basename(archive), "demo-cloud-v1.1.0.tar.zst")
        self.assertTrue(os.path.isfile(archive + ".sha256"))

        deploy = os.path.join(self.root, "deploy")
        files = cr.regular_files(deploy)
        self.assertEqual(files, [
            "MANIFEST", "SHA256SUMS",
            "containers/build.env", "containers/env/pb.env.example", "containers/pb.Containerfile",
            "containers/quadlets/demo-pb.container", "containers/quadlets/demo.pod", "containers/smoke.txt",
            "pb/pb_migrations/1_init.js", "pb/pb_public/index.html",
        ])
        self.assertTrue(os.path.isdir(os.path.join(deploy, "pb", "pb_hooks")))

        with open(os.path.join(deploy, "SHA256SUMS")) as f:
            summed = sorted(line.split("  ./", 1)[1].rstrip("\n") for line in f)
        self.assertEqual(summed, [rel for rel in files if rel != "SHA256SUMS"])

        with open(os.path.join(deploy, "MANIFEST")) as f:
            manifest = dict(line.rstrip("\n").split("=", 1) for line in f)
        self.assertEqual(manifest["contract"], "1")
        self.assertEqual(manifest["service"], "demo")
        self.assertEqual(manifest["release"], tag)
        self.assertEqual(manifest["commit"], git(self.root, "rev-parse", "HEAD"))
        self.assertEqual(manifest["profile"], "pocketbase")
        self.assertEqual(manifest["roles"], "pb")

    def test_tag_must_match_version(self):
        git(self.root, "tag", "-a", "cloud-v1.1.0", "-m", "x")
        with self.assertRaisesRegex(cr.ReleaseError, "does not match"):
            cr.deploy_release(self.root, CFG, "cloud-v1.1.0")

    def test_dirty_tree_fails(self):
        tag = cr.prepare_release(self.root, CFG, "1.1.0")
        self.write("containers/extra.txt", "x")
        with self.assertRaisesRegex(cr.ReleaseError, "not clean"):
            cr.deploy_release(self.root, CFG, tag)

    def test_head_must_be_the_tag(self):
        tag = cr.prepare_release(self.root, CFG, "1.1.0")
        self.write("README", "x")
        git(self.root, "add", "README")
        git(self.root, "commit", "-q", "-m", "later")
        with self.assertRaisesRegex(cr.ReleaseError, "HEAD is not"):
            cr.deploy_release(self.root, CFG, tag)

    def test_preflight_runs_before_the_bundle(self):
        tag = cr.prepare_release(self.root, CFG, "1.1.0")
        seen = []

        def fail(command, root):
            seen.append(command)
            raise cr.ReleaseError("preflight failed")
        cfg = cr.Config("demo", preflight=["cloud:test"])
        with self.assertRaisesRegex(cr.ReleaseError, "preflight failed"):
            cr.deploy_release(self.root, cfg, tag, run=fail)
        self.assertEqual(seen, [["mise", "run", "cloud:test"]])
        self.assertFalse(os.path.exists(os.path.join(self.root, "deploy")))

    def test_links_are_rejected(self):
        os.symlink("index.html", os.path.join(self.root, "server/pb_public/link.html"))
        git(self.root, "add", "server/pb_public/link.html")
        with self.assertRaisesRegex(cr.ReleaseError, "not a regular file"):
            cr.build_bundle(self.root, CFG, "pr-1")
        git(self.root, "rm", "-q", "--cached", "server/pb_public/link.html")
        os.remove(os.path.join(self.root, "server/pb_public/link.html"))
        os.link(os.path.join(self.root, "server/pb_public/index.html"),
                os.path.join(self.root, "server/pb_public/hard.html"))
        git(self.root, "add", "server/pb_public/hard.html")
        with self.assertRaisesRegex(cr.ReleaseError, "hardlink"):
            cr.build_bundle(self.root, CFG, "pr-1")

    def test_other_profile_with_includes(self):
        self.write("LICENSE", "GPL")
        git(self.root, "add", "LICENSE")
        cfg = cr.Config("demo", "go", ["server", "LICENSE"])
        cr.build_bundle(self.root, cfg)
        files = cr.regular_files(os.path.join(self.root, "deploy"))
        self.assertIn("server/pb_public/index.html", files)
        self.assertIn("LICENSE", files)
        self.assertFalse(any(f.startswith("pb/") for f in files))
        with self.assertRaisesRegex(cr.ReleaseError, "--include"):
            cr.build_bundle(self.root, cr.Config("demo", "go"))

    def test_archive_holds_the_bundle(self):
        archive = cr.build_bundle(self.root, CFG)
        self.assertTrue(archive.endswith("demo-dev.tar.zst"))
        with tempfile.TemporaryDirectory() as out:
            subprocess.run(["tar", "--zstd", "-xf", archive, "-C", out], check=True)
            self.assertEqual(cr.regular_files(out), cr.regular_files(os.path.join(self.root, "deploy")))


class Contract(Repo):
    def test_fixture_holds_it(self):
        cr.check_contract(self.root, CFG)

    def test_quadlet_without_read_only_fails(self):
        path = "containers/quadlets/demo-pb.container"
        self.write(path, QUADLET.replace("ReadOnly=true\n", "Network=host\n"))
        with self.assertRaises(cr.ReleaseError) as e:
            cr.check_contract(self.root, CFG)
        self.assertIn("ReadOnly=true is missing", str(e.exception))
        self.assertIn("Network= is not allowed", str(e.exception))

    def test_bad_build_env_and_smoke_fail(self):
        self.write("containers/build.env", "# comment\nPB_VERSION=0.40.6\n")
        self.write("containers/smoke.txt", "POST /api/health 200\n")
        with self.assertRaises(cr.ReleaseError) as e:
            cr.check_contract(self.root, CFG)
        self.assertIn("build.env:1", str(e.exception))
        self.assertIn("smoke.txt:1", str(e.exception))


if __name__ == "__main__":
    unittest.main()
