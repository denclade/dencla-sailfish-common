#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
"""What a harbour-* RPM must carry for a release, checked in the package itself.

    rpm-payload-check.py <rpm>

The check:rpm task, which release:deploy runs on each built RPM through
--rpm-check (docs/releasing.md). Reads the RPM directly (standard library
only; no rpm2cpio, which a host may lack) and fails when

  - share/<app>/legal/impressum.html or datenschutz.html is missing, empty,
    or still a development version (the preview notice of
    cmake/GenerateLegal.cmake, the privacy draft marker, an unreplaced
    @LEGAL_...@ placeholder),
  - share/<app>/licenses/index.json is missing or not JSON, or names a
    license file the package does not carry or carries empty.

<app> is the package name from the RPM header.
"""

import gzip
import json
import lzma
import struct
import subprocess
import sys

# Text only a development version of a legal document contains. The first
# is LEGAL_PREVIEW_NOTICE in cmake/GenerateLegal.cmake (a test keeps the two
# in step), the second the draft marker of a privacy template.
LEGAL_DEV_MARKERS = ("Entwicklungsfassung", "DENCLA-LEGAL-PRIVACY-DRAFT", "@LEGAL_")
LEGAL_DOCUMENTS = ("impressum.html", "datenschutz.html")

HEADER_MAGIC = b"\x8e\xad\xe8\x01"
TAG_NAME = 1000
TAG_PAYLOADCOMPRESSOR = 1125
TYPE_STRING = 6


class PayloadError(Exception):
    pass


def _header(data, offset):
    """(tags, end) of the rpm header at offset: string tags only."""
    if data[offset:offset + 4] != HEADER_MAGIC:
        raise PayloadError("not an RPM (no header magic)")
    count, size = struct.unpack(">II", data[offset + 8:offset + 16])
    index = offset + 16
    store = index + 16 * count
    tags = {}
    for i in range(count):
        tag, kind, at, _ = struct.unpack(">IIII", data[index + 16 * i:index + 16 * (i + 1)])
        if kind == TYPE_STRING:
            end = data.index(b"\0", store + at)
            tags[tag] = data[store + at:end].decode("utf-8", "replace")
    return tags, store + size


def _decompress(payload, compressor):
    if compressor == "zstd":
        try:
            from compression import zstd  # Python 3.14+
            return zstd.decompress(payload)
        except ImportError:
            return subprocess.run(["zstd", "-dc"], input=payload, capture_output=True,
                                  check=True).stdout
    if compressor in ("xz", "lzma"):
        return lzma.decompress(payload)
    if compressor == "gzip":
        return gzip.decompress(payload)
    raise PayloadError(f"unsupported payload compressor {compressor!r}")


def read_rpm(path):
    """(name, {payload path: bytes}) for the regular files of an RPM."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"\xed\xab\xee\xdb":
        raise PayloadError(f"{path} is not an RPM (no lead)")
    _, end = _header(data, 96)            # signature header,
    end += -end % 8                       # padded to 8 bytes
    tags, end = _header(data, end)        # then the main header
    if TAG_NAME not in tags:
        raise PayloadError(f"{path} has no package name")
    archive = _decompress(data[end:], tags.get(TAG_PAYLOADCOMPRESSOR, "gzip"))
    return tags[TAG_NAME], _cpio_files(archive)


def _cpio_files(archive):
    files = {}
    at = 0
    while True:
        head = archive[at:at + 110]
        if head[:6] not in (b"070701", b"070702"):
            raise PayloadError("payload is not a newc cpio archive")
        fields = [int(head[6 + 8 * i:14 + 8 * i], 16) for i in range(13)]
        mode, size, namesize = fields[1], fields[6], fields[11]
        name = archive[at + 110:at + 110 + namesize - 1].decode("utf-8", "replace")
        at += 110 + namesize
        at += -at % 4
        if name == "TRAILER!!!":
            return files
        if mode & 0o170000 == 0o100000:
            # A hardlinked file carries its data once; keep the copy with data.
            body = archive[at:at + size]
            if size or name.lstrip(".") not in files:
                files[name.lstrip(".")] = body
        at += size
        at += -at % 4


def problems(name, files):
    """What is missing or wrong, as a list of messages."""
    found = []
    share = f"/usr/share/{name}"
    for document in LEGAL_DOCUMENTS:
        path = f"{share}/legal/{document}"
        text = files.get(path)
        if text is None:
            found.append(f"{path} is missing")
            continue
        text = text.decode("utf-8", "replace")
        if not text.strip():
            found.append(f"{path} is empty")
        for marker in LEGAL_DEV_MARKERS:
            if marker in text:
                found.append(f"{path} is a development version ({marker!r}); "
                             "complete the publisher details and privacy notice (check:legal)")
    index_path = f"{share}/licenses/index.json"
    if index_path not in files:
        found.append(f"{index_path} is missing")
        return found
    try:
        index = json.loads(files[index_path])
    except ValueError as e:
        found.append(f"{index_path} is not JSON: {e}")
        return found
    for component in index.get("components", []):
        for license_file in component.get("files", []):
            path = f"{share}/licenses/{license_file}"
            if not files.get(path, b"").strip():
                found.append(f"{path} (for {component.get('name', '?')}) is missing or empty")
    return found


def main(argv):
    if len(argv) != 1:
        print("usage: rpm-payload-check.py <rpm>", file=sys.stderr)
        return 2
    try:
        name, files = read_rpm(argv[0])
    except (OSError, PayloadError, subprocess.CalledProcessError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    found = problems(name, files)
    for message in found:
        print(f"error: {message}", file=sys.stderr)
    if found:
        return 1
    print(f"{argv[0]}: legal documents and license texts complete")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
