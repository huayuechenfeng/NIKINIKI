#ifndef WILIWILI_SYMBIAN_BILIBILI_PLAYBACK_PARSER_H
#define WILIWILI_SYMBIAN_BILIBILI_PLAYBACK_PARSER_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVector>

#include "model/playback_source.h"

namespace wiliwili {

class BilibiliPlaybackParser
{
public:
    static bool parsePlaybackSource(
        const QByteArray &body,
        PlaybackSourceCompat *source,
        int *apiCode,
        QString *errorText);

    static bool parseLivePlaybackSource(
        const QByteArray &body,
        PlaybackSourceCompat *source,
        int *apiCode,
        QString *errorText);

    static bool parseLegacyLivePlaybackSource(
        const QByteArray &body,
        quint64 roomId,
        PlaybackSourceCompat *source,
        int *apiCode,
        QString *errorText);

    static bool parseDanmaku(
        const QByteArray &body,
        QVector<DanmakuItemCompat> *items,
        QString *errorText);
};

} // namespace wiliwili

#endif
