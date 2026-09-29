#include "pocketbaseclient.h"

#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QPointer>
#include <QTimer>
#include <QUrlQuery>
#include <QtDebug>

PocketBaseClient::PocketBaseClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void PocketBaseClient::setBaseUrl(const QUrl &url)
{
    QString text = url.toString();
    while (text.endsWith(QLatin1Char('/'))) {
        text.chop(1);
    }
    m_baseUrl = QUrl(text);
}

QString PocketBaseClient::errorMessage(const QByteArray &body, const QString &fallback)
{
    const QJsonObject object = QJsonDocument::fromJson(body).object();
    QString message = object.value(QStringLiteral("message")).toString();
    if (message.isEmpty()) {
        return fallback;
    }
    // Field errors ("data": {"email": {"message": "..."}}) say what exactly.
    const QJsonObject data = object.value(QStringLiteral("data")).toObject();
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        const QString detail = it.value().toObject().value(QStringLiteral("message")).toString();
        if (!detail.isEmpty()) {
            message += QStringLiteral(" (%1: %2)").arg(it.key(), detail);
        }
    }
    return message;
}

QNetworkRequest PocketBaseClient::request(const QString &path, const QUrlQuery &query) const
{
    QUrl url = m_baseUrl;
    url.setPath(url.path() + path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    if (!m_userAgent.isEmpty()) {
        request.setRawHeader("User-Agent", m_userAgent);
    }
    if (m_session.isValid()) {
        // The raw token, no "Bearer " -- PocketBase's convention.
        request.setRawHeader("Authorization", m_session.token.toUtf8());
    }
    return request;
}

void PocketBaseClient::finish(QNetworkReply *reply,
                              const std::function<void(const QByteArray &, const QString &)> &handler)
{
    QPointer<QNetworkReply> guarded(reply);
    QTimer *timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, reply, [guarded]() {
        if (guarded) {
            guarded->abort();
        }
    });
    timeout->start(m_timeoutMs);
    connect(reply, &QNetworkReply::finished, this, [reply, handler]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            const QString fallback = reply->error() == QNetworkReply::OperationCanceledError
                ? tr("The server did not answer in time.")
                : reply->errorString();
            const QString message = errorMessage(body, fallback);
            qWarning() << "[PocketBaseClient]" << reply->request().url().path() << status << message;
            handler(QByteArray(), message.isEmpty() ? QStringLiteral("HTTP %1").arg(status) : message);
            return;
        }
        handler(body, QString());
    });
}

void PocketBaseClient::postJson(const QString &path, const QJsonObject &body,
                                const std::function<void(const QByteArray &, const QString &)> &handler,
                                bool authorized)
{
    QNetworkRequest req = request(path);
    if (!authorized) {
        req.setRawHeader("Authorization", QByteArray());
    }
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));
    finish(m_network->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact)), handler);
}

PocketBaseClient::Session PocketBaseClient::sessionFrom(const QByteArray &body)
{
    const QJsonObject object = QJsonDocument::fromJson(body).object();
    Session session;
    session.token = object.value(QStringLiteral("token")).toString();
    const QJsonObject record = object.value(QStringLiteral("record")).toObject();
    session.userId = record.value(QStringLiteral("id")).toString();
    session.verified = record.value(QStringLiteral("verified")).toBool();
    return session;
}

void PocketBaseClient::signUp(const QString &email, const QString &password, const Done &done)
{
    QJsonObject body;
    body.insert(QStringLiteral("email"), email);
    body.insert(QStringLiteral("password"), password);
    body.insert(QStringLiteral("passwordConfirm"), password);
    postJson(QStringLiteral("/api/collections/users/records"), body,
             [done](const QByteArray &, const QString &error) { done(error.isEmpty(), error); }, false);
}

void PocketBaseClient::requestVerification(const QString &email, const Done &done)
{
    QJsonObject body;
    body.insert(QStringLiteral("email"), email);
    postJson(QStringLiteral("/api/collections/users/request-verification"), body,
             [done](const QByteArray &, const QString &error) { done(error.isEmpty(), error); }, false);
}

void PocketBaseClient::logIn(const QString &email, const QString &password, const SessionDone &done)
{
    QJsonObject body;
    body.insert(QStringLiteral("identity"), email);
    body.insert(QStringLiteral("password"), password);
    postJson(QStringLiteral("/api/collections/users/auth-with-password"), body,
             [this, done](const QByteArray &reply, const QString &error) {
                 if (!error.isEmpty()) {
                     done(Session(), error);
                     return;
                 }
                 m_session = sessionFrom(reply);
                 done(m_session, m_session.isValid() ? QString() : tr("The server sent no session."));
             },
             false);
}

void PocketBaseClient::refreshSession(const SessionDone &done)
{
    postJson(QStringLiteral("/api/collections/users/auth-refresh"), QJsonObject(),
             [this, done](const QByteArray &reply, const QString &error) {
                 if (!error.isEmpty()) {
                     done(Session(), error);
                     return;
                 }
                 m_session = sessionFrom(reply);
                 done(m_session, m_session.isValid() ? QString() : tr("The server sent no session."));
             },
             true);
}

void PocketBaseClient::listRecords(const QString &collection, int page, int perPage, const QString &filter,
                                   const QString &fields, const PageDone &done)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    query.addQueryItem(QStringLiteral("perPage"), QString::number(perPage));
    if (!filter.isEmpty()) {
        query.addQueryItem(QStringLiteral("filter"), QString::fromLatin1(QUrl::toPercentEncoding(filter)));
    }
    if (!fields.isEmpty()) {
        query.addQueryItem(QStringLiteral("fields"), fields);
    }
    finish(m_network->get(request(QStringLiteral("/api/collections/%1/records").arg(collection), query)),
           [done](const QByteArray &body, const QString &error) {
               Page result;
               if (error.isEmpty()) {
                   const QJsonObject object = QJsonDocument::fromJson(body).object();
                   result.page = object.value(QStringLiteral("page")).toInt();
                   result.totalPages = object.value(QStringLiteral("totalPages")).toInt();
                   for (const QJsonValue &item : object.value(QStringLiteral("items")).toArray()) {
                       result.items.append(item.toObject());
                   }
               }
               done(result, error);
           });
}

void PocketBaseClient::saveRecord(const QString &collection, const QString &recordId, const QJsonObject &fields,
                                  const QVector<FilePart> &files, const RecordDone &done)
{
    // multipart/form-data by hand: Qt 5.6 cannot send a QHttpMultiPart
    // with PATCH, which updating a record needs.
    const QByteArray boundary = "----dencla" + QByteArray::number(qrand()) + QByteArray::number(qrand());
    QByteArray body;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        const QJsonValue value = it.value();
        QByteArray text;
        if (value.isString()) {
            text = value.toString().toUtf8();
        } else {
            // A number or bool as JSON: "[42]" without the brackets.
            const QByteArray array = QJsonDocument(QJsonArray() << value).toJson(QJsonDocument::Compact);
            text = array.mid(1, array.size() - 2);
        }
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + it.key().toUtf8() + "\"\r\n\r\n";
        body += text + "\r\n";
    }
    for (const FilePart &file : files) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + file.field.toUtf8() + "\"; filename=\""
            + file.fileName.toUtf8() + "\"\r\n";
        body += "Content-Type: application/octet-stream\r\n\r\n";
        body += file.data + "\r\n";
    }
    body += "--" + boundary + "--\r\n";

    const QString path = recordId.isEmpty() ? QStringLiteral("/api/collections/%1/records").arg(collection)
                                            : QStringLiteral("/api/collections/%1/records/%2").arg(collection, recordId);
    QNetworkRequest req = request(path);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("multipart/form-data; boundary=") + boundary);
    QBuffer *buffer = new QBuffer;
    buffer->setData(body);
    buffer->open(QIODevice::ReadOnly);
    QNetworkReply *reply = m_network->sendCustomRequest(req, recordId.isEmpty() ? "POST" : "PATCH", buffer);
    buffer->setParent(reply);
    finish(reply, [done](const QByteArray &reply, const QString &error) {
        done(QJsonDocument::fromJson(reply).object(), error);
    });
}

void PocketBaseClient::deleteRecord(const QString &collection, const QString &recordId, const Done &done)
{
    finish(m_network->deleteResource(request(QStringLiteral("/api/collections/%1/records/%2").arg(collection, recordId))),
           [done](const QByteArray &, const QString &error) { done(error.isEmpty(), error); });
}

void PocketBaseClient::downloadFile(const QString &collection, const QString &recordId, const QString &fileName,
                                    const DataDone &done)
{
    postJson(QStringLiteral("/api/files/token"), QJsonObject(),
             [this, collection, recordId, fileName, done](const QByteArray &body, const QString &error) {
                 if (!error.isEmpty()) {
                     done(QByteArray(), error);
                     return;
                 }
                 QUrlQuery query;
                 query.addQueryItem(QStringLiteral("token"),
                                    QJsonDocument::fromJson(body).object().value(QStringLiteral("token")).toString());
                 finish(m_network->get(request(QStringLiteral("/api/files/%1/%2/%3").arg(collection, recordId, fileName),
                                               query)),
                        done);
             },
             true);
}
