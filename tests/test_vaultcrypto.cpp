#include <QtTest>

#include "crypto/vaultcrypto.h"

// The primitives against their published test vectors, and the scheme on
// top -- a wrong derivation here still "works" on one device and fails
// only when a second device (or nobo) tries the same account.
class TestVaultCrypto : public QObject {
    Q_OBJECT

    // The BLAKE3 test vectors' input: bytes 0, 1, ..., 250, 0, 1, ...
    static QByteArray vectorInput(int length)
    {
        QByteArray input(length, '\0');
        for (int i = 0; i < length; ++i) {
            input[i] = static_cast<char>(i % 251);
        }
        return input;
    }

private slots:
    void blake3MatchesTheOfficialVectors()
    {
        // BLAKE3 test_vectors.json, input lengths 0 and 3.
        QCOMPARE(VaultCrypto::hash(vectorInput(0)).toHex(),
                 QByteArray("af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"));
        QCOMPARE(VaultCrypto::hash(vectorInput(3)).toHex(),
                 QByteArray("e1be4d7a8ab5560aa4199eea339849ba8e293d55ca0a81006726d184519e647f"));
        const QByteArray key("whats the Elvish word for friend");
        QCOMPARE(VaultCrypto::blake3Keyed(key, vectorInput(0)).toHex(),
                 QByteArray("92b2b75604ed3c761f9d6f62392c8a9227ad0ea3f09573e783f1498a4ed60d26"));
        QCOMPARE(VaultCrypto::blake3Keyed(key, vectorInput(3)).toHex(),
                 QByteArray("39e67b76b5a007d4921969779fe666da67b5213b096084ab674742f0d5ec62b9"));
        const char *context = "BLAKE3 2019-12-27 16:29:52 test vectors context";
        QCOMPARE(VaultCrypto::blake3DeriveKey(context, vectorInput(0)).toHex(),
                 QByteArray("2cc39783c223154fea8dfb7c1b1660f2ac2dcbd1c1de8277b0b0dd39b7e50d7d"));
        QCOMPARE(VaultCrypto::blake3DeriveKey(context, vectorInput(3)).toHex(),
                 QByteArray("440aba35cb006b61fc17c0529255de438efc06a8c9ebf3f2ddac3b5a86705797"));
    }

    void argon2idMatchesRfc9106()
    {
        // RFC 9106, section 5.3.
        const QByteArray tag = VaultCrypto::argon2id(QByteArray(32, '\x01'), QByteArray(16, '\x02'), 32, 3, 4,
                                                     QByteArray(8, '\x03'), QByteArray(12, '\x04'));
        QCOMPARE(tag.toHex(), QByteArray("0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659"));
    }

    void saltIgnoresCaseAndSpaces()
    {
        QCOMPARE(VaultCrypto::deriveSalt(QStringLiteral(" User@Example.com ")),
                 VaultCrypto::deriveSalt(QStringLiteral("user@example.com")));
        QCOMPARE(VaultCrypto::deriveSalt(QStringLiteral("user@example.com")).size(), VaultCrypto::SaltSize);
        QVERIFY(VaultCrypto::deriveSalt(QStringLiteral("a@example.com"))
                != VaultCrypto::deriveSalt(QStringLiteral("b@example.com")));
    }

    void authSecretIsOneWayFromTheMasterKey()
    {
        const QByteArray master = VaultCrypto::deriveMasterKey(QStringLiteral("hunter2 hunter2"),
                                                               VaultCrypto::deriveSalt(QStringLiteral("me@x.org")));
        QCOMPARE(master.size(), VaultCrypto::KeySize);
        const QString secret = VaultCrypto::authSecret(master);
        QCOMPARE(secret.size(), 64);
        QVERIFY(secret != QString::fromLatin1(master.toHex()));
        QCOMPARE(VaultCrypto::authSecret(master), secret);
        QVERIFY(VaultCrypto::subKey(master, "a v1") != VaultCrypto::subKey(master, "b v1"));
    }

    void sealedDataOpensOnlyWithTheRightKeyAndUnchanged()
    {
        const QByteArray key = VaultCrypto::subKey(QByteArray(32, 'k'), "test v1");
        const QByteArray plain("heart rate at 06:45: 52");
        const QByteArray sealed = VaultCrypto::seal(key, plain, "file");
        QCOMPARE(sealed.size(), VaultCrypto::NonceSize + plain.size() + VaultCrypto::MacSize);
        QVERIFY(!sealed.contains(plain));
        // A fresh nonce every time.
        QVERIFY(VaultCrypto::seal(key, plain, "file") != sealed);

        QByteArray opened;
        QVERIFY(VaultCrypto::open(key, sealed, &opened, "file"));
        QCOMPARE(opened, plain);

        QVERIFY(!VaultCrypto::open(VaultCrypto::subKey(QByteArray(32, 'k'), "other v1"), sealed, &opened, "file"));
        QVERIFY(!VaultCrypto::open(key, sealed, &opened, "index"));
        QByteArray tampered = sealed;
        tampered[VaultCrypto::NonceSize + 2] = static_cast<char>(tampered.at(VaultCrypto::NonceSize + 2) ^ 1);
        QVERIFY(!VaultCrypto::open(key, tampered, &opened, "file"));
        QVERIFY(!VaultCrypto::open(key, QByteArray(10, 'x'), &opened));
    }

    void opaqueIdsAreStableAndKeyed()
    {
        const QByteArray a(32, 'a');
        const QByteArray b(32, 'b');
        const QString name = QStringLiteral("monitoring/20260929-161622-32-0092.fit");
        QCOMPARE(VaultCrypto::opaqueId(a, name), VaultCrypto::opaqueId(a, name));
        QVERIFY(VaultCrypto::opaqueId(a, name) != VaultCrypto::opaqueId(b, name));
        QVERIFY(!VaultCrypto::opaqueId(a, name).contains(QStringLiteral("2026")));
    }
};

QTEST_APPLESS_MAIN(TestVaultCrypto)
#include "test_vaultcrypto.moc"
