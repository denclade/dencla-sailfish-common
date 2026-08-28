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
    const QString finalPath = filePathFor(key);
    m_cacheDir.mkpath(QStringLiteral("."));

    const QString tmpPath = finalPath + QStringLiteral(".tmp");
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate) || tmpFile.write(data) != data.size()) {
        tmpFile.close();
        QFile::remove(tmpPath);
        return QString();
    }
    tmpFile.close();

    QFile::remove(finalPath);
    if (!QFile::rename(tmpPath, finalPath)) {
        QFile::remove(tmpPath);
        return QString();
    }
    return finalPath;
}

void DiskCache::remove(const QString &key) {
    QFile::remove(filePathFor(key));
}

void DiskCache::removeByFileUrl(const QString &fileUrl) {
    QFile::remove(QUrl(fileUrl).toLocalFile());
}

void DiskCache::purgeIf(const std::function<bool(const QString &, const QDateTime &)> &predicate) {
    if (!m_cacheDir.exists()) return;
    for (const QFileInfo &info : m_cacheDir.entryInfoList(QDir::Files)) {
        if (predicate(info.fileName(), info.lastModified())) {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

void DiskCache::purgeUnreferenced(const QSet<QString> &keepPaths) {
    if (!m_cacheDir.exists()) return;
    for (const QFileInfo &info : m_cacheDir.entryInfoList(QDir::Files)) {
        if (!keepPaths.contains(info.absoluteFilePath())) {
            QFile::remove(info.absoluteFilePath());
        }
    }
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
