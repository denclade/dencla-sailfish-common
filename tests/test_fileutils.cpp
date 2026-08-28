#include <QtTest>
#include <QStandardPaths>
#include "../util/fileutils.h"

class TestFileUtils : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void getCacheDirCreatesTheSubdirectory();
    void getTempDirCreatesTheSubdirectory();
    void getCacheDirIsIdempotentForTheSameSubdirName();
    void getCacheDirAndGetTempDirUseDifferentLocations();
};

void TestFileUtils::initTestCase() {
    // Redirects QStandardPaths::CacheLocation/TempLocation to an isolated per-run location
    // instead of the real user directories, so this test never touches real data.
    QStandardPaths::setTestModeEnabled(true);
}

void TestFileUtils::getCacheDirCreatesTheSubdirectory() {
    const QDir dir = FileUtils::getCacheDir(QStringLiteral("dencla-common-test-cache"));
    QVERIFY(dir.exists());
}

void TestFileUtils::getTempDirCreatesTheSubdirectory() {
    const QDir dir = FileUtils::getTempDir(QStringLiteral("dencla-common-test-temp"));
    QVERIFY(dir.exists());
}

void TestFileUtils::getCacheDirIsIdempotentForTheSameSubdirName() {
    const QDir first = FileUtils::getCacheDir(QStringLiteral("dencla-common-test-idempotent"));
    const QDir second = FileUtils::getCacheDir(QStringLiteral("dencla-common-test-idempotent"));
    QCOMPARE(first.absolutePath(), second.absolutePath());
    QVERIFY(second.exists());
}

void TestFileUtils::getCacheDirAndGetTempDirUseDifferentLocations() {
    const QDir cacheDir = FileUtils::getCacheDir(QStringLiteral("dencla-common-test-same-name"));
    const QDir tempDir = FileUtils::getTempDir(QStringLiteral("dencla-common-test-same-name"));
    QVERIFY(cacheDir.absolutePath() != tempDir.absolutePath());
}

QTEST_APPLESS_MAIN(TestFileUtils)
#include "test_fileutils.moc"
