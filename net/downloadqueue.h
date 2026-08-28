#ifndef DENCLA_COMMON_DOWNLOADQUEUE_H
#define DENCLA_COMMON_DOWNLOADQUEUE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QSet>
#include <QPair>
#include "../cache/diskcache.h"

// Concurrency-capped "fetch these URLs, write each into a DiskCache" queue - the shape shared
// by harbour-sailreader's MediaCacheManager and harbour-wetterstein's RadarManager (both:
// queue N URLs, run a handful concurrently, verify what comes back, write to a keyed disk
// cache). Deliberately scoped to "many small-to-medium files, care about which finished, not
// byte-level progress of any one of them" - harbour-audiobookfish's DownloadManager (a single
// large per-book archive with byte-level progress UI and a zip-extraction step afterwards) is
// a different enough shape that it should keep its own implementation rather than being forced
// to fit this one.
class DownloadQueue : public QObject {
    Q_OBJECT
public:
    // cache is not owned - the caller keeps its own DiskCache instance alive for as long as
    // this DownloadQueue exists.
    explicit DownloadQueue(DiskCache *cache, QObject *parent = nullptr);

    // How many downloads run concurrently. harbour-wetterstein's RadarManager hardcoded 5;
    // exposed as a setter here since different content (small tiles vs. larger images) may
    // want a different cap. Defaults to 4.
    void setMaxConcurrentDownloads(int max);

    // Enqueues one URL for download under the given cache key (see DiskCache::store()) - a
    // no-op if that key is already cached, or already queued/in flight. Starts draining the
    // queue immediately, respecting the concurrency cap.
    void enqueue(const QString &url, const QString &cacheKey);

    bool isIdle() const;

signals:
    // Emitted once per successfully downloaded-and-cached URL.
    void downloaded(const QString &cacheKey, const QString &filePath);
    // Emitted when a download fails - a real network error, or a successful-looking reply
    // whose body size doesn't match its own Content-Length header (a redirect or truncated
    // response can come back with QNetworkReply::NoError and a short/empty body - see
    // harbour-wetterstein's RadarManager, where this integrity check was first added after
    // observing exactly that).
    void downloadFailed(const QString &cacheKey, const QString &error);
    // True once the queue has fully drained (nothing pending or in flight) - false again as
    // soon as enqueue() adds new work. Useful for a caller that wants to know when a batch of
    // enqueue() calls it just made has finished (e.g. to decide whether to retry an
    // incomplete-looking batch - this class only reports per-URL outcomes, not batch
    // completeness, since it has no concept of what "the batch" was to begin with).
    void idleChanged(bool idle);

private:
    void startNextDownloads();
    void downloadOne(const QString &url, const QString &cacheKey);
    void setIdle(bool idle);

    DiskCache *m_cache;
    QNetworkAccessManager m_networkAccessManager;
    QQueue<QPair<QString, QString>> m_pendingQueue; // (url, cacheKey)
    QSet<QString> m_queuedOrInFlightKeys;
    QSet<QNetworkReply *> m_activeReplies;
    int m_maxConcurrentDownloads = 4;
    bool m_idle = true;
};

#endif // DENCLA_COMMON_DOWNLOADQUEUE_H
