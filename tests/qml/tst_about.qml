// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import QtTest 1.0
import "../qml/pages" as Legal
import "../qml/components" as About

TestCase {
    id: testCase
    name: "SharedAbout"
    property var temporaryObjects: []
    property QtObject navigation: QtObject {
        id: pageStack
        property url lastUrl
        property var lastProperties
        function animatorPush(url, properties) {
            lastUrl = url
            lastProperties = properties || {}
        }
    }

    // Qt 5.6 predates TestCase.createTemporaryObject().
    function createTemporaryObject(component, parent, properties) {
        var object = component.createObject(parent, properties || {})
        if (object) temporaryObjects.push(object)
        return object
    }
    function cleanup() {
        for (var i = 0; i < temporaryObjects.length; ++i) temporaryObjects[i].destroy()
        temporaryObjects = []
        wait(0)
    }

    Component { id: viewer; Legal.LegalDocumentPage { } }
    Component { id: thirdParty; Legal.ThirdPartyLicensesPage { } }
    Component { id: section; About.AboutLegalSection { width: 540 } }
    Component { id: notice; About.AboutLicenseNotice { width: 540 } }
    Component { id: legacy; Legal.ImprintPage { } }
    Component { id: credits; About.AboutAcknowledgements { width: 540 } }

    function test_license() {
        var page = createTemporaryObject(viewer, testCase)
        verify(page !== null)
        tryCompare(page, "loading", false)
        compare(page.loadFailed, false)
        verify(page.documentText.indexOf("<literal markup>") >= 0)
        var formatted = findChild(page, "licenseText")
        verify(formatted !== null)
        compare(findChild(formatted, "licenseBlockText").textFormat, Text.PlainText)
    }

    function test_legal_documents_data() {
        return [{tag: "imprint", documentId: "impressum"}, {tag: "privacy", documentId: "datenschutz"}]
    }
    function test_legal_documents(data) {
        var page = createTemporaryObject(viewer, testCase, {documentId: data.documentId})
        verify(page !== null)
        tryCompare(page, "loading", false)
        compare(page.loadFailed, false)
        verify(page.documentText.indexOf("Test Publisher") >= 0)
        compare(findChild(page, "legalDocumentText").textFormat, Text.StyledText)
    }

    function test_unknown_file() {
        var page = createTemporaryObject(viewer, testCase, {documentId: "third-party-license", licenseFile: "../secret.txt"})
        verify(page !== null)
        tryCompare(page, "loading", false)
        compare(page.loadFailed, true)
        compare(page.documentText, "")
    }

    function test_about_components() {
        var links = createTemporaryObject(section, testCase)
        verify(links !== null)
        var label = createTemporaryObject(notice, testCase)
        verify(label !== null)
        for (var i = 0; i < 250 && label.text.indexOf("Copyright © First Publisher") < 0; ++i) wait(20)
        verify(label.text.indexOf("Copyright © First Publisher") >= 0)
        verify(label.text.indexOf("without warranty") >= 0)
        var page = createTemporaryObject(thirdParty, testCase)
        verify(page !== null)
        var catalog = findChild(page, "licenseCatalog")
        verify(catalog !== null)
        tryCompare(catalog, "loading", false)
        compare(catalog.loadFailed, false)
        compare(catalog.index.components.length, 2)
    }

    function test_legacy_imprint() {
        var page = createTemporaryObject(legacy, testCase)
        verify(page !== null)
        tryCompare(page, "loading", false)
        compare(page.documentId, "impressum")
        compare(page.loadFailed, false)
    }

    function test_acknowledgements() {
        var list = createTemporaryObject(credits, testCase, { projects: [
            {name: "Server", description: "Server software", url: "https://example.test"},
            {name: "Media", description: "Media controls", url: ""}
        ] })
        verify(list !== null)
        verify(list.height > 0)
    }

    function test_navigation_data() {
        return [
            {tag: "license", row: "licenseLink", filename: "LegalDocumentPage.qml", document: "license"},
            {tag: "third-party", row: "thirdPartyLicensesLink", filename: "ThirdPartyLicensesPage.qml", document: ""},
            {tag: "imprint", row: "imprintLink", filename: "LegalDocumentPage.qml", document: "impressum"},
            {tag: "privacy", row: "privacyLink", filename: "LegalDocumentPage.qml", document: "datenschutz"}
        ]
    }
    function test_navigation(data) {
        var links = createTemporaryObject(section, testCase)
        var link = findChild(links, data.row)
        verify(link !== null)
        pageStack.lastUrl = ""
        link.clicked(null)
        verify(pageStack.lastUrl.toString().indexOf("/qml/pages/" + data.filename) >= 0)
        var component = Qt.createComponent(pageStack.lastUrl)
        compare(component.status, Component.Ready, component.errorString())
        var page = createTemporaryObject(component, testCase, pageStack.lastProperties)
        verify(page !== null)
        if (data.document) {
            compare(page.documentId, data.document)
            tryCompare(page, "loading", false)
            compare(page.loadFailed, false)
        }
    }
}
