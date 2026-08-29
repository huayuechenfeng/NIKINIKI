#ifndef WILIWILI_SYMBIAN_LOGIN_SESSION_H
#define WILIWILI_SYMBIAN_LOGIN_SESSION_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace wiliwili {

struct LoginProfileCompat
{
    LoginProfileCompat()
        : mid(0), level(0), coins(0.0), follower(0), following(0)
    {
    }

    quint64 mid;
    QString name;
    QString face;
    QString sign;
    int level;
    double coins;
    int follower;
    int following;
};

class LoginSession
{
public:
    static QByteArray cookieHeader();
    // Cookie header for ordinary read requests.  It keeps a stable anonymous
    // device identity when signed out and merges it with the authenticated
    // cookie jar after QR login.  Authentication state remains in
    // cookieHeader(), so clear() can safely remove only credentials.
    static QByteArray requestCookieHeader();
    static QByteArray newQrDeviceCookieHeader(QByteArray *uuid);
    static QString refreshToken();
    static bool isLoggedIn();
    static QByteArray cookieValue(const QByteArray &name);

    static void save(
        const QByteArray &cookieHeader,
        const QString &refreshToken);
    static void clear();

    static QByteArray cookiesFromQrResponse(
        const QByteArray &setCookieHeader,
        const QByteArray &pollUuid);
};

} // namespace wiliwili

#endif
