#include "network/bilibili_content_parser.h"

#include <cstdlib>
#include <QtCore/QDateTime>

extern "C" {
#include "../../third_party/mongoose_compat/mg_json.h"
}

namespace wiliwili {

static QString contentString(const struct mg_str &json, const char *path)
{
    char *value = mg_json_get_str(json, path);
    if (!value)
        return QString();
    QString result = QString::fromUtf8(value);
    std::free(value);
    return result;
}

static bool contentNumber(
    const struct mg_str &json, const char *path, double *value)
{
    return mg_json_get_num(json, path, value);
}

static QString firstContentString(
    const struct mg_str &json,
    const char *first,
    const char *second,
    const char *third)
{
    QString value = contentString(json, first);
    if (value.isEmpty() && second)
        value = contentString(json, second);
    if (value.isEmpty() && third)
        value = contentString(json, third);
    return value;
}

static QString cleanContentText(QString value)
{
    value.replace(QString::fromLatin1("<em class=\"keyword\">"), QString());
    value.replace(QString::fromLatin1("</em>"), QString());
    value.replace(QString::fromLatin1("<br>"), QString::fromLatin1(" "));
    value.replace(QString::fromLatin1("<br/>"), QString::fromLatin1(" "));
    bool insideTag = false;
    QString clean;
    int index;
    for (index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character == QLatin1Char('<')) {
            insideTag = true;
        } else if (character == QLatin1Char('>')) {
            insideTag = false;
        } else if (!insideTag) {
            clean.append(character);
        }
    }
    clean.replace(QString::fromLatin1("&amp;"), QString::fromLatin1("&"));
    clean.replace(QString::fromLatin1("&quot;"), QString::fromLatin1("\""));
    clean.replace(QString::fromLatin1("&#39;"), QString::fromLatin1("'"));
    return clean.simplified();
}

static bool contentRoot(
    const QByteArray &body,
    struct mg_str *root,
    struct mg_str *data,
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
    if (!contentNumber(*root, "$.code", &codeValue)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing API code");
        return false;
    }
    const int code = static_cast<int>(codeValue);
    if (apiCode)
        *apiCode = code;
    if (code != 0) {
        if (errorText) {
            *errorText = contentString(*root, "$.message");
            if (errorText->isEmpty())
                *errorText = QString::fromLatin1("API %1").arg(code);
        }
        return false;
    }
    *data = mg_json_get_tok(*root, "$.data");
    if (!data->buf || data->len < 2) {
        if (errorText)
            *errorText = QString::fromLatin1("missing data");
        return false;
    }
    return true;
}

static bool appendVideo(
    QVector<ContentItemCompat> *items,
    const struct mg_str &json,
    const char *titlePath,
    const char *bvidPath,
    const char *aidPath,
    const char *authorPath,
    const char *picturePath,
    const char *descriptionPath)
{
    ContentItemCompat item;
    item.kind = VideoContentItem;
    item.title = cleanContentText(contentString(json, titlePath));
    item.id = contentString(json, bvidPath);
    item.subtitle = cleanContentText(contentString(json, authorPath));
    item.picture = contentString(json, picturePath);
    item.description = descriptionPath
        ? cleanContentText(contentString(json, descriptionPath))
        : QString();
    double number = 0.0;
    if (contentNumber(json, aidPath, &number))
        item.numericId = static_cast<quint64>(number);
    if (item.title.isEmpty() ||
        (item.id.isEmpty() && item.numericId == 0))
        return false;
    items->append(item);
    return true;
}

static struct mg_str firstList(
    const struct mg_str &data,
    const char *first,
    const char *second)
{
    struct mg_str list = mg_json_get_tok(data, first);
    if ((!list.buf || list.len < 2) && second)
        list = mg_json_get_tok(data, second);
    return list;
}

bool BilibiliContentParser::parseSearchVideos(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText, int *pageCount)
{
    if (!items)
        return false;
    items->clear();
    if (pageCount)
        *pageCount = 0;
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.result");
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        if (errorText) {
            const struct mg_str voucher = mg_json_get_tok(data, "$.v_voucher");
            *errorText = voucher.buf
                ? QString::fromLatin1("search risk response")
                : QString::fromLatin1("missing search result array");
        }
        return false;
    }
    double pages = 0.0;
    if (pageCount && contentNumber(data, "$.numPages", &pages))
        *pageCount = qMax(0, static_cast<int>(pages));
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 50 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        const int before = items->size();
        appendVideo(items, entry, "$.title", "$.bvid", "$.aid",
                    "$.author", "$.pic", "$.description");
        if (items->size() > before) {
            ContentItemCompat &item = (*items)[items->size() - 1];
            double play = 0.0;
            if (contentNumber(entry, "$.play", &play))
                item.subtitle += QString::fromUtf8("  播放 %1")
                    .arg(static_cast<int>(play));
        }
    }
    return true;
}

bool BilibiliContentParser::parseSearchUsers(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText, int *pageCount)
{
    if (!items)
        return false;
    items->clear();
    if (pageCount)
        *pageCount = 0;
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.result");
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        if (errorText) {
            const struct mg_str voucher = mg_json_get_tok(data, "$.v_voucher");
            *errorText = voucher.buf
                ? QString::fromLatin1("search risk response")
                : QString::fromLatin1("missing search result array");
        }
        return false;
    }
    double pages = 0.0;
    if (pageCount && contentNumber(data, "$.numPages", &pages))
        *pageCount = qMax(0, static_cast<int>(pages));
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 50 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        ContentItemCompat item;
        item.kind = UserContentItem;
        item.title = cleanContentText(firstContentString(
            entry, "$.uname", "$.name", 0));
        item.picture = firstContentString(
            entry, "$.upic", "$.face", 0);
        item.description = cleanContentText(firstContentString(
            entry, "$.usign", "$.sign", 0));
        double number = 0.0;
        if (contentNumber(entry, "$.mid", &number))
            item.numericId = static_cast<quint64>(number);
        int videos = 0;
        if (contentNumber(entry, "$.videos", &number))
            videos = static_cast<int>(number);
        item.subtitle = QString::fromUtf8("UID %1  /  %2 个投稿")
            .arg(item.numericId).arg(videos);
        if (!item.title.isEmpty() && item.numericId != 0)
            items->append(item);
    }
    return true;
}

static QString commentReplyPreview(const struct mg_str &entry)
{
    const struct mg_str replies = mg_json_get_tok(entry, "$.replies");
    if (!replies.buf || replies.len < 2 || replies.buf[0] != '[')
        return QString();

    QString preview;
    size_t offset = 0;
    int replyIndex = 0;
    struct mg_str reply;
    while (replyIndex < 2 &&
           (offset = mg_json_next(replies, offset, 0, &reply)) != 0) {
        const QString author = cleanContentText(
            contentString(reply, "$.member.uname"));
        const QString message = cleanContentText(
            contentString(reply, "$.content.message"));
        if (message.isEmpty())
            continue;
        QString line = author.isEmpty()
            ? message : author + QString::fromUtf8("：") + message;
        double likes = 0.0;
        if (contentNumber(reply, "$.like", &likes) && likes > 0.0) {
            line += QString::fromUtf8("  赞 %1")
                .arg(static_cast<int>(likes));
        }
        if (!preview.isEmpty())
            preview += QString::fromLatin1("\n");
        preview += line;
        ++replyIndex;
    }
    return preview;
}

static bool appendCommentItem(
    QVector<ContentItemCompat> *items,
    const struct mg_str &entry,
    bool nested,
    bool pinned = false,
    const char *explicitId = 0)
{
    if (!items || items->size() >= 80)
        return false;
    ContentItemCompat item;
    item.kind = TextContentItem;
    item.id = explicitId
        ? QString::fromLatin1(explicitId)
        : nested
            ? QString::fromLatin1("comment-reply")
            : QString::fromLatin1("comment-root");
    item.title = cleanContentText(contentString(entry, "$.content.message"));
    item.subtitle = contentString(entry, "$.member.uname");
    // Keep the avatar URL for parity with the main client, but the Symbian
    // renderer deliberately uses a stable local initials avatar instead of
    // issuing a TLS/GL request for every scrolled comment.
    item.picture = contentString(entry, "$.member.avatar");
    item.pinned = pinned;
    double number = 0.0;
    if (contentNumber(entry, "$.rpid", &number))
        item.numericId = static_cast<quint64>(number);
    if (contentNumber(entry, "$.member.mid", &number))
        item.secondaryId = static_cast<quint64>(number);
    if (contentNumber(entry, "$.like", &number))
        item.count = static_cast<int>(number);
    if (contentNumber(entry, "$.rcount", &number))
        item.replyCount = static_cast<int>(number);
    if (contentNumber(entry, "$.member.level_info.current_level", &number))
        item.level = static_cast<int>(number);
    if (contentNumber(entry, "$.ctime", &number)) {
        item.timestamp = static_cast<quint64>(number);
    }
    QString timeText = cleanContentText(
        contentString(entry, "$.reply_control.time_desc"));
    if (timeText.isEmpty() && item.timestamp != 0) {
        timeText = QDateTime::fromTime_t(
            static_cast<uint>(item.timestamp)).toString(
                QString::fromLatin1("yyyy-MM-dd hh:mm"));
    }
    const QString location = cleanContentText(
        contentString(entry, "$.reply_control.location"));
    item.description = timeText;
    if (!location.isEmpty()) {
        if (!item.description.isEmpty())
            item.description += QString::fromUtf8("  ·  ");
        item.description += location;
    }
    if (!nested && item.replyCount > 0)
        item.previewText = commentReplyPreview(entry);
    if (item.title.isEmpty())
        return false;
    items->append(item);
    return true;
}

bool BilibiliContentParser::parseComments(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.replies");
    struct mg_str topList = mg_json_get_tok(data, "$.top_replies");
    if (!topList.buf)
        topList = mg_json_get_tok(data, "$.hots");
    size_t topOffset = 0;
    struct mg_str topEntry;
    while (items->size() < 80 &&
           (topOffset = mg_json_next(
                topList, topOffset, 0, &topEntry)) != 0) {
        appendCommentItem(items, topEntry, false, true);
    }
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 80 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        appendCommentItem(items, entry, false);
    }
    return true;
}

bool BilibiliContentParser::parseCommentReplies(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str rootComment = mg_json_get_tok(data, "$.root");
    if (rootComment.buf && rootComment.len >= 2 &&
        rootComment.buf[0] == '{') {
        appendCommentItem(
            items, rootComment, false, false, "comment-thread-root");
    }
    // Upstream /reply/detail returns data.root.replies. The legacy -352
    // fallback returns data.replies directly, so accept both shapes while
    // keeping the same structured thread UI.
    const struct mg_str nestedList =
        mg_json_get_tok(rootComment, "$.replies");
    const struct mg_str list = nestedList.buf
        ? nestedList : mg_json_get_tok(data, "$.replies");
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 80 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        appendCommentItem(items, entry, true);
    }
    return true;
}

bool BilibiliContentParser::parseHistory(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText,
    quint64 *nextMax, quint64 *nextViewAt, bool *hasMore)
{
    if (!items)
        return false;
    items->clear();
    if (nextMax)
        *nextMax = 0;
    if (nextViewAt)
        *nextViewAt = 0;
    if (hasMore)
        *hasMore = false;
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.list");
    if (!list.buf || list.len < 2 || list.buf[0] != '[') {
        if (errorText)
            *errorText = QString::fromLatin1("missing history list");
        return false;
    }
    double cursorMax = 0.0;
    double cursorViewAt = 0.0;
    contentNumber(data, "$.cursor.max", &cursorMax);
    contentNumber(data, "$.cursor.view_at", &cursorViewAt);
    if (nextMax && cursorMax > 0.0)
        *nextMax = static_cast<quint64>(cursorMax);
    if (nextViewAt && cursorViewAt > 0.0)
        *nextViewAt = static_cast<quint64>(cursorViewAt);
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 50 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        const int before = items->size();
        if (!appendVideo(items, entry, "$.title", "$.history.bvid",
                         "$.history.oid", "$.author_name", "$.cover", 0)) {
            continue;
        }
        ContentItemCompat &item = (*items)[before];
        const QString showTitle = cleanContentText(
            contentString(entry, "$.show_title"));
        if (!showTitle.isEmpty()) {
            if (!item.subtitle.isEmpty())
                item.subtitle += QString::fromUtf8(" · ");
            item.subtitle += showTitle;
        }
        double progress = 0.0;
        double duration = 0.0;
        if (contentNumber(entry, "$.progress", &progress)) {
            if (progress < 0.0) {
                item.description = QString::fromUtf8("已看完");
            } else if (contentNumber(entry, "$.duration", &duration) &&
                       duration > 0.0) {
                item.description = QString::fromUtf8("进度 %1/%2 秒")
                    .arg(static_cast<int>(progress))
                    .arg(static_cast<int>(duration));
            }
        }
        if (!item.description.isEmpty()) {
            if (!item.subtitle.isEmpty())
                item.subtitle += QString::fromUtf8(" · ");
            item.subtitle += item.description;
        }
        double viewedAt = 0.0;
        if (contentNumber(entry, "$.view_at", &viewedAt) && viewedAt > 0.0)
            item.timestamp = static_cast<quint64>(viewedAt);
    }
    if (hasMore) {
        *hasMore = items->size() >= 20 && cursorMax > 0.0 &&
            cursorViewAt > 0.0;
    }
    return true;
}

bool BilibiliContentParser::parseFavoriteFolders(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.list");
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 50 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        ContentItemCompat item;
        item.kind = FolderContentItem;
        item.title = contentString(entry, "$.title");
        double number = 0.0;
        if (contentNumber(entry, "$.id", &number))
            item.numericId = static_cast<quint64>(number);
        if (contentNumber(entry, "$.media_count", &number))
            item.count = static_cast<int>(number);
        item.subtitle = QString::fromUtf8("%1 个内容").arg(item.count);
        if (!item.title.isEmpty() && item.numericId != 0)
            items->append(item);
    }
    return true;
}

bool BilibiliContentParser::parseFavoriteResources(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.medias");
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 40 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        appendVideo(items, entry, "$.title", "$.bvid", "$.id",
                    "$.upper.name", "$.cover", "$.intro");
    }
    return true;
}

bool BilibiliContentParser::parseWatchLater(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.list");
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 40 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        appendVideo(items, entry, "$.title", "$.bvid", "$.aid",
                    "$.owner.name", "$.pic", "$.desc");
    }
    return true;
}

bool BilibiliContentParser::parseUserVideos(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = firstList(data, "$.list.vlist", "$.list");
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 40 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        appendVideo(items, entry, "$.title", "$.bvid", "$.aid",
                    "$.author", "$.pic", "$.description");
    }
    return true;
}

bool BilibiliContentParser::parseFollowing(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.list");
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 50 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        ContentItemCompat item;
        item.kind = UserContentItem;
        item.title = firstContentString(entry, "$.uname", "$.name", 0);
        item.subtitle = firstContentString(entry, "$.sign", "$.official_verify.desc", 0);
        item.picture = contentString(entry, "$.face");
        double number = 0.0;
        if (contentNumber(entry, "$.mid", &number))
            item.numericId = static_cast<quint64>(number);
        if (!item.title.isEmpty() && item.numericId != 0)
            items->append(item);
    }
    return true;
}

bool BilibiliContentParser::parseChatMessages(
    const QByteArray &body, QVector<ContentItemCompat> *items,
    int *apiCode, QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    struct mg_str root, data;
    if (!contentRoot(body, &root, &data, apiCode, errorText))
        return false;
    const struct mg_str list = mg_json_get_tok(data, "$.messages");
    size_t offset = 0;
    struct mg_str entry;
    while (items->size() < 40 &&
           (offset = mg_json_next(list, offset, 0, &entry)) != 0) {
        ContentItemCompat item;
        item.kind = TextContentItem;
        QString packed = contentString(entry, "$.content");
        if (!packed.isEmpty()) {
            const QByteArray nestedBytes = packed.toUtf8();
            const struct mg_str nested = mg_str_n(
                nestedBytes.constData(),
                static_cast<size_t>(nestedBytes.size()));
            const QString text = contentString(nested, "$.content");
            item.title = text.isEmpty() ? packed : text;
        }
        double number = 0.0;
        if (contentNumber(entry, "$.sender_uid", &number))
            item.secondaryId = static_cast<quint64>(number);
        item.subtitle = QString::fromLatin1("UID %1")
            .arg(item.secondaryId);
        if (contentNumber(entry, "$.timestamp", &number))
            item.description = QString::fromLatin1("TIME %1")
                .arg(static_cast<qulonglong>(number));
        if (!item.title.trimmed().isEmpty())
            items->append(item);
    }
    return true;
}

bool BilibiliContentParser::parseActionResult(
    const QByteArray &body, int *apiCode, QString *message)
{
    if (apiCode)
        *apiCode = -9999;
    if (message)
        message->clear();
    if (body.isEmpty()) {
        if (message)
            *message = QString::fromUtf8("服务器返回为空");
        return false;
    }

    const struct mg_str root = mg_str_n(
        body.constData(), static_cast<size_t>(body.size()));
    double value = -9999.0;
    if (!contentNumber(root, "$.code", &value)) {
        if (message)
            *message = QString::fromUtf8("响应格式错误");
        return false;
    }
    const int code = static_cast<int>(value);
    if (apiCode)
        *apiCode = code;
    if (code == 0) {
        if (message)
            *message = QString::fromUtf8("操作成功");
        return true;
    }
    if (message) {
        *message = contentString(root, "$.message");
        if (message->isEmpty())
            *message = QString::fromLatin1("API %1").arg(code);
    }
    return false;
}

} // namespace wiliwili
