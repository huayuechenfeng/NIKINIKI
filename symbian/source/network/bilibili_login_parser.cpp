#include "network/bilibili_login_parser.h"

#include <cstdlib>

extern "C" {
#include "../../third_party/mongoose_compat/mg_json.h"
}

namespace wiliwili {

static QString loginJsonString(const struct mg_str &json, const char *path)
{
    char *value = mg_json_get_str(json, path);
    if (!value)
        return QString();
    const QString result = QString::fromUtf8(value);
    std::free(value);
    return result;
}

static bool loginJsonNumber(
    const struct mg_str &json,
    const char *path,
    double *value)
{
    return mg_json_get_num(json, path, value);
}

static bool rootData(
    const QByteArray &body,
    struct mg_str *root,
    struct mg_str *data,
    QString *errorText,
    int *apiCode = 0)
{
    if (apiCode)
        *apiCode = -9999;
    if (body.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("empty response");
        return false;
    }
    *root = mg_str_n(body.constData(), static_cast<size_t>(body.size()));
    double code = -9999.0;
    const bool hasCode = loginJsonNumber(*root, "$.code", &code);
    if (hasCode && apiCode)
        *apiCode = static_cast<int>(code);
    if (!hasCode || static_cast<int>(code) != 0) {
        if (errorText) {
            const QString message = loginJsonString(*root, "$.message");
            *errorText = message.isEmpty()
                ? QString::fromLatin1("Bilibili API error %1")
                      .arg(static_cast<int>(code))
                : message;
        }
        return false;
    }
    *data = mg_json_get_tok(*root, "$.data");
    if (!data->buf || data->len < 2 || data->buf[0] != '{') {
        if (errorText)
            *errorText = QString::fromLatin1("missing data");
        return false;
    }
    return true;
}

bool BilibiliLoginParser::parseQrToken(
    const QByteArray &body,
    QString *url,
    QString *key,
    QString *errorText)
{
    if (!url || !key)
        return false;
    struct mg_str root;
    struct mg_str data;
    if (!rootData(body, &root, &data, errorText))
        return false;
    *url = loginJsonString(data, "$.url");
    *key = loginJsonString(data, "$.qrcode_key");
    if (url->isEmpty() || key->isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("incomplete QR token");
        return false;
    }
    return true;
}

bool BilibiliLoginParser::parseQrPoll(
    const QByteArray &body,
    QrLoginPollCompat *poll,
    QString *errorText)
{
    if (!poll)
        return false;
    struct mg_str root;
    struct mg_str data;
    if (!rootData(body, &root, &data, errorText))
        return false;
    double code = -1.0;
    if (!loginJsonNumber(data, "$.code", &code)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing login state");
        return false;
    }
    poll->code = static_cast<int>(code);
    poll->message = loginJsonString(data, "$.message");
    poll->loginUrl = loginJsonString(data, "$.url");
    poll->refreshToken = loginJsonString(data, "$.refresh_token");

    // Some passport-compatible endpoints return cookies in the JSON body
    // instead of (or in addition to) multiple Set-Cookie headers.
    const struct mg_str cookies =
        mg_json_get_tok(data, "$.cookie_info.cookies");
    size_t offset = 0;
    struct mg_str item;
    while (cookies.buf && cookies.len >= 2 &&
           (offset = mg_json_next(cookies, offset, 0, &item)) != 0) {
        const QString name = loginJsonString(item, "$.name");
        const QString value = loginJsonString(item, "$.value");
        if (name.isEmpty() || value.isEmpty())
            continue;
        if (!poll->cookieHeader.isEmpty())
            poll->cookieHeader += "; ";
        poll->cookieHeader += name.toLatin1();
        poll->cookieHeader += '=';
        poll->cookieHeader += value.toUtf8();
    }
    return true;
}

bool BilibiliLoginParser::parseProfile(
    const QByteArray &body,
    LoginProfileCompat *profile,
    int *apiCode,
    QString *errorText)
{
    if (!profile)
        return false;
    struct mg_str root;
    struct mg_str data;
    if (!rootData(body, &root, &data, errorText, apiCode))
        return false;

    LoginProfileCompat parsed;
    double number = 0.0;
    bool isLogin = true;
    if (mg_json_get_bool(data, "$.isLogin", &isLogin) && !isLogin) {
        if (errorText)
            *errorText = QString::fromLatin1("not logged in");
        return false;
    }
    parsed.name = loginJsonString(data, "$.name");
    if (parsed.name.isEmpty())
        parsed.name = loginJsonString(data, "$.uname");
    parsed.face = loginJsonString(data, "$.face");
    parsed.sign = loginJsonString(data, "$.sign");
    if (parsed.sign.isEmpty())
        parsed.sign = loginJsonString(data, "$.signature");
    if (loginJsonNumber(data, "$.mid", &number))
        parsed.mid = static_cast<quint64>(number);
    if (loginJsonNumber(data, "$.level", &number) ||
        loginJsonNumber(data, "$.level_info.current_level", &number))
        parsed.level = static_cast<int>(number);
    if (loginJsonNumber(data, "$.coins", &number) ||
        loginJsonNumber(data, "$.money", &number))
        parsed.coins = number;
    if (parsed.mid == 0 || parsed.name.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("incomplete profile");
        return false;
    }
    *profile = parsed;
    return true;
}

bool BilibiliLoginParser::parseProfileStats(
    const QByteArray &body,
    int *following,
    int *follower,
    QString *errorText)
{
    if (!following || !follower)
        return false;
    struct mg_str root;
    struct mg_str data;
    if (!rootData(body, &root, &data, errorText))
        return false;
    double number = 0.0;
    if (!loginJsonNumber(data, "$.following", &number)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing following");
        return false;
    }
    *following = static_cast<int>(number);
    if (!loginJsonNumber(data, "$.follower", &number)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing follower");
        return false;
    }
    *follower = static_cast<int>(number);
    return true;
}

} // namespace wiliwili
