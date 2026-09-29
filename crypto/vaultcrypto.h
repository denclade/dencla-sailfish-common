#ifndef DENCLA_COMMON_VAULTCRYPTO_H
#define DENCLA_COMMON_VAULTCRYPTO_H

#include <QByteArray>
#include <QString>

// End-to-end encryption for syncing an app's data through a server that
// must not be able to read it -- the key scheme of harbour-nobo's vault
// sync (nobo-sync/src/crypto.rs), so one account works for every app:
//
//   salt        = BLAKE3(lower(trim(identity)))[0..16]      (identity: the email)
//   master key  = Argon2id(password, salt, m = 19 MiB, t = 2, p = 1)  -- 32 bytes
//   auth secret = hex(BLAKE3-derive_key("nobo sync auth-secret v1", master key))
//
// The auth secret is what the account's server password is set to; it is
// one-way from the master key, so a server operator who captures it can
// log in as the user but decrypt nothing. The password and the master key
// never leave the device.
//
// Data is sealed with XChaCha20-Poly1305 under keys derived from the
// master key per purpose (subKey), with a fresh random 24-byte nonce per
// message -- random nonces are safe at that size, so no per-file key
// wrapping is needed for small files.
//
// Built on two vendored, audited C libraries: Monocypher (Argon2id,
// XChaCha20-Poly1305) and the BLAKE3 reference implementation. Qt Core
// only; no Sailfish or network dependency.
namespace VaultCrypto {

const int KeySize = 32;
const int SaltSize = 16;
const int NonceSize = 24;
const int MacSize = 16;

// The context of nobo's auth secret -- reusing it is what makes the same
// account work across apps.
extern const char *const AuthSecretContext;

QByteArray deriveSalt(const QString &identity);
// Argon2id with OWASP's minimum profile. Takes about a second and 19 MiB
// on a phone -- run it off the UI thread. Empty on allocation failure.
QByteArray deriveMasterKey(const QString &password, const QByteArray &salt);
QString authSecret(const QByteArray &masterKey);
// A key for one purpose, e.g. "harbour-fitfish backup data v1".
QByteArray subKey(const QByteArray &masterKey, const char *context);

// nonce || ciphertext || mac. `associatedData` is authenticated, not
// encrypted (e.g. what the blob is for).
QByteArray seal(const QByteArray &key, const QByteArray &plaintext, const QByteArray &associatedData = QByteArray());
// The plaintext, or false if the key is wrong or anything was changed.
bool open(const QByteArray &key, const QByteArray &sealed, QByteArray *plaintext,
          const QByteArray &associatedData = QByteArray());

// An opaque, stable id for a name under a key: hex(BLAKE3-keyed(key, name)),
// 64 characters. The server can match it, not read it.
QString opaqueId(const QByteArray &key, const QString &name);

// Plain BLAKE3, 32 bytes -- e.g. to tell whether content changed.
QByteArray hash(const QByteArray &data);

// Primitives, for tests against the published vectors.
QByteArray blake3DeriveKey(const char *context, const QByteArray &material);
QByteArray blake3Keyed(const QByteArray &key, const QByteArray &data);
QByteArray argon2id(const QByteArray &password, const QByteArray &salt, int memoryKiB, int passes, int lanes,
                    const QByteArray &secret = QByteArray(), const QByteArray &associatedData = QByteArray());

} // namespace VaultCrypto

#endif // DENCLA_COMMON_VAULTCRYPTO_H
