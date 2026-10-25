// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../pages/LegalFiles.js" as LegalFiles

Column {
    id: document
    property string sourceText: ""
    readonly property var blocks: LegalFiles.licenseBlocks(sourceText)
    spacing: Theme.paddingMedium

    Repeater {
        model: document.blocks
        delegate: Item {
            id: block
            width: document.width
            height: content.height + content.y
            property var paragraph: modelData
            readonly property bool heading: paragraph.kind === "heading" || paragraph.kind === "title"
            readonly property real inset: paragraph.kind === "list" ? Theme.fontSizeSmall * 1.5 : 0
            Label {
                x: 0
                y: content.y
                width: block.inset
                visible: block.paragraph.kind === "list"
                text: block.paragraph.marker
                textFormat: Text.PlainText
                font.pixelSize: Theme.fontSizeSmall
                lineHeight: 1.3
            }
            Label {
                id: content
                objectName: "licenseBlockText"
                x: block.inset
                y: block.heading && index > 0 ? Theme.paddingLarge : 0
                width: parent.width - x
                text: block.paragraph.text
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                font.pixelSize: block.heading ? Theme.fontSizeMedium : Theme.fontSizeSmall
                font.bold: block.heading
                font.family: block.paragraph.kind === "example" ? "monospace" : Theme.fontFamily
                color: block.paragraph.kind === "subtitle" ? Theme.secondaryColor : Theme.primaryColor
                lineHeight: 1.3
            }
        }
    }
}
