#include "network/bilibili_section_parser.h"

#include <cstdlib>
#include <QtCore/QDateTime>

extern "C" {
#include "../../third_party/mongoose_compat/mg_json.h"
}

namespace wiliwili {

static QString sectionJsonString(
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

static QString sectionJsonScalarText(
    const struct mg_str &json,
    const char *path)
{
    const struct mg_str token = mg_json_get_tok(json, path);
    if (!token.buf || token.len == 0)
        return QString();
    if (token.buf[0] == '"')
        return sectionJsonString(json, path);
    const QString value = QString::fromLatin1(
        token.buf, static_cast<int>(token.len)).trimmed();
    return value == QString::fromLatin1("null") ? QString() : value;
}

static QString firstString(
    const struct mg_str &json,
    const char *first,
    const char *second,
    const char *third)
{
    QString value = sectionJsonString(json, first);
    if (value.isEmpty() && second)
        value = sectionJsonString(json, second);
    if (value.isEmpty() && third)
        value = sectionJsonString(json, third);
    return value;
}

static QString firstString4(
    const struct mg_str &json,
    const char *first,
    const char *second,
    const char *third,
    const char *fourth)
{
    QString value = firstString(json, first, second, third);
    if (value.isEmpty() && fourth)
        value = sectionJsonString(json, fourth);
    return value;
}

static bool sectionJsonNumber(
    const struct mg_str &json,
    const char *path,
    double *value)
{
    return mg_json_get_num(json, path, value);
}

static QString sectionTimeText(quint64 timestamp)
{
    if (timestamp == 0)
        return QString();
    return QDateTime::fromTime_t(static_cast<uint>(timestamp)).toString(
        QString::fromLatin1("MM-dd hh:mm"));
}

static QString unpackMessageText(const QString &packed)
{
    if (packed.isEmpty())
        return packed;
    const QByteArray bytes = packed.toUtf8();
    const struct mg_str nested = mg_str_n(
        bytes.constData(), static_cast<size_t>(bytes.size()));
    const QString text = sectionJsonString(nested, "$.content");
    return text.isEmpty() ? packed : text;
}

static bool sectionRoot(
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
    if (!mg_json_get_num(*root, "$.code", &codeValue)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing API code");
        return false;
    }
    const int code = static_cast<int>(codeValue);
    if (apiCode)
        *apiCode = code;
    if (code != 0) {
        if (errorText) {
            *errorText = sectionJsonString(*root, "$.message");
            if (errorText->isEmpty())
                *errorText = QString::fromLatin1("API %1").arg(code);
        }
        return false;
    }
    return true;
}

static QString cleanSectionText(QString value)
{
    value.replace(QChar('\r'), QChar(' '));
    value.replace(QChar('\n'), QChar(' '));
    value.replace(QChar('\t'), QChar(' '));
    return value.simplified();
}

static QString messageBadge(const QString &rawType)
{
    const QString type = rawType.toLower();
    if (type.contains(QString::fromLatin1("reply")))
        return QString::fromUtf8("回复");
    if (type.contains(QString::fromLatin1("at")))
        return QString::fromUtf8("@ 提及");
    if (type.contains(QString::fromLatin1("like")))
        return QString::fromUtf8("赞");
    if (type.contains(QString::fromLatin1("video")))
        return QString::fromUtf8("视频");
    if (type.contains(QString::fromLatin1("dynamic")))
        return QString::fromUtf8("动态");
    if (type.contains(QString::fromLatin1("article")))
        return QString::fromUtf8("图文");
    return rawType;
}

static void appendSectionItem(
    QVector<ContentItemCompat> *items,
    const ContentItemCompat &candidate)
{
    if (!items || candidate.title.trimmed().isEmpty())
        return;
    ContentItemCompat item = candidate;
    item.title = item.title.trimmed();
    item.subtitle = item.subtitle.trimmed();
    item.description = item.description.trimmed();
    item.id = item.id.trimmed();
    items->append(item);
}

// The current feed/all API stores modules as an array in some responses and
// as an object in older desktop responses.  Keep the extraction in one place
// so either wire shape produces the same compact Symbian card.
static void parseDynamicModuleToken(
    const struct mg_str &module,
    ContentItemCompat *result)
{
    if (!result)
        return;

    const struct mg_str author = mg_json_get_tok(
        module, "$.module_author");
    if (author.buf) {
        result->title = cleanSectionText(firstString4(
            author,
            "$.name",
            "$.user.name",
            "$.uname",
            "$.user.uname"));
        result->picture = firstString4(
            author,
            "$.face",
            "$.user.face",
            "$.avatar.remote.url",
            "$.user.avatar");
        result->subtitle = cleanSectionText(firstString4(
            author,
            "$.pub_time",
            "$.pub_text",
            "$.pub_action",
            "$.user.pub_time"));
        double number = 0.0;
        if (sectionJsonNumber(author, "$.pub_ts", &number) ||
            sectionJsonNumber(author, "$.user.pub_ts", &number)) {
            result->timestamp = static_cast<quint64>(number);
            if (result->subtitle.isEmpty())
                result->subtitle = sectionTimeText(result->timestamp);
        }
    }

    const struct mg_str dynamic = mg_json_get_tok(
        module, "$.module_dynamic");
    if (dynamic.buf) {
        result->description = cleanSectionText(firstString4(
            dynamic,
            "$.desc.text",
            "$.description",
            "$.major.archive.desc",
            "$.major.opus.summary"));
        result->mediaTitle = cleanSectionText(firstString4(
            dynamic,
            "$.major.archive.title",
            "$.major.opus.title",
            "$.major.article.title",
            "$.major.live_rcmd.title"));
        if (result->mediaTitle.isEmpty())
            result->mediaTitle = cleanSectionText(firstString4(
                dynamic,
                "$.major.live_rcmd.content.title",
                "$.major.live_rcmd.card_info.live_play_info.title",
                "$.major.live.card_info.live_play_info.title",
                "$.major.live.title"));
        const QString bvid = cleanSectionText(firstString4(
            dynamic,
            "$.major.archive.bvid",
            "$.major.pgc.bvid",
            "$.major.ugc_season.bvid",
            "$.bvid"));
        if (!bvid.isEmpty()) {
            result->kind = VideoContentItem;
            result->id = bvid;
            result->badge = QString::fromUtf8("视频");
        }
        result->mediaPicture = firstString4(
            dynamic,
            "$.major.archive.cover",
            "$.major.opus.pics[0].url",
            "$.major.draw.items[0].src",
            "$.major.article.covers[0]");
        if (result->mediaPicture.isEmpty())
            result->mediaPicture = firstString4(
                dynamic,
                "$.major.live_rcmd.content.cover",
                "$.major.live_rcmd.cover",
                "$.major.live.cover",
                "$.major.live_rcmd.card_info.live_play_info.cover");
        if (result->mediaPicture.isEmpty())
            result->mediaPicture = sectionJsonString(dynamic, "$.pic");
        if (result->badge.isEmpty() && !result->mediaPicture.isEmpty())
            result->badge = QString::fromUtf8("图文");
        if (result->badge.isEmpty() &&
            firstString(dynamic, "$.major.type", "$.type", 0)
                .toLower().contains(QString::fromLatin1("live")))
            result->badge = QString::fromUtf8("直播");
        if (result->badge.isEmpty() && !result->mediaTitle.isEmpty())
            result->badge = QString::fromUtf8("图文");
    }

    const struct mg_str moduleDescription = mg_json_get_tok(
        module, "$.module_desc");
    if (result->description.isEmpty() && moduleDescription.buf)
        result->description = cleanSectionText(firstString4(
            moduleDescription, "$.text", "$.desc.text", "$.description", 0));

    const struct mg_str stat = mg_json_get_tok(
        module, "$.module_stat");
    double number = 0.0;
    if (stat.buf && sectionJsonNumber(stat, "$.like.count", &number))
        result->count = static_cast<int>(number);
    if (stat.buf && sectionJsonNumber(stat, "$.comment.count", &number))
        result->replyCount = static_cast<int>(number);
}

static void parseDynamicModules(
    const struct mg_str &modules,
    ContentItemCompat *result)
{
    if (!result || !modules.buf || modules.len < 2)
        return;
    if (modules.buf[0] == '{') {
        parseDynamicModuleToken(modules, result);
        return;
    }
    if (modules.buf[0] != '[')
        return;
    size_t offset = 0;
    struct mg_str module;
    while ((offset = mg_json_next(modules, offset, 0, &module)) != 0)
        parseDynamicModuleToken(module, result);
}

bool BilibiliSectionParser::parseDynamic(
    const QByteArray &body,
    QVector<ContentItemCompat> *items,
    int *apiCode,
    QString *errorText,
    QString *nextOffset,
    bool *hasMore)
{
    if (!items)
        return false;
    items->clear();
    if (nextOffset)
        nextOffset->clear();
    if (hasMore)
        *hasMore = false;
    struct mg_str root;
    if (!sectionRoot(body, &root, apiCode, errorText))
        return false;
    struct mg_str list = mg_json_get_tok(root, "$.data.items");
    if (!list.buf || list.len < 2 || list.buf[0] != '[')
        list = mg_json_get_tok(root, "$.data.cards");
    if (!list.buf || list.len < 2 || list.buf[0] != '[')
        list = mg_json_get_tok(root, "$.data.list");
    if (!list.buf || list.len < 2 || list.buf[0] != '[')
        list = mg_json_get_tok(root, "$.items");
    if (!list.buf || list.len < 2 || list.buf[0] != '[')
        list = mg_json_get_tok(root, "$.data");
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        if (errorText)
            *errorText = QString::fromLatin1("missing dynamic item array");
        return false;
    }
    if (nextOffset)
        *nextOffset = firstString(root, "$.data.offset", "$.offset", 0);
    if (nextOffset && nextOffset->isEmpty()) {
        *nextOffset = sectionJsonScalarText(root, "$.data.offset");
        if (nextOffset->isEmpty())
            *nextOffset = sectionJsonScalarText(root, "$.offset");
    }
    if (hasMore) {
        bool more = false;
        if (mg_json_get_bool(root, "$.data.has_more", &more) ||
            mg_json_get_bool(root, "$.has_more", &more))
            *hasMore = more;
        else {
            double moreNumber = 0.0;
            if (sectionJsonNumber(root, "$.data.has_more", &moreNumber) ||
                sectionJsonNumber(root, "$.has_more", &moreNumber))
                *hasMore = moreNumber != 0.0;
        }
    }
    size_t offset = 0;
    struct mg_str item;
    while (items->size() < 30 &&
           (offset = mg_json_next(list, offset, 0, &item)) != 0) {
        ContentItemCompat result;
        result.kind = TextContentItem;
        parseDynamicModules(
            mg_json_get_tok(item, "$.modules"), &result);
        if (result.title.isEmpty())
            result.title = cleanSectionText(firstString(
                item,
                "$.modules.module_author.name",
                "$.owner.name",
                "$.author.name"));
        if (result.title.isEmpty())
            result.title = cleanSectionText(firstString4(
                item,
                "$.module_author.name",
                "$.owner.name",
                "$.author.name",
                "$.user.name"));
        if (result.title.isEmpty())
            result.title = QString::fromUtf8("动态用户");
        if (result.picture.isEmpty())
            result.picture = firstString4(
                item,
                "$.modules.module_author.face",
                "$.owner.face",
                "$.author.face",
                "$.module_author.face");
        if (result.picture.isEmpty())
            result.picture = sectionJsonString(item, "$.pic");

        QString time = cleanSectionText(firstString4(
            item,
            "$.modules.module_author.pub_time",
            "$.pub_time",
            "$.time",
            "$.pub_text"));
        if (time.isEmpty())
            time = cleanSectionText(firstString(
                item, "$.module_author.pub_time", "$.module_author.pub_text", 0));
        if (time.isEmpty())
            time = result.subtitle;
        double number = 0.0;
        if (sectionJsonNumber(
                item, "$.modules.module_author.pub_ts", &number) ||
            sectionJsonNumber(item, "$.module_author.pub_ts", &number) ||
            sectionJsonNumber(item, "$.pub_ts", &number) ||
            sectionJsonNumber(item, "$.pubdate", &number)) {
            result.timestamp = static_cast<quint64>(number);
            if (time.isEmpty())
                time = sectionTimeText(result.timestamp);
        }

        if (result.description.isEmpty())
            result.description = cleanSectionText(firstString4(
            item,
            "$.modules.module_dynamic.desc.text",
            "$.modules.module_dynamic.major.opus.summary",
            "$.desc.text",
            "$.description"));
        if (result.mediaTitle.isEmpty())
            result.mediaTitle = cleanSectionText(firstString4(
            item,
            "$.modules.module_dynamic.major.archive.title",
            "$.modules.module_dynamic.major.opus.title",
            "$.title",
            "$.archive.title"));
        const QString bvid = cleanSectionText(firstString(
            item,
            "$.modules.module_dynamic.major.archive.bvid",
            "$.bvid",
            "$.archive.bvid"));
        if (result.id.isEmpty() && !bvid.isEmpty()) {
            result.kind = VideoContentItem;
            result.id = bvid;
            result.badge = QString::fromUtf8("视频");
        }
        if (result.mediaPicture.isEmpty())
            result.mediaPicture = firstString4(
                item,
                "$.modules.module_dynamic.major.archive.cover",
                "$.modules.module_dynamic.major.opus.pics[0].url",
                "$.modules.module_dynamic.major.draw.items[0].src",
                "$.pic");
        if (result.mediaPicture.isEmpty()) {
            result.mediaPicture = firstString(
                item,
                "$.modules.module_dynamic.major.opus.pics[0].url",
                "$.modules.module_dynamic.major.opus.pics[0].src",
                "$.modules.module_dynamic.major.draw.items[0].url");
        }
        if (result.badge.isEmpty() && !result.mediaPicture.isEmpty()) {
            result.badge = QString::fromUtf8("图文");
        } else if (result.badge.isEmpty() && !firstString(
                       item,
                       "$.modules.module_dynamic.major.live_rcmd",
                       "$.modules.module_dynamic.major.live",
                       0).isEmpty()) {
            result.badge = QString::fromUtf8("直播");
        } else if (result.badge.isEmpty()) {
            result.badge = QString::fromUtf8("动态");
        }
        if (result.description.isEmpty())
            result.description = result.mediaTitle;
        if (result.mediaTitle.isEmpty() && !result.description.isEmpty())
            result.mediaTitle = result.description;
        if (!time.isEmpty())
            result.subtitle = time + QString::fromUtf8(" · ") + result.badge;
        else
            result.subtitle = result.badge;
        if (result.count == 0 &&
            (sectionJsonNumber(item, "$.modules.module_stat.like.count", &number) ||
             sectionJsonNumber(item, "$.module_stat.like.count", &number)))
            result.count = static_cast<int>(number);
        if (result.replyCount == 0 &&
            (sectionJsonNumber(item, "$.modules.module_stat.comment.count", &number) ||
             sectionJsonNumber(item, "$.module_stat.comment.count", &number)))
            result.replyCount = static_cast<int>(number);
        if (sectionJsonNumber(item, "$.id", &number))
            result.numericId = static_cast<quint64>(number);
        if (result.id.isEmpty()) {
            const QString dynamicId = cleanSectionText(firstString(
                item, "$.id_str", "$.id", "$.basic.comment_id"));
            if (!dynamicId.isEmpty())
                result.id = QString::fromLatin1("dynamic:") + dynamicId;
        }
        appendSectionItem(items, result);
    }
    return true;
}

bool BilibiliSectionParser::parseMessages(
    const QByteArray &body,
    QVector<ContentItemCompat> *items,
    int *apiCode,
    QString *errorText,
    quint64 *nextId,
    quint64 *nextTime,
    bool *hasMore)
{
    if (!items)
        return false;
    items->clear();
    if (nextId)
        *nextId = 0;
    if (nextTime)
        *nextTime = 0;
    if (hasMore)
        *hasMore = false;
    struct mg_str root;
    if (!sectionRoot(body, &root, apiCode, errorText))
        return false;
    struct mg_str list = mg_json_get_tok(root, "$.data.items");
    struct mg_str cursor = mg_json_get_tok(root, "$.data.cursor");
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        list = mg_json_get_tok(root, "$.data.total.items");
        cursor = mg_json_get_tok(root, "$.data.total.cursor");
    }
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        if (errorText)
            *errorText = QString::fromLatin1("missing message item array");
        return false;
    }
    double cursorNumber = 0.0;
    if (nextId && sectionJsonNumber(cursor, "$.id", &cursorNumber))
        *nextId = static_cast<quint64>(cursorNumber);
    if (nextTime && sectionJsonNumber(cursor, "$.time", &cursorNumber))
        *nextTime = static_cast<quint64>(cursorNumber);
    if (hasMore) {
        bool isEnd = false;
        if (mg_json_get_bool(cursor, "$.is_end", &isEnd))
            *hasMore = !isEnd;
    }
    size_t offset = 0;
    struct mg_str item;
    while (items->size() < 30 &&
            (offset = mg_json_next(list, offset, 0, &item)) != 0) {
        ContentItemCompat result;
        result.kind = TextContentItem;
        result.title = cleanSectionText(firstString(
            item,
            "$.user.nickname",
            "$.users[0].nickname",
            "$.nickname"));
        if (result.title.isEmpty())
            result.title = QString::fromUtf8("互动消息");
        result.picture = firstString(
            item, "$.user.avatar", "$.user.face", "$.users[0].avatar");
        result.badge = messageBadge(cleanSectionText(firstString(
            item, "$.item.type", "$.type", 0)));
        if (result.badge.isEmpty())
            result.badge = QString::fromUtf8("互动");
        double timeValue = 0.0;
        if (!sectionJsonNumber(item, "$.reply_time", &timeValue) &&
            !sectionJsonNumber(item, "$.at_time", &timeValue) &&
            !sectionJsonNumber(item, "$.like_time", &timeValue)) {
            timeValue = 0.0;
        }
        result.timestamp = static_cast<quint64>(timeValue);
        result.description = cleanSectionText(firstString(
            item,
            "$.item.source_content",
            "$.item.title",
            "$.item.content"));
        result.mediaTitle = cleanSectionText(firstString(
            item, "$.item.title", "$.item.source_content", 0));
        result.mediaPicture = firstString(
            item, "$.item.image", "$.item.cover", 0);
        const QString time = sectionTimeText(result.timestamp);
        result.subtitle = time;
        result.id = firstString(item, "$.item.uri", "$.uri", 0);
        if (sectionJsonNumber(item, "$.id", &timeValue)) {
            result.numericId = static_cast<quint64>(timeValue);
            if (result.id.isEmpty())
                result.id = QString::fromLatin1("message:") +
                    QString::number(static_cast<qulonglong>(timeValue));
        }
        appendSectionItem(items, result);
    }
    return true;
}

bool BilibiliSectionParser::parseChatSessions(
    const QByteArray &body,
    QVector<ContentItemCompat> *items,
    int *apiCode,
    QString *errorText,
    quint64 *nextBeginTimestamp,
    bool *hasMore)
{
    if (!items)
        return false;
    items->clear();
    if (nextBeginTimestamp)
        *nextBeginTimestamp = 0;
    if (hasMore)
        *hasMore = false;
    struct mg_str root;
    if (!sectionRoot(body, &root, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(
        root, "$.data.session_list");
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        if (errorText)
            *errorText = QString::fromLatin1("missing chat session array");
        return false;
    }
    if (hasMore) {
        bool more = false;
        if (mg_json_get_bool(root, "$.data.has_more", &more))
            *hasMore = more;
    }
    quint64 earliestTimestamp = 0;
    size_t offset = 0;
    struct mg_str item;
    while (items->size() < 30 &&
           (offset = mg_json_next(list, offset, 0, &item)) != 0) {
        ContentItemCompat result;
        result.kind = TextContentItem;
        result.title = cleanSectionText(firstString(
            item, "$.account_info.name", "$.talker_name", 0));
        result.picture = firstString(
            item, "$.account_info.pic", "$.account_info.pic_url",
            "$.account_info.face");
        double talker = 0.0;
        if (!mg_json_get_num(item, "$.talker_id", &talker) || talker <= 0.0)
            continue;
        if (result.title.isEmpty())
            result.title = QString::fromLatin1("UID %1")
                .arg(static_cast<qulonglong>(talker));
        double unread = 0.0;
        mg_json_get_num(item, "$.unread_count", &unread);
        result.description = cleanSectionText(unpackMessageText(firstString(
            item, "$.last_msg.content", "$.last_msg_content", 0)));
        double timestamp = 0.0;
        if (!sectionJsonNumber(item, "$.last_msg.timestamp", &timestamp) &&
            !sectionJsonNumber(item, "$.last_msg_time", &timestamp)) {
            timestamp = 0.0;
        }
        const quint64 timeValue = static_cast<quint64>(timestamp);
        if (timeValue != 0 &&
            (earliestTimestamp == 0 || timeValue < earliestTimestamp)) {
            earliestTimestamp = timeValue;
        }
        const QString time = sectionTimeText(timeValue);
        if (result.description.isEmpty())
            result.description = QString::fromUtf8("点击进入会话");
        result.subtitle = time;
        result.badge = QString::fromUtf8("私信");
        result.count = static_cast<int>(unread);
        result.pinned = unread > 0.0;
        result.numericId = static_cast<quint64>(talker);
        result.id = QString::number(static_cast<qulonglong>(talker));
        appendSectionItem(items, result);
    }
    if (nextBeginTimestamp)
        *nextBeginTimestamp = earliestTimestamp;
    if (hasMore && !*hasMore && items->size() >= 20 &&
        earliestTimestamp != 0) {
        // Older firmware/API combinations omit has_more.  A full session page
        // with an advancing timestamp is still a safe next-page hint.
        *hasMore = true;
    }
    return true;
}

} // namespace wiliwili
