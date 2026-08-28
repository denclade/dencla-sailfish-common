import QtQuick 2.0
import Sailfish.Silica 1.0

// Shared Impressum (German §5 TMG) page - identical content across every app under this
// author (same legal person/address), previously copy-pasted verbatim into
// harbour-sailreader/harbour-wetterstein/harbour-audiobookfish independently. Fill in the
// placeholders below ONCE here and every app picks up the fix at its next update, instead of
// needing the same edit made three times.
//
// Legally-binding content bound to a specific person/jurisdiction - stays German-only
// regardless of system language (see the sailfish/CLAUDE.md "Sprache" section), so none of
// this page's text goes through qsTr() - qsTr()-wrapping it would make it a candidate for
// translation via each app's own .ts files, exactly what must not happen here.
Page {
    id: imprintPage
    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: "Impressum"
            }

            SectionHeader { text: "Angaben gemäß § 5 TMG" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
                textFormat: Text.StyledText
                text: "[Anbieter]<br>" +
                      "[Name]<br>" +
                      "[Straße Hausnummer]<br>" +
                      "[PLZ Ort]"
            }

            SectionHeader { text: "Kontakt" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
                textFormat: Text.StyledText
                text: "E-Mail: contact@example.invalid<br>" +
                      "Telefon: [Telefon]"
            }

            SectionHeader { text: "Verantwortlich für den Inhalt nach § 18 Abs. 2 MStV" }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
                textFormat: Text.StyledText
                text: "[Anbieter]<br>" +
                      "[Name]<br>" +
                      "[Straße Hausnummer]<br>" +
                      "[PLZ Ort]"
            }

            Item { width: 1; height: Theme.paddingLarge }
        }
    }
}
