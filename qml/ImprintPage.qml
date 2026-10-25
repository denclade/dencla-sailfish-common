// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0

// Compatibility for existing apps using the former imprint/privacy viewer.
LegalDocumentPage {
    property string documentName: "impressum"
    documentId: documentName
}
