// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// Generic "dock at the top edge once its natural scroll position would go negative" container -
// any single piece of a page's header content - a search
// field, a title, anything - can get the same "scrolls normally along with the header until it
// would go off the top, then clamps in place and shows a backdrop" behavior without re-deriving
// the position math each time. It avoids two common clamping mistakes (using raw contentY
// instead of contentY - originY; clamping contentY itself instead of only the final topMargin).
//
// Must be declared as a PAGE-LEVEL sibling of `view`, never nested inside it (not in header:,
// not as a plain child of the Flickable, and the Flickable itself must never be wrapped in an
// extra containing Item just to "contain" a sibling either): an interactive/focusable child
// (a SearchField) loses keyboard focus whenever the view's
// row set changes if nested inside it at all, and wrapping the Flickable in an extra Item to
// contain a sibling breaks Silica's own initial-scroll-position handling.
//
// Assumes this is the BOTTOM-MOST element of `view`'s own header - naturalTopMargin is measured
// from the header's own bottom edge (headerItem.height - height), matching every current use
// (a search field/title placed after whatever else the header shows). A caller needing this
// mid-header instead would need a different offset - not implemented since nothing needs it yet.
//
// The view's own header must still reserve this item's height while it's visible (e.g. a plain
// `Item { height: dockedThing.height; visible: dockedThing.visible }` as the header Column's own
// last child) so real list content doesn't start underneath its un-docked resting position.
Item {
    id: root

    // The Flickable (SilicaListView/BounceListView/SilicaGridView) this item tracks.
    property Flickable view
    // Content goes here, not as a plain child of root directly - root also owns its own
    // ChromePanelBackground child below, and this redirection (the same default-property
    // trick Silica's own TextBase.qml uses for leftItem/rightItem vs. contentContainer) is
    // what keeps that internal chrome from being mistaken for the caller's own content when
    // sizing this item off it.
    default property alias content: contentItem.data

    readonly property real naturalTopMargin: view && view.headerItem
        ? view.headerItem.height - height - (view.contentY - view.originY)
        : 0
    readonly property bool docked: naturalTopMargin <= 0

    anchors.top: parent.top
    anchors.topMargin: Math.max(0, naturalTopMargin)
    width: view ? view.width : 0
    height: contentItem.childrenRect.height
    z: 20

    ChromePanelBackground {
        anchors.fill: parent
        visible: root.docked
        z: -1
    }

    Item {
        id: contentItem
        anchors.fill: parent
    }
}
