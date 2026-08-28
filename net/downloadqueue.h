#ifndef DENCLA_COMMON_DOWNLOADQUEUE_H
#define DENCLA_COMMON_DOWNLOADQUEUE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQueue>
#include <QHash>
#include <QPair>
#include "../cache/diskcache.h"

// Concurrency-capped "fetch these URLs, write each into a DiskCache" queue - the shape shared
// by harbour-sailreader's MediaCacheManager, harbour-wetterstein's RadarManager, and
// harbour-audiobookfish's DownloadManager (all: queue N URLs, run a handful concurrently,
// verify what comes back, write to a keyed disk cache). Streams each download straight to its
// DiskCache::beginWrite() tmp file as bytes arrive (QNetworkReply::readyRead), never buffering
// a whole response in memory - the difference between a few KB (an article image, a radar
// tile) and a few hundred MB (an audiobook archive) would otherwise be the difference between
// fine and a real memory-pressure problem on a phone.
//
// Deliberately scoped to "one key produces exactly one cached file" - a download whose real
// result is more than one file (e.g. extracting a multi-file zip archive into a directory,
// harbour-audiobookfish's own DownloadManager) doesn't fit DiskCache's one-key-one-file model
// at all. That's not this class's job either way: connect to downloaded() and do the
// extraction yourself once the archive itself has landed in the cache, then DiskCache::remove()
// the now-redundant cached archive - see harbour-audiobookfish's DownloadManager for exactly
// that pattern.
//
// Owns its own QNetworkAccessManager rather than sharing one with an HttpApiClient the caller
// might also have: an app's download URLs are frequently on a different host than its API
// (arbitrary third-party image URLs embedded in synced content, for harbour-sailreader) where
// reusing the API client's auth header would be actively wrong. setUserAgent()/setAuthHeader()/
// setRequestTimeoutMs() below mirror HttpApiClient's own setters by name for a caller that
// does want the same configuration applied to both - deliberately duplicated setters, not a
// shared instance, so the two classes stay independently usable.
class DownloadQueue : public QObject {
    Q_OBJECT
public:
    // cache is not owned - the caller keeps its own DiskCache instance alive for as long as
    // this DownloadQueue exists. That includes destruction order: if both are members of the
    // same owning object with DiskCache declared before this class's own pointer to it (the
    // pattern every current consumer uses, so its DiskCache is fully constructed before the
    // DownloadQueue that references it), the DiskCache member is destructed FIRST regardless -
    // Qt deletes QObject children (this DownloadQueue, parented to the owner) from inside
    // ~QObject(), which only runs after the owner's own non-QObject members, DiskCache
    // included, have already been destroyed. See ~DownloadQueue()'s own comment for how its
    // cleanup deliberately avoids touching `cache` because of exactly this.
    explicit DownloadQueue(DiskCache *cache, QObject *parent = nullptr);
    ~DownloadQueue();

    // How many downloads run concurrently. harbour-wetterstein's RadarManager hardcoded 5,
    // harbour-audiobookfish's DownloadManager had no cap at all; exposed as a setter here
    // since different content (small tiles vs. large archives) may want a different cap.
    // Defaults to 4.
    void setMaxConcurrentDownloads(int max);
    // Sent as the User-Agent header on every download. Empty (the default) means don't set one.
    void setUserAgent(const QString &userAgent);
    // Sent as an extra header on every download (e.g. "Authorization"/"Bearer <token>") - pass
    // an empty headerName to stop sending one. Off by default: most download sources (third-
    // party image URLs, a public radar tile proxy) neither need nor want the app's own API
    // credentials attached - a caller downloading from the same authenticated API it already
    // talks to (harbour-audiobookfish's book archives) opts in explicitly. Re-callable at any
    // time (e.g. after a token refresh) without losing whatever is already queued/in flight.
    void setAuthHeader(const QByteArray &headerName, const QByteArray &headerValue);
    // A download still running after this many ms is aborted (counted as a failure, same as
    // any other network error). 0 disables the timeout. Defaults to 30000 (30s) - none of the
    // three source apps' own former download loops had a timeout at all, so a stuck download
    // previously occupied a concurrency slot (or, for harbour-audiobookfish, ran fully
    // unbounded) forever.
    void setRequestTimeoutMs(int ms);

    // Enqueues one URL for download under the given cache key (see DiskCache::store()). Every
    // accepted call ends in exactly one downloaded()/downloadFailed() signal for that key,
    // including when the key turns out to already be cached (fired on the next event loop
    // iteration, not synchronously, so behavior is consistent whether or not a network fetch
    // actually happened, and so a slot connected to downloaded() can safely call enqueue()
    // again without reentrancy concerns) - callers don't need to duplicate DiskCache::isCached()
    // themselves just to know whether to expect a signal. A second enqueue() for a key that's
    // already queued/in-flight is folded into the first one's eventual single signal, not a
    // second one - this class has no notion of "two different callers asked for the same key",
    // only "is this key covered right now".
    void enqueue(const QString &url, const QString &cacheKey);

    // Cancels one specific download, whether still pending or already in flight, without
    // touching any others - for a per-item "Cancel" action (e.g. a user tapping "Cancel" on
    // one specific in-progress book download while others keep running). A no-op if cacheKey
    // isn't currently queued/in-flight. Deliberately silent, same as cancelAll() below: neither
    // downloaded() nor downloadFailed() fires for a cancelled key - the caller already knows
    // it cancelled it and is expected to update its own state immediately rather than wait for
    // a signal that (like any other OperationCanceledError) is never emitted.
    void cancel(const QString &cacheKey);
    // Aborts every in-flight download and drops everything still pending, without touching
    // anything already written to the cache - for a "Clear Cache" action that wants to stop
    // downloads a moment before wiping the cache directory itself, rather than racing an
    // in-flight write against the wipe.
    void cancelAll();

    bool isIdle() const;

signals:
    // Emitted once per accepted enqueue() - see its own comment on when this fires for an
    // already-cached key.
    void downloaded(const QString &cacheKey, const QString &filePath);
    // Emitted when a download fails - a real network error, or a successful-looking reply
    // whose final size doesn't match its own Content-Length header (a redirect or truncated
    // response can come back with QNetworkReply::NoError and a short/empty body - see
    // harbour-wetterstein's RadarManager, where this integrity check was first added after
    // observing exactly that). Not emitted for a deliberate cancel() / cancelAll() - see their
    // own comments.
    void downloadFailed(const QString &cacheKey, const QString &error);
    // Raw passthrough of QNetworkReply::downloadProgress for an in-flight download - no
    // throttling done here (harbour-audiobookfish's DownloadManager, the one consumer that
    // cares about this at all, already has its own "max 5 UI updates/sec" throttling; baking
    // a policy like that into this class would only constrain a future caller that wants
    // different throttling, or none). bytesTotal is -1 if the server didn't send a
    // Content-Length header.
    void downloadProgress(const QString &cacheKey, qint64 bytesReceived, qint64 bytesTotal);
    // True once the queue has fully drained (nothing pending or in flight) - false again as
    // soon as enqueue() adds new work. Useful for a caller that wants to know when a batch of
    // enqueue() calls it just made has finished (e.g. to decide whether to retry an
    // incomplete-looking batch - this class only reports per-URL outcomes, not batch
    // completeness, since it has no concept of what "the batch" was to begin with).
    void idleChanged(bool idle);

private:
    QNetworkRequest buildRequest(const QUrl &url) const;
    void armRequestTimeout(QNetworkReply *reply) const;
    void startNextDownloads();
    void downloadOne(const QString &url, const QString &cacheKey);
    void setIdle(bool idle);

    DiskCache *m_cache;
    QNetworkAccessManager m_networkAccessManager;
    QString m_userAgent;
    QByteArray m_authHeaderName;
    QByteArray m_authHeaderValue;
    int m_requestTimeoutMs = 30000;
    QQueue<QPair<QString, QString>> m_pendingQueue; // (url, cacheKey)
    // Keyed by cacheKey rather than a plain QSet<QNetworkReply*> so cancel() can find one
    // specific in-flight download directly instead of every caller needing its own
    // key<->reply bookkeeping just to cancel a single item.
    QHash<QString, QNetworkReply *> m_activeReplies;
    QSet<QString> m_queuedOrInFlightKeys;
    int m_maxConcurrentDownloads = 4;
    bool m_idle = true;
};

#endif // DENCLA_COMMON_DOWNLOADQUEUE_H
