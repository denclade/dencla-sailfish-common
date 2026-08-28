#include "httpapiclient.h"

#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QTimer>
#include <QDebug>

HttpApiClient::HttpApiClient(QObject *parent)
    : QObject(parent), m_networkAccessManager(new QNetworkAccessManager(this)) {
}

void HttpApiClient::setUserAgent(const QString &userAgent) {
    m_userAgent = userAgent;
}

void HttpApiClient::setAuthHeader(const QByteArray &headerName, const QByteArray &headerValue) {
    m_authHeaderName = headerName;
    m_authHeaderValue = headerValue;
}

void HttpApiClient::setRequestTimeoutMs(int ms) {
    m_requestTimeoutMs = ms;
}

void HttpApiClient::setErrorMessageJsonField(const QString &field) {
    m_errorMessageJsonField = field;
}

QNetworkRequest HttpApiClient::buildRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    if (!m_userAgent.isEmpty()) {
        request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);
    }
    if (!m_authHeaderName.isEmpty()) {
        request.setRawHeader(m_authHeaderName, m_authHeaderValue);
    }
    // A surprising number of real-world URLs are actually a redirect to the real content, not
    // the content itself (a GitHub "raw" link being the concrete case that surfaced this on
    // harbour-sailreader: a real multi-MB file written as an empty 0-byte file, since Qt does
    // not auto-follow redirects unless this is set explicitly). Without it, a redirect reply
    // still comes back with error() == NoError (a redirect is not a network error) and a tiny/
    // empty body.
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    return request;
}

void HttpApiClient::armRequestTimeout(QNetworkReply *reply) const {
    if (m_requestTimeoutMs <= 0) return;
    QTimer::singleShot(m_requestTimeoutMs, reply, [reply]() {
        if (!reply->isFinished()) reply->abort();
    });
}

QNetworkReply *HttpApiClient::get(const QUrl &url, const QVariant &requestTag) {
    QNetworkRequest request = buildRequest(url);
    if (requestTag.isValid()) request.setAttribute(QNetworkRequest::User, requestTag);
    QNetworkReply *reply = m_networkAccessManager->get(request);
    armRequestTimeout(reply);
    return reply;
}

QNetworkReply *HttpApiClient::put(const QUrl &url, const QByteArray &body, const QByteArray &contentType,
                                   const QVariant &requestTag) {
    QNetworkRequest request = buildRequest(url);
    if (!contentType.isEmpty()) request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (requestTag.isValid()) request.setAttribute(QNetworkRequest::User, requestTag);
    QNetworkReply *reply = m_networkAccessManager->put(request, body);
    armRequestTimeout(reply);
    return reply;
}

QNetworkReply *HttpApiClient::post(const QUrl &url, const QByteArray &body, const QByteArray &contentType,
                                    const QVariant &requestTag) {
    QNetworkRequest request = buildRequest(url);
    if (!contentType.isEmpty()) request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (requestTag.isValid()) request.setAttribute(QNetworkRequest::User, requestTag);
    QNetworkReply *reply = m_networkAccessManager->post(request, body);
    armRequestTimeout(reply);
    return reply;
}

QNetworkReply *HttpApiClient::sendCustomRequest(const QByteArray &verb, const QUrl &url, const QByteArray &body,
                                                 const QByteArray &contentType, const QVariant &requestTag) {
    QNetworkRequest request = buildRequest(url);
    if (!contentType.isEmpty()) request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (requestTag.isValid()) request.setAttribute(QNetworkRequest::User, requestTag);

    // QNetworkAccessManager::sendCustomRequest()'s QByteArray-body overload only exists from
    // Qt 5.8 onward - the actual Sailfish OS SDK target ships Qt 5.6.3, where only the
    // QIODevice* overload exists at all (confirmed directly: the QByteArray call here compiled
    // fine against a newer host Qt but failed with an ambiguous-conversion error against the
    // real SDK target). A QBuffer wrapping the QByteArray is the portable way to pass a fixed
    // body through that older signature - it must outlive the request, so it's parented to the
    // reply once that exists and cleaned up automatically once the reply itself is deleted.
    QNetworkReply *reply;
    if (body.isEmpty()) {
        reply = m_networkAccessManager->sendCustomRequest(request, verb);
    } else {
        QBuffer *buffer = new QBuffer;
        buffer->setData(body);
        buffer->open(QIODevice::ReadOnly);
        reply = m_networkAccessManager->sendCustomRequest(request, verb, buffer);
        buffer->setParent(reply);
    }
    armRequestTimeout(reply);
    return reply;
}

QString HttpApiClient::extractErrorMessage(const QByteArray &body, const QString &jsonField, const QString &fallback) {
    if (jsonField.isEmpty()) return fallback;
    const QJsonObject errorJson = QJsonDocument::fromJson(body).object();
    if (!errorJson.contains(jsonField)) return fallback;
    return errorJson.value(jsonField).toString();
}

bool HttpApiClient::checkAndHandleError(QNetworkReply *reply, const QString &context, QString *errorMessage) const {
    if (reply->error() == QNetworkReply::NoError) return false;

    const QString message = extractErrorMessage(reply->readAll(), m_errorMessageJsonField, reply->errorString());
    // A deliberate abort() - by armRequestTimeout() above, or by a caller that grabbed the
    // QNetworkReply* this class returned and cancelled it itself - isn't worth a qWarning(),
    // same as DownloadQueue's own cancel()/cancelAll() staying silent. Still reported as a
    // failure via the return value/errorMessage either way, so callers that do care can look at
    // reply->error() themselves.
    if (reply->error() != QNetworkReply::OperationCanceledError) {
        qWarning() << "[HttpApiClient]" << context << ":" << message;
    }
    if (errorMessage) *errorMessage = message;
    return true;
}
