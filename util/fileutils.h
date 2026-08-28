#ifndef DENCLA_COMMON_FILEUTILS_H
#define DENCLA_COMMON_FILEUTILS_H

#include <QString>
#include <QDir>

// QStandardPaths-backed "give me an app-private subdirectory, creating it if needed" helper -
// lifted from harbour-wetterstein's own FileUtils, which every app that needs a temp/cache
// subdirectory (radar frames, station JSON, article images, ...) was going to reimplement
// identically otherwise.
namespace FileUtils {

// Subdirectory under the platform temp location - not guaranteed to survive app restarts.
QDir getTempDir(const QString &subdirName);

// Subdirectory under the platform cache location - should survive app restarts, but (like any
// cache location) isn't guaranteed to.
QDir getCacheDir(const QString &subdirName);

}

#endif // DENCLA_COMMON_FILEUTILS_H
