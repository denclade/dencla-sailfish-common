<!-- SPDX-FileCopyrightText: 2026 Dennis Burgermeister -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# dencla-sailfish-common

Shared Qt 5 and Sailfish Silica components: disk caching, HTTP/download
helpers, file utilities, encryption, PocketBase, UI/map components and optional
Sailfish Secrets support.

## Build

Requires CMake 3.19+, C/C++14 compilers and Qt 5 Core/Network development files.

```sh
cmake -S . -B build -DDENCLA_COMMON_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`DENCLA_COMMON_ENABLE_SECRETS=ON` additionally requires Sailfish Secrets
development files. Applications can use `add_subdirectory()` and link the
`dencla_common_cache`, `dencla_common_net`, `dencla_common_util` or
`dencla_common_secrets` targets. Encryption and PocketBase are provided by
`dencla_common_crypto` and `dencla_common_pocketbase`.
`qml/OsmMap.qml` needs QtLocation/QtPositioning and the OSM geoservices plugin
in the consuming application.

The included tests cover disk caching, file utilities, HTTP, encryption and
legal document and license generation/installation. Qt tests require Qt 5 Test; the legal
tests can also run standalone without Qt.

## Legal documents

Reusable imprint/privacy integration, neutral templates and standalone tests:
[guide for agents and maintainers](docs/legal.md). This CMake helper can also
be used independently of the Qt library build.

## License packaging

`cmake/DenclaLicenses.cmake` provides `dencla_install_licenses()` for all apps.
It generates component notices, preserves upstream license texts and installs
them with the package. See [license packaging](docs/licenses.md) for usage,
component selection and standalone tests. This helper also works without Qt.

The [About and legal UI guide](docs/about-and-legal.md) describes the shared
presentation of licenses, third-party notices, imprint and privacy.
`cmake/DenclaAbout.cmake` provides the combined `dencla_install_about()` helper,
including the QML pages and generated offline license catalog.

## Tasks, builds and releases

All harbour apps share one mise task scheme (`dev:`, `phone:`, `cloud:`,
`release:prepare`/`release:deploy`) and call the shared scripts here:
[mise tasks](docs/mise-tasks.md) and [releasing](docs/releasing.md) (in German).

| Script | Task |
|---|---|
| `scripts/sdk-rpm.sh` | `dev:build`, `phone:build`, `check:harbour` |
| `scripts/release.py` | `release:prepare`: version, changelog, commit, tag `v<version>` |
| `scripts/app_release.py` | `release:deploy`: RPMs for all architectures, Harbour check, source archive |
| `scripts/cloud_release.py` | `cloud:build`, `cloud:release:prepare`/`deploy`: d0b bundle, tags `cloud-v<version>` |
| `scripts/cloud-test-containers.sh` | `cloud:test:containers` |
| `scripts/pb-get.sh` | `cloud:get`: PocketBase from `containers/build.env` |

Tests: `python3 -m unittest discover -s tests -p 'test_*release.py'`.

## App screenshots and tours

The [app media guide](docs/app-media-guide.md) defines shared demo-data,
screenshot and README conventions. `scripts/capture-screenshot.py` captures
the emulator using an app's SSH wrapper; `scripts/render-promo.py` generates
thumbnails and GIF/MP4 tours from an app-specific JSON manifest.

## License

Copyright 2026 Dennis Burgermeister. Original code: GPL-3.0-or-later;
see [LICENSE](LICENSE). The borrowed Silica opacity formula also retains
Jolla's BSD-3-Clause terms. Bundled cryptography retains its upstream licenses;
complete notices are included locally in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and `LICENSES/`.

Publish source files selected by Git. Generated Makefiles, CMake state,
libraries, test output and private publisher profiles are ignored build/local data.
