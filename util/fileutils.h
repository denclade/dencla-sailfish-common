// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DENCLA_COMMON_FILEUTILS_H
#define DENCLA_COMMON_FILEUTILS_H

#include <QString>
#include <QDir>

// QStandardPaths-backed "give me an app-private subdirectory, creating it if needed" helper -
// lifted from harbour-wetterstein's own FileUtils, which every app that needs a temp/cache
// subdirectory (radar frames, station JSON, article images, ...) was going to reimplement
// identically otherwise.
namespace FileUtils {

// A subdirectory for data with no persistence requirement at all - safe to lose at any time,
// including across a reboot, without the app needing to care. NOT QStandardPaths::TempLocation:
// on several Linux systems, including Sailfish OS, that location is backed by tmpfs, i.e. RAM,
// not disk - actively dangerous for anything that can grow large (an audiobook archive staged
// there crashed harbour-audiobookfish with an out-of-memory error that looked like a storage
// problem, not a RAM one, since `df -h` on the real disk still showed plenty free). Backed by
// real disk instead (see the .cpp) - the "temp" in the name describes the data's lifetime
// requirement (none), not a promise about which physical storage holds it.
QDir getTempDir(const QString &subdirName);

// Subdirectory under the platform cache location - should survive app restarts, but (like any
// cache location) isn't guaranteed to. Use this instead of getTempDir() above when the data
// benefits from surviving a restart even though it isn't required to (radar frames, article
// images, ...) - the two are backed by the same real disk location, just kept in separate
// subdirectories, so the choice between them is purely about which one better describes the
// data's intent to a reader of the code that calls it.
QDir getCacheDir(const QString &subdirName);

}

#endif // DENCLA_COMMON_FILEUTILS_H
