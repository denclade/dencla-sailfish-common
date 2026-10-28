<!-- SPDX-FileCopyrightText: 2026 Dennis Burgermeister -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Screenshots und App-Touren für Sailfish-Apps

Gemeinsames Format für README und spätere Website-Promotion. Diese Anleitung
gilt für alle `harbour-*`-Projekte, die `dencla-sailfish-common` einbinden. README, App-Sprache,
Bildunterschriften und Alternativtexte sind Englisch; Entwicklerdoku darf
Deutsch sein. FitFish verwendet dieses Format in seiner Projekt-README und `docs/media/`.

## 1. Testdaten vorbereiten

Auf dem Testemulator oder einem getrennten Testprofil arbeiten. Vorher
App-Daten, Demo-Markierungen, Einstellungen und Laufzustand sichern; danach
wiederherstellen. Auch automatisch beim Start oder Sync erzeugte Dateien
berücksichtigen. Gemeinsame Profile nicht pauschal zurücksetzen.

Synthetische, plausible Daten über den echten Datenpfad laden: etwa Fixtures,
einen Mock-Server oder einen Gerätesimulator. Die App soll daraus selbst ihre
Listen, Kennzahlen und Graphen berechnen. Keine Werte nur für das Foto in QML
eintragen. Bei zeitabhängigen Ansichten Daten relativ zum Aufnahmedatum
erzeugen und genügend Historie für aussagekräftige Verläufe bereitstellen.
Keine privaten Konten, Nachrichten, Gesundheitsdaten oder GPS-Spuren verwenden.

In `docs/media/README.md` App-Version, Aufnahmedatum, Emulatorauflösung,
Datenquelle und Schritte zum Befüllen festhalten. Auch benötigte Testserver
und deren Start-/Seed-Befehle dokumentieren, ohne Zugangsdaten abzulegen.

## 2. Highlights aufnehmen

Sechs Motive auswählen: Übersicht, zentrale Datenansicht, Verlauf/Auswertung,
Liste, Detailansicht und eine besondere Interaktion. App auf Englisch starten,
Display entsperren, Daten vollständig laden lassen und normale Touch-Eingaben
verwenden. Nach jedem Screenshot mindestens fünf Sekunden warten, damit dessen
Benachrichtigung nicht in der nächsten Aufnahme erscheint.

Bei Projekten mit den gemeinsamen `dev:*`-Tasks:

```sh
mkdir -p docs/media
mise run dev:run
sleep 5
mise run dev:screenshot -- docs/media/overview.png
# Ansicht öffnen; Koordinaten aus einem Screenshot ablesen.
mise run dev:touch -- tap X Y
sleep 5
mise run dev:screenshot -- docs/media/detail.png
```

Die Tasks und ihre Voraussetzungen sind projektspezifisch. Der gemeinsame
[Screenshot-Helfer](../scripts/capture-screenshot.py) kann direkt oder aus
`dev:screenshot` aufgerufen werden (aus dem App-Projektverzeichnis):

```sh
python3 3rdparty/dencla-sailfish-common/scripts/capture-screenshot.py docs/media/overview.png
# Abweichenden SSH/SCP-Wrapper angeben:
# --emulator path/to/emulator.sh
```

Er wartet standardmäßig fünf Sekunden, verwendet einen eindeutigen temporären
Pfad im Home-Verzeichnis des Emulators und räumt ihn auch bei Fehlern auf.
Python läuft auf dem Entwicklungsrechner, nicht auf dem Emulator. Der
App-Wrapper muss `ssh COMMAND` und `scp :REMOTE LOCAL` unterstützen und
die Verbindung inklusive Schlüssel konfigurieren. Der Helfer startet oder
entsperrt die App nicht. Touch-Steuerung und Demo-Daten bleiben app-spezifisch.
Sailjail-Apps mit ihrer regulären Startzeile starten; Demo-Modi gegebenenfalls
über Konfiguration statt zusätzlicher Kommandozeilenargumente aktivieren.

Originale als PNG in nativer Auflösung speichern, beispielsweise 720 × 1600
Pixel im Hochformat. Kein Geräterahmen und keine nachträglich erfundene UI. Bilder auf
abgeschnittene Inhalte, Ladezustände und Benachrichtigungen prüfen. Für eine
Interaktion echte Vorher-/Nachher-Zustände aufnehmen und das Verhalten prüfen,
etwa gemeinsame Auswahlmarker in mehreren Graphen.

## 3. Einheitlich ablegen und darstellen

```text
docs/media/
  README.md            # Herkunft, Aufnahme- und Exportanleitung
  overview.png         # Originale mit kurzen englischen Dateinamen
  detail.png
  ...
  thumbs/              # Sechs PNG-Vorschaubilder, 220 Pixel breit
    overview.png
    detail.png
    ...
  workflow.gif         # Optionale Tour für die README
  workflow.mp4         # Dieselbe Tour für die Website
  promo.json           # App-spezifische Motive, Texte und Standzeiten
```

In der Projekt-README den englischen Abschnitt `## Screenshots` mit zwei
Markdown-Tabellen zu je drei Spalten verwenden. Vorschaubilder verlinken auf
die Originale; ausschließlich Markdown, keine HTML-`img`-Tags oder CSS:

```markdown
## Screenshots

Screenshots from the Sailfish OS test emulator with synthetic demo data.
Click a thumbnail to view the full-resolution image.

| Overview | Data | History |
| :---: | :---: | :---: |
| [![App overview](docs/media/thumbs/overview.png)](docs/media/overview.png) | [![Data view](docs/media/thumbs/data.png)](docs/media/data.png) | [![History](docs/media/thumbs/history.png)](docs/media/history.png) |

| List | Details | Interaction |
| :---: | :---: | :---: |
| [![Item list](docs/media/thumbs/list.png)](docs/media/list.png) | [![Item details](docs/media/thumbs/detail.png)](docs/media/detail.png) | [![Interaction result](docs/media/thumbs/interaction.png)](docs/media/interaction.png) |
```

Thumbnails proportional verkleinern; Originale behalten. Der gemeinsame
[Exporthelfer](../scripts/render-promo.py) liest die Konfiguration aus dem
App-Projekt. Bildpfade sind relativ zur JSON-Datei; die Exporte entstehen
daneben. Beispiel für `docs/media/promo.json` (die übrigen Motive ergänzen):

```json
{
  "thumbnails": ["overview.png", "detail.png"],
  "scenes": [
    {"image": "overview.png", "caption": "Explore the overview", "duration": 3},
    {"image": "detail.png", "caption": "Open an item", "duration": 3}
  ]
}
```

```sh
python3 3rdparty/dencla-sailfish-common/scripts/render-promo.py --config docs/media/promo.json --thumbnails-only
```

Das Skript benötigt Python 3 und FFmpeg (`--ffmpeg /pfad/zu/ffmpeg` für einen
abweichenden Programmpfad). Alle Bild- und Linkziele relativ zur jeweiligen README
prüfen und PNGs direkt öffnen. Zusätzlich die Vorschau im verwendeten Editor
und im Repository-Host prüfen; eine Pfadprüfung allein bestätigt deren
Darstellung nicht.

## 4. Optionale kurze Tour

Etwa 15–30 Sekunden: Übersicht → Daten/Liste → Detail → besondere Interaktion.
Das Skript erzeugt aus echten Screenshots eine geschnittene Folge mit englischen
Bildunterschriften außerhalb der App: GIF mit 360 Pixel Breite und Endlosschleife,
MP4 in Originalbreite mit H.264, ohne Ton und mit `faststart`. Den Export mit
`python3 3rdparty/dencla-sailfish-common/scripts/render-promo.py --config docs/media/promo.json` starten; dafür braucht FFmpeg zusätzlich
`drawtext`, Font-Unterstützung und `libx264`.

In der Medien-Doku kenntlich machen, ob eine Tour Schnitte oder eine laufende
Bildschirmaufnahme zeigt. In der README das GIF als normales Markdown-Bild
einbinden und die MP4 verlinken. Auf Websites die MP4 mit `muted`, `playsinline`,
`loop`, einem PNG-Poster und Rücksicht auf `prefers-reduced-motion` verwenden.
Die Medienlizenz passend zum jeweiligen Projekt dokumentieren.

## Helfer prüfen

Die Screenshot-Tests verwenden einen simulierten SSH/SCP-Wrapper und brauchen
keinen laufenden Emulator:

```sh
python3 3rdparty/dencla-sailfish-common/tests/test_app_media.py
```
