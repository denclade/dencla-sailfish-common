// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// Installed in qml/pages/. Legal documents are plain local package files;
// publisher details are supplied at build time, never embedded here.
Page {
    id: page
    allowedOrientations: Orientation.All

    property string documentName: "impressum"
    readonly property string documentTitle: documentName === "datenschutz" ? "Datenschutz" : "Impressum"
    property string documentText: ""
    property bool loading: true
    property bool loadFailed: false

    function loadDocument() {
        if (documentName !== "impressum" && documentName !== "datenschutz") {
            loading = false
            loadFailed = true
            return
        }
        loading = true
        loadFailed = false
        var request = new XMLHttpRequest()
        request.onreadystatechange = function () {
            if (request.readyState !== XMLHttpRequest.DONE) return
            page.loading = false
            if ((request.status === 200 || request.status === 0) && request.responseText.trim().length > 0) {
                page.documentText = request.responseText
            } else {
                page.loadFailed = true
            }
        }
        request.open("GET", Qt.resolvedUrl("../../legal/" + documentName + ".html"), true)
        request.send()
    }

    Component.onCompleted: loadDocument()

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: page.documentTitle }
            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: page.loading
                visible: running
                size: BusyIndicatorSize.Medium
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !page.loading && !page.loadFailed
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
                linkColor: Theme.highlightColor
                textFormat: Text.StyledText
                text: page.documentText
                onLinkActivated: {
                    if (/^(https?:|mailto:|tel:)/i.test(link)) Qt.openUrlExternally(link)
                }
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: page.loadFailed
                wrapMode: Text.WordWrap
                text: "Der Rechtstext konnte nicht geladen werden."
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.loadFailed
                text: "Erneut versuchen"
                onClicked: page.loadDocument()
            }
            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator { }
    }
}
