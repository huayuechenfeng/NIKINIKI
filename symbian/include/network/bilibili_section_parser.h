#ifndef WILIWILI_SYMBIAN_BILIBILI_SECTION_PARSER_H
#define WILIWILI_SYMBIAN_BILIBILI_SECTION_PARSER_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>

#include "model/content_item.h"

namespace wiliwili {

class BilibiliSectionParser
{
public:
    static bool parseDynamic(
        const QByteArray &body,
        QVector<ContentItemCompat> *items,
        int *apiCode,
        QString *errorText,
        QString *nextOffset,
        bool *hasMore);
    static bool parseMessages(
        const QByteArray &body,
        QVector<ContentItemCompat> *items,
        int *apiCode,
        QString *errorText,
        quint64 *nextId,
        quint64 *nextTime,
        bool *hasMore);
    static bool parseChatSessions(
        const QByteArray &body,
        QVector<ContentItemCompat> *items,
        int *apiCode,
        QString *errorText,
        quint64 *nextBeginTimestamp,
        bool *hasMore);
};

} // namespace wiliwili

#endif
