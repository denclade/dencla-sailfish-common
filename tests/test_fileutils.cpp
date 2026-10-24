// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

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
    void getTempDirIsNotRootedUnderTempLocation();
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

void TestFileUtils::getTempDirIsNotRootedUnderTempLocation() {
    // Regression test for the actual incident this function's own header comment describes:
    // QStandardPaths::TempLocation is RAM-backed (tmpfs) on this platform, so getTempDir()
    // must never be implemented in terms of it again, no matter how the internals get
    // refactored later - a large file staged there is how a real download once crashed an app
    // with an out-of-memory error. Asserts the actual, load-bearing property (not rooted under
    // TempLocation) rather than just re-asserting today's specific implementation choice
    // (CacheLocation) - a future switch to some other guaranteed-disk-backed location would
    // still be fine here.
    const QString tempLocationRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QDir dir = FileUtils::getTempDir(QStringLiteral("dencla-common-test-not-ram-backed"));
    QVERIFY(!dir.absolutePath().startsWith(tempLocationRoot));
}

QTEST_APPLESS_MAIN(TestFileUtils)
#include "test_fileutils.moc"
