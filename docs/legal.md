<!-- SPDX-FileCopyrightText: 2026 Dennis Burgermeister -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Gemeinsames Legal-System für Sailfish-Apps

Diese Anleitung ist der Einstieg für Agenten und Maintainer, die Impressum
und Datenschutz in einer weiteren Qt/Silica-App einrichten. Die Implementierung
hier wiederverwenden; keine app-eigenen Kopien des Generators oder der QML-Seite
anlegen. Benötigt werden CMake 3.19+ und QtQuick/Silica zur Laufzeit.
Die CMake-Tests selbst benötigen weder Qt noch ein Sailfish SDK.

## Aufteilung

| Ort in dencla-sailfish-common | Aufgabe |
|---|---|
| `cmake/DenclaLegal.cmake` | öffentliche CMake-Funktion, Build-/Prüf-Targets, Installation |
| `cmake/GenerateLegal.cmake` | JSON-Prüfung, HTML-Escaping, Erzeugung und reine Validierung |
| `legal/provider.example.json` | neutrale Feldvorlage ohne echte Kontaktdaten |
| `legal/impressum.html.in` | gemeinsames Impressum mit dem jeweiligen App-Namen |
| `legal/datenschutz.example.html.in` | auszuarbeitender Datenschutzentwurf |
| `qml/ImprintPage.qml` | gemeinsame Anzeige beider Dokumente |
| `tests/test_legal.cmake` | eigenständige Tests mit künstlichen Daten und mehreren Apps |

Im App-Repository liegen nur `legal/datenschutz.html.in` und eine ignorierte
`legal/provider.local.json`. Die Datenschutzbeschreibung muss zum tatsächlichen
Verhalten dieser App passen. Anbieterangaben, Renderer und allgemeines Impressum
werden nicht in jeden Datenschutztext kopiert.

## Eine App anbinden

1. `dencla-sailfish-common` unter `3rdparty/` einbinden beziehungsweise aktualisieren.
2. Die beiden Beispiel-Dateien kopieren: Anbieterprofil nach
   `legal/provider.local.json`, Datenschutzentwurf nach `legal/datenschutz.html.in`.
   Vorhandene lokale Profile nicht überschreiben.
3. In der App `.gitignore` ergänzen:

   ```gitignore
   /legal/*.local.json
   build/
   CMakeFiles/
   CMakeCache.txt
   cmake_install.cmake
   Makefile
   ```

4. Das Datenschutzdokument inhaltlich ausarbeiten. Den Marker
   `DENCLA-LEGAL-PRIVACY-DRAFT` erst danach entfernen. Der Release-Modus lehnt
   ein Dokument mit diesem Marker ab, auch wenn das Anbieterprofil vollständig ist.
5. Nach dem Anlegen des App-Targets in dessen `CMakeLists.txt`:

   ```cmake
   include("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/dencla-sailfish-common/cmake/DenclaLegal.cmake")
   dencla_install_legal(
       TARGET harbour-reader
       APP_NAME "Reader"
       PRIVACY_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/legal/datenschutz.html.in"
   )
   ```

Die Funktion kann auch ohne `add_subdirectory()` der übrigen Common-Module
verwendet werden. Sie installiert die QML-Seite selbst; bisherige manuelle
Installationsregeln für `ImprintPage.qml` entfernen.

### Schnittstelle

`TARGET` muss existieren. `PRIVACY_TEMPLATE` ist erforderlich. Optionale Argumente:

| Argument | Standard / Bedeutung |
|---|---|
| `APP_ID` | Targetname; Installationsverzeichnis unter `share/` |
| `APP_NAME` | App-ID; öffentlicher Anzeigename, wird HTML-escaped |
| `LEGAL_DIRECTORY` | `legal/` der aufrufenden App; Ort des lokalen Profils |
| `PROFILE` | explizites privates Profil für dieses Target |
| `IMPRINT_TEMPLATE` | gemeinsames Impressum; nur für tatsächlich benötigte Abweichungen ersetzen |

`-DDENCLA_LEGAL_CONFIG=/absoluter/pfad/provider.json` setzt ein gemeinsames
externes Profil für den Build; `PROFILE` hat Vorrang. So kann derselbe Anbieter
mehrere Apps bauen, ohne die persönlichen Angaben mehrfach zu pflegen.
Der Pfad muss für die ausführende Build-Engine lesbar sein.

Das Profil enthält ausschließlich einzeilige Strings: `name`, `street`,
`postal_code`, `city`, `country`, `email`, `phone`. `phone` darf leer sein.
Echte Werte nur lokal eintragen, die Datei nach Möglichkeit mit Modus `0600`
schützen und niemals mit `git add -f` aufnehmen. Weitere anbieterabhängige
Pflichtangaben erfordern eine entsprechende Erweiterung des gemeinsamen Schemas;
keine persönlichen Ersatzwerte in versionierte Templates schreiben.

### In der Über-Seite verlinken

Die installierte Seite liegt neben der Über-Seite unter `qml/pages/`:

```qml
// Impressum
pageStack.animatorPush(Qt.resolvedUrl("ImprintPage.qml"))
// Datenschutz
pageStack.animatorPush(Qt.resolvedUrl("ImprintPage.qml"),
                       { documentName: "datenschutz" })
```

Beide Einträge gut erreichbar anbieten. Die Texte bleiben hier deutsch;
rechtliche Inhalte nicht ungeprüft durch die UI-Übersetzungspipeline schicken.

## Build und Release

Die Funktion erzeugt `<TARGET>-legal` und hängt es an das App-Target. Zusätzlich
entsteht `<TARGET>-legal-check`, das die tatsächlich konfigurierte Profil- und
Template-Auswahl streng prüft, ohne Dokumente auszugeben:

```sh
cmake --build build --target harbour-reader-legal-check
```

Diesen Check in den Release-Ablauf vor Versionsänderung, Commit und Tag einbauen.
Er muss auch bei übersprungenen Tests laufen. Distributions-Builds zusätzlich
mit `-DDENCLA_REQUIRE_LEGAL_DETAILS=ON` konfigurieren: Dann schlagen Konfiguration,
Build und Installation bei unvollständigen Angaben oder einem Datenschutzentwurf fehl.

Der gemeinsame Release-Helfer kann dies explizit als verpflichtenden Preflight
ausführen. Für eine mise-App beispielsweise:

```sh
python3 3rdparty/dencla-sailfish-common/scripts/release.py patch \
    --runner mise --branch main --preflight legal:check --test test
```

`--preflight` bleibt auch mit `--skip-checks` aktiv. Bestehende just-Apps
behalten ohne `--runner` ihr bisheriges Verhalten.

Ohne diesen Modus bleiben neutrale Entwicklungs-Builds möglich. Fehlende Angaben
oder ein Datenschutzentwurf werden im Dokument sichtbar als Entwicklungsfassung
gekennzeichnet. Die Feldprüfung bestätigt keine Identität, Anschrift oder rechtliche
Vollständigkeit; der Datenschutztext muss fachlich geprüft werden.

Für Prüfungen ohne konfigurierte App steht derselbe Generator als Skript bereit:

```sh
cmake -DDENCLA_LEGAL_DIRECTORY=legal \
      -DDENCLA_LEGAL_PRIVACY_TEMPLATE=legal/datenschutz.html.in \
      -DDENCLA_REQUIRE_LEGAL_DETAILS=ON \
      -DDENCLA_LEGAL_VALIDATE_ONLY=ON \
      -P 3rdparty/dencla-sailfish-common/cmake/GenerateLegal.cmake
```

## Technische Regeln

- Generierte Dateien liegen nur im Build-Baum unter
  `CMakeFiles/dencla-legal-<TARGET>/`. Der Verzeichnisname darf nicht mit dem
  internen Make-Target-Pfad kollidieren.
- Konfiguration, jeder Legal-Build und jede Installation lesen das Profil erneut.
  Nach dem Entfernen eines Profils darf kein altes persönliches Dokument ausgeliefert
  werden. Ein explizit konfiguriertes, fehlendes Profil ist ein Fehler.
- Die Installationskomponente `Legal` enthält `legal/impressum.html`,
  `legal/datenschutz.html` und `qml/pages/ImprintPage.qml` unter `share/<APP_ID>/`.
  Diese Pfade müssen in der RPM-Dateiliste enthalten sein.
- Die QML-Seite liest ausschließlich die zwei erlaubten lokalen Dateinamen relativ
  zum App-Verzeichnis. Keine WebView, kein Serverabruf und keine Markdown-Abhängigkeit.
  `Text.StyledText` funktioniert auch mit dem hier geprüften Qt-5.6-SDK.
- Personenfelder werden als Text escaped. Templates benutzen nur einfaches HTML
  wie `p`, `h2`, `br`, `b` und `a`; keine Skripte, Bilder oder externen Ressourcen.
  Links öffnen nur nach einem Klick und nur für HTTP(S), `mailto:` und `tel:`.
- Unterstützte Platzhalter: `@LEGAL_CONTACT_HTML@`, `@LEGAL_PREVIEW_NOTICE@` und
  `@LEGAL_APPLICATION_NAME@`. Kontaktblock und Entwicklungshinweis in beiden
  Vorlagen beibehalten. Die erzeugten HTML-Dateien enthalten absichtlich die
  öffentlichen Anbieterangaben im Paket; sie gehören trotzdem nicht ins Quell-Git.

## Datenschutztext und Prüfung

Vor dem Ausarbeiten Code und Datenflüsse lesen: lokale Dateien/Datenbanken,
Konten und Schlüsselbund, automatische Netzabrufe, externe Inhalte, Protokolle,
Diagnoseversand, Empfänger, Verschlüsselungsgrenzen, Aufbewahrung und Löschung.
Keine angeblich fehlenden Netzwerkzugriffe oder pauschalen Löschfristen aus einer
anderen App oder einer Website übernehmen. Verarbeitungen des Anbieters von
lokaler Nutzung und konfigurierten Drittservern unterscheiden.

```sh
cmake -P 3rdparty/dencla-sailfish-common/tests/test_legal.cmake
```

Die gemeinsamen Tests laufen auch standalone aus diesem Ordner mit
`cmake -P tests/test_legal.cmake`. Sie prüfen Fallback, vollständige/unvollständige
Profile, Escaping, Entwurfsablehnung, mehrere Apps, QML-Installation und den Wechsel
von Profilen nach der Konfiguration. Außerdem beide Seiten mit der echten
Sailfish-QML-Laufzeit prüfen. In der App-CI lokale Profile und generierte HTML-Dateien
aus dem veröffentlichbaren Dateisatz ausschließen; Fehlermeldungen dürfen keine
persönlichen Werte ausgeben.
