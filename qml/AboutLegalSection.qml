// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0

Column {
    width: parent.width
    SectionHeader { text: qsTranslate("DenclaLegal", "Legal") }
    AboutLinkItem {
        objectName: "licenseLink"
        text: qsTranslate("DenclaLegal", "License")
        onClicked: pageStack.animatorPush(Qt.resolvedUrl("../pages/LegalDocumentPage.qml"))
    }
    AboutLinkItem {
        objectName: "thirdPartyLicensesLink"
        text: qsTranslate("DenclaLegal", "Third-party software and licenses")
        onClicked: pageStack.animatorPush(Qt.resolvedUrl("../pages/ThirdPartyLicensesPage.qml"))
    }
    AboutLinkItem {
        objectName: "imprintLink"
        text: qsTranslate("DenclaLegal", "Imprint")
        onClicked: pageStack.animatorPush(Qt.resolvedUrl("../pages/LegalDocumentPage.qml"), { documentId: "impressum" })
    }
    AboutLinkItem {
        objectName: "privacyLink"
        text: qsTranslate("DenclaLegal", "Privacy Policy")
        onClicked: pageStack.animatorPush(Qt.resolvedUrl("../pages/LegalDocumentPage.qml"), { documentId: "datenschutz" })
    }
}
