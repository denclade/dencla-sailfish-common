#include "secretsstore.h"

#include <Secrets/secret.h>
#include <Secrets/createcollectionrequest.h>
#include <Secrets/storesecretrequest.h>
#include <Secrets/storedsecretrequest.h>
#include <Secrets/deletesecretrequest.h>
#include <QDebug>

using namespace Sailfish::Secrets;

SecretsStore::SecretsStore(const QString &collectionName, QObject *parent)
    : QObject(parent), m_collectionName(collectionName) {
}

void SecretsStore::ensureCollection(const std::function<void(bool, const QString &)> &onReady) {
    if (m_collectionEnsured) {
        onReady(true, QString());
        return;
    }

    CreateCollectionRequest *request = new CreateCollectionRequest(this);
    request->setManager(&m_secretManager);
    request->setCollectionName(m_collectionName);
    request->setAccessControlMode(SecretManager::OwnerOnlyMode);
    request->setCollectionLockType(CreateCollectionRequest::DeviceLock);
    request->setDeviceLockUnlockSemantic(SecretManager::DeviceLockKeepUnlocked);
    request->setStoragePluginName(SecretManager::DefaultEncryptedStoragePluginName);
    request->setEncryptionPluginName(SecretManager::DefaultEncryptedStoragePluginName);

    connect(request, &CreateCollectionRequest::statusChanged, this, [this, request, onReady]() {
        if (request->status() != Request::Finished) return;
        const Result result = request->result();
        // A repeat run (e.g. app restart) hits an already-existing collection - that is
        // success, not failure, for an idempotent ensureCollection().
        const bool ok = result.code() == Result::Succeeded
            || result.errorCode() == Result::CollectionAlreadyExistsError;
        if (ok) {
            m_collectionEnsured = true;
        } else {
            qWarning() << "[SecretsStore] Failed to create collection" << m_collectionName
                       << ":" << result.errorMessage();
        }
        onReady(ok, ok ? QString() : result.errorMessage());
        request->deleteLater();
    });
    request->startRequest();
}

void SecretsStore::storeSecret(const QString &key, const QString &value) {
    ensureCollection([this, key, value](bool ok, const QString &error) {
        if (!ok) {
            emit secretStored(key, false, error);
            return;
        }
        doStoreSecret(key, value, false);
    });
}

void SecretsStore::doStoreSecret(const QString &key, const QString &value, bool isRetryAfterDelete) {
    Secret secret(Secret::Identifier(key, m_collectionName, SecretManager::DefaultEncryptedStoragePluginName));
    secret.setData(value.toUtf8());
    secret.setType(Secret::TypeBlob);

    StoreSecretRequest *request = new StoreSecretRequest(this);
    request->setManager(&m_secretManager);
    request->setSecretStorageType(StoreSecretRequest::CollectionSecret);
    request->setUserInteractionMode(SecretManager::SystemInteraction);
    request->setSecret(secret);

    connect(request, &StoreSecretRequest::statusChanged, this,
        [this, request, key, value, isRetryAfterDelete]() {
            if (request->status() != Request::Finished) return;
            const Result result = request->result();
            const bool ok = result.code() == Result::Succeeded;

            // A stale secret is already sitting under this identifier - see the class comment
            // for how that happens. Usually reported as SecretAlreadyExistsError, but some
            // daemon/storage-plugin versions instead surface a raw storage error ("Unable to
            // execute insert secret query: UNIQUE constraint failed: Secrets.CollectionName,
            // Secrets.SecretName") - checking the message text too catches that case as well
            // (confirmed directly on harbour-sailreader: relying on the error code alone let
            // this fall through to a hard failure).
            const bool staleSecretConflict = result.errorCode() == Result::SecretAlreadyExistsError
                || result.errorMessage().contains(QStringLiteral("UNIQUE constraint"), Qt::CaseInsensitive);
            if (!ok && !isRetryAfterDelete && staleSecretConflict) {
                qWarning() << "[SecretsStore] Secret" << key
                           << "already exists - deleting and retrying store";
                request->deleteLater();
                deleteExistingThenRetryStore(key, value);
                return;
            }

            if (!ok) {
                qWarning() << "[SecretsStore] Failed to store secret" << key << ":" << result.errorMessage();
            }
            emit secretStored(key, ok, ok ? QString() : result.errorMessage());
            request->deleteLater();
        });
    request->startRequest();
}

void SecretsStore::deleteExistingThenRetryStore(const QString &key, const QString &value) {
    DeleteSecretRequest *request = new DeleteSecretRequest(this);
    request->setManager(&m_secretManager);
    request->setIdentifier(Secret::Identifier(key, m_collectionName, SecretManager::DefaultEncryptedStoragePluginName));
    request->setUserInteractionMode(SecretManager::SystemInteraction);

    connect(request, &DeleteSecretRequest::statusChanged, this, [this, request, key, value]() {
        if (request->status() != Request::Finished) return;
        const Result result = request->result();
        if (result.code() != Result::Succeeded) {
            // Proceed with the retry regardless - if the delete failed because the secret was
            // somehow already gone, the retry succeeds anyway; if it failed for another
            // reason, the retry's own error is the more useful one to surface.
            qWarning() << "[SecretsStore] Failed to delete stale secret" << key << ":" << result.errorMessage();
        }
        request->deleteLater();
        doStoreSecret(key, value, true);
    });
    request->startRequest();
}

void SecretsStore::retrieveSecret(const QString &key) {
    StoredSecretRequest *request = new StoredSecretRequest(this);
    request->setManager(&m_secretManager);
    request->setIdentifier(Secret::Identifier(key, m_collectionName, SecretManager::DefaultEncryptedStoragePluginName));
    request->setUserInteractionMode(SecretManager::SystemInteraction);

    connect(request, &StoredSecretRequest::statusChanged, this, [this, request, key]() {
        if (request->status() != Request::Finished) return;
        const Result result = request->result();
        const bool ok = result.code() == Result::Succeeded;
        const QString value = ok ? QString::fromUtf8(request->secret().data()) : QString();
        // See the class comment on secretRetrieved() - these three specifically mean "not
        // unlocked since boot yet", not "something is actually wrong with this secret".
        const bool locked = result.errorCode() == Result::CollectionIsLockedError
            || result.errorCode() == Result::SecretsDaemonLockedError
            || result.errorCode() == Result::SecretsPluginIsLockedError;
        if (!ok) {
            qWarning() << "[SecretsStore] Failed to retrieve secret" << key
                       << (locked ? "(device not unlocked since boot yet)" : "")
                       << ":" << result.errorMessage();
        }
        emit secretRetrieved(key, value, ok, locked, ok ? QString() : result.errorMessage());
        request->deleteLater();
    });
    request->startRequest();
}

void SecretsStore::deleteSecret(const QString &key) {
    DeleteSecretRequest *request = new DeleteSecretRequest(this);
    request->setManager(&m_secretManager);
    request->setIdentifier(Secret::Identifier(key, m_collectionName, SecretManager::DefaultEncryptedStoragePluginName));
    request->setUserInteractionMode(SecretManager::SystemInteraction);

    connect(request, &DeleteSecretRequest::statusChanged, this, [this, request, key]() {
        if (request->status() != Request::Finished) return;
        const Result result = request->result();
        const bool ok = result.code() == Result::Succeeded;
        if (!ok) {
            qWarning() << "[SecretsStore] Failed to delete secret" << key << ":" << result.errorMessage();
        }
        emit secretDeleted(key, ok, ok ? QString() : result.errorMessage());
        request->deleteLater();
    });
    request->startRequest();
}
