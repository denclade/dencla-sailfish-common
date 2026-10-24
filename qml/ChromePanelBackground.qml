// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// Two-layer "docked chrome panel" background - an opaque dark scrim first, so list content
// scrolling underneath a pinned/docked copy of this never shows through and hurts legibility,
// then a subtle highlight-colored wash on top so the panel still reads as this app's accent
// rather than a generic dark bar (a saturated accent-colored fill on its own was tried and
// rejected: near-zero contrast against same-colored text/icons drawn on top of it). Originally
// harbour-sailreader-only (its sticky section header/multi-select footer), then duplicated
// verbatim into harbour-audiobookfish for the same reasoning before landing here.
// Caller sets anchors.fill (or width/height) on the instance, same as any other Item.
Item {
    id: root

    Rectangle {
        anchors.fill: root
        color: "black"
        opacity: 0.6
    }
    Rectangle {
        anchors.fill: root
        color: Theme.highlightColor
        opacity: 0.18
    }
}
