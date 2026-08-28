#include "diskcache.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

DiskCache::DiskCache(const QDir &cacheDir) : m_cacheDir(cacheDir) {
}

QString DiskCache::sanitizedFileName(const QString &key) const {
    // Defensive only - every current caller already passes either a hash (keyForUrl()) or a
    // server-controlled filename with no path separators in it, but a cache class taking
    // arbitrary caller-supplied strings as filenames shouldn't trust that blindly forever.
    QString safe = key;
    safe.replace(QLatin1Char('/'), QLatin1Char('_'));
    safe.replace(QLatin1Char('\\'), QLatin1Char('_'));
    return safe;
}

QString DiskCache::filePathFor(const QString &key) const {
    return m_cacheDir.absoluteFilePath(sanitizedFileName(key));
}

QString DiskCache::fileUrlFor(const QString &key) const {
    return QUrl::fromLocalFile(filePathFor(key)).toString();
}

bool DiskCache::isCached(const QString &key) const {
    const QFileInfo info(filePathFor(key));
    return info.exists() && info.isFile() && info.size() > 0;
}

QString DiskCache::store(const QString &key, const QByteArray &data) {
    QFile *file = beginWrite(key);
    if (!file) return QString();

    if (file->write(data) != data.size()) {
        discardWrite(key, file);
        delete file;
        return QString();
    }

    const QString path = commitWrite(key, file);
    delete file;
    return path;
}

QFile *DiskCache::beginWrite(const QString &key) {
    m_cacheDir.mkpath(QStringLiteral("."));
    QFile *file = new QFile(filePathFor(key) + QStringLiteral(".tmp"));
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete file;
        return nullptr;
    }
    return file;
}

QString DiskCache::commitWrite(const QString &key, QFile *file) {
    file->close();
    const QString finalPath = filePathFor(key);
    const QString tmpPath = finalPath + QStringLiteral(".tmp");

    QFile::remove(finalPath);
    if (!QFile::rename(tmpPath, finalPath)) {
        QFile::remove(tmpPath);
        return QString();
    }
    return finalPath;
}

void DiskCache::discardWrite(const QString &key, QFile *file) {
    file->close();
    QFile::remove(filePathFor(key) + QStringLiteral(".tmp"));
}

void DiskCache::remove(const QString &key) {
    QFile::remove(filePathFor(key));
}

void DiskCache::removeByFileUrl(const QString &fileUrl) {
    QFile::remove(QUrl(fileUrl).toLocalFile());
}

void DiskCache::purgeIf(const std::function<bool(const QFileInfo &)> &predicate) {
    if (!m_cacheDir.exists()) return;
    for (const QFileInfo &info : m_cacheDir.entryInfoList(QDir::Files)) {
        if (predicate(info)) {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

void DiskCache::purgeUnreferenced(const QSet<QString> &keepPaths) {
    purgeIf([&keepPaths](const QFileInfo &info) {
        // A ".tmp" file is always some other in-progress beginWrite() - never a candidate for
        // "unreferenced" eviction, no matter what keepPaths says. Its own final path (once
        // committed) is what could ever legitimately appear in keepPaths; the ".tmp" itself
        // never will, so without this check an unrelated download still streaming in when a
        // caller (e.g. a post-sync garbage-collection pass, possibly running on another thread
        // via QtConcurrent while the download itself runs on the main thread) happens to call
        // this would have its ".tmp" deleted out from under it - the write looks like it
        // "failed to write to cache" when commitWrite()'s rename can no longer find its source
        // file, even though nothing was actually wrong with the download itself.
        if (info.fileName().endsWith(QStringLiteral(".tmp"))) return false;
        return !keepPaths.contains(info.absoluteFilePath());
    });
}

void DiskCache::clear() {
    if (!m_cacheDir.exists()) return;
    for (const QString &fileName : m_cacheDir.entryList(QDir::Files)) {
        m_cacheDir.remove(fileName);
    }
}

qint64 DiskCache::totalSizeBytes() const {
    if (!m_cacheDir.exists()) return 0;
    qint64 total = 0;
    for (const QFileInfo &info : m_cacheDir.entryInfoList(QDir::Files)) total += info.size();
    return total;
}

QString DiskCache::keyForUrl(const QString &url, const QString &fallbackExtension) {
    const QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString suffix = QFileInfo(QUrl(url).path()).suffix();
    const QString extension = suffix.isEmpty() ? fallbackExtension : suffix;
    return QString::fromLatin1(hash) + QLatin1Char('.') + extension;
}
