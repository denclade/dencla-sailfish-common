<!-- SPDX-FileCopyrightText: 2026 Dennis Burgermeister -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Releases von harbour-Apps und ihren Servern

Ein Verfahren für alle `harbour-*`-Apps. Jede App hat ein App-Release;
Apps mit Server (Standard: PocketBase auf d0b) zusätzlich ein davon
getrenntes Server-Release. Die Skripte liegen hier unter `scripts/`, was
sich je App unterscheidet, steht als Option im Task der App. Die
Task-Namen: [mise-tasks.md](mise-tasks.md).

## Prinzip

| | App | Server |
|---|---|---|
| Versionsquelle | `Version:` in `rpm/<app>.spec` | `server/VERSION` (eine Zeile) |
| Tag | `v<version>` | `cloud-v<version>` |
| vorbereiten | `release:prepare` (`scripts/release.py`) | `cloud:release:prepare` (`scripts/cloud_release.py`) |
| ausliefern | `release:deploy` (`scripts/app_release.py`) | `cloud:release:deploy` (`scripts/cloud_release.py`) |
| Lieferung | `dist/<app>-<tag>/` | `dist/<service>-<tag>.tar.zst` + `.sha256` |
| CI | – | Tag `cloud-v*` → Anhang am Forgejo-Release |

1. **Eine Versionsquelle.** Alles andere leitet sich daraus ab. Von Hand
   wird keine Version geändert, nur über `prepare`.
2. **SemVer 2.0.0 ohne Build-Metadaten** (`+…` ist in d0b-Tags nicht
   erlaubt). Vorabversionen mit Punkt: `1.2.0-beta.2`, damit `beta.10`
   nach `beta.2` kommt. Vor 1.0 erhöht eine neue Funktion die zweite
   Stelle, eine Korrektur die dritte.
3. **Getrennte Tag-Namensräume.** `v*` passt nicht auf `cloud-v…`; App
   und Server stören sich weder lokal noch in CI.
4. **Zwei Schritte.** `prepare` setzt die Version, committet und setzt
   ein annotiertes Tag; es baut nichts. `deploy` setzt keine Version und
   baut nur den getaggten, sauberen Stand.
5. **Lieferung = genau ein Commit.** `deploy` verlangt: Tag passt zur
   Version, Arbeitsbaum sauber, HEAD auf dem Tag; danach prüft es, dass
   der Build den Baum nicht verändert hat. Neu bauen ohne Versionswechsel:
   dasselbe Tag noch einmal. Nach einer Änderung braucht es eine höhere
   Version.
6. **Nichts wird automatisch veröffentlicht.** Kein Push, kein Upload,
   kein `app deploy`. Tags einzeln pushen (`git push origin <tag>`, nie
   `--tags`); ein gepushtes Tag wird nie verschoben.

## App-Release

```sh
mise run release:prepare -- minor --dry-run   # neue Version und Changelog-Abschnitt zeigen
mise run release:prepare -- minor             # Prüfungen, Version, Changelog, Commit, Tag v0.10.0
mise run release:deploy -- v0.10.0            # Lieferung nach dist/harbour-<app>-v0.10.0/
git push origin main && git push origin v0.10.0
```

**`release:prepare`** (`scripts/release.py`, `--help` listet alle
Optionen) erhöht `Version:` in der `.spec`, setzt `Release:` auf 1,
schreibt `CHANGELOG.md` ([Keep a Changelog](https://keepachangelog.com/de/1.1.0/),
Abschnitt `## [Unreleased]` oben) und `rpm/<app>.changes`, committet
`(c) Release X.Y.Z` und setzt das Tag. Vorher laufen `--preflight` (auch
mit `--skip-checks`) und `--test`. Die Commit-Präfixe ordnen den
Changelog: `(+)` neu, `(f)` behoben, `(c)` geändert, `(-)` entfernt.

| Option | Wofür |
|---|---|
| `--runner mise` | Task-Runner (alte Apps: `just`) |
| `--branch main` | Release-Branch (Standard `master`) |
| `--preflight check:legal` | Pflichtprüfung, läuft auch mit `--skip-checks` (mehrfach) |
| `--test check` | Tests vor dem Release (mehrfach) |
| `--carry "$DENCLA_COMMON"` | darf uncommittet sein und kommt in den Release-Commit (Submodul-Zeiger) |
| `--require-release PATH` | Submodul muss selbst auf einem `v*`-Tag stehen |
| `--lang de`, `--heading +=Text` | deutscher Changelog, einzelne Überschrift umbenennen |
| `--mirror-version FILE` | zweite Datei mit `version = "X.Y.Z"` mitziehen (z. B. `Cargo.toml`) |
| `--sync CMD`, `--sync-file PATH` | abgeleitete Dateien neu erzeugen (Lock-File) |

**`release:deploy <tag>`** (`scripts/app_release.py`) bricht ab, wenn das
Tag nicht `v` + `Version:` lautet, der Baum nicht sauber ist oder HEAD
nicht auf dem Tag steht. Dann:

1. `--preflight`-Tasks (z. B. `check:legal`),
2. für aarch64, armv7hl und i486 (`--arch` schränkt ein; das SDK-Release
   kommt aus `SAILFISH_SDK_TARGET`) das RPM mit `scripts/sdk-rpm.sh`
   bauen, Tests im `%check` inklusive, und Jollas Harbour-Prüfung darauf,
3. prüfen, dass der Build den Arbeitsbaum nicht verändert hat,
4. `dist/<app>-<tag>/` neu anlegen: die RPMs, das Quellarchiv
   `<app>-X.Y.Z-source.tar.bz2` mit allen Submodulen an den referenzierten
   Commits (ein GitHub-Archiv enthält sie nicht) und `SHA256SUMS`.

Den Ordner als Release-Artefakt anbieten, den RPM-Inhalt vorher auf
Rechtstexte und Lizenzbeilage prüfen. App-Tag und Submodul-Commits müssen
in den öffentlichen Repositories liegen.

**Version in der App.** CMake liest die `.spec` (das RPM übergibt
`-DAPP_VERSION=%{version}`, jeder andere Build hängt `-dev` an), `main()`
setzt `app->setApplicationVersion(APP_VERSION)`, QML zeigt
`Qt.application.version`.

```cmake
file(STRINGS "${CMAKE_SOURCE_DIR}/rpm/<app>.spec" SPEC_VERSION REGEX "^Version:")
string(REGEX REPLACE "^Version:[ \\t]*" "" SPEC_VERSION "${SPEC_VERSION}")
set(APP_VERSION "${SPEC_VERSION}-dev" CACHE STRING "Application version")
target_compile_definitions(<app> PRIVATE APP_VERSION="${APP_VERSION}")
```

## Server-Release

Für Apps mit eigenem Dienst auf d0b. Standard ist PocketBase; ein anderer
Server nennt `--profile` und `--include` selbst oder baut sein Bündel
selbst, hält dann aber Tags und Tasks gleich.

```sh
mise run cloud:release:prepare -- 1.1.0          # server/VERSION, Commit "(c) Cloud release 1.1.0", Tag cloud-v1.1.0
mise run cloud:release:deploy -- cloud-v1.1.0    # prüfen, deploy/ und dist/<service>-cloud-v1.1.0.tar.zst
mise run cloud:test:containers                   # braucht Podman
git push origin main && git push origin cloud-v1.1.0
```

**`cloud:release:prepare <version>`** bricht ab, wenn die Version kein
SemVer ist, nicht größer als die in `server/VERSION`, der Baum nicht
sauber ist oder `cloud-v<version>` schon existiert. Sonst ändert es nur
`server/VERSION`, committet und taggt. Neue App: `server/VERSION` mit
`0.0.0` anlegen (plus `server/VERSION.license` für REUSE).

**`cloud:release:deploy <tag>`** prüft Tag, Version, sauberen Baum und
HEAD wie beim App-Release, dann `containers/` gegen den d0b-Vertrag
(unten), dann `--preflight`-Tasks, dann baut es `deploy/` neu und packt
es. **`cloud:build [tag]`** baut dasselbe Bündel ohne die
Release-Prüfungen (Tag `dev` oder `pr-<n>`); es geht nie auf den Server.

**Bündel.** Nur versionierte Dateien (`git ls-files`): ignorierte Daten
neben den Quellen (Caches, `pb_data`, `.env`) kommen nie hinein.

```text
deploy/
  MANIFEST      contract=1, service, release=<tag>, commit=<sha>, built, profile, roles
  SHA256SUMS    "<sha256>  ./<pfad>" für jede übrige Datei, sortiert
  containers/   ohne REUSE-Dateien *.license
  pb/pb_migrations/, pb/pb_hooks/, pb/pb_public/   (Profil pocketbase, aus server/)
```

Die Rollen ergeben sich aus `containers/quadlets/<service>-<rolle>.container`.
Symlinks, Hardlinks, Sonderdateien und Namen mit CR, LF oder `\` werden
vor dem Kopieren und vor dem Packen abgelehnt. Archiv mit GNU tar
`--zstd`, Name `<service>-<tag>.tar.zst`, mit dem Tag unverändert.

| Option von `cloud_release.py` | Standard | Wofür |
|---|---|---|
| `--service NAME` | – | d0b-Dienst = Linux-Benutzer = Pod (`$CLOUD_SERVICE`) |
| `--profile P` | `pocketbase` | `profile=` im MANIFEST |
| `--include SRC[:DEST]` | die drei `server/pb_*` → `pb/` | ersetzt die Vorgabe des Profils (mehrfach) |
| `--version-file` | `server/VERSION` | |
| `--preflight TASK` | – | muss vor dem Bauen bestehen, z. B. `cloud:test` |

**d0b-Vertrag, den `deploy` und `build` vorab prüfen.** Jede
`<service>-<rolle>.container` hat `ReadOnly=true`, `NoNewPrivileges=true`,
`DropCapability=all`, `HealthCmd=`, `Notify=healthy`,
`Image=localhost/<service>-<rolle>:@RELEASE@` und `Pull=never`; es gibt
`<service>.pod` und `<rolle>.Containerfile`. Verboten sind `Network=`,
`PodmanArgs=`, `AddCapability=`, `Sysctl=` und Zeilenfortsetzungen.
`build.env` nur `KEY=wert` ohne Kommentare (jede Zeile wird
`--build-arg`; den Lizenzkopf in `build.env.license`), `smoke.txt` nur
`GET <pfad> <status> [header]`. Den vollständigen Vertrag (Caddy,
Ports, Volumes) prüft erst `app deploy` auf d0b; die App dokumentiert ihn
in ihrem `docs/deploy.md`.

**`cloud:test:containers`** (`scripts/cloud-test-containers.sh`) baut
jede Rolle aus `deploy/` mit `--pull=always` und den `build.env`-Werten
und startet sie wie auf d0b: `--read-only`, `--cap-drop=all`,
`no-new-privileges`, UID 1000, Volumes als Wegwerf-tmpfs, keine echten
Geheimnisse. Es wartet bis 60 s auf den `HealthCmd` und räumt immer auf.

### CI (Forgejo)

`.forgejo/workflows/release.yml` der App:

```yaml
on:
  push:
    tags: ["cloud-v*"]
env:
  MISE_TRUSTED_CONFIG_PATHS: ${{ github.workspace }}
jobs:
  release:
    runs-on: docker          # eigener Runner mit Podman, nicht auf d0b
    steps:
      - uses: https://code.forgejo.org/actions/checkout@v4
        with:
          submodules: recursive
      - uses: https://github.com/jdx/mise-action@v2
      - run: mise run test:release
      # Dieselbe Prüfung wie lokal: ein Tag, das nicht zu server/VERSION passt, scheitert.
      - run: mise run cloud:release:deploy "${{ github.ref_name }}"
      - run: mise run cloud:test:containers
      - run: |
          set -eu
          mkdir -p release-assets
          # <service> wörtlich: mise-Variablen gibt es in einem run-Schritt nicht.
          cp "dist/<service>-${GITHUB_REF_NAME}.tar.zst" "dist/<service>-${GITHUB_REF_NAME}.tar.zst.sha256" release-assets/
      - uses: https://code.forgejo.org/actions/forgejo-release@v2
        with:
          direction: upload
          release-dir: release-assets
          token: ${{ secrets.RELEASE_TOKEN }}
```

Für PRs dasselbe mit `mise run cloud:build "pr-${{ github.event.number }}"`
statt `cloud:release:deploy`, ohne Upload. Sobald `.forgejo/workflows/`
existiert, ignoriert Forgejo `.github/workflows/`. Runner-Label,
Actions und Upload auf der eigenen Instanz prüfen; ein lokaler Build
testet das nicht. Auf d0b holt `app deploy <service> cloud-v<version>`
das Archiv vom Release.

## mise-Tasks

```toml
[tasks."release:prepare"]
run = 'python3 "$DENCLA_COMMON/scripts/release.py" --runner mise --branch main --preflight check:legal --test check --carry "$DENCLA_COMMON"'

[tasks."release:deploy"]
usage = 'arg "<tag>" help="v<.spec Version>, e.g. v1.1.0"'
run = 'python3 "$DENCLA_COMMON/scripts/app_release.py" deploy "$usage_tag" --preflight check:legal'

# nur mit Server
[tasks."cloud:build"]
usage = 'arg "[tag]" default="dev" help="Any tag d0b accepts, e.g. pr-12"'
run = 'python3 "$DENCLA_COMMON/scripts/cloud_release.py" build "$usage_tag" --service "$CLOUD_SERVICE"'

[tasks."cloud:test:containers"]
run = '"$DENCLA_COMMON/scripts/cloud-test-containers.sh" "$CLOUD_SERVICE"'

[tasks."cloud:release:prepare"]
usage = 'arg "<version>" help="SemVer without +build, e.g. 1.1.0 or 1.2.0-beta.1"'
run = 'python3 "$DENCLA_COMMON/scripts/cloud_release.py" prepare "$usage_version" --service "$CLOUD_SERVICE"'

[tasks."cloud:release:deploy"]
usage = 'arg "<tag>" help="cloud-v<server/VERSION>, e.g. cloud-v1.1.0"'
run = 'python3 "$DENCLA_COMMON/scripts/cloud_release.py" deploy "$usage_tag" --service "$CLOUD_SERVICE"'
```

`.gitignore` der App: `/deploy/`, `/dist/`, `/release-assets/`.

## Tests

```sh
python3 -m unittest discover -s tests -p 'test_*release.py'   # hier
mise run test:release                                          # aus einer App
```

Abgedeckt: gültige und ungültige Versionen und Tags, Präzedenz
(`beta.2 < beta.10`), Tag ≠ Version, `prepare` in einem Wegwerf-Repo
(ein Commit, nur die Versionsdatei, annotiertes Tag; schmutziger Baum,
kleinere Version, vorhandenes Tag scheitern), App-Lieferung mit
Submodul-Quellarchiv und Ersatz für das SDK, Server-Bündel (nur
versionierte Dateien, `SHA256SUMS` vollständig, Links abgelehnt,
Vertrag).

## Was Agents nicht tun dürfen

- Kein `git push`, kein Upload, kein `app deploy` ohne ausdrücklichen
  Auftrag.
- Keine Version von Hand setzen; nur über `release:prepare` bzw.
  `cloud:release:prepare`.
- `deploy` nicht mit schmutzigem Baum erzwingen (Stash, Prüfung
  auskommentieren). Erst committen, dann eine höhere Version taggen.
- Tags nicht verschieben oder löschen, wenn sie gepusht sein könnten.
