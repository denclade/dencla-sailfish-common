#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

"""Exercise screenshot transfer and cleanup without touching an emulator."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


CAPTURE = Path(__file__).resolve().parents[1] / "scripts/capture-screenshot.py"


class ScreenshotTests(unittest.TestCase):
    def capture(self, failure=""):
        with tempfile.TemporaryDirectory(prefix="sailfish-media-test-") as directory:
            scratch = Path(directory)
            wrapper = scratch / "emulator.py"
            log = scratch / "calls.jsonl"
            output = scratch / "media" / "shot.png"
            wrapper.write_text(f"#!{sys.executable}\n" + '''
import json, os, pathlib, sys
with open(os.environ["MEDIA_TEST_LOG"], "a") as log:
    log.write(json.dumps(sys.argv[1:]) + "\\n")
if sys.argv[1] == "ssh":
    command = sys.argv[2]
    if command.startswith("mktemp"):
        print("/home/testuser/app-screenshot.TEST")
    elif "dbus-send" in command and os.environ.get("MEDIA_TEST_FAIL") == "capture":
        sys.exit(1)
elif sys.argv[1] == "scp":
    if os.environ.get("MEDIA_TEST_FAIL") == "copy":
        sys.exit(1)
    pathlib.Path(sys.argv[3]).write_bytes(b"test screenshot")
''')
            wrapper.chmod(0o755)
            result = subprocess.run(
                [sys.executable, str(CAPTURE), str(output), "--emulator", str(wrapper),
                 "--settle-seconds", "0"],
                env={**os.environ, "MEDIA_TEST_LOG": str(log), "MEDIA_TEST_FAIL": failure},
                capture_output=True, text=True,
            )
            calls = [json.loads(line) for line in log.read_text().splitlines()]
            self.assertEqual(calls[-1], ["ssh", "rm -rf -- /home/testuser/app-screenshot.TEST"])
            self.assertIn("$(id -u)", calls[1][1])
            return result, calls, output.read_bytes() if output.exists() else None

    def test_capture_and_transfer(self):
        result, calls, image = self.capture()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(image, b"test screenshot")
        self.assertEqual(calls[2][0:2], ["scp", ":/home/testuser/app-screenshot.TEST/screenshot.png"])

    def test_capture_failure_cleans_up_without_copying(self):
        result, calls, image = self.capture("capture")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(call[0] == "scp" for call in calls))
        self.assertIsNone(image)

    def test_copy_failure_cleans_up(self):
        result, _, image = self.capture("copy")
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(image)


if __name__ == "__main__":
    unittest.main()
