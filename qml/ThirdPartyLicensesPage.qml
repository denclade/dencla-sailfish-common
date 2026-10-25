// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "LegalFiles.js" as LegalFiles

Page {
    id: page
    allowedOrientations: Orientation.All
    LicenseCatalog { id: catalog; objectName: "licenseCatalog" }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height
        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium
            PageHeader { title: qsTranslate("DenclaLegal", "Third-party software and licenses") }
            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: catalog.loading
                visible: running
                size: BusyIndicatorSize.Medium
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: catalog.loadFailed
                wrapMode: Text.WordWrap
                text: qsTranslate("DenclaLegal", "The license information could not be loaded.")
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: catalog.loadFailed
                text: qsTranslate("DenclaLegal", "Try again")
                onClicked: catalog.reload()
            }
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                spacing: Theme.paddingLarge
                Repeater {
                    model: catalog.index ? catalog.index.components : []
                    delegate: Item {
                        id: componentCard
                        width: parent.width
                        height: cardContent.height + 2 * Theme.paddingLarge
                        property var entry: modelData
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.paddingSmall
                            color: Theme.rgba(Theme.primaryColor, 0.06)
                            border.width: 1
                            border.color: Theme.rgba(Theme.primaryColor, 0.12)
                        }
                        Column {
                            id: cardContent
                            x: Theme.paddingLarge
                            y: Theme.paddingLarge
                            width: parent.width - 2 * x
                            spacing: Theme.paddingMedium
                            Column {
                                width: parent.width
                                spacing: Theme.paddingSmall
                                Label {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    font.pixelSize: Theme.fontSizeMedium
                                    font.bold: true
                                    text: componentCard.entry.kind === "application-notices"
                                          ? qsTranslate("DenclaLegal", "Additional notices") : componentCard.entry.name
                                }
                                Label {
                                    width: parent.width
                                    visible: componentCard.entry.license.length > 0
                                    wrapMode: Text.WordWrap
                                    textFormat: Text.PlainText
                                    color: Theme.secondaryColor
                                    font.pixelSize: Theme.fontSizeSmall
                                    text: componentCard.entry.license
                                }
                            }
                            Repeater {
                                model: componentCard.entry.files
                                delegate: Button {
                                    property string filename: modelData
                                    width: cardContent.width
                                    text: componentCard.entry.files.length === 1
                                          ? qsTranslate("DenclaLegal", "License and notices") : filename
                                    onClicked: pageStack.animatorPush(Qt.resolvedUrl("LegalDocumentPage.qml"), {
                                        documentId: "third-party-license", licenseFile: filename,
                                        documentTitle: componentCard.entry.name
                                    })
                                }
                            }
                            // Secondary action stays inside its component's visual boundary.
                            BackgroundItem {
                                width: parent.width
                                height: Theme.itemSizeExtraSmall
                                visible: LegalFiles.externalUrl(componentCard.entry.url)
                                Label {
                                    anchors.centerIn: parent
                                    text: qsTranslate("DenclaLegal", "Project website")
                                    color: Theme.highlightColor
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                onClicked: {
                                    if (LegalFiles.externalUrl(componentCard.entry.url))
                                        Qt.openUrlExternally(componentCard.entry.url)
                                }
                            }
                        }
                    }
                }
            }
            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator { }
    }
}
