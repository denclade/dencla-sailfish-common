#include "downloadqueue.h"

#include <QNetworkRequest>

DownloadQueue::DownloadQueue(DiskCache *cache, QObject *parent)
    : QObject(parent), m_cache(cache) {
}

void DownloadQueue::setMaxConcurrentDownloads(int max) {
    m_maxConcurrentDownloads = max;
}

bool DownloadQueue::isIdle() const {
    return m_idle;
}

void DownloadQueue::setIdle(bool idle) {
    if (m_idle == idle) return;
    m_idle = idle;
    emit idleChanged(idle);
}

void DownloadQueue::enqueue(const QString &url, const QString &cacheKey) {
    if (m_queuedOrInFlightKeys.contains(cacheKey) || m_cache->isCached(cacheKey)) return;
    m_queuedOrInFlightKeys.insert(cacheKey);
    m_pendingQueue.enqueue(qMakePair(url, cacheKey));
    setIdle(false);
    startNextDownloads();
}

void DownloadQueue::startNextDownloads() {
    while (m_activeReplies.size() < m_maxConcurrentDownloads && !m_pendingQueue.isEmpty()) {
        const QPair<QString, QString> next = m_pendingQueue.dequeue();
        downloadOne(next.first, next.second);
    }
}

void DownloadQueue::downloadOne(const QString &url, const QString &cacheKey) {
    QNetworkRequest request{QUrl(url)};
    // See HttpApiClient::buildRequest()'s own comment on why this matters - a redirect can
    // otherwise land as a successful-looking, empty/tiny reply.
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    QNetworkReply *reply = m_networkAccessManager.get(request);
    m_activeReplies.insert(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, url, cacheKey]() {
        m_activeReplies.remove(reply);
        m_queuedOrInFlightKeys.remove(cacheKey);

        const QByteArray data = reply->readAll();
        const QVariant expectedSize = reply->header(QNetworkRequest::ContentLengthHeader);

        if (reply->error() == QNetworkReply::NoError && !data.isEmpty()
                && (!expectedSize.isValid() || expectedSize.toLongLong() == data.size())) {
            const QString path = m_cache->store(cacheKey, data);
            if (!path.isEmpty()) {
                emit downloaded(cacheKey, path);
            } else {
                emit downloadFailed(cacheKey, QStringLiteral("failed to write to cache"));
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
