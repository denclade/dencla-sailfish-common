// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import Sailfish.Silica 1.0
import QtLocation 5.0
import QtPositioning 5.2

// A map on OpenStreetMap tiles through the osm geoservices plugin, set up
// the way every app here needs it: the plugin's online provider
// repository switched off (it points at a long-dead server and only adds
// a slow request before the first tile), an app-specific user agent as
// the OSM tile usage policy asks for, and zoom buttons that fit Silica.
//
// Map items (MapPolyline, MapQuickItem, ...) go in as children, like
// with a plain Map:
//
//     OsmMap {
//         userAgent: "harbour-myapp"
//         MapPolyline { path: ...; line.color: Theme.highlightColor }
//     }
//
// Needs, in the app: Permissions=Internet in the .desktop file, and
// Requires qml(QtLocation), qml(QtPositioning), qt5-plugin-geoservices-osm
// in the .spec. Every tile request tells the tile server which area is
// looked at -- an app showing personal places (a home, a running route)
// should only load tiles once the user has agreed to that.
Item {
    id: root

    // Sent with every tile request; name the app.
    property string userAgent: "dencla-sailfish"
    property bool showZoomButtons: false
    // Pan and pinch. Off for a map that sits in a flickable page and
    // should not steal its gestures.
    property bool interactive: true
    property alias center: map.center
    property alias zoomLevel: map.zoomLevel
    // With `interactive` off: a tap, or a sideways drag, on the map
    // reports where it was -- e.g. to pick the nearest point of a route.
    property bool pickEnabled: false
    signal picked(real latitude, real longitude)
    // false when the osm geoservices plugin is missing or cannot map --
    // then the map stays empty, and the app should say so.
    readonly property bool serviceAvailable: mapPlugin.isAttached && mapPlugin.supportsMapping()
    // A finger is on the map while picking -- e.g. for a Page to turn off
    // its sideways back gesture meanwhile.
    readonly property bool pickPressed: pickArea.pressed

    // The QtLocation Map itself, for what this does not wrap.
    readonly property alias map: map
    // Collected here and handed to map.addMapItem() once complete:
    // QtLocation 5.4 does not pick up map items that arrive as children of
    // anything but a Map declared right there, so an alias to map.data
    // would draw nothing. (A "default property list<...>" does not parse
    // on Qt 5.6.)
    default property alias mapItems: holder.data

    Component.onCompleted: {
        // One line per map, so a map that stays empty on a device can be
        // told apart: no plugin, or no tiles (network, permission).
        console.warn("[OsmMap] plugin", mapPlugin.name, "attached", mapPlugin.isAttached,
                     "mapping", mapPlugin.isAttached && mapPlugin.supportsMapping(),
                     "map types", map.supportedMapTypes.length)
        var items = []
        for (var i = 0; i < holder.data.length; ++i)
            items.push(holder.data[i])
        for (i = 0; i < items.length; ++i)
            map.addMapItem(items[i])
    }

    Item {
        id: holder
        visible: false
    }

    // Centers and zooms so the box from `south`..`north`, `west`..`east`
    // (degrees) is visible with a margin. Plain numbers rather than
    // coordinates: on Qt 5.6 a coordinate kept in a JS array loses its
    // latitude and longitude.
    function fitBounds(north, south, east, west) {
        // root's size: the Map's own lags behind while anchors settle.
        if (root.width <= 0 || root.height <= 0)
            return
        map.center = QtPositioning.coordinate((north + south) / 2, (east + west) / 2)
        // Web Mercator: 256 px tiles, 2^zoom tiles around the world.
        var latRadians = (north + south) / 2 * Math.PI / 180
        var spanX = Math.max(1e-6, (east - west) / 360)
        var spanY = Math.max(1e-6, (north - south) / 360 / Math.cos(latRadians))
        var margin = 1.25
        var zoomX = Math.log(root.width / 256 / (spanX * margin)) / Math.LN2
        var zoomY = Math.log(root.height / 256 / (spanY * margin)) / Math.LN2
        map.zoomLevel = Math.max(map.minimumZoomLevel,
                                 Math.min(map.maximumZoomLevel, 18, Math.floor(Math.min(zoomX, zoomY))))
    }

    Map {
        id: map

        anchors.fill: parent
        gesture.enabled: root.interactive
        plugin: Plugin {
            id: mapPlugin
            name: "osm"

            PluginParameter {
                name: "osm.mapping.providersrepository.disabled"
                value: "true"
            }
            PluginParameter {
                name: "osm.useragent"
                value: root.userAgent
            }
        }
    }

    MouseArea {
        id: pickArea
        anchors.fill: parent
        enabled: root.pickEnabled && !root.interactive

        property real startX
        property real startY
        property bool scrubbing: false

        function report(x, y) {
            var coordinate = map.toCoordinate(Qt.point(x, y))
            root.picked(coordinate.latitude, coordinate.longitude)
        }

        onPressed: {
            startX = mouse.x
            startY = mouse.y
            scrubbing = false
        }
        // Sideways scrubs; up and down still scrolls a page around the map.
        onPositionChanged: {
            if (!scrubbing && Math.abs(mouse.x - startX) > Theme.startDragDistance / 2
                    && Math.abs(mouse.x - startX) > Math.abs(mouse.y - startY)) {
                scrubbing = true
                preventStealing = true
            }
            if (scrubbing)
                report(mouse.x, mouse.y)
        }
        onReleased: preventStealing = false
        onCanceled: preventStealing = false
        onClicked: report(mouse.x, mouse.y)
    }

    Column {
        anchors {
            right: parent.right
            rightMargin: Theme.paddingLarge
            verticalCenter: parent.verticalCenter
        }
        spacing: Theme.paddingLarge
        visible: root.showZoomButtons

        IconButton {
            icon.source: "image://theme/icon-m-add"
            onClicked: map.zoomLevel = Math.min(map.maximumZoomLevel, map.zoomLevel + 1)

            Rectangle {
                anchors.fill: parent
                z: -1
                radius: width / 2
                color: Theme.rgba(Theme.overlayBackgroundColor || "black", 0.6)
            }
        }

        IconButton {
            icon.source: "image://theme/icon-m-remove"
            onClicked: map.zoomLevel = Math.max(map.minimumZoomLevel, map.zoomLevel - 1)

            Rectangle {
                anchors.fill: parent
                z: -1
                radius: width / 2
                color: Theme.rgba(Theme.overlayBackgroundColor || "black", 0.6)
            }
        }
    }
}
