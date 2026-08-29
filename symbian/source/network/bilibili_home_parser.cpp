#include "network/bilibili_home_parser.h"

#include <cstdlib>

extern "C" {
#include "../../third_party/mongoose_compat/mg_json.h"
}

namespace wiliwili {

static bool jsonNumber(
    const struct mg_str &json,
    const char *path,
    double *value)
{
    return mg_json_get_num(json, path, value);
}

static QString jsonString(
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

static bool readApiRoot(
    const QByteArray &body,
    struct mg_str *root,
    int *apiCode,
    QString *errorText)
{
    if (apiCode)
        *apiCode = -9999;
    if (errorText)
        errorText->clear();
    if (body.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("empty response");
        return false;
    }

    *root = mg_str_n(body.constData(), static_cast<size_t>(body.size()));
    double codeValue = -9999.0;
    if (!jsonNumber(*root, "$.code", &codeValue)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing API code");
        return false;
    }
    const int code = static_cast<int>(codeValue);
    if (apiCode)
        *apiCode = code;
    if (code != 0) {
        if (errorText) {
            const QString message = jsonString(*root, "$.message");
            *errorText = message.isEmpty()
                ? QString::fromLatin1("Bilibili API error %1").arg(code)
                : message;
        }
        return false;
    }
    return true;
}

static void appendVideoCard(
    const struct mg_str &item,
    QVector<RecommendVideoResultCompat> *cards)
{
    RecommendVideoResultCompat card;
    double number = 0.0;
    if (jsonNumber(item, "$.aid", &number) ||
        jsonNumber(item, "$.id", &number))
        card.id = static_cast<quint64>(number);
    card.bvid = jsonString(item, "$.bvid");
    card.pic = jsonString(item, "$.pic");
    card.title = jsonString(item, "$.title");
    if (jsonNumber(item, "$.duration", &number))
        card.duration = static_cast<int>(number);
    if (jsonNumber(item, "$.pubdate", &number))
        card.pubdate = static_cast<int>(number);
    if (jsonNumber(item, "$.owner.mid", &number))
        card.owner.mid = static_cast<quint64>(number);
    card.owner.name = jsonString(item, "$.owner.name");
    if (jsonNumber(item, "$.stat.view", &number))
        card.stat.view = static_cast<int>(number);
    if (jsonNumber(item, "$.stat.danmaku", &number))
        card.stat.danmaku = static_cast<int>(number);
    if (jsonNumber(item, "$.is_followed", &number))
        card.is_followed = static_cast<int>(number);
    card.recommendationReason = jsonString(item, "$.rcmd_reason.content");
    if (card.recommendationReason.isEmpty())
        card.recommendationReason = jsonString(item, "$.rcmd_reason.reason");
    bool isAd = false;
    if (mg_json_get_bool(item, "$.business_info.is_ad", &isAd))
        card.isAdvertisement = isAd;
    if (!card.title.isEmpty() && !card.bvid.isEmpty())
        cards->append(card);
}

bool BilibiliHomeParser::parseHotsAll(
    const QByteArray &body,
    QVector<RecommendVideoResultCompat> *cards,
    int *apiCode,
    QString *errorText)
{
    if (!cards)
        return false;

    cards->clear();
    struct mg_str root;
    if (!readApiRoot(body, &root, apiCode, errorText))
        return false;

    const struct mg_str list = mg_json_get_tok(root, "$.data.list");
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        if (errorText)
            *errorText = QString::fromLatin1("missing data.list");
        return false;
    }

    size_t offset = 0;
    struct mg_str item;
    while (cards->size() < 30 &&
           (offset = mg_json_next(list, offset, 0, &item)) != 0) {
        if (!item.buf || item.len < 2 || item.buf[0] != '{')
            continue;

        appendVideoCard(item, cards);
    }

    if (cards->isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("data.list has no videos");
        return false;
    }
    return true;
}

bool BilibiliHomeParser::parseRecommend(
    const QByteArray &body,
    QVector<RecommendVideoResultCompat> *cards,
    int *apiCode,
    QString *errorText)
{
    if (!cards)
        return false;
    cards->clear();
    struct mg_str root;
    if (!readApiRoot(body, &root, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(root, "$.data.item");
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        if (errorText)
            *errorText = QString::fromLatin1("missing data.item");
        return false;
    }
    size_t offset = 0;
    struct mg_str item;
    while (cards->size() < 30 &&
           (offset = mg_json_next(list, offset, 0, &item)) != 0) {
        if (item.buf && item.len >= 2 && item.buf[0] == '{')
            appendVideoCard(item, cards);
    }
    if (cards->isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("data.item has no videos");
        return false;
    }
    return true;
}

bool BilibiliHomeParser::parseBangumi(
    const QByteArray &body,
    QVector<RecommendVideoResultCompat> *cards,
    int *apiCode,
    QString *errorText)
{
    if (!cards)
        return false;
    cards->clear();
    struct mg_str root;
    if (!readApiRoot(body, &root, apiCode, errorText))
        return false;
    const struct mg_str modules = mg_json_get_tok(root, "$.data.modules");
    size_t moduleOffset = 0;
    struct mg_str module;
    while (cards->size() < 12 &&
           (moduleOffset = mg_json_next(
                modules, moduleOffset, 0, &module)) != 0) {
        if (!module.buf || module.len < 2 || module.buf[0] != '{')
            continue;
        const QString moduleTitle = jsonString(module, "$.title");
        const struct mg_str items = mg_json_get_tok(module, "$.items");
        size_t itemOffset = 0;
        struct mg_str item;
        while (cards->size() < 12 &&
               (itemOffset = mg_json_next(
                    items, itemOffset, 0, &item)) != 0) {
            RecommendVideoResultCompat card;
            double number = 0.0;
            card.kind = BangumiHomeCard;
            card.title = jsonString(item, "$.title");
            card.pic = jsonString(item, "$.cover");
            card.badge = jsonString(item, "$.bottom_right_badge.text");
            card.owner.name = jsonString(item, "$.desc");
            if (card.owner.name.isEmpty())
                card.owner.name = moduleTitle;
            if (jsonNumber(item, "$.season_id", &number))
                card.id = static_cast<quint64>(number);
            if (!card.title.isEmpty() && !card.pic.isEmpty())
                cards->append(card);
        }
    }
    if (cards->isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("bangumi modules are empty");
        return false;
    }
    return true;
}

bool BilibiliHomeParser::parseLive(
    const QByteArray &body,
    QVector<RecommendVideoResultCompat> *cards,
    int *apiCode,
    QString *errorText)
{
    if (!cards)
        return false;
    cards->clear();
    struct mg_str root;
    if (!readApiRoot(body, &root, apiCode, errorText))
        return false;
    const struct mg_str list =
        mg_json_get_tok(root, "$.data.recommend_room_list");
    size_t offset = 0;
    struct mg_str item;
    while (cards->size() < 12 &&
           (offset = mg_json_next(list, offset, 0, &item)) != 0) {
        RecommendVideoResultCompat card;
        double number = 0.0;
        card.kind = LiveHomeCard;
        card.title = jsonString(item, "$.title");
        card.pic = jsonString(item, "$.cover");
        if (card.pic.isEmpty())
            card.pic = jsonString(item, "$.keyframe");
        card.owner.name = jsonString(item, "$.uname");
        card.badge = jsonString(item, "$.area_v2_name");
        if (jsonNumber(item, "$.roomid", &number))
            card.id = static_cast<quint64>(number);
        if (jsonNumber(item, "$.online", &number))
            card.stat.view = static_cast<int>(number);
        if (!card.title.isEmpty() && !card.pic.isEmpty())
            cards->append(card);
    }
    if (cards->isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("live list is empty");
        return false;
    }
    return true;
}

} // namespace wiliwili
