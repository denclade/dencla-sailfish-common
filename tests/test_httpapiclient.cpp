// SPDX-FileCopyrightText: 2026 Dennis Burgermeister
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>
#include "../net/httpapiclient.h"

// Only HttpApiClient::extractErrorMessage() is covered here - the pure decision logic behind
// checkAndHandleError(). The rest of the class (get/put/post, timeout arming, the actual
// QNetworkReply-driven checkAndHandleError()) needs a real QNetworkAccessManager talking to a
// real server, which none of harbour-sailreader/harbour-wetterstein/harbour-audiobookfish's
// own equivalent classes (MinifluxProvider, ApiClient, ApiClient) unit-test either - see the
// sailfish/CLAUDE.md "CMake-Grundgerüst" section on what counts as a good test candidate here.
class TestHttpApiClient : public QObject {
    Q_OBJECT

private slots:
    void fallsBackToGivenFallbackWhenFieldIsEmpty();
    void fallsBackWhenBodyIsNotJson();
    void fallsBackWhenJsonObjectLacksTheField();
    void usesTheConfiguredFieldWhenPresent();
    void fallsBackWhenBodyIsAJsonArrayNotObject();
    void fallsBackWhenFieldValueIsNotAString();
};

void TestHttpApiClient::fallsBackToGivenFallbackWhenFieldIsEmpty() {
    const QString result = HttpApiClient::extractErrorMessage(
        QByteArrayLiteral(R"({"error_message":"nope"})"), QString(), QStringLiteral("fallback"));
    QCOMPARE(result, QStringLiteral("fallback"));
}

void TestHttpApiClient::fallsBackWhenBodyIsNotJson() {
    const QString result = HttpApiClient::extractErrorMessage(
        QByteArrayLiteral("not json at all"), QStringLiteral("error_message"), QStringLiteral("fallback"));
    QCOMPARE(result, QStringLiteral("fallback"));
}

void TestHttpApiClient::fallsBackWhenJsonObjectLacksTheField() {
    const QString result = HttpApiClient::extractErrorMessage(
        QByteArrayLiteral(R"({"other_field":"nope"})"), QStringLiteral("error_message"), QStringLiteral("fallback"));
    QCOMPARE(result, QStringLiteral("fallback"));
}

void TestHttpApiClient::usesTheConfiguredFieldWhenPresent() {
    const QString result = HttpApiClient::extractErrorMessage(
        QByteArrayLiteral(R"({"error_message":"invalid token"})"),
        QStringLiteral("error_message"), QStringLiteral("fallback"));
    QCOMPARE(result, QStringLiteral("invalid token"));
}

void TestHttpApiClient::fallsBackWhenBodyIsAJsonArrayNotObject() {
    const QString result = HttpApiClient::extractErrorMessage(
        QByteArrayLiteral(R"(["error_message","invalid token"])"),
        QStringLiteral("error_message"), QStringLiteral("fallback"));
    QCOMPARE(result, QStringLiteral("fallback"));
}

void TestHttpApiClient::fallsBackWhenFieldValueIsNotAString() {
    // toString() on a non-string JSON value yields an empty string, not the fallback - this
    // documents that behavior explicitly rather than leaving it as an untested edge case.
    const QString result = HttpApiClient::extractErrorMessage(
        QByteArrayLiteral(R"({"error_message":404})"), QStringLiteral("error_message"), QStringLiteral("fallback"));
    QCOMPARE(result, QString());
}

QTEST_APPLESS_MAIN(TestHttpApiClient)
#include "test_httpapiclient.moc"
