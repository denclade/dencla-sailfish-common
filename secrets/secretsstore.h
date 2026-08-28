#ifndef DENCLA_COMMON_SECRETSSTORE_H
#define DENCLA_COMMON_SECRETSSTORE_H

#include <QObject>
#include <QString>
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
    Q_INVOKABLE void deleteSecret(const QString &key);

signals:
    void secretStored(const QString &key, bool ok, const QString &error);
    // locked is true specifically for CollectionIsLockedError/SecretsDaemonLockedError/
    // SecretsPluginIsLockedError - the device hasn't been unlocked with its lock code since
    // boot yet, not "something is actually wrong with this secret". Callers should treat this
    // as "retry later" (e.g. on the next user-initiated action), not as a reason to discard
    // the secret or ask the user to re-enter it - see harbour-sailreader's
    // SyncEngine::needsDeviceUnlock for the pattern this distinction exists to support.
    void secretRetrieved(const QString &key, const QString &value, bool ok, bool locked, const QString &error);
    void secretDeleted(const QString &key, bool ok, const QString &error);

private:
    void ensureCollection(const std::function<void(bool ok, const QString &error)> &onReady);
    void doStoreSecret(const QString &key, const QString &value, bool isRetryAfterDelete);
    void deleteExistingThenRetryStore(const QString &key, const QString &value);

    Sailfish::Secrets::SecretManager m_secretManager;
    QString m_collectionName;
    bool m_collectionEnsured = false;
};

#endif // DENCLA_COMMON_SECRETSSTORE_H
