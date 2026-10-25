// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import Sailfish.Silica 1.0
import "../pages"

Label {
    id: notice
    font.pixelSize: Theme.fontSizeSmall
    color: Theme.secondaryColor
    horizontalAlignment: Text.AlignLeft
    wrapMode: Text.WordWrap
    textFormat: Text.PlainText
    LicenseCatalog { id: catalog }
    text: {
        if (!catalog.index) return catalog.loadFailed
                    ? qsTranslate("DenclaLegal", "The license information could not be loaded.") : ""
        var app = catalog.index.application
        var lines = app.copyright ? [app.copyright] : []
        if (app.license === "GPL-3.0-or-later") {
            lines.push(qsTranslate("DenclaLegal", "Free software under the GNU General Public License, version 3 or later. You may modify and redistribute it under this license."))
            lines.push(qsTranslate("DenclaLegal", "Provided without warranty, to the extent permitted by law."))
        } else if (app.license) {
            lines.push(qsTranslate("DenclaLegal", "Licensed under %1.").arg(app.license))
        }
        lines.push(qsTranslate("DenclaLegal", "See License below for the full terms."))
        return lines.join("\n")
    }
}
