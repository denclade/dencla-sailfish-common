<!-- SPDX-FileCopyrightText: 2026 Dennis Burgermeister -->
<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Konzept: About-Seite und rechtliche Hinweise

Status: Implementiert. Die gemeinsamen QML-Komponenten und der generierte
Dokumentindex sind über `dencla_install_about()` verfügbar. Audiobookfish
verwendet diese Integration; weitere Apps können denselben Helfer einbinden.

## Einbindung

Nach dem Anlegen des App-Targets ersetzt dieser Aufruf getrennte Aufrufe von
`dencla_install_legal()` und `dencla_install_licenses()`:

```cmake
include("${DENCLA_COMMON_SOURCE_DIR}/cmake/DenclaAbout.cmake")
dencla_install_about(
    TARGET harbour-example
    APP_NAME "Example"
    LICENSE_ID "GPL-3.0-or-later"
    COPYRIGHT "Copyright © 2026 Example Publisher"
    SOURCE_URL "https://github.com/owner/harbour-example"
    PRIVACY_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/legal/datenschutz.html.in"
    COMPONENTS BOUNCE_LIST_VIEW
)
```

Die Argumente der bisherigen Helfer einschließlich `APP_ID`, `PROJECT_LICENSE`,
`PROFILE`, `MINIZ_SOURCE`, `NOTICES_FILE` und `EXTRA_LICENSE_FILES` stehen auch
hier zur Verfügung. `COPYRIGHT` und `LICENSE_ID` sind für den gemeinsamen
About-Hinweis erforderlich. Die strengen Prüfungen werden weiterhin über
`DENCLA_REQUIRE_LEGAL_DETAILS` gesteuert.

In der app-spezifischen About-Seite, die `../components` importiert, wird der
frühere Copyright-/Lizenzblock durch `AboutLicenseNotice` ersetzt. Der gemeinsame
`AboutLegalSection` steht einmalig am Seitenende. Die Komponenten werden vom
Helfer nach `qml/components/`, die Dokumentseiten nach `qml/pages/` installiert.
App-Name, Lizenz und Copyright werden aus dem generierten Index gelesen.

Die gemeinsame Übersetzung wird im App-Katalog geführt. Bei `lupdate` zusätzlich
das gemeinsame `qml/`-Verzeichnis angeben; alle neuen Oberflächentexte verwenden
den Übersetzungskontext `DenclaLegal`. Audiobookfish enthält die deutschen
Übersetzungen und berücksichtigt das Verzeichnis bereits im `lupdate`-Task.

## Aufbau der About-Seite

Die About-Seite bleibt die zentrale Anlaufstelle für Informationen zur App.
Ihre Reihenfolge soll in allen Apps gleich sein:

1. App-Icon, Name, Version und kurze Beschreibung.
2. Copyright und ein gut lesbarer, kurzer Lizenz- und Gewährleistungshinweis.
3. Projektlinks: Website und Quellcode.
4. Optional Danksagungen für Projekte, Mitwirkende oder Unterstützer.
5. Ganz unten der gemeinsame Bereich **Rechtliches** mit vier Einträgen:
   **Lizenz**, **Drittanbieter und Lizenzen**, **Impressum**, **Datenschutz**.

Diese vier Dokumente werden innerhalb der App geöffnet. Ihre Einstiegspunkte
stehen ausschließlich im unteren Rechtliches-Bereich der About-Seite.
Kein zusätzlicher Lizenzlink im allgemeinen Links-Bereich und keine parallelen
Impressums- oder Datenschutzlinks in den Einstellungen oder anderen Menüs.
Der Zugang „Über …“ steht ausschließlich in den Einstellungen, nicht im
Pull-down-Menü.

Das ist die vereinbarte Navigation für diese Apps. Sollte eine zukünftige
Funktion einen eigenen Datenschutzhinweis direkt bei der Dateneingabe brauchen,
muss dieser Bedarf gesondert behandelt werden; die zentrale Dokumentablage
und die About-Verlinkung bleiben dabei bestehen.

## Kurztext auf der About-Seite

Für eine App unter GPL-3.0-or-later ist dieser Text als Vorlage vorgesehen:

> Copyright © 2026 Dennis Burgermeister.
> Diese Anwendung ist freie Software unter der GNU General Public License,
> Version 3 oder jeder späteren Version. Sie dürfen sie unter den Bedingungen
> dieser Lizenz verändern und weitergeben. Soweit gesetzlich zulässig, wird
> die Anwendung ohne Gewährleistung bereitgestellt.
> Den vollständigen Lizenztext finden Sie unten unter „Lizenz“.

Copyright, Jahr und Lizenzbezeichnung kommen aus den Metadaten der jeweiligen
App. Der Text darf nicht pauschal GPL nennen, wenn eine andere App eine andere
Lizenz verwendet. Die Kurzfassung ersetzt den mitgelieferten Lizenztext nicht.
Sie wird normal lesbar dargestellt, nicht als kaum lesbare Fußnote.

Die GPLv3 beschreibt entsprechende Hinweise für interaktive Oberflächen in
Abschnitt 0 und 5(d), einschließlich der dort genannten Ausnahme. Eine Rubrik
namens „Acknowledgements“ oder eine automatische Generierung wird damit nicht
vorgeschrieben. Grundlage ist der [vollständige GPL-Text](../LICENSE), zusätzlich
[online bei GNU](https://www.gnu.org/licenses/gpl.en.html).
Dieses Konzept legt die technische Darstellung fest; es bestätigt keine
vollständige rechtliche Prüfung der jeweiligen App oder Veröffentlichung.

## Inhalt der vier Einträge

| Eintrag | Inhalt und Quelle |
|---|---|
| Lizenz | Vollständige, unveränderte App-Lizenz aus `licenses/APP-LICENSE.txt`, offline lesbar |
| Drittanbieter und Lizenzen | Generierte Übersicht der ausgelieferten Komponenten; pro Komponente Zugang zu vollständigen Copyright-Hinweisen, Lizenzbedingungen und Haftungsausschlüssen |
| Impressum | Bereits generierte `legal/impressum.html`, einschließlich der für den Build gewählten Anbieterangaben |
| Datenschutz | Bereits generierte `legal/datenschutz.html` mit der app-spezifischen Beschreibung der Datenverarbeitung |

Unter „Drittanbieter und Lizenzen“ wird auch die gemeinsame Bibliothek mit
ihrem GPL-Text aufgeführt. Weitere Einträge richten sich nach den tatsächlich
ausgelieferten Komponenten: beispielsweise Jollas Silica-Formel in
`BounceListView`, miniz und bei Apps mit entsprechender Nutzung die
Kryptografie-Bibliotheken. Die Herkunftsangaben und vollständigen Originaltexte
bleiben erhalten. Gemeinsam genutzte Lizenzdateien müssen nicht dupliziert
werden, solange die Zuordnung zu jeder Komponente klar ist.

## Danksagungen und Lizenzinformationen

Danksagungen sind freiwillige redaktionelle Inhalte. Beispielsweise kann eine
App Audiobookshelf für das Serverprojekt danken, obwohl dessen Servercode
nicht Teil des App-Pakets ist. Solche Danksagungen können als kurze Liste mit
Projektlinks auf der About-Seite bleiben. `AboutAcknowledgements.qml` stellt
diese als einheitliche, linksbündige Zeilen mit Projektname und optionaler
Funktionsbeschreibung dar; kleine Bullet- oder HTML-Linklisten entfallen.

Die rechtlichen Komponentenangaben werden dagegen aus der Build-Konfiguration
abgeleitet und stehen unter „Drittanbieter und Lizenzen“. Dafür wird keine
zweite, manuell gepflegte Lizenzliste im About-QML angelegt. Von Sailfish OS
bereitgestellte Systembibliotheken können als solche benannt werden, ohne
eine tatsächlich mit der App ausgelieferte Bibliothek daraus abzuleiten.
In der Lizenzübersicht bilden Name, Lizenz und Aktionen pro Komponente einen
umrandeten Block. Kleine Abstände innerhalb des Blocks und größere Abstände
zwischen den Blöcken zeigen die Zugehörigkeit. Der Lizenztext ist die primäre
Aktion; ein vorhandener Projektlink steht als sekundäre Aktion im selben Block.

## Gemeinsame Umsetzung in dencla-sailfish-common

Die Apps behalten ihre Beschreibung, ihr Icon und ihre Projektlinks. Commons
stellt die wiederverwendbaren Bausteine für den unteren Bereich und die
Dokumentanzeige bereit. Enthalten sind:

- `AboutLegalSection.qml`: die vier zentralen Einträge mit einheitlicher
  Reihenfolge und übersetzbaren Beschriftungen.
- `LegalDocumentPage.qml`: gemeinsamer lokaler Dokumentbetrachter. Lizenztexte
  werden durch `LicenseText.qml` als strukturierte Plaintext-Blöcke angezeigt,
  generierte Impressums- und Datenschutztexte
  im dafür unterstützten HTML-Format. Der bestehende Betrachter
  `ImprintPage.qml` bleibt als kompatible Weiterleitung verfügbar.
- `ThirdPartyLicensesPage.qml`: Übersicht der generierten Komponenten mit
  Navigation zu ihren vollständigen Lizenztexten.

`DenclaLicenses.cmake` bleibt die einzige Stelle zur Auswahl der Lizenzquellen
und Komponenten. Der Generator erzeugt zusätzlich zu den vollständigen Texten
und `THIRD_PARTY_NOTICES.txt` einen strukturierten Index `licenses/index.json`. Dieser beschreibt Komponenten, Lizenzbezeichnungen, Herkunftslinks
und die zugehörigen lokalen Dokumentdateien. Die QML-Übersicht liest diesen
Index; sie zerlegt keine menschenlesbare Textdatei und enthält keine kopierte
Liste der Bibliotheken. `EXTRA_LICENSE_FILES` und `NOTICES_FILE` sind im Index ebenfalls zugänglich.
Zusätzliche Lizenzdateien werden anhand ihres Dateinamens bezeichnet;
`NOTICES_FILE` erscheint als übersetzbarer Eintrag für weitere Hinweise.

`DenclaLegal.cmake` erzeugt Impressum und Datenschutz weiterhin aus dem bereits
gewählten Anbieterprofil und den Templates. Hier wird kein zweites Profil und
keine zusätzliche Kopie der Kontaktdaten im QML oder Lizenzindex eingeführt.

Alle Dateien werden innerhalb von `share/<APP_ID>/` installiert. Index und
Texte werden bei Konfiguration, Build und Installation gemeinsam erneuert.
Die Apps binden den gemeinsamen Bereich ein und übergeben ihre Metadaten;
sie pflegen keine Kopien der gemeinsamen QML-Dateien.

## Verhalten der Dokumentanzeige

Die vollständigen Rechtstexte funktionieren ohne Netz, Browser oder externe
Viewer. Die mitgelieferten Originaldateien bleiben unverändert. Für die Anzeige
führt `LicenseText.qml` feste Zeilenumbrüche innerhalb von Fließtextabsätzen
zusammen. Titel, Versionszeile und nummerierte Abschnitte erhalten eine klare
Hierarchie; Aufzählungen werden mit hängenden Einzügen dargestellt. Beispiele
behalten ihre Zeilenumbrüche und relative Einrückung. Alle Wörter,
Copyright-Hinweise und Listenkennzeichen bleiben erhalten. Text bleibt Plaintext,
sodass beispielsweise `<year>` nicht als HTML interpretiert wird.

Der Betrachter verwendet linksbündigen Satz, einheitliche Absatzabstände und
größere Abstände vor Abschnittsüberschriften. Eine begrenzte Lesebreite verhindert
zu lange Zeilen im Querformat; ein proportionaler Zeilenabstand hält lange
Absätze lesbar. Lange Texte sind scrollbar und dürfen
nicht abgeschnitten werden. Beschriftungen, Ladefehler und Wiederholen-Aktionen
sind übersetzbar; die Original-Lizenztexte werden nicht durch Übersetzungen
ersetzt.

Der Betrachter akzeptiert nur bekannte Dokumente innerhalb der installierten
App-Verzeichnisse. Ein Indexeintrag darf keine beliebige lokale Datei oder
einen entfernten Inhalt als Rechtstext laden. Externe Projekt-, E-Mail- oder
Telefonlinks werden nur nach Benutzeraktion geöffnet. Bei einer fehlenden Datei
erscheint eine verständliche Fehlermeldung; es gibt keinen stillen Wechsel zu
einem Online-Lizenztext.

Der Quellcode-Projektlink bleibt bei den Projektlinks. Für veröffentlichte
Binärpakete soll der zur Version gehörende Quellstand auffindbar sein, etwa über
einen öffentlichen Release-Tag mit Anleitung zu den benötigten Submodulen.
Die Anzeige eines Repositorylinks allein bestätigt noch nicht die vollständige
Bereitstellung des zugehörigen Quellcodes.

## Abnahme und Übernahme in weitere Apps

1. In weiteren Apps `dencla_install_about()` mit App-Metadaten, Datenschutz-Template
   und der tatsächlichen Komponentenauswahl einbinden.
2. `AboutLicenseNotice` und `AboutLegalSection` auf der About-Seite verwenden;
   doppelte Links und manuelle Lizenzübersichten entfernen.
3. Die gemeinsamen QML-Dateien in die Übersetzungsextraktion aufnehmen; sie
   verwenden einheitlich den Kontext `DenclaLegal`. Freiwillige Danksagungen
   können bestehen bleiben.
4. Mit mindestens zwei unterschiedlich konfigurierten Apps prüfen, dass ihre
   Komponenten und Pfade getrennt bleiben. Nicht ausgewählte Komponenten
   dürfen nicht in der generierten Übersicht erscheinen.
5. Auf dem Gerät offline alle vier Einträge öffnen; lange Lizenztexte,
   Sprachwechsel, fehlende Dateien und Zurück-Navigation prüfen. Einen
   RPM-Installationsbaum auf Index, Originaltexte und Rechtstexte kontrollieren.

Die vorhandenen strengen RPM-Prüfungen für fehlende Anbieterangaben und
Datenschutzentwürfe bleiben erhalten. Die gemeinsamen Tests prüfen zusätzlich die Konsistenz zwischen Index,
Komponentenauswahl und installierten Dateien. Die optionalen QML-Tests laden
die installierten Seiten und prüfen Offline-Dateien, Plaintext-/HTML-Darstellung,
Fehlerfälle und den kompatiblen Impressumszugang.

Details der vorhandenen Helfer: [Lizenzpaketierung](licenses.md) und
[Impressum/Datenschutz](legal.md).

## Tests

Ohne Qt beziehungsweise mit optionalem Node.js:

```sh
cmake -P tests/test_licenses.cmake
cmake -P tests/test_about.cmake
cmake -P tests/test_legal.cmake
node tests/test_legal_files.js
```

Mit Qt Quick Test in einer passenden Qt-/Sailfish-Umgebung:

```sh
cmake -DDENCLA_BUILD_QML_TEST_RUNNER=ON -P tests/test_about.cmake
```

Der letzte Aufruf baut einen kleinen Runner für die vorhandene QuickTest-
Laufzeitbibliothek. Alternativ kann ein vorhandener Runner über
`-DDENCLA_QML_TEST_RUNNER=/pfad/zu/qmltestrunner` verwendet werden. Die Tests
verwenden das Qt-Plattformplugin `minimal` und ersetzen keinen visuellen Test
auf dem Gerät.
