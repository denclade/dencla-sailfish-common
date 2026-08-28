#ifndef DENCLA_COMMON_DISKCACHE_H
#define DENCLA_COMMON_DISKCACHE_H

#include <QDir>
#include <QString>
#include <QByteArray>
#include <QSet>
#include <QFileInfo>
#include <functional>

// Generic keyed blob-on-disk cache - the mechanics every app's own cache class
// (harbour-sailreader's MediaCacheManager/ImageCachePaths, harbour-wetterstein's
// RadarFrameCache) reimplemented separately: turn a key into a path, check/write/remove a
// file, walk the directory for cleanup. Eviction POLICY (when something should be purged) is
// deliberately NOT this class's job - that's app-specific (age-based per prefix for radar
// frames, "still referenced by any article" for images, size-capped, ...). purgeIf()/
// purgeUnreferenced() below give the caller two different mechanisms to express whatever
// policy it needs; this class only owns walking the directory and deleting what the caller
// says to delete.
//
// Not thread-affine - every method here is synchronous, plain QFile/QDir I/O with no signals
// and no Qt object lifetime of its own. A caller that wants this off the main thread (e.g. via
// QtConcurrent, as harbour-sailreader's MediaCacheManager::garbageCollect() does) is
// responsible for that itself; this class doesn't know or care what thread it runs on, as long
// as one instance isn't used concurrently from two threads without external synchronization.
class DiskCache {
public:
    explicit DiskCache(const QDir &cacheDir);

    // Absolute path a given key would be stored at, whether or not it currently is. Never
    // creates the cache directory - callers that need it to exist (store() below) do so
    // themselves, so a pure path-builder can't surprise a caller that's just asking a question
    // (isCached() below is commonly called from a background thread on every list-row
    // creation; silently creating directories as a side effect there would be a surprise).
    QString filePathFor(const QString &key) const;
    // Same as filePathFor(), as a "file://" URL - for handing straight to a QML Image source.
    QString fileUrlFor(const QString &key) const;

    // True iff filePathFor(key) exists on disk as a non-empty file.
    bool isCached(const QString &key) const;

    // Writes data under key and returns its path, or an empty string if the write failed.
    // Writes to a ".tmp" sibling first and renames it into place afterwards, so a concurrent
    // isCached()/filePathFor() check on another thread never observes a partially-written
    // file - it only appears at its final name once the whole write has landed on disk.
    QString store(const QString &key, const QByteArray &data);

    // Deletes the cached file for a key, if any. No-op if it doesn't exist.
    void remove(const QString &key);
    // Same as remove(), but takes a "file://" URL as previously returned by fileUrlFor()/
    // store() - for callers (e.g. a QML Image whose source just failed to decode) that only
    // have the URL, not the original key, at hand.
    void removeByFileUrl(const QString &fileUrl);

    // Deletes every cached file for which predicate(fileInfo) returns true - an age/name-
    // pattern-based eviction policy (e.g. "delete anything older than 3 hours whose name
    // starts with radar_"). The caller owns the policy; this just walks the directory and
    // applies it. Takes the full QFileInfo (not just name/mtime) so a predicate that needs the
    // absolute path (see purgeUnreferenced() below, which is built on top of this) or size
    // doesn't need a second directory-walk implementation of its own.
    void purgeIf(const std::function<bool(const QFileInfo &fileInfo)> &predicate);
    // Deletes every cached file whose full path is NOT in keepPaths - a "reference-counted"
    // eviction policy (e.g. "keep only images still referenced by some article's content, drop
    // everything else"), as opposed to purgeIf()'s "is this file itself stale" question. A thin
    // purgeIf() call underneath, not a separate directory walk.
    void purgeUnreferenced(const QSet<QString> &keepPaths);

    // Deletes every file in the cache directory.
    void clear();
    // Sum of every file's size currently in the cache directory. Synchronous, walks the
    // directory on every call - callers that need this off the main thread are responsible for
    // that themselves, same as the rest of this class.
    qint64 totalSizeBytes() const;

    // Turns an arbitrary remote URL into a filesystem-safe, deterministic cache key: the
    // SHA-1 hex digest of the URL, with the URL's own path extension appended (falls back to
    // fallbackExtension when the URL's path carries none, e.g. a bare CDN endpoint like
    // "/image?id=123") - so "is this URL cached?" never needs a separate URL<->key table
    // anywhere. Optional - only useful when the caller doesn't already have a natural, safe
    // key of its own (e.g. a server-supplied filename, which can be passed to store()/
    // isCached() directly instead).
    static QString keyForUrl(const QString &url, const QString &fallbackExtension = QStringLiteral("img"));

private:
    QString sanitizedFileName(const QString &key) const;

    QDir m_cacheDir;
};

#endif // DENCLA_COMMON_DISKCACHE_H
