<!-- SPDX-FileCopyrightText: 2026 Dennis Burgermeister -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# mise-Tasks in harbour-Apps

Alle `harbour-*`-Apps mit diesem Submodul haben dieselben Task-Namen,
damit Menschen und Agenten in jedem Repo dieselben Befehle finden.
`mise tasks` listet sie mit Beschreibung. Referenz ist
`harbour-fitfish/mise.toml` (App mit PocketBase-Server). Releases
beschreibt [releasing.md](releasing.md).

## Schema

**Präfix = wo es läuft, danach das Verb.**

| Präfix | Ort |
|---|---|
| keiner | das Repo selbst: Tests, Prüfungen, Releases der App |
| `dev:` | Sailfish OS Emulator (geteilt mit dem Nutzer) |
| `phone:` | echtes Telefon (`PHONE_SSH` in `.env`) |
| `cloud:` | Server der App auf d0b, lokal oder als Bündel |

| Verb | Bedeutung |
|---|---|
| `build` | bauen, nichts installieren (`dev:build`, `phone:build`, `cloud:build`) |
| `deploy` | bei `dev:`/`phone:` aufs Gerät spielen; bei `release:` die Lieferung bauen |
| `run`, `stop`, `serve` | starten, beenden; `serve` für einen lokalen Server |
| `test`, `test:<was>` | Tests; `<ort>:test` für Tests des Orts (`cloud:test`) |
| `check`, `check:<was>` | Prüfungen, die ein Release verlangt; `check` hängt von allen ab |
| `logs`, `ssh` | Journal folgen, Shell |
| `release:prepare`, `release:deploy` | Version setzen und taggen; Lieferung bauen |

Teile werden mit `:` getrennt, Wörter in einem Teil mit `-`. Keine
Bindestrich-Namen wie `test-one` oder `install-test-deps` für etwas, das
ins Schema passt.

## Standard-Tasks

Pflicht in jeder App; Name und Bedeutung genau so:

| Task | Was |
|---|---|
| `vm` | Entwicklungs-VM starten (auf dem Host) |
| `lupdate` | `.ts`-Dateien über die SDK-Toolchain neu erzeugen |
| `test` | Unit-Tests gegen das echte SDK-Target |
| `test:one` | ein Test-Binary neu bauen und laufen lassen |
| `test:setup` | einmalig je Target: Test-Abhängigkeiten im Target installieren |
| `test:release` | Tests der Release-Skripte hier im Submodul |
| `check` | alles, was ohne Emulator scheitern kann |
| `check:harbour` | Jollas Harbour-Prüfung auf das RPM von `dev:build` |
| `check:legal` | Herausgeberangaben und Datenschutz streng prüfen ([legal.md](legal.md)) |
| `check:rpm <rpm>` | Rechtstexte und Lizenzbeilage im fertigen RPM prüfen (`--rpm-check` von `release:deploy`) |
| `dev:build`, `dev:deploy`, `dev:run`, `dev:screenshot`, `dev:logs`, `dev:ssh` | Emulator-Zyklus |
| `release:prepare` | App-Version setzen, Commit, Tag `v<version>` |
| `release:deploy` | App-Lieferung aus dem Tag bauen |

Nach Bedarf, dann mit diesen Namen: `dev:stop`, `dev:touch`, `dev:demo`,
`dev:reset`, `dev:seed`, `dev:qmlcheck`, `phone:build`, `phone:deploy`,
`phone:logs`, `check:licenses`, `check:contrast`, `audit:secrets`.
App-eigene Werkzeuge bekommen einen eigenen Präfix (`rust:test`,
`rust:build`).

Mit Server (Standard: PocketBase):

| Task | Was |
|---|---|
| `cloud:get` | PocketBase-Version aus `containers/build.env` laden (`scripts/pb-get.sh`) |
| `cloud:serve` | lokalen Server starten |
| `cloud:test` | Tests gegen den Server |
| `cloud:build [tag]` | d0b-Bündel ohne Release-Prüfungen (PRs, Ausprobieren) |
| `cloud:test:containers` | Rollen aus `deploy/` so eingeschränkt wie auf d0b starten |
| `cloud:release:prepare` | Server-Version setzen, Commit, Tag `cloud-v<version>` |
| `cloud:release:deploy` | Server-Lieferung aus dem Tag bauen |

## Aufbau der mise.toml

```toml
# SPDX-Kopf

[env]
_.file = ".env"
SAILFISH_SDK_PATH = "…"            # Host: ~/SailfishOS; VM: /opt/sailfish-sdk
SAILFISH_SDK_TARGET = "…"          # Standard SailfishOS-<release>-i486
HOST_WORKSPACE_FOLDER = "…"        # aus .microvm-state/host-workspace
# Was sich je App unterscheidet; die Tasks lesen es nur.
DENCLA_COMMON = "3rdparty/dencla-sailfish-common"
CLOUD_SERVICE = "<app>cloud"       # nur mit Server
SDK_RPM_DEFINES = "<app>_distribution 1"   # nur wenn die .spec Makros braucht

# --- Setup ---        vm, lupdate
# --- Tests ---        test, test:one, test:setup, test:release
# --- Checks ---       check:*, check
# --- Emulator ---     dev:*
# --- Real phone ---   phone:*
# --- Server ---       cloud:* (ohne cloud:release:*)
# --- Releases ---     release:prepare, release:deploy, cloud:release:*
```

Abschnitte in dieser Reihenfolge, je mit einer Kommentarzeile. Jede
Aufgabe hat eine `description` auf Englisch, die sagt, was sie tut, was
sie voraussetzt und was sie nicht tut ("Does not build - phone:build
first"). Tasks mit Argumenten haben `usage`.

**Wo der Code lebt.** Was in allen Apps gleich ist, liegt hier unter
`scripts/` und wird mit `"$DENCLA_COMMON/scripts/…"` aufgerufen; der
Task-Text ist dann in jeder App wörtlich gleich. Kurze app-eigene Tasks
stehen inline, längere in `scripts/` der App. Nie dieselbe Logik in zwei
Apps kopieren: dann gehört sie hierher.

| Skript hier | Tasks |
|---|---|
| `scripts/sdk-rpm.sh build <target>` | `dev:build`, `phone:build`, `release:deploy` |
| `scripts/sdk-rpm.sh check <target> <rpm>` | `check:harbour`, `release:deploy` |
| `scripts/release.py` | `release:prepare` |
| `scripts/app_release.py` | `release:deploy` |
| `scripts/cloud_release.py` | `cloud:build`, `cloud:release:prepare`, `cloud:release:deploy` |
| `scripts/cloud-test-containers.sh` | `cloud:test:containers` |
| `scripts/pb-get.sh` | `cloud:get` |
| `scripts/rpm-payload-check.py` | `check:rpm` |
| `scripts/capture-screenshot.py` | `dev:screenshot` |

`sdk-rpm.sh` läuft aus der Wurzel der App: auf dem Host über
`$SAILFISH_SDK_PATH/bin/sfdk`, in der VM (`IN_SAILFISH_SANDBOX=true`)
über das `scripts/engine-exec.sh` der App.
`SDK_RPM_DEFINES` aus `[env]` gibt jedem `build` rpm-Makros mit
(`"name wert"`, mehrere mit `;` getrennt, je ein `--define`). So bauen
`dev:build`, `phone:build`, `check:harbour` und `release:deploy` dasselbe
Paket, ohne dass sich der Task-Text unterscheidet; Qt Creator ist nicht
betroffen.

Die Tasks im Wortlaut:

```toml
[tasks."dev:build"]
run = '"$DENCLA_COMMON/scripts/sdk-rpm.sh" build "$SAILFISH_SDK_TARGET"'

[tasks."phone:build"]
run = '"$DENCLA_COMMON/scripts/sdk-rpm.sh" build "${PHONE_SDK_TARGET:-SailfishOS-5.1.0.11-aarch64}"'

[tasks."check:harbour"]
depends = ["dev:build"]
run = """
RPM=$(ls -t RPMS/<app>-[0-9]*."${SAILFISH_SDK_TARGET##*-}".rpm | head -1)
"$DENCLA_COMMON/scripts/sdk-rpm.sh" check "$SAILFISH_SDK_TARGET" "$RPM"
"""

[tasks."test:release"]
run = "python3 -m unittest discover -s \"$DENCLA_COMMON/tests\" -p 'test_*release.py'"
```

Die Release-Tasks stehen in [releasing.md](releasing.md#mise-tasks).

## Umstellen einer App

| alt | neu |
|---|---|
| `install-test-deps` | `test:setup` |
| `test-one` | `test:one` |
| `legal:check` | `check:legal` (auch in `--preflight` von `release:prepare`) |
| `release` | `release:prepare` |
| `release:source`, `release:check` | Teil von `release:deploy` bzw. `--preflight` |
| `phone:install` | `phone:deploy` |
| `server:run` | `cloud:serve` |
| `server:test` | `cloud:test` |
| `build:deploy <tag>` | `cloud:build [tag]`; Releases über `cloud:release:deploy` |
| `test:containers` | `cloud:test:containers` |
| Tags `server-v…`, `cloud-<datum>-<n>` | `cloud-v<version>`, Version in `server/VERSION` |
| eigene Kopie von `scripts/release.py` | `"$DENCLA_COMMON/scripts/release.py"` |

Danach alle Verweise mitziehen: `README.md`, `CLAUDE.md`, `docs/`,
`.vscode/tasks.json`, CMake-Meldungen, `.forgejo/workflows/`. Prüfen mit
`grep -rn "<alter name>"` ohne `3rdparty/`.
