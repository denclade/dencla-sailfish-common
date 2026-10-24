// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// A DockedItem (see its own header comment for the general docking/positioning mechanism)
// specialized for the one thing every app's own list search needs: a debounced query signal,
// clear-on-hide/focus-on-show, and Silica SearchField's own active/canHide fade transition -
// kept as a page-level sibling because nesting an interactive field inside the list
// can lose keyboard focus when the tracked model's row set changes. See DockedItem.qml.
//
// active is deliberately never written internally (e.g. when the field's own canHide "X" is
// tapped) - only ever read from the caller-owned property of the same name, exactly like
// Silica's own SearchField.canHide/hideClicked contract. Doing otherwise would silently sever
// whatever one-way binding the caller has on active (a caller typically does
// `active: pageLevelSearchActiveFlag`) the first time this component wrote to it itself,
// desyncing that flag from what's actually on screen. hideClicked() is emitted instead, so the
// caller's own state stays the single source of truth throughout - same reasoning as
// DockedItem's own "content, don't reparent" pattern above it.
DockedItem {
    id: root

    // Emitted debounceMs after the reader stops typing, and immediately (with an empty string)
    // the moment active goes false - not on every keystroke, since the typical reaction to this
    // (re-querying a model) is too expensive to run on every letter.
    signal queryChanged(string query)
    // The field's own canHide "X" was tapped - see this component's own header comment for why
    // this doesn't just flip `active` itself.
    signal hideClicked()

    property bool active: false
    property bool canHide: true
    property int debounceMs: 300
    property alias placeholderText: field.placeholderText
    // For a caller that needs the current text directly (e.g. to decide whether "widen search"
    // has anything to widen) - the debounced queryChanged() signal alone doesn't cover every
    // such need, and the two are independent: reading this never restarts the debounce itself.
    property alias text: field.text

    visible: active

    SearchField {
        id: field
        width: parent.width
        active: root.active
        canHide: root.canHide
        // Tapping the virtual keyboard's own enter/done key closes the keyboard without also
        // closing search itself (that's still only ever the caller's own hideClicked()/active
        // handling) - a plain focus loss, not an accept/commit action, since the debounced
        // queryChanged() below already fires on its own regardless of whether the keyboard is
        // still open.
        EnterKey.iconSource: "image://theme/icon-m-enter-close"
        EnterKey.onClicked: focus = false
        onHideClicked: root.hideClicked()
        onActiveChanged: {
            if (active) {
                forceActiveFocus()
            } else {
                debounce.stop()
                root.queryChanged("")
            }
        }
        onTextChanged: debounce.restart()
        Component.onCompleted: if (active) forceActiveFocus() // deep-linked straight into search
    }

    Timer {
        id: debounce
        interval: root.debounceMs
        onTriggered: root.queryChanged(field.text)
    }
}
