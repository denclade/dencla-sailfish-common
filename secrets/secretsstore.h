// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DENCLA_COMMON_SECRETSSTORE_H
#define DENCLA_COMMON_SECRETSSTORE_H

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>
#include <Secrets/secretmanager.h>

// Thin async wrapper around libsailfishsecrets, generalized from harbour-sailreader's and
// harbour-audiobookfish's own near-identical SecretsManager classes - a keyed collection of
// secrets rather than either app's own choice of "one hardcoded key" vs. "caller-supplied key
// per account". An app that only ever has one secret just uses a fixed constant as its key.
//
// Fixed policy for every collection this class creates (matches both source apps, which
// independently converged on the same settings): DeviceLock (ties into the phone's own lock
// code, no separate app-specific passphrase to explain to the user - see
// harbour-audiobookfish's original SecretsManager class comment for the fuller DeviceLock-vs-
// CustomLock reasoning and the Sailjail-ApplicationName pitfall that looks like a DeviceLock
// failure but isn't), OwnerOnlyMode, DeviceLockKeepUnlocked, and the default encrypted storage
// plugin for both storage and encryption.
//
// Requests are SERIALISED: one goes to the daemon at a time, the rest wait their turn. The
// daemon serves one interactive request at a time, and a second one arriving while the first
// is still waiting for the user's authorisation prompt gets a second prompt - an empty, broken
// one - after which both fail. Confirmed on device: an app whose constructors asked for four
// secrets at once got exactly that, while apps asking for a single secret never saw it. This
// costs nothing in the normal case, because once the first request is authorised the
// collection stays unlocked and everything queued behind it goes through without prompting.
//
// storeSecret() is idempotent in the face of a stale secret already sitting under the same
// identifier (e.g. a schema/database reset that starts a fresh account row - and therefore a
// fresh generated key - while the old secret is still sitting in the vault under the old key):
// StoreSecretRequest has no upsert semantics, so a SecretAlreadyExistsError (or, on some
// daemon/storage-plugin versions, a raw "UNIQUE constraint failed" error message instead of
// that specific error code - both are checked for) triggers a delete-then-retry instead of
// surfacing as a hard failure.
class SecretsStore : public QObject {
    Q_OBJECT
public:
    // collectionName must be unique per app, and should be re-versioned (renamed) if this
    // collection's lock type/settings ever need to change - ensureCollection() treats an
    // already-existing collection as success regardless of what it was actually created with,
    // so reusing a name from an earlier, abandoned setup would silently keep using that
    // instead of a fresh one (see both source apps' original class comments - each has its own
    // history of exactly this happening during development).
    explicit SecretsStore(const QString &collectionName, QObject *parent = nullptr);

    Q_INVOKABLE void storeSecret(const QString &key, const QString &value);
    Q_INVOKABLE void retrieveSecret(const QString &key);
    // Named to match DiskCache::remove() (deleteSecret/secretDeleted would be an equally
    // reasonable choice - "remove" was picked so the two keyed-store classes in this library
    // use the same verb for "get rid of this stored item" rather than each mirroring its own
    // underlying platform API's wording, which is what caused the mismatch to begin with).
    Q_INVOKABLE void removeSecret(const QString &key);

signals:
    void secretStored(const QString &key, bool ok, const QString &error);
    // locked is true specifically for CollectionIsLockedError/SecretsDaemonLockedError/
    // SecretsPluginIsLockedError - the device hasn't been unlocked with its lock code since
    // boot yet, not "something is actually wrong with this secret". Callers should treat this
    // as "retry later" (e.g. on the next user-initiated action), not as a reason to discard
    // the secret or ask the user to re-enter it - see harbour-sailreader's
    // SyncEngine::needsDeviceUnlock for the pattern this distinction exists to support.
    void secretRetrieved(const QString &key, const QString &value, bool ok, bool locked, const QString &error);
    void secretRemoved(const QString &key, bool ok, const QString &error);

private:
    void ensureCollection(const std::function<void(bool ok, const QString &error)> &onReady);
    void doStoreSecret(const QString &key, const QString &value, bool isRetryAfterDelete, qint64 generation);
    void deleteExistingThenRetryStore(const QString &key, const QString &value, qint64 generation);

    // See the class comment. An "operation" is one public call from start to the signal that
    // answers it - a store that has to delete a stale secret first is still one operation, so
    // nothing else can slip in between its two halves.
    void enqueue(const std::function<void()> &operation);
    void runNextOperation();
    // Called by an operation's terminal path, with the generation it was started under, so a
    // request that answers after its watchdog already gave up on it cannot advance the queue a
    // second time and start two requests at once - the exact situation this queue prevents.
    void operationFinished(qint64 generation);

    Sailfish::Secrets::SecretManager m_secretManager;
    QString m_collectionName;
    bool m_collectionEnsured = false;

    QVector<std::function<void()>> m_queue;
    bool m_operationInFlight = false;
    qint64 m_operationGeneration = 0;
};

#endif // DENCLA_COMMON_SECRETSSTORE_H
