#ifndef WILIWILI_SYMBIAN_FFMPEG_H264_DECODER_H
#define WILIWILI_SYMBIAN_FFMPEG_H264_DECODER_H

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QThread>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QWaitCondition>
#include <QtCore/QtGlobal>
#include <QtGui/QImage>

#include "platform/yuv420_frame.h"

namespace wiliwili {

// A deliberately small libavcodec adapter for the Symbian software fallback.
// The caller supplies complete Annex-B access units and millisecond DTS/PTS.
// Decode and the compact YUV plane copy stay off the UI thread; the UI first
// consumes a bounded presentation-ordered frame queue using the MMF audio
// clock and only then converts the selected picture to RGB565.
class FfmpegH264Decoder : private QThread
{
public:
    FfmpegH264Decoder();
    ~FfmpegH264Decoder();

    bool startDecoder(
        const QVector<QByteArray> &accessUnits,
        const QVector<qint64> &decodingTimesMilliseconds,
        const QVector<qint64> &presentationTimesMilliseconds,
        int codedWidth,
        int codedHeight,
        bool yuv420Output);
    bool append(
        const QVector<QByteArray> &accessUnits,
        const QVector<qint64> &decodingTimesMilliseconds,
        const QVector<qint64> &presentationTimesMilliseconds);
    void finishInput();
    void stopDecoder();

    bool isActive() const;
    int bufferedUnitCount() const;
    int pictureCount() const;
    int decodedPictureCount() const;
    int outputDropCount() const;
    int outputQueueDepth() const;
    QString telemetry(
        qint64 audioPositionMilliseconds,
        quint64 presentedCount,
        quint64 uploadedCount,
        qint64 uploadMilliseconds,
        qint64 lastPresentedPts) const;
    int error() const;
    bool takeFrame(
        qint64 audioPositionMilliseconds,
        QImage *frame,
        qint64 *timestampMilliseconds,
        int *serial);
    bool takeYuv420Frame(
        qint64 audioPositionMilliseconds,
        Yuv420Frame *frame,
        int *serial);

protected:
    virtual void run();

private:
    FfmpegH264Decoder(const FfmpegH264Decoder &);
    FfmpegH264Decoder &operator=(const FfmpegH264Decoder &);

    struct InputUnit
    {
        QByteArray bytes;
        qint64 dts;
        qint64 pts;
    };

    struct OutputFrame
    {
        QImage image;
        Yuv420Frame yuv420;
        qint64 pts;
        int serial;
    };

    void setFailure(int errorCode, const char *stage);
    bool enqueueOutput(
        const QImage &image,
        const Yuv420Frame &yuv420,
        qint64 pts);
    bool takeOutput(
        qint64 audioPositionMilliseconds,
        OutputFrame *output,
        int *serial);

    mutable QMutex m_mutex;
    QWaitCondition m_inputReady;
    QWaitCondition m_outputSpace;
    QVector<InputUnit> m_input;
    QVector<OutputFrame> m_output;
    bool m_abort;
    bool m_inputFinished;
    int m_state;
    int m_error;
    int m_decodedPictureCount;
    int m_pictureCount;
    int m_outputDropCount;
    int m_nextSerial;
    int m_catchupEnterCount;
    qint64 m_catchupMilliseconds;
    qint64 m_catchupStartedMilliseconds;
    qint64 m_decodeMilliseconds;
    qint64 m_convertMilliseconds;
    qint64 m_repackCopyMilliseconds;
    qint64 m_outputMutexWaitMilliseconds;
    qint64 m_performanceMilliseconds;
    qint64 m_lastOutputPts;
    bool m_catchupActive;
    int m_codedWidth;
    int m_codedHeight;
    bool m_yuv420Output;
    qint64 m_audioPositionMilliseconds;
};

} // namespace wiliwili

#endif
