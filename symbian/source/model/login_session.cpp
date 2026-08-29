#include "model/login_session.h"

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QSettings>

namespace wiliwili {

static QByteArray rawCookieValue(
    const QByteArray &headers,
    const QByteArray &name)
{
    const QByteArray needle = name + '=';
    const QByteArray lowerHeaders = headers.toLower();
    const QByteArray lowerNeedle = needle.toLower();
    int position = 0;
    while ((position = lowerHeaders.indexOf(lowerNeedle, position)) >= 0) {
        if (position == 0 ||
            headers.at(position - 1) == ' ' ||
            headers.at(position - 1) == '\t' ||
            headers.at(position - 1) == ',' ||
            headers.at(position - 1) == '\r' ||
            headers.at(position - 1) == '\n' ||
            headers.at(position - 1) == ':' ||
            headers.at(position - 1) == '?' ||
            headers.at(position - 1) == '&' ||
            headers.at(position - 1) == ';' ||
            headers.at(position - 1) == '"' ||
            headers.at(position - 1) == '\'') {
            const int valueStart = position + needle.size();
            int valueEnd = headers.indexOf(';', valueStart);
            const int commaEnd = headers.indexOf(',', valueStart);
            const int ampersandEnd = headers.indexOf('&', valueStart);
            const int quoteEnd = headers.indexOf('"', valueStart);
            const int carriageReturnEnd = headers.indexOf('\r', valueStart);
            const int lineFeedEnd = headers.indexOf('\n', valueStart);
            if (valueEnd < 0 || (commaEnd >= 0 && commaEnd < valueEnd))
                valueEnd = commaEnd;
            if (valueEnd < 0 ||
                (ampersandEnd >= 0 && ampersandEnd < valueEnd))
                valueEnd = ampersandEnd;
            if (valueEnd < 0 || (quoteEnd >= 0 && quoteEnd < valueEnd))
                valueEnd = quoteEnd;
            if (valueEnd < 0 ||
                (carriageReturnEnd >= 0 && carriageReturnEnd < valueEnd))
                valueEnd = carriageReturnEnd;
            if (valueEnd < 0 ||
                (lineFeedEnd >= 0 && lineFeedEnd < valueEnd))
                valueEnd = lineFeedEnd;
            if (valueEnd < 0)
                valueEnd = headers.size();
            return headers.mid(valueStart, valueEnd - valueStart).trimmed();
        }
        position += lowerNeedle.size();
    }
    return QByteArray();
}

static QByteArray randomHex(int length)
{
    static bool seeded = false;
    if (!seeded) {
        seeded = true;
        qsrand(QDateTime::currentDateTime().toTime_t() ^
               static_cast<uint>(reinterpret_cast<quintptr>(&seeded)));
    }
    static const char digits[] = "0123456789ABCDEF";
    QByteArray result;
    result.reserve(length);
    int index;
    for (index = 0; index < length; ++index)
        result.append(digits[qrand() & 15]);
    return result;
}

static QByteArray persistentDeviceValue(
    const QString &key,
    const QByteArray &generated)
{
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    QByteArray value = settings.value(key).toByteArray();
    if (value.isEmpty()) {
        value = generated;
        settings.setValue(key, value);
        settings.sync();
    }
    return value;
}

static void appendCookie(
    QByteArray *header,
    const QByteArray &name,
    const QByteArray &value)
{
    if (!header || name.isEmpty() || value.isEmpty())
        return;
    if (!header->isEmpty())
        *header += "; ";
    *header += name;
    *header += '=';
    *header += value;
}

static void appendCookieIfMissing(
    QByteArray *header,
    const QByteArray &name,
    const QByteArray &value)
{
    if (!header || value.isEmpty() ||
        !rawCookieValue(*header, name).isEmpty())
        return;
    appendCookie(header, name, value);
}

static QByteArray requestUuid()
{
    return persistentDeviceValue(
        QString::fromLatin1("device/request_uuid"),
        randomHex(8) + '-' + randomHex(4) + '-' + randomHex(4) + '-' +
        randomHex(4) + '-' + randomHex(17) + "infoc");
}

QByteArray LoginSession::cookieHeader()
{
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    return settings.value(QString::fromLatin1("auth/cookies")).toByteArray();
}

QByteArray LoginSession::requestCookieHeader()
{
    QByteArray result = cookieHeader().trimmed();
    appendCookieIfMissing(
        &result, "buvid3",
        persistentDeviceValue(
            QString::fromLatin1("device/request_buvid3"), randomHex(32)));
    appendCookieIfMissing(&result, "_uuid", requestUuid());

    // Bilibili's web APIs commonly use DedeUserID=0 for an anonymous device.
    // Do not inject it into an authenticated jar whose UID is temporarily
    // being recovered from /nav after QR login.
    if (!isLoggedIn())
        appendCookieIfMissing(&result, "DedeUserID", "0");
    return result;
}

QByteArray LoginSession::newQrDeviceCookieHeader(QByteArray *pollUuid)
{
    // Deliberately generate this UUID for every poll request. This mirrors
    // upstream BilibiliClient::get_login_info_v2(), which captures the UUID
    // belonging to the successful response and later saves it as _uuid.
    const QByteArray uuid =
        randomHex(8) + '-' + randomHex(4) + '-' + randomHex(4) + '-' +
        randomHex(4) + '-' + randomHex(17) + "infoc";
    if (pollUuid)
        *pollUuid = uuid;
    const QByteArray deviceId = persistentDeviceValue(
        QString::fromLatin1("device/id"), randomHex(32));

    QByteArray result;
    appendCookie(&result, "appkey", "aa1e74ee4874176e");
    appendCookie(&result, "mobi_app", "pc_electron");
    appendCookie(&result, "device", "mac");
    appendCookie(&result, "innersign", "0");
    appendCookie(&result, "buvid3", uuid);
    appendCookie(&result, "device_id", deviceId);
    appendCookie(&result, "device_name", "NIKINIKI - Symbian");
    return result;
}

QString LoginSession::refreshToken()
{
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    return settings.value(QString::fromLatin1("auth/refresh_token")).toString();
}

QByteArray LoginSession::cookieValue(const QByteArray &name)
{
    return rawCookieValue(cookieHeader(), name);
}

bool LoginSession::isLoggedIn()
{
    const QByteArray cookies = cookieHeader();
    // SESSDATA is the actual authentication credential. DedeUserID is
    // normally returned with it, but can be reconstructed from /nav after a
    // successful login on proxy/legacy passport responses.
    return !rawCookieValue(cookies, "SESSDATA").isEmpty();
}

void LoginSession::save(
    const QByteArray &cookies,
    const QString &token)
{
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    settings.setValue(QString::fromLatin1("auth/cookies"), cookies);
    settings.setValue(QString::fromLatin1("auth/refresh_token"), token);
    settings.sync();
}

void LoginSession::clear()
{
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    settings.remove(QString::fromLatin1("auth"));
    settings.sync();
}

QByteArray LoginSession::cookiesFromQrResponse(
    const QByteArray &setCookieHeader,
    const QByteArray &pollUuid)
{
    // Exact upstream get_login_info_v2 semantics: start a fresh authenticated
    // cookie jar with _uuid and a new buvid3, then append only cookies supplied
    // by the successful poll response. data.url and JSON cookie fallbacks are
    // intentionally not interpreted as authentication credentials.
    static const char *names[] = {
        "SESSDATA",
        "bili_jct",
        "DedeUserID",
        "DedeUserID__ckMd5",
        "sid",
        "buvid3",
        "buvid4",
        "buvid_fp",
        "b_nut",
        "_uuid",
        "CURRENT_FNVAL",
        "CURRENT_QUALITY"
    };

    QByteArray result;
    appendCookie(&result, "_uuid", pollUuid);
    appendCookie(&result, "buvid3", randomHex(32));
    int index;
    for (index = 0;
         index < static_cast<int>(sizeof(names) / sizeof(names[0]));
        ++index) {
        const QByteArray name(names[index]);
        if (name == "_uuid" || name == "buvid3")
            continue;
        const QByteArray value = rawCookieValue(setCookieHeader, name);
        if (value.isEmpty())
            continue;
        qDebug() << "WW:QR_COOKIE_CAPTURE" << name << value.size();
        appendCookie(&result, name, value);
    }
    return result;
}

} // namespace wiliwili
