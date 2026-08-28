#include <QtTest>
#include <QTemporaryDir>
#include "../cache/diskcache.h"

class TestDiskCache : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void isCachedFalseForMissingKey();
    void storeThenIsCachedReportsTrue();
    void filePathForMatchesWhereStoreWrote();
    void storeLeavesNoTmpFileBehind();
    void storeOverwritesExistingKey();
    void fileUrlForRoundTripsThroughQUrl();
    void removeDeletesTheFile();
    void removeIsNoopForMissingKey();
    void removeByFileUrlDeletesTheFile();
    void keyForUrlIsDeterministicForTheSameUrl();
    void keyForUrlDiffersForDifferentUrls();
    void keyForUrlKeepsTheUrlsOwnExtension();
    void keyForUrlFallsBackWhenUrlHasNoExtension();
    void keyForUrlStripsQueryStringButKeepsExtension();
    void purgeIfDeletesOnlyMatchingFiles();
    void purgeUnreferencedKeepsOnlyListedPaths();
    void clearRemovesEveryFile();
    void totalSizeBytesSumsAllFiles();
    void keysWithPathSeparatorsAreSanitized();

private:
    QTemporaryDir *m_dir = nullptr;
};

void TestDiskCache::init() {
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
}

void TestDiskCache::cleanup() {
    delete m_dir;
    m_dir = nullptr;
}

void TestDiskCache::isCachedFalseForMissingKey() {
    DiskCache cache(QDir(m_dir->path()));
    QVERIFY(!cache.isCached(QStringLiteral("missing")));
}

void TestDiskCache::storeThenIsCachedReportsTrue() {
    DiskCache cache(QDir(m_dir->path()));
    QVERIFY(!cache.store(QStringLiteral("a.png"), QByteArrayLiteral("some bytes")).isEmpty());
    QVERIFY(cache.isCached(QStringLiteral("a.png")));
}

void TestDiskCache::filePathForMatchesWhereStoreWrote() {
    DiskCache cache(QDir(m_dir->path()));
    const QString written = cache.store(QStringLiteral("a.png"), QByteArrayLiteral("bytes"));
    QCOMPARE(written, cache.filePathFor(QStringLiteral("a.png")));
    QVERIFY(QFileInfo::exists(cache.filePathFor(QStringLiteral("a.png"))));
}

void TestDiskCache::storeLeavesNoTmpFileBehind() {
    DiskCache cache(QDir(m_dir->path()));
    cache.store(QStringLiteral("a.png"), QByteArrayLiteral("bytes"));
    QVERIFY(!QFileInfo::exists(cache.filePathFor(QStringLiteral("a.png")) + QStringLiteral(".tmp")));
}

void TestDiskCache::storeOverwritesExistingKey() {
    DiskCache cache(QDir(m_dir->path()));
    cache.store(QStringLiteral("a.png"), QByteArrayLiteral("first"));
    cache.store(QStringLiteral("a.png"), QByteArrayLiteral("second, longer content"));

    QFile file(cache.filePathFor(QStringLiteral("a.png")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArrayLiteral("second, longer content"));
}

void TestDiskCache::fileUrlForRoundTripsThroughQUrl() {
    DiskCache cache(QDir(m_dir->path()));
    const QString path = cache.filePathFor(QStringLiteral("a.png"));
    QCOMPARE(QUrl(cache.fileUrlFor(QStringLiteral("a.png"))).toLocalFile(), path);
}

void TestDiskCache::removeDeletesTheFile() {
    DiskCache cache(QDir(m_dir->path()));
    cache.store(QStringLiteral("a.png"), QByteArrayLiteral("bytes"));
    QVERIFY(cache.isCached(QStringLiteral("a.png")));

    cache.remove(QStringLiteral("a.png"));
    QVERIFY(!cache.isCached(QStringLiteral("a.png")));
}

void TestDiskCache::removeIsNoopForMissingKey() {
    DiskCache cache(QDir(m_dir->path()));
    cache.remove(QStringLiteral("never-existed.png")); // must not crash/throw
}

void TestDiskCache::removeByFileUrlDeletesTheFile() {
    DiskCache cache(QDir(m_dir->path()));
    cache.store(QStringLiteral("a.png"), QByteArrayLiteral("bytes"));
    const QString url = cache.fileUrlFor(QStringLiteral("a.png"));

    cache.removeByFileUrl(url);
    QVERIFY(!cache.isCached(QStringLiteral("a.png")));
}

void TestDiskCache::keyForUrlIsDeterministicForTheSameUrl() {
    const QString url = QStringLiteral("https://example.com/photo.jpg");
    QCOMPARE(DiskCache::keyForUrl(url), DiskCache::keyForUrl(url));
}

void TestDiskCache::keyForUrlDiffersForDifferentUrls() {
    QVERIFY(DiskCache::keyForUrl(QStringLiteral("https://example.com/a.jpg"))
        != DiskCache::keyForUrl(QStringLiteral("https://example.com/b.jpg")));
}

void TestDiskCache::keyForUrlKeepsTheUrlsOwnExtension() {
    QVERIFY(DiskCache::keyForUrl(QStringLiteral("https://example.com/a.gif")).endsWith(QStringLiteral(".gif")));
}

void TestDiskCache::keyForUrlFallsBackWhenUrlHasNoExtension() {
    QVERIFY(DiskCache::keyForUrl(QStringLiteral("https://example.com/image"))
        .endsWith(QStringLiteral(".img")));
    QVERIFY(DiskCache::keyForUrl(QStringLiteral("https://example.com/image"), QStringLiteral("bin"))
        .endsWith(QStringLiteral(".bin")));
}

void TestDiskCache::keyForUrlStripsQueryStringButKeepsExtension() {
    QVERIFY(DiskCache::keyForUrl(QStringLiteral("https://example.com/a.gif?v=2")).endsWith(QStringLiteral(".gif")));
}

void TestDiskCache::purgeIfDeletesOnlyMatchingFiles() {
    DiskCache cache(QDir(m_dir->path()));
    cache.store(QStringLiteral("keep.png"), QByteArrayLiteral("x"));
    cache.store(QStringLiteral("drop.png"), QByteArrayLiteral("x"));

    cache.purgeIf([](const QFileInfo &info) {
        return info.fileName() == QStringLiteral("drop.png");
    });

    QVERIFY(cache.isCached(QStringLiteral("keep.png")));
    QVERIFY(!cache.isCached(QStringLiteral("drop.png")));
}

void TestDiskCache::purgeUnreferencedKeepsOnlyListedPaths() {
    DiskCache cache(QDir(m_dir->path()));
    cache.store(QStringLiteral("keep.png"), QByteArrayLiteral("x"));
    cache.store(QStringLiteral("drop.png"), QByteArrayLiteral("x"));

    cache.purgeUnreferenced(QSet<QString>{ cache.filePathFor(QStringLiteral("keep.png")) });

    QVERIFY(cache.isCached(QStringLiteral("keep.png")));
    QVERIFY(!cache.isCached(QStringLiteral("drop.png")));
}

void TestDiskCache::clearRemovesEveryFile() {
    DiskCache cache(QDir(m_dir->path()));
    cache.store(QStringLiteral("a.png"), QByteArrayLiteral("x"));
    cache.store(QStringLiteral("b.png"), QByteArrayLiteral("x"));

    cache.clear();

    QVERIFY(!cache.isCached(QStringLiteral("a.png")));
    QVERIFY(!cache.isCached(QStringLiteral("b.png")));
}

void TestDiskCache::totalSizeBytesSumsAllFiles() {
    DiskCache cache(QDir(m_dir->path()));
    cache.store(QStringLiteral("a.png"), QByteArray(10, 'x'));
    cache.store(QStringLiteral("b.png"), QByteArray(5, 'x'));

    QCOMPARE(cache.totalSizeBytes(), qint64(15));
}

void TestDiskCache::keysWithPathSeparatorsAreSanitized() {
    DiskCache cache(QDir(m_dir->path()));
    // A key containing "/" must never be interpreted as a subdirectory/traversal - it should
    // land as a single sanitized file directly inside the cache dir, not escape it.
    const QString path = cache.store(QStringLiteral("../../etc/passwd"), QByteArrayLiteral("x"));
    QVERIFY(!path.isEmpty());
    QCOMPARE(QFileInfo(path).absolutePath(), QDir(m_dir->path()).absolutePath());
}

QTEST_APPLESS_MAIN(TestDiskCache)
#include "test_diskcache.moc"
