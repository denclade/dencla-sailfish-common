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
    void purgeUnreferencedNeverDeletesInProgressTmpFiles();
    void clearRemovesEveryFile();
    void totalSizeBytesSumsAllFiles();
    void keysWithPathSeparatorsAreSanitized();
    void beginWriteThenCommitWriteMakesKeyCached();
    void beginWriteSupportsMultipleWriteCalls();
    void commitWriteLeavesNoTmpFileBehind();
    void discardWriteLeavesKeyUncachedAndNoTmpFile();
    void beginWriteReturnsNullptrWhenDirCannotBeCreated();

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

void TestDiskCache::purgeUnreferencedNeverDeletesInProgressTmpFiles() {
    // Regression test: a real sync would call this (via garbageCollect(), possibly from
    // another thread) while a DownloadQueue elsewhere still has an unrelated download
    // in-flight - keepPaths reflects only what's fully committed/referenced right now, so
    // without the ".tmp" exclusion this would delete that download's ".tmp" file out from
    // under it, and its eventual commitWrite() rename would then fail (confirmed as the real
    // cause behind a "[MediaCacheManager] Download failed ... failed to write to cache" report
    // seen right after a sync's own post-sync garbage collection pass).
    DiskCache cache(QDir(m_dir->path()));
    QFile *file = cache.beginWrite(QStringLiteral("in-progress.png"));
    QVERIFY(file != nullptr);
    file->write("partial data");
    file->flush();

    cache.purgeUnreferenced(QSet<QString>()); // nothing referenced - worst case for the sweep

    QVERIFY(QFileInfo::exists(cache.filePathFor(QStringLiteral("in-progress.png")) + QStringLiteral(".tmp")));
    delete file;
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

void TestDiskCache::beginWriteThenCommitWriteMakesKeyCached() {
    DiskCache cache(QDir(m_dir->path()));
    QFile *file = cache.beginWrite(QStringLiteral("a.bin"));
    QVERIFY(file != nullptr);
    file->write(QByteArrayLiteral("streamed bytes"));
    const QString path = cache.commitWrite(QStringLiteral("a.bin"), file);
    delete file;

    QVERIFY(!path.isEmpty());
    QVERIFY(cache.isCached(QStringLiteral("a.bin")));
    QFile readBack(path);
    QVERIFY(readBack.open(QIODevice::ReadOnly));
    QCOMPARE(readBack.readAll(), QByteArrayLiteral("streamed bytes"));
}

void TestDiskCache::beginWriteSupportsMultipleWriteCalls() {
    DiskCache cache(QDir(m_dir->path()));
    QFile *file = cache.beginWrite(QStringLiteral("a.bin"));
    QVERIFY(file != nullptr);
    file->write(QByteArrayLiteral("first-"));
    file->write(QByteArrayLiteral("second"));
    const QString path = cache.commitWrite(QStringLiteral("a.bin"), file);
    delete file;

    QFile readBack(path);
    QVERIFY(readBack.open(QIODevice::ReadOnly));
    QCOMPARE(readBack.readAll(), QByteArrayLiteral("first-second"));
}

void TestDiskCache::commitWriteLeavesNoTmpFileBehind() {
    DiskCache cache(QDir(m_dir->path()));
    QFile *file = cache.beginWrite(QStringLiteral("a.bin"));
    file->write(QByteArrayLiteral("x"));
    cache.commitWrite(QStringLiteral("a.bin"), file);
    delete file;

    QVERIFY(!QFileInfo::exists(cache.filePathFor(QStringLiteral("a.bin")) + QStringLiteral(".tmp")));
}

void TestDiskCache::discardWriteLeavesKeyUncachedAndNoTmpFile() {
    DiskCache cache(QDir(m_dir->path()));
    QFile *file = cache.beginWrite(QStringLiteral("a.bin"));
    file->write(QByteArrayLiteral("partial, network dropped"));
    cache.discardWrite(QStringLiteral("a.bin"), file);
    delete file;

    QVERIFY(!cache.isCached(QStringLiteral("a.bin")));
    QVERIFY(!QFileInfo::exists(cache.filePathFor(QStringLiteral("a.bin")) + QStringLiteral(".tmp")));
}

void TestDiskCache::beginWriteReturnsNullptrWhenDirCannotBeCreated() {
    // A regular file where a directory needs to go - mkpath() can't create anything under it,
    // so beginWrite() must fail to open the tmp file rather than crash or silently succeed.
    QFile blocker(m_dir->path() + QStringLiteral("/blocker"));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    DiskCache cache(QDir(m_dir->path() + QStringLiteral("/blocker/subdir")));
    QFile *file = cache.beginWrite(QStringLiteral("a.bin"));
    QVERIFY(file == nullptr);
}

QTEST_APPLESS_MAIN(TestDiskCache)
#include "test_diskcache.moc"
