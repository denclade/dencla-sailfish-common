// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vaultcrypto.h"

#include <cstdlib>

#include <QFile>

extern "C" {
#include "vendor/blake3/blake3.h"
#include "vendor/monocypher/monocypher.h"
}

namespace VaultCrypto {

const char *const AuthSecretContext = "nobo sync auth-secret v1";

namespace {

const quint8 *bytes(const QByteArray &data)
{
    return reinterpret_cast<const quint8 *>(data.constData());
}

quint8 *bytes(QByteArray &data)
{
    return reinterpret_cast<quint8 *>(data.data());
}

// The kernel's random source; the app is sandboxed but /dev/urandom is
// always there. Fails loudly rather than sealing with a weak nonce.
QByteArray randomBytes(int size)
{
    QByteArray out(size, '\0');
    QFile random(QStringLiteral("/dev/urandom"));
    if (!random.open(QIODevice::ReadOnly) || random.read(out.data(), size) != size) {
        qFatal("[VaultCrypto] no random source");
    }
    return out;
}

} // namespace

QByteArray blake3DeriveKey(const char *context, const QByteArray &material)
{
    blake3_hasher hasher;
    blake3_hasher_init_derive_key(&hasher, context);
    blake3_hasher_update(&hasher, material.constData(), static_cast<size_t>(material.size()));
    QByteArray out(KeySize, '\0');
    blake3_hasher_finalize(&hasher, bytes(out), KeySize);
    return out;
}

QByteArray blake3Keyed(const QByteArray &key, const QByteArray &data)
{
    Q_ASSERT(key.size() == KeySize);
    blake3_hasher hasher;
    blake3_hasher_init_keyed(&hasher, bytes(key));
    blake3_hasher_update(&hasher, data.constData(), static_cast<size_t>(data.size()));
    QByteArray out(KeySize, '\0');
    blake3_hasher_finalize(&hasher, bytes(out), KeySize);
    return out;
}

QByteArray hash(const QByteArray &data)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.constData(), static_cast<size_t>(data.size()));
    QByteArray out(KeySize, '\0');
    blake3_hasher_finalize(&hasher, bytes(out), KeySize);
    return out;
}

QByteArray argon2id(const QByteArray &password, const QByteArray &salt, int memoryKiB, int passes, int lanes,
                    const QByteArray &secret, const QByteArray &associatedData)
{
    crypto_argon2_config config;
    config.algorithm = CRYPTO_ARGON2_ID;
    config.nb_blocks = static_cast<uint32_t>(memoryKiB);
    config.nb_passes = static_cast<uint32_t>(passes);
    config.nb_lanes = static_cast<uint32_t>(lanes);
    crypto_argon2_inputs inputs;
    inputs.pass = bytes(password);
    inputs.pass_size = static_cast<uint32_t>(password.size());
    inputs.salt = bytes(salt);
    inputs.salt_size = static_cast<uint32_t>(salt.size());
    crypto_argon2_extras extras;
    extras.key = secret.isEmpty() ? nullptr : bytes(secret);
    extras.key_size = static_cast<uint32_t>(secret.size());
    extras.ad = associatedData.isEmpty() ? nullptr : bytes(associatedData);
    extras.ad_size = static_cast<uint32_t>(associatedData.size());

    // 1 KiB per block.
    void *workArea = std::malloc(static_cast<size_t>(memoryKiB) * 1024);
    if (!workArea) {
        return QByteArray();
    }
    QByteArray out(KeySize, '\0');
    crypto_argon2(bytes(out), KeySize, workArea, config, inputs, extras);
    crypto_wipe(workArea, static_cast<size_t>(memoryKiB) * 1024);
    std::free(workArea);
    return out;
}

QByteArray deriveSalt(const QString &identity)
{
    return hash(identity.trimmed().toLower().toUtf8()).left(SaltSize);
}

QByteArray deriveMasterKey(const QString &password, const QByteArray &salt)
{
    return argon2id(password.toUtf8(), salt, 19 * 1024, 2, 1);
}

QString authSecret(const QByteArray &masterKey)
{
    return QString::fromLatin1(blake3DeriveKey(AuthSecretContext, masterKey).toHex());
}

QByteArray subKey(const QByteArray &masterKey, const char *context)
{
    return blake3DeriveKey(context, masterKey);
}

QByteArray seal(const QByteArray &key, const QByteArray &plaintext, const QByteArray &associatedData)
{
    Q_ASSERT(key.size() == KeySize);
    const QByteArray nonce = randomBytes(NonceSize);
    QByteArray out = nonce;
    out.resize(NonceSize + plaintext.size() + MacSize);
    quint8 *cipher = bytes(out) + NonceSize;
    quint8 *mac = cipher + plaintext.size();
    crypto_aead_lock(cipher, mac, bytes(key), bytes(nonce), bytes(associatedData),
                     static_cast<size_t>(associatedData.size()), bytes(plaintext),
                     static_cast<size_t>(plaintext.size()));
    return out;
}

bool open(const QByteArray &key, const QByteArray &sealed, QByteArray *plaintext, const QByteArray &associatedData)
{
    if (key.size() != KeySize || sealed.size() < NonceSize + MacSize) {
        return false;
    }
    const int size = sealed.size() - NonceSize - MacSize;
    QByteArray out(size, '\0');
    const quint8 *nonce = bytes(sealed);
    const quint8 *cipher = nonce + NonceSize;
    const quint8 *mac = cipher + size;
    if (crypto_aead_unlock(bytes(out), mac, bytes(key), nonce, bytes(associatedData),
                           static_cast<size_t>(associatedData.size()), cipher, static_cast<size_t>(size)) != 0) {
        return false;
    }
    *plaintext = out;
    return true;
}

QString opaqueId(const QByteArray &key, const QString &name)
{
    return QString::fromLatin1(blake3Keyed(key, name.toUtf8()).toHex());
}

} // namespace VaultCrypto
