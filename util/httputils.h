#ifndef DENCLA_COMMON_HTTPUTILS_H
#define DENCLA_COMMON_HTTPUTILS_H

#include <QCoreApplication>
#include <QString>

// Single source of truth for the HTTP User-Agent an app's own requests should send - lifted
// from harbour-wetterstein's own httpuseragent.h, the only one of the three source apps that
// had ever set one at all (found during the shared library's own architecture review: neither
// harbour-sailreader's MinifluxProvider nor harbour-audiobookfish's ApiClient/DownloadManager
// ever set a User-Agent, before or after moving onto HttpApiClient/DownloadQueue). Uses
// QCoreApplication::applicationName()/applicationVersion() - every app already sets both in its
// own main() (the latter from its build's APP_VERSION) - so nothing here needs updating by hand,
// or duplicating per app, when either changes.
namespace HttpUtils {

inline QString defaultUserAgent() {
    return QCoreApplication::applicationName() + QStringLiteral("/") + QCoreApplication::applicationVersion();
}

}

#endif // DENCLA_COMMON_HTTPUTILS_H
