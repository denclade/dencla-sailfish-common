// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"
import "LegalFiles.js" as LegalFiles

Page {
    id: page
    allowedOrientations: Orientation.All
    property string documentId: "license"
    property string licenseFile: ""
    property string documentTitle: documentId === "impressum" ? qsTranslate("DenclaLegal", "Imprint")
                                    : documentId === "datenschutz" ? qsTranslate("DenclaLegal", "Privacy Policy")
                                    : qsTranslate("DenclaLegal", "License")
    readonly property bool isLicense: documentId !== "impressum" && documentId !== "datenschutz"
    property string documentText: ""
    property bool loading: true
    property bool loadFailed: false
    property bool ready: false
    property int generation: 0
    property var pendingRequest: null

    LicenseCatalog {
        id: catalog
        active: page.isLicense
        onLoadingChanged: { if (!loading && page.ready && page.isLicense) page.loadDocument() }
    }

    function loadDocument() {
        var token = ++generation
        if (pendingRequest) pendingRequest.abort()
        loading = true
        loadFailed = false
        documentText = ""
        if (isLicense && catalog.loading) return
        var path = LegalFiles.documentPath(documentId, licenseFile, catalog.index)
        if (!path || (isLicense && catalog.loadFailed)) {
            loadFailed = true
            loading = false
            return
        }
        var request = new XMLHttpRequest()
        pendingRequest = request
        request.onreadystatechange = function () {
            if (token !== generation || request.readyState !== XMLHttpRequest.DONE) return
            if ((request.status === 200 || request.status === 0) && request.responseText.trim().length > 0)
                page.documentText = request.responseText
            else page.loadFailed = true
            page.pendingRequest = null
            page.loading = false
        }
        try {
            request.open("GET", Qt.resolvedUrl("../../" + path), true)
            request.send()
        } catch (error) {
            pendingRequest = null
            loadFailed = true
            loading = false
        }
    }

    onDocumentIdChanged: { if (ready) loadDocument() }
    onLicenseFileChanged: { if (ready) loadDocument() }
    Component.onCompleted: { ready = true; loadDocument() }
    Component.onDestruction: {
        ++generation
        if (pendingRequest) pendingRequest.abort()
    }

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
                objectName: "legalDocumentText"
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: !page.isLicense && !page.loading && !page.loadFailed
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.primaryColor
                linkColor: Theme.highlightColor
                textFormat: Text.StyledText
                text: page.isLicense ? "" : page.documentText
                onLinkActivated: {
                    if (/^(https?:|mailto:|tel:)/i.test(link)) Qt.openUrlExternally(link)
                }
            }
            LicenseText {
                objectName: "licenseText"
                width: Math.min(parent.width - 2 * Theme.horizontalPageMargin, Theme.fontSizeSmall * 36)
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.isLicense && !page.loading && !page.loadFailed
                sourceText: page.isLicense ? page.documentText : ""
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: page.loadFailed
                wrapMode: Text.WordWrap
                text: qsTranslate("DenclaLegal", "The document could not be loaded.")
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.loadFailed
                text: qsTranslate("DenclaLegal", "Try again")
                onClicked: {
                    if (page.isLicense) catalog.reload()
                    else page.loadDocument()
                }
            }
            Item { width: 1; height: Theme.paddingLarge }
        }
        VerticalScrollDecorator { }
    }
}
