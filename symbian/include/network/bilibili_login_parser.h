#ifndef WILIWILI_SYMBIAN_BILIBILI_LOGIN_PARSER_H
#define WILIWILI_SYMBIAN_BILIBILI_LOGIN_PARSER_H

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include "model/login_session.h"

namespace wiliwili {

struct QrLoginPollCompat
{
    QrLoginPollCompat() : code(-1) {}

    int code;
    QString message;
    QString loginUrl;
    QString refreshToken;
    QByteArray cookieHeader;
};

class BilibiliLoginParser
{
public:
    static bool parseQrToken(
        const QByteArray &body,
        QString *url,
        QString *key,
        QString *errorText);

    static bool parseQrPoll(
        const QByteArray &body,
        QrLoginPollCompat *poll,
        QString *errorText);

    static bool parseProfile(
        const QByteArray &body,
        LoginProfileCompat *profile,
        int *apiCode,
        QString *errorText);
    static bool parseProfileStats(
        const QByteArray &body,
        int *following,
        int *follower,
        QString *errorText);
};

} // namespace wiliwili

#endif
