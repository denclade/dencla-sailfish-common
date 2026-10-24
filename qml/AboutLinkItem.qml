// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0

// A single tappable link row for an AboutPage's "Links"/"Legal" sections - lifted verbatim
// from harbour-sailreader/harbour-audiobookfish/harbour-wetterstein, which all had an
// identical copy of this already.
ListItem {
    id: root
    width: parent.width
    contentHeight: Theme.itemSizeSmall

    property alias text: label.text

    Label {
        id: label
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        color: root.highlighted ? Theme.highlightColor : Theme.primaryColor
        truncationMode: TruncationMode.Fade
    }
}
