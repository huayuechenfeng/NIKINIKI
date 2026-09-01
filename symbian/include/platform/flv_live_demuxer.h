#ifndef WILIWILI_SYMBIAN_FLV_LIVE_DEMUXER_H
#define WILIWILI_SYMBIAN_FLV_LIVE_DEMUXER_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>

namespace wiliwili {

// Incremental, network-agnostic FLV demuxer for the live fallback.  It keeps
// only an incomplete FLV tag between calls.  AVC is emitted as complete
// Annex-B access units and AAC is emitted as ADTS so the existing FFmpeg
// video/MMF-audio split can be retained without enabling libavformat or an
// AAC software decoder.
class FlvLiveDemuxer
{
public:
    FlvLiveDemuxer();

    void reset();
    bool append(
        const QByteArray &bytes,
        QVector<QByteArray> *videoAccessUnits,
        QVector<qint64> *videoDecodingTimesMilliseconds,
        QVector<qint64> *videoPresentationTimesMilliseconds,
        QByteArray *adtsAudio,
        QString *errorText);

    bool hasVideoConfiguration() const;
    bool hasAudioConfiguration() const;
    bool hasStartedVideo() const;
    int width() const;
    int height() const;
    int audioSampleRate() const;
    int audioChannels() const;

private:
    bool parseAvcConfiguration(
        const QByteArray &payload, QString *errorText);
    bool parseAacConfiguration(
        const QByteArray &payload, QString *errorText);
    bool makeAnnexBAccessUnit(
        const QByteArray &payload,
        bool keyFrame,
        QByteArray *accessUnit,
        QString *errorText) const;
    bool appendAdtsFrame(
        const QByteArray &payload,
        QByteArray *adtsAudio,
        QString *errorText) const;
    qint64 normalizedTimestamp(quint32 timestamp);

    QByteArray m_buffer;
    QByteArray m_parameterSets;
    int m_nalLengthSize;
    int m_width;
    int m_height;
    int m_aacObjectType;
    int m_aacSampleRateIndex;
    int m_aacSampleRate;
    int m_aacChannels;
    qint64 m_firstTimestamp;
    bool m_headerParsed;
    bool m_videoConfigured;
    bool m_audioConfigured;
    bool m_videoStarted;
};

} // namespace wiliwili

#endif
