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
legal document generation/installation. Qt tests require Qt 5 Test; the legal
tests can also run standalone without Qt.

## Legal documents

Reusable imprint/privacy integration, neutral templates and standalone tests:
[guide for agents and maintainers](docs/legal.md). This CMake helper can also
be used independently of the Qt library build.

## Release helper

The shared `scripts/release.py` supports existing just apps and mise apps
(`--runner mise`). Use `--preflight legal:check` for the required legal check;
preflights also run with `--skip-checks`.

## License

Copyright 2026 Dennis Burgermeister. Original code: GPL-3.0-or-later;
see [LICENSE](LICENSE). The borrowed Silica opacity formula also retains
Jolla's BSD-3-Clause terms. Bundled cryptography retains its upstream licenses;
complete notices are included locally in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and `LICENSES/`.

Publish source files selected by Git. Generated Makefiles, CMake state,
libraries, test output and private publisher profiles are ignored build/local data.
