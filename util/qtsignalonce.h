// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DENCLA_COMMON_QTSIGNALONCE_H
#define DENCLA_COMMON_QTSIGNALONCE_H

#include <QObject>
#include <QSharedPointer>
#include <utility>

// Connects `signal` on `sender` to `handler`, self-disconnecting the first time `handler`
// returns true. For a broadcast Qt signal shared by many concurrent callers - e.g.
// SecretsStore::secretRetrieved/secretStored (any key, every request) or
// SyncProvider::actionFinished-style signals (whichever batch happens to be in flight) - a
// call site only cares about the one response matching what it just asked for. `handler`
// decides per firing whether that's this one (return true, consumed and disconnected) or not
// (return false, stays connected for the next firing).
//
// Returns the underlying connection handle so a caller that also needs an independent way to
// cancel waiting (e.g. a timeout that should stop listening even if the signal never fires
// with a matching key - see harbour-audiobookfish's ApiClient::restoreSession()) can disconnect
// it itself via QObject::disconnect(*connection). Most callers have no such need and simply
// discard the return value.
//
// Originally a harbour-sailreader-only header; moved here once harbour-audiobookfish's own
// SecretsStore consumer needed the exact same pattern and had to hand-write the boilerplate
// this replaces - a generic, zero-dependency template belongs in the shared library, not
// duplicated per app that happens to consume a keyed broadcast signal.
template <typename Sender, typename Signal, typename Handler>
QSharedPointer<QMetaObject::Connection> connectUntilHandled(Sender *sender, Signal signal, QObject *context, Handler handler) {
    auto connection = QSharedPointer<QMetaObject::Connection>::create();
    *connection = QObject::connect(sender, signal, context,
        [connection, handler](auto &&...args) {
            if (!handler(std::forward<decltype(args)>(args)...)) return;
            QObject::disconnect(*connection);
        });
    return connection;
}

#endif // DENCLA_COMMON_QTSIGNALONCE_H
