#ifndef DENCLA_COMMON_HTTPAPICLIENT_H
#define DENCLA_COMMON_HTTPAPICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

// Generic authenticated-JSON-HTTP-API plumbing, factored out of what every app's own API
// class (harbour-wetterstein's ApiClient, harbour-sailreader's MinifluxProvider,
// harbour-audiobookfish's ApiClient) reimplemented separately to some degree: a shared
// QNetworkAccessManager, a User-Agent header, an optional auth header applied to every
// request, a request timeout (aborts a hung reply instead of leaving it to dangle forever),
// and error/JSON-error-body classification. What's still entirely up to the caller: building
// endpoint URLs, parsing success-path JSON bodies, and deciding what to do about a 401 (this
// class only reports the failure, it doesn't know what "logged out" means for a given app).
class HttpApiClient : public QObject {
    Q_OBJECT
public:
    explicit HttpApiClient(QObject *parent = nullptr);

    // Sent as the User-Agent header on every request. Empty (the default) means don't set one.
    void setUserAgent(const QString &userAgent);
    // Sent as an extra header on every request (e.g. "Authorization"/"Bearer <token>", or
    // "X-Auth-Token"/<token>) - pass an empty headerName to stop sending one.
    void setAuthHeader(const QByteArray &headerName, const QByteArray &headerValue);
    // A request still running after this many ms is aborted (its reply's finished() signal
    // still fires, with an error - callers don't need to special-case a timeout separately
    // from any other network error). 0 disables the timeout. Defaults to 30000 (30s).
    void setRequestTimeoutMs(int ms);
    // The JSON object field this API's own error responses carry their human-readable message
    // under (e.g. Miniflux's "error_message"). checkAndHandleError() below prefers this over
    // the generic Qt network error string when the reply's body parses as JSON and has it.
    // Empty (the default) skips JSON error-body parsing entirely and always uses Qt's own
    // reply->errorString().
    void setErrorMessageJsonField(const QString &field);

    // requestTag is stashed on the request as QNetworkRequest::User and retrievable via
    // reply->request().attribute(QNetworkRequest::User) - lets a caller dispatch several
    // different request kinds off one shared QNetworkAccessManager::finished(QNetworkReply*)
    // signal instead of a fresh per-call-site lambda (see harbour-wetterstein's RadarManager
    // for the pattern this is lifted from). Ignored (left unset) when the given QVariant is
    // invalid, which is the default.
    QNetworkReply *get(const QUrl &url, const QVariant &requestTag = QVariant());
    QNetworkReply *put(const QUrl &url, const QByteArray &body, const QByteArray &contentType,
                        const QVariant &requestTag = QVariant());
    QNetworkReply *post(const QUrl &url, const QByteArray &body, const QByteArray &contentType,
                         const QVariant &requestTag = QVariant());

    // True (and *errorMessage set, if given) if reply finished with an error. Tries the JSON
    // error field configured via setErrorMessageJsonField() first, falls back to Qt's own
    // reply->errorString(). Also logs the error via qWarning(), prefixed with
    // "[HttpApiClient] <context>:", so call sites don't each need their own logging for this.
    // False (no side effects, reply's body left untouched) for a successful reply - callers
    // read the body themselves on success.
    bool checkAndHandleError(QNetworkReply *reply, const QString &context, QString *errorMessage = nullptr) const;

    // The pure decision behind checkAndHandleError() above, split out so it's testable without
    // a real QNetworkReply/QNetworkAccessManager: if jsonField is non-empty and body parses as
    // a JSON object containing it, returns that field's string value; otherwise returns
    // fallback unchanged (fallback is meant to be reply->errorString() at the real call site).
    static QString extractErrorMessage(const QByteArray &body, const QString &jsonField, const QString &fallback);

private:
    QNetworkRequest buildRequest(const QUrl &url) const;
    void armRequestTimeout(QNetworkReply *reply) const;

    QNetworkAccessManager *m_networkAccessManager;
    QString m_userAgent;
    QByteArray m_authHeaderName;
    QByteArray m_authHeaderValue;
    int m_requestTimeoutMs = 30000;
    QString m_errorMessageJsonField;
};

#endif // DENCLA_COMMON_HTTPAPICLIENT_H
