#include "downloadqueue.h"

#include <QTimer>

DownloadQueue::DownloadQueue(DiskCache *cache, QObject *parent)
    : QObject(parent), m_cache(cache) {
}

void DownloadQueue::setMaxConcurrentDownloads(int max) {
    m_maxConcurrentDownloads = max;
}

void DownloadQueue::setUserAgent(const QString &userAgent) {
    m_userAgent = userAgent;
}

void DownloadQueue::setAuthHeader(const QByteArray &headerName, const QByteArray &headerValue) {
    m_authHeaderName = headerName;
    m_authHeaderValue = headerValue;
}

void DownloadQueue::setRequestTimeoutMs(int ms) {
    m_requestTimeoutMs = ms;
}

bool DownloadQueue::isIdle() const {
    return m_idle;
}

void DownloadQueue::setIdle(bool idle) {
    if (m_idle == idle) return;
    m_idle = idle;
    emit idleChanged(idle);
}

QNetworkRequest DownloadQueue::buildRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    if (!m_userAgent.isEmpty()) {
        request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
    }
    if (!m_authHeaderName.isEmpty()) {
        request.setRawHeader(m_authHeaderName, m_authHeaderValue);
    }
    // See HttpApiClient::buildRequest()'s own comment on why this matters - a redirect can
    // otherwise land as a successful-looking, empty/tiny reply.
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    return request;
}

void DownloadQueue::armRequestTimeout(QNetworkReply *reply) const {
    if (m_requestTimeoutMs <= 0) return;
    QTimer::singleShot(m_requestTimeoutMs, reply, [reply]() {
        if (!reply->isFinished()) reply->abort();
    });
}

void DownloadQueue::enqueue(const QString &url, const QString &cacheKey) {
    if (m_queuedOrInFlightKeys.contains(cacheKey)) return; // already covered by its own eventual signal

    if (m_cache->isCached(cacheKey)) {
        // Deferred rather than emitted synchronously from within enqueue() - see this method's
        // own header comment on why: keeps behavior consistent whether or not a network fetch
        // actually happened, and avoids reentrancy surprises for a slot that calls enqueue()
        // again in response to downloaded().
        const QString path = m_cache->filePathFor(cacheKey);
        QTimer::singleShot(0, this, [this, cacheKey, path]() { emit downloaded(cacheKey, path); });
        return;
    }

    m_queuedOrInFlightKeys.insert(cacheKey);
    m_pendingQueue.enqueue(qMakePair(url, cacheKey));
    setIdle(false);
    startNextDownloads();
}

void DownloadQueue::cancel(const QString &cacheKey) {
    if (!m_queuedOrInFlightKeys.contains(cacheKey)) return;

    // Still pending (not yet started)? Just drop it from the queue - nothing to abort.
    for (auto it = m_pendingQueue.begin(); it != m_pendingQueue.end(); ++it) {
        if (it->second == cacheKey) {
            m_pendingQueue.erase(it);
            m_queuedOrInFlightKeys.remove(cacheKey);
            if (m_pendingQueue.isEmpty() && m_activeReplies.isEmpty()) setIdle(true);
            return;
        }
    }

    // Already in flight - abort() triggers its own finished() handler in downloadOne() below,
    // which does the rest (discards the partial write, removes the bookkeeping, checks idle) -
    // OperationCanceledError is treated the same as any other cancellation there, so no signal
    // fires for it, same as cancelAll().
    if (QNetworkReply *reply = m_activeReplies.value(cacheKey)) {
        reply->abort();
    }
}

void DownloadQueue::cancelAll() {
    for (const auto &pending : m_pendingQueue) {
        m_queuedOrInFlightKeys.remove(pending.second);
    }
    m_pendingQueue.clear();
    // abort() still triggers each reply's own finished() lambda in downloadOne() below, which
    // cleans up m_activeReplies/m_queuedOrInFlightKeys and re-checks idle state itself -
    // nothing further to do with in-flight replies here.
    for (QNetworkReply *reply : m_activeReplies.values()) reply->abort();
    if (m_pendingQueue.isEmpty() && m_activeReplies.isEmpty()) setIdle(true);
}

void DownloadQueue::startNextDownloads() {
    while (m_activeReplies.size() < m_maxConcurrentDownloads && !m_pendingQueue.isEmpty()) {
        const QPair<QString, QString> next = m_pendingQueue.dequeue();
        downloadOne(next.first, next.second);
    }
}

void DownloadQueue::downloadOne(const QString &url, const QString &cacheKey) {
    QFile *file = m_cache->beginWrite(cacheKey);
    if (!file) {
        m_queuedOrInFlightKeys.remove(cacheKey);
        QTimer::singleShot(0, this, [this, cacheKey]() {
            emit downloadFailed(cacheKey, QStringLiteral("failed to open cache file for writing"));
            startNextDownloads();
            if (m_pendingQueue.isEmpty() && m_activeReplies.isEmpty()) setIdle(true);
        });
        return;
    }

    QNetworkReply *reply = m_networkAccessManager.get(buildRequest(QUrl(url)));
    m_activeReplies.insert(cacheKey, reply);
    file->setParent(reply); // cleaned up automatically once the reply is deleted
    armRequestTimeout(reply);

    // Streamed straight to disk as it arrives instead of buffering the whole response in
    // memory first (see this class's own header comment on why that matters) - QFile::write()
    // here is exactly what DiskCache::store() itself does in one shot for a caller that
    // already has the whole blob; this just spreads the same writes out over the download's
    // lifetime.
    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() {
        file->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, cacheKey](qint64 received, qint64 total) {
        emit downloadProgress(cacheKey, received, total);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, file, url, cacheKey]() {
        m_activeReplies.remove(cacheKey);
        m_queuedOrInFlightKeys.remove(cacheKey);

        const QVariant expectedSize = reply->header(QNetworkRequest::ContentLengthHeader);
        file->flush(); // make sure size() below reflects every write(), not just what's on disk yet
        const qint64 writtenSize = file->size();

        if (reply->error() == QNetworkReply::NoError && writtenSize > 0
                && (!expectedSize.isValid() || expectedSize.toLongLong() == writtenSize)) {
            const QString path = m_cache->commitWrite(cacheKey, file);
            if (!path.isEmpty()) {
                emit downloaded(cacheKey, path);
            } else {
                emit downloadFailed(cacheKey, QStringLiteral("failed to write to cache"));
            }
        } else {
            m_cache->discardWrite(cacheKey, file);
            if (reply->error() == QNetworkReply::NoError) {
                // NoError but either empty, or a size mismatch against the reply's own
                // Content-Length - a redirect or truncated transfer landed as a "successful"
                // reply with unusable content (see this class's own header comment).
                emit downloadFailed(cacheKey, QStringLiteral("incomplete or empty response for %1").arg(url));
            } else if (reply->error() != QNetworkReply::OperationCanceledError) {
                emit downloadFailed(cacheKey, reply->errorString());
            }
            // OperationCanceledError (cancel()/cancelAll()): no signal, matching their own
            // "deliberately silent" contract.
        }

        reply->deleteLater(); // also deletes `file`, parented to it above
        startNextDownloads();
        if (m_pendingQueue.isEmpty() && m_activeReplies.isEmpty()) setIdle(true);
    });
}
