#include "network/bilibili_wbi.h"

#include <cstdlib>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPair>
#include <QtCore/QUrl>

extern "C" {
#include "../../third_party/mongoose_compat/mg_json.h"
}

namespace wiliwili {

static QString wbiString(const struct mg_str &json, const char *path)
{
    char *value = mg_json_get_str(json, path);
    if (!value)
        return QString();
    const QString result = QString::fromUtf8(value);
    std::free(value);
    return result;
}

static QString keyFromUrl(const QString &url)
{
    const int slash = url.lastIndexOf(QLatin1Char('/'));
    const int dot = url.lastIndexOf(QLatin1Char('.'));
    if (slash < 0 || dot <= slash)
        return QString();
    return url.mid(slash + 1, dot - slash - 1);
}

bool BilibiliWbi::requiresSigning(const QString &endpoint)
{
    return endpoint.contains(QString::fromLatin1("/wbi/")) ||
           endpoint.contains(QString::fromLatin1("/x/space/arc/search")) ||
           endpoint.contains(QString::fromLatin1("/x/web-interface/search/type"));
}

bool BilibiliWbi::parseMixinKey(
    const QByteArray &body, QString *mixinKey, QString *errorText)
{
    if (!mixinKey || body.isEmpty())
        return false;
    const struct mg_str root = mg_str_n(
        body.constData(), static_cast<size_t>(body.size()));
    double code = -9999.0;
    if (!mg_json_get_num(root, "$.code", &code) ||
        static_cast<int>(code) != 0) {
        if (errorText)
            *errorText = QString::fromLatin1("nav API %1")
                .arg(static_cast<int>(code));
        return false;
    }
    const QString imgKey = keyFromUrl(
        wbiString(root, "$.data.wbi_img.img_url"));
    const QString subKey = keyFromUrl(
        wbiString(root, "$.data.wbi_img.sub_url"));
    const QString raw = imgKey + subKey;
    static const int table[64] = {
        46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35,
        27, 43, 5, 49, 33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13,
        37, 48, 7, 16, 24, 55, 40, 61, 26, 17, 0, 1, 60, 51, 30, 4,
        22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11, 36, 20, 34, 44, 52
    };
    if (raw.size() < 64) {
        if (errorText)
            *errorText = QString::fromLatin1("incomplete WBI keys");
        return false;
    }
    QString mixed;
    int index;
    for (index = 0; index < 32; ++index)
        mixed.append(raw.at(table[index]));
    *mixinKey = mixed;
    return true;
}

QString BilibiliWbi::signUrl(
    const QString &endpoint, const QString &mixinKey)
{
    if (mixinKey.isEmpty())
        return endpoint;
    const int question = endpoint.indexOf(QLatin1Char('?'));
    const QString base = question < 0 ? endpoint : endpoint.left(question);
    const QByteArray encodedQuery = question < 0
        ? QByteArray() : endpoint.mid(question + 1).toLatin1();
    const QList<QByteArray> fields = encodedQuery.split('&');
    QMap<QString, QString> parameters;
    int index;
    for (index = 0; index < fields.size(); ++index) {
        const QByteArray field = fields.at(index);
        const int separator = field.indexOf('=');
        if (separator < 0)
            continue;
        const QString name = QUrl::fromPercentEncoding(field.left(separator));
        QString value = QUrl::fromPercentEncoding(field.mid(separator + 1));
        value.remove(QLatin1Char('!'));
        value.remove(QLatin1Char('\''));
        value.remove(QLatin1Char('('));
        value.remove(QLatin1Char(')'));
        value.remove(QLatin1Char('*'));
        parameters.insert(name, value);
    }
    parameters.insert(
        QString::fromLatin1("wts"),
        QString::number(QDateTime::currentDateTime().toTime_t()));

    QByteArray canonical;
    QMap<QString, QString>::const_iterator iterator = parameters.constBegin();
    for (; iterator != parameters.constEnd(); ++iterator) {
        if (!canonical.isEmpty())
            canonical += '&';
        canonical += QUrl::toPercentEncoding(iterator.key());
        canonical += '=';
        canonical += QUrl::toPercentEncoding(iterator.value());
    }
    const QByteArray digest = QCryptographicHash::hash(
        canonical + mixinKey.toLatin1(), QCryptographicHash::Md5).toHex();
    return base + QString::fromLatin1("?") +
        QString::fromLatin1(canonical) +
        QString::fromLatin1("&w_rid=") + QString::fromLatin1(digest);
}

} // namespace wiliwili
