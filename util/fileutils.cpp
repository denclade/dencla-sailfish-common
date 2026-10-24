// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileutils.h"

#include <QStandardPaths>

namespace {

QDir ensureSubdir(QStandardPaths::StandardLocation location, const QString &subdirName) {
    const QString path = QStandardPaths::writableLocation(location) + QLatin1Char('/') + subdirName;
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir;
}

}

namespace FileUtils {

QDir getTempDir(const QString &subdirName) {
    // Deliberately NOT QStandardPaths::TempLocation - see this function's own header comment
    // for why (RAM-backed tmpfs on this platform, confirmed directly by a real crash). Rooted
    // under CacheLocation instead (guaranteed real disk) but in its own "tmp/" subdirectory,
    // kept apart from getCacheDir()'s own subdirectories below - the *name* "temp" still means
    // exactly what it says (no persistence requirement, safe to lose at any time), it's just
    // backed by reliable storage instead of an OS-provided temp location that turned out not
    // to be one on this platform.
    return ensureSubdir(QStandardPaths::CacheLocation, QStringLiteral("tmp/") + subdirName);
}

QDir getCacheDir(const QString &subdirName) {
    return ensureSubdir(QStandardPaths::CacheLocation, subdirName);
}

}
