#ifndef WILIWILI_SYMBIAN_VIDEO_PLAYBACK_BACKEND_H
#define WILIWILI_SYMBIAN_VIDEO_PLAYBACK_BACKEND_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <QtGui/QImage>

#include "platform/yuv420_frame.h"

class QMediaContent;
class QWidget;

namespace wiliwili {

// Small player facade used by the Symbian UI.  The implementation deliberately
// owns the native MMF display window instead of reaching into Qt Mobility's
// private QMediaPlayer backend.
class VideoPlaybackBackend
{
public:
    enum State {
        StoppedState = 0,
        PlayingState,
        PausedState
    };

    enum MediaStatus {
        NoMedia = 0,
        LoadingMedia,
        LoadedMedia,
        BufferingMedia,
        StalledMedia,
        EndOfMedia,
        InvalidMedia
    };

    enum Error {
        NoError = 0
    };

    // A synchronous, deliberately narrow DevVideo capability check. It
    // selects the Nokia/Broadcom H.264 decoder, supplies one Annex-B coded
    // picture (with SPS/PPS), and asks only for its parsed picture header.
    // It never configures, initializes, starts, or displays a decoder.
    enum AvcHeaderProbeResult {
        AvcHeaderProbeAccepted = 0,
        AvcHeaderProbeRejected
    };

    explicit VideoPlaybackBackend(QWidget *videoHost);
    ~VideoPlaybackBackend();

    void setMedia(
        const QMediaContent &media,
        const QByteArray &mimeType = QByteArray());
    void clearMedia();
    void play();
    void pause();
    void stop();
    void setPosition(qint64 milliseconds);
    qint64 position() const;
    qint64 duration() const;
    void setPlaybackRate(qreal rate);
    qreal playbackRate() const;
    void setVolume(int volume);
    int volume() const;
    State state() const;
    MediaStatus mediaStatus() const;
    int bufferStatus() const;
    int error() const;
    QString errorString() const;
    bool isVideoAvailable() const;
    void setNativeVideoEnabled(bool enabled);
    bool isNativeVideoPolicyApplied() const;
    AvcHeaderProbeResult probeAvcHardwareHeader(
        const QByteArray &annexBAccessUnit,
        int *errorCode);
    bool startAvcHardwarePlayback(
        const QVector<QByteArray> &accessUnits,
        const QVector<qint64> &decodingTimesMilliseconds,
        const QVector<qint64> &presentationTimesMilliseconds,
        int codedWidth,
        int codedHeight);
    bool appendAvcHardwarePlayback(
        const QVector<QByteArray> &accessUnits,
        const QVector<qint64> &decodingTimesMilliseconds,
        const QVector<qint64> &presentationTimesMilliseconds);
    void finishAvcHardwareInput();
    void stopAvcHardwarePlayback();
    void pumpAvcHardwarePlayback();
    bool isAvcHardwarePlaybackActive() const;
    int avcHardwareBufferedUnitCount() const;
    int avcHardwarePictureCount() const;
    int avcHardwareError() const;
    void setAvcHardwareYuv420OutputEnabled(bool enabled);
    bool takeAvcHardwareFrame(
        QImage *frame,
        qint64 *timestampMilliseconds,
        int *serial) const;
    bool takeAvcHardwareFrameAt(
        qint64 audioPositionMilliseconds,
        QImage *frame,
        qint64 *timestampMilliseconds,
        int *serial) const;
    bool takeAvcHardwareYuv420Frame(
        Yuv420Frame *frame,
        int *serial) const;
    bool takeAvcHardwareYuv420FrameAt(
        qint64 audioPositionMilliseconds,
        Yuv420Frame *frame,
        int *serial) const;
    QString softPlaybackTelemetry(
        quint64 presentedCount,
        quint64 uploadedCount,
        qint64 uploadMilliseconds,
        qint64 lastPresentedPts) const;
    void updateVideoWindow();

private:
    VideoPlaybackBackend(const VideoPlaybackBackend &);
    VideoPlaybackBackend &operator=(const VideoPlaybackBackend &);

    class Impl;
    Impl *m_impl;
};

} // namespace wiliwili

#endif
