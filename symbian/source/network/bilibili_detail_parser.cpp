#include "network/bilibili_detail_parser.h"

#include <cstdlib>

extern "C" {
#include "../../third_party/mongoose_compat/mg_json.h"
}

namespace wiliwili {

static bool detailJsonNumber(
    const struct mg_str &json,
    const char *path,
    double *value)
{
    return mg_json_get_num(json, path, value);
}

static QString detailJsonString(
    const struct mg_str &json,
    const char *path)
{
    char *value = mg_json_get_str(json, path);
    if (!value)
        return QString();
    const QString result = QString::fromUtf8(value);
    std::free(value);
    return result;
}

bool BilibiliDetailParser::parseVideoDetail(
    const QByteArray &body,
    VideoDetailCompat *detail,
    int *apiCode,
    QString *errorText,
    QVector<RecommendVideoResultCompat> *related)
{
    if (!detail)
        return false;

    if (apiCode)
        *apiCode = -9999;
    if (errorText)
        errorText->clear();
    if (related)
        related->clear();
    if (body.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("empty response");
        return false;
    }

    const struct mg_str root = mg_str_n(
        body.constData(), static_cast<size_t>(body.size()));
    double number = -9999.0;
    if (!detailJsonNumber(root, "$.code", &number)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing API code");
        return false;
    }

    const int code = static_cast<int>(number);
    if (apiCode)
        *apiCode = code;
    if (code != 0) {
        if (errorText) {
            const QString message = detailJsonString(root, "$.message");
            *errorText = message.isEmpty()
                ? QString::fromLatin1("Bilibili API error %1").arg(code)
                : message;
        }
        return false;
    }

    const struct mg_str data = mg_json_get_tok(root, "$.data");
    if (!data.buf || data.len < 2 || data.buf[0] != '{') {
        if (errorText)
            *errorText = QString::fromLatin1("missing data");
        return false;
    }

    const struct mg_str wrappedView = mg_json_get_tok(data, "$.View");
    const struct mg_str view = wrappedView.buf ? wrappedView : data;

    VideoDetailCompat parsed;
    parsed.bvid = detailJsonString(view, "$.bvid");
    parsed.pic = detailJsonString(view, "$.pic");
    parsed.title = detailJsonString(view, "$.title");
    parsed.description = detailJsonString(view, "$.desc");
    parsed.owner.name = detailJsonString(view, "$.owner.name");

    if (detailJsonNumber(view, "$.aid", &number))
        parsed.aid = static_cast<quint64>(number);
    if (detailJsonNumber(view, "$.pubdate", &number))
        parsed.pubdate = static_cast<int>(number);
    if (detailJsonNumber(view, "$.duration", &number))
        parsed.duration = static_cast<int>(number);
    if (detailJsonNumber(view, "$.owner.mid", &number))
        parsed.owner.mid = static_cast<quint64>(number);
    if (detailJsonNumber(view, "$.stat.view", &number))
        parsed.stat.view = static_cast<int>(number);
    if (detailJsonNumber(view, "$.stat.danmaku", &number))
        parsed.stat.danmaku = static_cast<int>(number);
    if (detailJsonNumber(view, "$.stat.favorite", &number))
        parsed.stat.favorite = static_cast<int>(number);
    if (detailJsonNumber(view, "$.stat.coin", &number))
        parsed.stat.coin = static_cast<int>(number);
    if (detailJsonNumber(view, "$.stat.share", &number))
        parsed.stat.share = static_cast<int>(number);
    if (detailJsonNumber(view, "$.stat.like", &number))
        parsed.stat.like = static_cast<int>(number);
    if (detailJsonNumber(view, "$.stat.reply", &number))
        parsed.stat.reply = static_cast<int>(number);

    const struct mg_str pages = mg_json_get_tok(view, "$.pages");
    if (pages.buf && pages.len >= 2 && pages.buf[0] == '[') {
        size_t offset = 0;
        struct mg_str item;
        while (parsed.pages.size() < 64 &&
               (offset = mg_json_next(pages, offset, 0, &item)) != 0) {
            if (!item.buf || item.len < 2 || item.buf[0] != '{')
                continue;
            VideoDetailPageCompat page;
            if (detailJsonNumber(item, "$.cid", &number))
                page.cid = static_cast<quint64>(number);
            if (detailJsonNumber(item, "$.page", &number))
                page.page = static_cast<int>(number);
            if (detailJsonNumber(item, "$.duration", &number))
                page.duration = static_cast<int>(number);
            if (detailJsonNumber(item, "$.dimension.width", &number))
                page.width = static_cast<int>(number);
            if (detailJsonNumber(item, "$.dimension.height", &number))
                page.height = static_cast<int>(number);
            page.part = detailJsonString(item, "$.part");
            if (page.cid != 0)
                parsed.pages.append(page);
        }
    }

    if (parsed.bvid.isEmpty() || parsed.title.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("detail is incomplete");
        return false;
    }

    if (related) {
        const struct mg_str relatedItems =
            mg_json_get_tok(data, "$.Related");
        size_t offset = 0;
        struct mg_str item;
        while (related->size() < 10 &&
               (offset = mg_json_next(
                    relatedItems, offset, 0, &item)) != 0) {
            if (!item.buf || item.len < 2 || item.buf[0] != '{')
                continue;
            RecommendVideoResultCompat candidate;
            candidate.bvid = detailJsonString(item, "$.bvid");
            candidate.pic = detailJsonString(item, "$.pic");
            candidate.title = detailJsonString(item, "$.title");
            candidate.owner.name = detailJsonString(item, "$.owner.name");
            if (detailJsonNumber(item, "$.aid", &number))
                candidate.id = static_cast<quint64>(number);
            if (detailJsonNumber(item, "$.duration", &number))
                candidate.duration = static_cast<int>(number);
            if (detailJsonNumber(item, "$.stat.view", &number))
                candidate.stat.view = static_cast<int>(number);
            if (detailJsonNumber(item, "$.stat.danmaku", &number))
                candidate.stat.danmaku = static_cast<int>(number);
            if (!candidate.bvid.isEmpty() &&
                candidate.bvid != parsed.bvid &&
                !candidate.title.isEmpty()) {
                related->append(candidate);
            }
        }
    }

    *detail = parsed;
    return true;
}

} // namespace wiliwili
