#ifndef WILIWILI_SYMBIAN_BILIBILI_CONTENT_PARSER_H
#define WILIWILI_SYMBIAN_BILIBILI_CONTENT_PARSER_H

#include <QtCore/QByteArray>
#include <QtCore/QtGlobal>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "model/content_item.h"

namespace wiliwili {

class BilibiliContentParser
{
public:
    static bool parseSearchVideos(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText, int *pageCount);
    static bool parseSearchUsers(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText, int *pageCount);
    static bool parseComments(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText);
    static bool parseCommentReplies(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText);
    static bool parseHistory(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText,
        quint64 *nextMax, quint64 *nextViewAt, bool *hasMore);
    static bool parseFavoriteFolders(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText);
    static bool parseFavoriteResources(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText);
    static bool parseWatchLater(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText);
    static bool parseUserVideos(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText);
    static bool parseFollowing(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText);
    static bool parseChatMessages(
        const QByteArray &body, QVector<ContentItemCompat> *items,
        int *apiCode, QString *errorText);
    static bool parseActionResult(
        const QByteArray &body, int *apiCode, QString *message);
};

} // namespace wiliwili

#endif
