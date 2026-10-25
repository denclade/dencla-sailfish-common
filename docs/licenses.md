<!-- SPDX-FileCopyrightText: 2026 Dennis Burgermeister -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# License packaging for consuming applications

For the shared presentation in the application's About page, see the
[About and legal UI guide](about-and-legal.md). `DenclaAbout.cmake` combines the
license generator, legal-document generator and QML UI in one integration.

Include this helper after creating the application target. It does not require
the Qt library build or the legal-document helper.

```cmake
include("${DENCLA_COMMON_SOURCE_DIR}/cmake/DenclaLicenses.cmake")
dencla_install_licenses(
    TARGET harbour-example
    APP_NAME "Example"
    SOURCE_URL "https://github.com/owner/harbour-example"
    COMPONENTS BOUNCE_LIST_VIEW MINIZ
    MINIZ_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/miniz/miniz.c"
)
```

The app's `LICENSE` and the shared GPL text are always included. Select the
additional components actually shipped by the app; the helper does not infer
runtime usage from all targets defined by `add_subdirectory()`.

| Component | Bundled notices |
|---|---|
| `BOUNCE_LIST_VIEW` | Original Jolla BSD-3-Clause notice for the Silica opacity formula |
| `CRYPTO` | Original Monocypher dual-license notice and the selected BLAKE3 CC0 text |
| `MINIZ` | Complete MIT and public-domain comment blocks extracted from `MINIZ_SOURCE`, with duplicate blocks removed |

Other shared modules use the shared GPL. Apps without these optional
components can omit `COMPONENTS` entirely. The miniz extractor expects the
MIT and public-domain notices used by the bundled amalgamated implementation;
a different upstream license layout must be reviewed and the extractor adapted.

## Arguments

| Argument | Meaning |
|---|---|
| `TARGET` | Required existing application target |
| `APP_ID` | Package directory; defaults to the target name |
| `APP_NAME` | Display name in the generated summary; defaults to `APP_ID` |
| `PROJECT_LICENSE` | Application license file; defaults to the calling directory's `LICENSE` |
| `LICENSE_ID` | SPDX identifier displayed in the About notice; no license is inferred from the filename |
| `COPYRIGHT` | Application copyright notice for the About page |
| `SOURCE_URL` | Optional application source URL in the summary |
| `COMPONENTS` | Optional list from the component table above |
| `MINIZ_SOURCE` | Required source file when selecting `MINIZ` |
| `NOTICES_FILE` | Optional application-specific notices, copied as `APP-NOTICES.txt` |
| `EXTRA_LICENSE_FILES` | Additional complete license files, with unique `.txt` basenames |

Relative input paths are resolved against the calling `CMakeLists.txt`.
The application license is preserved as `APP-LICENSE.txt`; it is not relabeled
as the shared library's GPL. Missing inputs, unknown components and conflicting
extra filenames fail generation. Original texts are copied unchanged.

## Generation and installation

Generation runs at configuration, through the `<target>-licenses` build target,
and again at installation, including installation without a preceding build.
Each app has a separate output directory under
`CMakeFiles/dencla-licenses-<target>/output/`. Generated files are not source
artifacts and need not be committed. Changing the component selection removes
obsolete notices from that generated directory.

The generated `index.json` contains schema version 1, an `application` object
(`name`, `license`, `copyright`, `url`) and a `components` array. Each component
has `name`, `license`, `url` and a `files` array of local `.txt` basenames.
Application license and the full upstream texts remain unchanged. The QML UI
uses this index directly; it does not parse `THIRD_PARTY_NOTICES.txt`.
If the application license is byte-for-byte identical to the shared GPL,
`APP-LICENSE.txt` is a relative symlink to that text to avoid packaging it twice.

Files are installed under `share/<app-id>/licenses/` using the `Licenses`
installation component. The generated `THIRD_PARTY_NOTICES.txt` identifies
the selected components and points to their full license texts. Ensure the
RPM's `%files` includes this directory (a rule for `share/<app-id>` covers it).
Add application-specific attributions through `NOTICES_FILE` and upstream
license files through `EXTRA_LICENSE_FILES` when introducing dependencies.

The helper packages selected notices; it does not establish the licensing or
provenance of newly added material. Runtime system packages retain their own
license packaging.

## Tests without Qt

```sh
cmake -P tests/test_licenses.cmake
```

These tests cover two applications, optional components, original text
preservation, miniz extraction, regeneration, missing inputs and installation.
They also run as `test_licenses` in the shared project's CTest suite.
