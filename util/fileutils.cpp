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
    return ensureSubdir(QStandardPaths::TempLocation, subdirName);
}

QDir getCacheDir(const QString &subdirName) {
    return ensureSubdir(QStandardPaths::CacheLocation, subdirName);
}

}
