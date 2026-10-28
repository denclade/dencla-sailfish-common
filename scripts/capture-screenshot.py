#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

"""Capture the Sailfish emulator through the app's SSH/SCP wrapper."""

import argparse
from pathlib import Path
import shlex
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--emulator", type=Path, default=Path("scripts/emulator.sh"))
    parser.add_argument("--settle-seconds", type=float, default=5)
    args = parser.parse_args()
    if not 0 <= args.settle_seconds <= 60:
        parser.error("--settle-seconds must be between 0 and 60")
    wrapper = str(args.emulator.resolve())

    def remote(command):
        return subprocess.check_output([wrapper, "ssh", command], text=True).strip()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    time.sleep(args.settle_seconds)
    # Lipstick accepts only paths under the user's home, refuses overwrite and rejects any
    # hidden path component, so the temporary directory must not start with a dot.
    directory = remote('mktemp -d "$HOME/app-screenshot.XXXXXX"')
    if not directory.startswith("/") or not Path(directory).name.startswith("app-screenshot."):
        raise RuntimeError(f"Unexpected remote temporary directory: {directory!r}")
    image = directory + "/screenshot.png"
    try:
        remote(
            'sudo env DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$(id -u)/dbus/user_bus_socket '
            'dbus-send --print-reply --dest=org.nemomobile.lipstick '
            '/org/nemomobile/lipstick/screenshot '
            'org.nemomobile.lipstick.saveScreenshot '
            + shlex.quote("string:" + image) + ' >/dev/null'
        )
        subprocess.run([wrapper, "scp", ":" + image, str(args.output.resolve())], check=True)
    finally:
        remote("rm -rf -- " + shlex.quote(directory))
    print(f"Saved to {args.output}")


if __name__ == "__main__":
    main()
