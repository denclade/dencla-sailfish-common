// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../pages/LegalFiles.js" as LegalFiles

Column {
    width: parent.width
    property var projects: []
    spacing: Theme.paddingSmall
    SectionHeader { text: qsTranslate("DenclaLegal", "Acknowledgements") }
    Repeater {
        model: projects
        delegate: ListItem {
            id: projectRow
            width: parent.width
            contentHeight: Math.max(Theme.itemSizeMedium, projectText.height + 2 * Theme.paddingMedium)
            property var project: modelData
            enabled: LegalFiles.externalUrl(project.url)
            Column {
                id: projectText
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.paddingSmall
                Label {
                    width: parent.width
                    text: projectRow.project.name
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeMedium
                    color: projectRow.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    text: projectRow.project.description || ""
                    visible: text.length > 0
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryColor
                }
            }
            onClicked: { if (LegalFiles.externalUrl(project.url)) Qt.openUrlExternally(project.url) }
        }
    }
}
