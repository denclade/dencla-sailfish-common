#ifndef DENCLA_COMMON_DOWNLOADQUEUE_H
#define DENCLA_COMMON_DOWNLOADQUEUE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQueue>
#include <QSet>
#include <QPair>
#include <functional>
#include "../cache/diskcache.h"

// Concurrency-capped "fetch these URLs, write each into a DiskCache" queue - the shape shared
// by harbour-sailreader's MediaCacheManager and harbour-wetterstein's RadarManager (both:
// queue N URLs, run a handful concurrently, verify what comes back, write to a keyed disk
// cache). Deliberately scoped to "many small-to-medium files, care about which finished, not
// byte-level progress of any one of them" AND "one key produces exactly one cached file" -
// harbour-audiobookfish's DownloadManager doesn't fit either constraint (a single large
// per-book archive with byte-level progress UI, whose real result after extraction is many
// files, not one) and should keep its own implementation rather than being forced to fit this
// one. setRawDataFilter() below closes the gap for single-file transforms (decompress,
// verify, ...) but deliberately not for anything that fans one download out into several
// files - see its own comment.
//
// Owns its own QNetworkAccessManager rather than sharing one with an HttpApiClient the caller
// might also have: an app's download URLs are frequently on a different host than its API
// (arbitrary third-party image URLs embedded in synced content, for harbour-sailreader) where
// reusing the API client's auth header would be actively wrong. setUserAgent()/setAuthHeader()/
// setRequestTimeoutMs() below mirror HttpApiClient's own setters by name for a caller that
// does want the same configuration applied to both (harbour-wetterstein's radar images share
// the API's own proxy host, for instance) - deliberately duplicated setters, not a shared
// instance, so the two classes stay independently usable.
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
    // Sent as the User-Agent header on every download. Empty (the default) means don't set one.
    void setUserAgent(const QString &userAgent);
    // Sent as an extra header on every download (e.g. "Authorization"/"Bearer <token>") - pass
    // an empty headerName to stop sending one. Off by default: most download sources (third-
    // party image URLs, a public radar tile proxy) neither need nor want the app's own API
    // credentials attached - a caller downloading from the same authenticated API it already
    // talks to (unlike either current consumer of this class) opts in explicitly.
    void setAuthHeader(const QByteArray &headerName, const QByteArray &headerValue);
    // A download still running after this many ms is aborted (counted as a failure, same as
    // any other network error). 0 disables the timeout. Defaults to 30000 (30s) - neither
    // harbour-sailreader's nor harbour-wetterstein's own former download loops had a timeout
    // at all, so a stuck download previously occupied a concurrency slot forever.
    void setRequestTimeoutMs(int ms);

    // Optional hook, run synchronously right after a download's bytes pass the integrity
    // check and before they're written to the cache - lets a caller transform `data` in place
    // (e.g. decompress a single-file gzip, verify a checksum) or reject it outright (return
    // false, with *error set) before DiskCache::store() ever sees it. A std::function rather
    // than a signal deliberately: with more than one slot connected, a signal mutating a
    // shared QByteArray in place would have order-dependent, unclear semantics; a single
    // settable hook has one well-defined owner. The outcome still surfaces through the normal
    // downloaded()/downloadFailed() signals below either way - only the transform step itself
    // is a function, not a broadcast.
    //
    // Not a general solution for a download whose real result is more than one file (e.g.
    // extracting a multi-file zip archive into a directory) - that doesn't fit this class's
    // one-key-one-cached-file model at all, hook or no hook, since DiskCache itself has no
    // concept of "one key, many files". A download shaped like that needs its own handling
    // outside this class regardless (see harbour-audiobookfish's DownloadManager).
    void setRawDataFilter(const std::function<bool(const QString &cacheKey, QByteArray &data, QString *error)> &filter);

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

    // Aborts every in-flight download and drops everything still pending, without touching
    // anything already written to the cache - for a "Clear Cache" action that wants to stop
    // downloads a moment before wiping the cache directory itself, rather than racing an
    // in-flight write against the wipe. Each aborted reply's own finished() handler still
    // fires (aborting is not silent), so idleChanged(true) follows shortly after this returns,
    // not necessarily synchronously within the call.
    void cancelAll();

    bool isIdle() const;

signals:
    // Emitted once per accepted enqueue() - see its own comment on when this fires for an
    // already-cached key.
    void downloaded(const QString &cacheKey, const QString &filePath);
    // Emitted when a download fails - a real network error, a successful-looking reply whose
    // body size doesn't match its own Content-Length header (a redirect or truncated response
    // can come back with QNetworkReply::NoError and a short/empty body - see
    // harbour-wetterstein's RadarManager, where this integrity check was first added after
    // observing exactly that), or a rawDataFilter rejection.
    void downloadFailed(const QString &cacheKey, const QString &error);
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
    std::function<bool(const QString &, QByteArray &, QString *)> m_rawDataFilter;
    QQueue<QPair<QString, QString>> m_pendingQueue; // (url, cacheKey)
    QSet<QString> m_queuedOrInFlightKeys;
    QSet<QNetworkReply *> m_activeReplies;
    int m_maxConcurrentDownloads = 4;
    bool m_idle = true;
};

#endif // DENCLA_COMMON_DOWNLOADQUEUE_H
