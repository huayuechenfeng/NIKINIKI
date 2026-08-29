#ifndef WILIWILI_SYMBIAN_PLAYBACK_SOURCE_H
#define WILIWILI_SYMBIAN_PLAYBACK_SOURCE_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>

namespace wiliwili {

struct PlaybackQualityCompat
{
    PlaybackQualityCompat() : quality(0) {}
    PlaybackQualityCompat(int value, const QString &label)
        : quality(value), description(label) {}

    int quality;
    QString description;
};

struct PlaybackSourceCompat
{
    PlaybackSourceCompat()
        : quality(0), durationMilliseconds(0),
          videoWidth(0), videoHeight(0), live(false)
    {
    }

    QString url;
    QVector<QString> backupUrls;
    QString format;
    int quality;
    qint64 durationMilliseconds;
    int videoWidth;
    int videoHeight;
    bool live;
    QString referer;
    QVector<PlaybackQualityCompat> qualities;
};

struct DanmakuItemCompat
{
    DanmakuItemCompat()
        : timeMilliseconds(0), mode(1), fontSize(25), color(0xffffff)
    {
    }

    qint64 timeMilliseconds;
    int mode;
    int fontSize;
    int color;
    QString text;
};

} // namespace wiliwili

#endif
