#ifndef DENCLA_COMMON_POCKETBASECLIENT_H
#define DENCLA_COMMON_POCKETBASECLIENT_H

#include <functional>

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QUrlQuery>
#include <QVector>

// The part of PocketBase's REST API an app needs to keep records of its
// own on a user's server: the built-in "users" auth collection (sign up,
// verification mail, log in), records with a file field, and the file
// behind one. There is no C++ SDK; the API is small enough for plain
// QNetworkAccessManager calls.
//
// Asynchronous, one callback per call, always called exactly once (also
// on network errors and timeouts). `error` is PocketBase's own message
// where it sends one ("Failed to authenticate."), else Qt's.
//
// Collections, fields and their API rules are the server operator's side
// -- see the app's docs for the schema it expects.
class PocketBaseClient : public QObject {
    Q_OBJECT
public:
    struct Session {
        QString token;
        QString userId;
        bool verified = false;
        bool isValid() const { return !token.isEmpty(); }
    };

    struct Page {
        QVector<QJsonObject> items;
        int page = 0;
        int totalPages = 0;
    };

    // A file field's content for create/update.
    struct FilePart {
        QString field;
        QString fileName;
        QByteArray data;
    };

    typedef std::function<void(bool ok, const QString &error)> Done;
    typedef std::function<void(const Session &session, const QString &error)> SessionDone;
    typedef std::function<void(const Page &page, const QString &error)> PageDone;
    typedef std::function<void(const QJsonObject &record, const QString &error)> RecordDone;
    typedef std::function<void(const QByteArray &data, const QString &error)> DataDone;

    explicit PocketBaseClient(QObject *parent = nullptr);

    // "https://pb.example.org" -- with or without a trailing slash.
    void setBaseUrl(const QUrl &url);
    QUrl baseUrl() const { return m_baseUrl; }
    void setUserAgent(const QByteArray &userAgent) { m_userAgent = userAgent; }
    void setSession(const Session &session) { m_session = session; }
    Session session() const { return m_session; }
    // Aborts requests still running after this long. Default 30 s.
    void setTimeoutMs(int ms) { m_timeoutMs = ms; }

    void signUp(const QString &email, const QString &password, const Done &done);
    void requestVerification(const QString &email, const Done &done);
    // Sets the session on success.
    void logIn(const QString &email, const QString &password, const SessionDone &done);
    // A fresh token for the current session (tokens expire; PocketBase's
    // default is two weeks). Sets the session on success.
    void refreshSession(const SessionDone &done);

    // One page of a collection's records. `filter` in PocketBase's syntax,
    // `fields` a comma-separated projection ("id,name") -- both optional.
    void listRecords(const QString &collection, int page, int perPage, const QString &filter,
                     const QString &fields, const PageDone &done);
    // Create (recordId empty) or update a record: plain fields plus files.
    void saveRecord(const QString &collection, const QString &recordId, const QJsonObject &fields,
                    const QVector<FilePart> &files, const RecordDone &done);
    void deleteRecord(const QString &collection, const QString &recordId, const Done &done);
    // The content of a record's file field (`fileName` as the record lists
    // it). Asks for a short-lived file token first, so it works for
    // protected file fields too.
    void downloadFile(const QString &collection, const QString &recordId, const QString &fileName,
                      const DataDone &done);

    // PocketBase's error message from a response body, or `fallback`.
    static QString errorMessage(const QByteArray &body, const QString &fallback);

private:
    QNetworkRequest request(const QString &path, const QUrlQuery &query = QUrlQuery()) const;
    // Calls `handler` once with the reply's body and error ("" if none).
    void finish(QNetworkReply *reply, const std::function<void(const QByteArray &, const QString &)> &handler);
    void postJson(const QString &path, const QJsonObject &body,
                  const std::function<void(const QByteArray &, const QString &)> &handler, bool authorized);
    static Session sessionFrom(const QByteArray &body);

    QNetworkAccessManager *m_network;
    QUrl m_baseUrl;
    QByteArray m_userAgent;
    Session m_session;
    int m_timeoutMs = 30000;
};

#endif // DENCLA_COMMON_POCKETBASECLIENT_H
