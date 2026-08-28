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

void DownloadQueue::setRawDataFilter(const std::function<bool(const QString &, QByteArray &, QString *)> &filter) {
    m_rawDataFilter = filter;
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

void DownloadQueue::cancelAll() {
    for (const auto &pending : m_pendingQueue) {
        m_queuedOrInFlightKeys.remove(pending.second);
    }
    m_pendingQueue.clear();
    // abort() still triggers each reply's own finished() lambda in downloadOne() below, which
    // cleans up m_activeReplies/m_queuedOrInFlightKeys and re-checks idle state itself -
    // nothing further to do with in-flight replies here.
    for (QNetworkReply *reply : m_activeReplies) reply->abort();
    if (m_pendingQueue.isEmpty() && m_activeReplies.isEmpty()) setIdle(true);
}

void DownloadQueue::startNextDownloads() {
    while (m_activeReplies.size() < m_maxConcurrentDownloads && !m_pendingQueue.isEmpty()) {
        const QPair<QString, QString> next = m_pendingQueue.dequeue();
        downloadOne(next.first, next.second);
    }
}

void DownloadQueue::downloadOne(const QString &url, const QString &cacheKey) {
    QNetworkReply *reply = m_networkAccessManager.get(buildRequest(QUrl(url)));
    m_activeReplies.insert(reply);
    armRequestTimeout(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, url, cacheKey]() {
        m_activeReplies.remove(reply);
        m_queuedOrInFlightKeys.remove(cacheKey);

        QByteArray data = reply->readAll();
        const QVariant expectedSize = reply->header(QNetworkRequest::ContentLengthHeader);

        if (reply->error() == QNetworkReply::NoError && !data.isEmpty()
                && (!expectedSize.isValid() || expectedSize.toLongLong() == data.size())) {
            QString filterError;
            if (m_rawDataFilter && !m_rawDataFilter(cacheKey, data, &filterError)) {
                emit downloadFailed(cacheKey, filterError.isEmpty()
                    ? QStringLiteral("rejected by raw data filter") : filterError);
            } else {
                const QString path = m_cache->store(cacheKey, data);
                if (!path.isEmpty()) {
                    emit downloaded(cacheKey, path);
                } else {
                    emit downloadFailed(cacheKey, QStringLiteral("failed to write to cache"));
                }
            }
        } else if (reply->error() == QNetworkReply::NoError) {
            // NoError but either empty, or a size mismatch against the reply's own
            // Content-Length - a redirect or truncated transfer landed as a "successful" reply
            // with unusable content (see this class's own header comment).
            emit downloadFailed(cacheKey, QStringLiteral("incomplete or empty response for %1").arg(url));
        } else if (reply->error() != QNetworkReply::OperationCanceledError) {
            emit downloadFailed(cacheKey, reply->errorString());
        }

        reply->deleteLater();
        startNextDownloads();
        if (m_pendingQueue.isEmpty() && m_activeReplies.isEmpty()) setIdle(true);
    });
}
