// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// Normal Theme text/icons need a backdrop in the same light/dark scheme.
// An opaque base hides scrolling content; a faint accent wash ties the
// panel to the ambience without reducing contrast against accent text.
// Caller sets anchors.fill (or width/height) on the instance, same as any other Item.
Rectangle {
    id: root

    color: Qt.tint(Theme.colorScheme === Theme.LightOnDark ? "#202020" : "#f0f0f0",
                   Qt.rgba(Theme.highlightColor.r, Theme.highlightColor.g, Theme.highlightColor.b, 0.08))
}
