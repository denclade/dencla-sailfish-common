#!/bin/sh
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The cloud:get task (docs/mise-tasks.md): downloads the PocketBase release
# from containers/build.env (PB_VERSION, PB_SHA256 - the one d0b runs) into
# build/pocketbase/, checksum-verified. Runs from the app's root; does
# nothing when that version is already there.

set -eu
VERSION=$(sed -n 's/^PB_VERSION=//p' containers/build.env)
SHA256=$(sed -n 's/^PB_SHA256=//p' containers/build.env)
[ -n "$VERSION" ] && [ -n "$SHA256" ] || { echo "PB_VERSION and PB_SHA256 missing in containers/build.env" >&2; exit 1; }
DIR=build/pocketbase
if [ -x "$DIR/pocketbase" ] && "$DIR/pocketbase" --version 2>/dev/null | grep -q "$VERSION"; then exit 0; fi
mkdir -p "$DIR"
ZIP=pocketbase_${VERSION}_linux_amd64.zip
curl -sSL -o "$DIR/$ZIP" "https://github.com/pocketbase/pocketbase/releases/download/v$VERSION/$ZIP"
echo "$SHA256  $ZIP" | (cd "$DIR" && sha256sum -c -)
python3 -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extract('pocketbase', sys.argv[2])" "$DIR/$ZIP" "$DIR"
chmod +x "$DIR/pocketbase"
