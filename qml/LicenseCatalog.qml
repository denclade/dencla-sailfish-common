// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.0
import "LegalFiles.js" as LegalFiles

QtObject {
    id: catalog
    property bool active: true
    property bool loading: active
    property bool loadFailed: false
    property var index: null
    property int generation: 0
    property var pendingRequest: null

    function reload() {
        var token = ++generation
        if (pendingRequest) pendingRequest.abort()
        index = null
        loading = true
        loadFailed = false
        var request = new XMLHttpRequest()
        pendingRequest = request
        request.onreadystatechange = function () {
            if (token !== generation || request.readyState !== XMLHttpRequest.DONE) return
            try {
                if (request.status !== 200 && request.status !== 0) throw new Error("Read failed")
                catalog.index = LegalFiles.parseIndex(request.responseText)
            } catch (error) {
                catalog.loadFailed = true
            }
            catalog.pendingRequest = null
            catalog.loading = false
        }
        try {
            request.open("GET", Qt.resolvedUrl("../../licenses/index.json"), true)
            request.send()
        } catch (error) {
            pendingRequest = null
            loadFailed = true
            loading = false
        }
    }

    onActiveChanged: { if (active) reload() }
    Component.onCompleted: { if (active) reload() }
    Component.onDestruction: {
        ++generation
        if (pendingRequest) pendingRequest.abort()
    }
}
