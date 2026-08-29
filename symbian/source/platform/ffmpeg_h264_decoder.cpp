#include "platform/ffmpeg_h264_decoder.h"

#include <QtCore/QDebug>
#include <QtCore/QMutexLocker>
#include <QtCore/QTime>

#include <string.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

namespace wiliwili {
namespace {

enum {
    DecoderIdle = 0,
    DecoderStarting = 1,
    DecoderRunning = 2,
    DecoderHasPictures = 3,
    DecoderFailed = 4,
    DecoderEnded = 5,
    MaximumOutputFrames = 6
};

// Keep normal presentation order unless a frame is far enough behind the
// audio clock that showing it would only extend visible latency. The old
// policy selected the newest due frame and removed every older frame.
static const qint64 KOutputDueToleranceMilliseconds = 80;
static const qint64 KOutputSeverelyLateMilliseconds = 250;
static const qint64 KCatchupEnterLagMilliseconds = 900;
static const qint64 KCatchupExitLagMilliseconds = 280;
static const qint64 KCatchupConfirmMilliseconds = 500;

static int clampByte(int value)
{
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

static quint16 rgb565(int red, int green, int blue)
{
    return static_cast<quint16>(
        ((red & 0xf8) << 8) |
        ((green & 0xfc) << 3) |
        (blue >> 3));
}

enum { Rgb565ClipOffset = 384, Rgb565ClipTableSize = 1024 };

static int limitedLuma[256];
static int limitedRedV[256];
static int limitedGreenU[256];
static int limitedGreenV[256];
static int limitedBlueU[256];
static quint16 clippedRed565[Rgb565ClipTableSize];
static quint16 clippedGreen565[Rgb565ClipTableSize];
static quint16 clippedBlue565[Rgb565ClipTableSize];
static bool rgb565TablesReady = false;

static void initializeRgb565Tables()
{
    if (rgb565TablesReady)
        return;
    int value;
    for (value = 0; value < 256; ++value) {
        const int chroma = value - 128;
        limitedLuma[value] = 298 * qMax(0, value - 16);
        limitedRedV[value] = 409 * chroma;
        limitedGreenU[value] = -100 * chroma;
        limitedGreenV[value] = -208 * chroma;
        limitedBlueU[value] = 516 * chroma;
    }
    for (value = 0; value < Rgb565ClipTableSize; ++value) {
        const int component = clampByte(value - Rgb565ClipOffset);
        clippedRed565[value] = static_cast<quint16>((component & 0xf8) << 8);
        clippedGreen565[value] = static_cast<quint16>((component & 0xfc) << 3);
        clippedBlue565[value] = static_cast<quint16>(component >> 3);
    }
    rgb565TablesReady = true;
}

#if defined(__GNUC__)
static inline quint16 __attribute__((always_inline)) limitedRgb565(
#else
static inline quint16 limitedRgb565(
#endif
    int y, int redChroma, int greenChroma, int blueChroma)
{
    const int luma = limitedLuma[y];
    const int red = ((luma + redChroma + 128) >> 8) + Rgb565ClipOffset;
    const int green = ((luma + greenChroma + 128) >> 8) + Rgb565ClipOffset;
    const int blue = ((luma + blueChroma + 128) >> 8) + Rgb565ClipOffset;
    return static_cast<quint16>(
        clippedRed565[red] | clippedGreen565[green] | clippedBlue565[blue]);
}

#if defined(__GNUC__)
__attribute__((hot, optimize("O3")))
#endif
static bool copyYuv420Frame(const AVFrame *source, QImage *result)
{
    if (!source || !result || !source->data[0] || !source->data[1] ||
        !source->data[2] || source->width <= 0 || source->height <= 0 ||
        (source->format != AV_PIX_FMT_YUV420P &&
         source->format != AV_PIX_FMT_YUVJ420P)) {
        return false;
    }

    QSize targetSize(source->width, source->height);
    targetSize.scale(QSize(640, 360), Qt::KeepAspectRatio);
    if (targetSize.isEmpty())
        return false;

    // RGB565 halves the persistent frame memory and is drawn by the dedicated
    // opaque native video surface. A separate ARGB window above it supplies
    // danmaku, controls and transparency.
    QImage converted(targetSize, QImage::Format_RGB16);
    if (converted.isNull())
        return false;

    const bool fullRange = source->format == AV_PIX_FMT_YUVJ420P;
    initializeRgb565Tables();

    // The common 640x360 path is intentionally division-free inside the
    // pixel loop. The bootstrap implementation divided twice per output
    // pixel and repeated the U/V multiplications for both luma samples;
    // those costs dominate an ARM1176 even after H.264 has produced a frame.
    const bool unscaled = converted.width() == source->width &&
                          converted.height() == source->height;
    // The normal Bilibili fallback is unscaled limited-range 640x360. Walk
    // 2x2 luma blocks so four output pixels share one U/V lookup, and use
    // precomputed coefficients plus RGB565 clipping tables. This hot function
    // is explicitly optimized even in a GCCE Debug application build.
    if (unscaled && !fullRange &&
        (converted.width() & 1) == 0 && (converted.height() & 1) == 0) {
        int dy;
        for (dy = 0; dy < converted.height(); dy += 2) {
            quint16 *out0 = reinterpret_cast<quint16 *>(converted.scanLine(dy));
            quint16 *out1 = reinterpret_cast<quint16 *>(converted.scanLine(dy + 1));
            const unsigned char *yRow0 = source->data[0] +
                dy * source->linesize[0];
            const unsigned char *yRow1 = source->data[0] +
                (dy + 1) * source->linesize[0];
            const unsigned char *uRow = source->data[1] +
                (dy >> 1) * source->linesize[1];
            const unsigned char *vRow = source->data[2] +
                (dy >> 1) * source->linesize[2];
            int dx;
            for (dx = 0; dx < converted.width(); dx += 2) {
                const int chromaIndex = dx >> 1;
                const int u = uRow[chromaIndex];
                const int v = vRow[chromaIndex];
                const int redChroma = limitedRedV[v];
                const int greenChroma = limitedGreenU[u] + limitedGreenV[v];
                const int blueChroma = limitedBlueU[u];
                out0[dx] = limitedRgb565(
                    yRow0[dx], redChroma, greenChroma, blueChroma);
                out0[dx + 1] = limitedRgb565(
                    yRow0[dx + 1], redChroma, greenChroma, blueChroma);
                out1[dx] = limitedRgb565(
                    yRow1[dx], redChroma, greenChroma, blueChroma);
                out1[dx + 1] = limitedRgb565(
                    yRow1[dx + 1], redChroma, greenChroma, blueChroma);
            }
        }
        *result = converted;
        return true;
    }

    int dy;
    for (dy = 0; dy < converted.height(); ++dy) {
        quint16 *out = reinterpret_cast<quint16 *>(converted.scanLine(dy));
        const int sy = unscaled
            ? dy : dy * source->height / converted.height();
        const unsigned char *yRow = source->data[0] +
            sy * source->linesize[0];
        const unsigned char *uRow = source->data[1] +
            (sy / 2) * source->linesize[1];
        const unsigned char *vRow = source->data[2] +
            (sy / 2) * source->linesize[2];
        int dx;
        if (unscaled) {
            for (dx = 0; dx + 1 < converted.width(); dx += 2) {
                const int u = static_cast<int>(uRow[dx >> 1]) - 128;
                const int v = static_cast<int>(vRow[dx >> 1]) - 128;
                const int redChroma = fullRange ? 359 * v : 409 * v;
                const int greenChroma = fullRange
                    ? -(88 * u + 183 * v) : -(100 * u + 208 * v);
                const int blueChroma = fullRange ? 454 * u : 516 * u;
                int pair;
                for (pair = 0; pair < 2; ++pair) {
                    const int y = yRow[dx + pair];
                    int red;
                    int green;
                    int blue;
                    if (fullRange) {
                        red = y + (redChroma >> 8);
                        green = y + (greenChroma >> 8);
                        blue = y + (blueChroma >> 8);
                    } else {
                        const int luma = limitedLuma[y];
                        red = (luma + redChroma + 128) >> 8;
                        green = (luma + greenChroma + 128) >> 8;
                        blue = (luma + blueChroma + 128) >> 8;
                    }
                    out[dx + pair] = rgb565(
                        clampByte(red), clampByte(green), clampByte(blue));
                }
            }
            if (dx < converted.width()) {
                const int u = static_cast<int>(uRow[dx >> 1]) - 128;
                const int v = static_cast<int>(vRow[dx >> 1]) - 128;
                const int y = yRow[dx];
                const int red = fullRange
                    ? y + ((359 * v) >> 8)
                    : (limitedLuma[y] + 409 * v + 128) >> 8;
                const int green = fullRange
                    ? y - ((88 * u + 183 * v) >> 8)
                    : (limitedLuma[y] - 100 * u - 208 * v + 128) >> 8;
                const int blue = fullRange
                    ? y + ((454 * u) >> 8)
                    : (limitedLuma[y] + 516 * u + 128) >> 8;
                out[dx] = rgb565(
                    clampByte(red), clampByte(green), clampByte(blue));
            }
        } else {
            for (dx = 0; dx < converted.width(); ++dx) {
                const int sx = dx * source->width / converted.width();
                const int y = yRow[sx];
                const int u = static_cast<int>(uRow[sx / 2]) - 128;
                const int v = static_cast<int>(vRow[sx / 2]) - 128;
                const int red = fullRange
                    ? y + ((359 * v) >> 8)
                    : (limitedLuma[y] + 409 * v + 128) >> 8;
                const int green = fullRange
                    ? y - ((88 * u + 183 * v) >> 8)
                    : (limitedLuma[y] - 100 * u - 208 * v + 128) >> 8;
                const int blue = fullRange
                    ? y + ((454 * u) >> 8)
                    : (limitedLuma[y] + 516 * u + 128) >> 8;
                out[dx] = rgb565(
                    clampByte(red), clampByte(green), clampByte(blue));
            }
        }
    }
    *result = converted;
    return true;
}

// The CPU renderer queues the compact YUV copy and waits until takeFrame()
// has selected the frame worth displaying before doing the RGB565 work.  A
// small AVFrame view lets the already-optimized conversion above be reused
// without another plane copy or a second conversion implementation.
static bool convertPackedYuv420Frame(
    const Yuv420Frame &source,
    QImage *result)
{
    if (!source.isValid() || !result)
        return false;
    AVFrame view;
    memset(&view, 0, sizeof(view));
    view.data[0] = reinterpret_cast<uint8_t *>(
        const_cast<char *>(source.yPlane.constData()));
    view.data[1] = reinterpret_cast<uint8_t *>(
        const_cast<char *>(source.uPlane.constData()));
    view.data[2] = reinterpret_cast<uint8_t *>(
        const_cast<char *>(source.vPlane.constData()));
    view.linesize[0] = source.width;
    view.linesize[1] = (source.width + 1) / 2;
    view.linesize[2] = (source.width + 1) / 2;
    view.width = source.width;
    view.height = source.height;
    view.format = source.fullRange ? AV_PIX_FMT_YUVJ420P : AV_PIX_FMT_YUV420P;
    return copyYuv420Frame(&view, result);
}

static void copyPlane(
    const unsigned char *source,
    int sourceStride,
    int width,
    int height,
    QByteArray *destination)
{
    destination->resize(width * height);
    unsigned char *target = reinterpret_cast<unsigned char *>(
        destination->data());
    if (sourceStride == width) {
        memcpy(target, source, width * height);
        return;
    }
    int row;
    for (row = 0; row < height; ++row)
        memcpy(target + row * width, source + row * sourceStride, width);
}

static bool copyYuv420Planes(
    const AVFrame *source,
    Yuv420Frame *result)
{
    if (!source || !result || !source->data[0] || !source->data[1] ||
        !source->data[2] || source->width <= 0 || source->height <= 0 ||
        (source->format != AV_PIX_FMT_YUV420P &&
         source->format != AV_PIX_FMT_YUVJ420P)) {
        return false;
    }
    const int chromaWidth = (source->width + 1) / 2;
    const int chromaHeight = (source->height + 1) / 2;
    Yuv420Frame packed;
    packed.width = source->width;
    packed.height = source->height;
    packed.fullRange = source->format == AV_PIX_FMT_YUVJ420P;
    copyPlane(source->data[0], source->linesize[0],
              source->width, source->height, &packed.yPlane);
    copyPlane(source->data[1], source->linesize[1],
              chromaWidth, chromaHeight, &packed.uPlane);
    copyPlane(source->data[2], source->linesize[2],
              chromaWidth, chromaHeight, &packed.vPlane);
    if (!packed.isValid())
        return false;
    *result = packed;
    return true;
}

} // namespace

FfmpegH264Decoder::FfmpegH264Decoder()
    : m_abort(false), m_inputFinished(false), m_state(DecoderIdle),
      m_error(0), m_decodedPictureCount(0), m_pictureCount(0),
      m_outputDropCount(0), m_nextSerial(0),
      m_catchupEnterCount(0), m_catchupMilliseconds(0),
      m_catchupStartedMilliseconds(-1), m_decodeMilliseconds(0),
      m_convertMilliseconds(0), m_repackCopyMilliseconds(0),
      m_outputMutexWaitMilliseconds(0), m_performanceMilliseconds(0),
      m_lastOutputPts(-1),
      m_catchupActive(false),
      m_codedWidth(0), m_codedHeight(0), m_yuv420Output(false),
      m_audioPositionMilliseconds(0)
{
}

FfmpegH264Decoder::~FfmpegH264Decoder()
{
    stopDecoder();
}

bool FfmpegH264Decoder::startDecoder(
    const QVector<QByteArray> &accessUnits,
    const QVector<qint64> &decodingTimesMilliseconds,
    const QVector<qint64> &presentationTimesMilliseconds,
    int codedWidth,
    int codedHeight,
    bool yuv420Output)
{
    if (isRunning() || accessUnits.isEmpty() || codedWidth <= 0 ||
        codedHeight <= 0 || accessUnits.size() != decodingTimesMilliseconds.size() ||
        accessUnits.size() != presentationTimesMilliseconds.size()) {
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_input.clear();
        m_output.clear();
        m_abort = false;
        m_inputFinished = false;
        m_state = DecoderStarting;
        m_error = 0;
        m_decodedPictureCount = 0;
        m_pictureCount = 0;
        m_outputDropCount = 0;
        m_nextSerial = 0;
        m_catchupEnterCount = 0;
        m_catchupMilliseconds = 0;
        m_catchupStartedMilliseconds = -1;
        m_decodeMilliseconds = 0;
        m_convertMilliseconds = 0;
        m_repackCopyMilliseconds = 0;
        m_outputMutexWaitMilliseconds = 0;
        m_performanceMilliseconds = 0;
        m_lastOutputPts = -1;
        m_catchupActive = false;
        m_codedWidth = codedWidth;
        m_codedHeight = codedHeight;
        m_yuv420Output = yuv420Output;
        m_audioPositionMilliseconds = 0;
        int index;
        for (index = 0; index < accessUnits.size(); ++index) {
            InputUnit unit;
            unit.bytes = accessUnits.at(index);
            unit.dts = decodingTimesMilliseconds.at(index);
            unit.pts = presentationTimesMilliseconds.at(index);
            m_input.append(unit);
        }
    }
    start(QThread::NormalPriority);
    return true;
}

bool FfmpegH264Decoder::append(
    const QVector<QByteArray> &accessUnits,
    const QVector<qint64> &decodingTimesMilliseconds,
    const QVector<qint64> &presentationTimesMilliseconds)
{
    if (accessUnits.isEmpty() ||
        accessUnits.size() != decodingTimesMilliseconds.size() ||
        accessUnits.size() != presentationTimesMilliseconds.size()) {
        return false;
    }
    QMutexLocker locker(&m_mutex);
    if (m_abort || m_inputFinished || m_state == DecoderFailed ||
        m_state == DecoderEnded || m_state == DecoderIdle) {
        return false;
    }
    int index;
    for (index = 0; index < accessUnits.size(); ++index) {
        InputUnit unit;
        unit.bytes = accessUnits.at(index);
        unit.dts = decodingTimesMilliseconds.at(index);
        unit.pts = presentationTimesMilliseconds.at(index);
        m_input.append(unit);
    }
    m_inputReady.wakeOne();
    return true;
}

void FfmpegH264Decoder::finishInput()
{
    QMutexLocker locker(&m_mutex);
    m_inputFinished = true;
    m_inputReady.wakeOne();
}

void FfmpegH264Decoder::stopDecoder()
{
    {
        QMutexLocker locker(&m_mutex);
        m_abort = true;
        m_inputReady.wakeAll();
        m_outputSpace.wakeAll();
    }
    if (isRunning())
        wait();
    QMutexLocker locker(&m_mutex);
    m_input.clear();
    m_output.clear();
    m_state = DecoderIdle;
}

bool FfmpegH264Decoder::isActive() const
{
    QMutexLocker locker(&m_mutex);
    return m_state == DecoderStarting || m_state == DecoderRunning ||
           m_state == DecoderHasPictures;
}

int FfmpegH264Decoder::bufferedUnitCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_input.size();
}

int FfmpegH264Decoder::pictureCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_pictureCount;
}

int FfmpegH264Decoder::decodedPictureCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_decodedPictureCount;
}

int FfmpegH264Decoder::outputDropCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_outputDropCount;
}

int FfmpegH264Decoder::outputQueueDepth() const
{
    QMutexLocker locker(&m_mutex);
    return m_output.size();
}

QString FfmpegH264Decoder::telemetry(
    qint64 audioPositionMilliseconds,
    quint64 presentedCount,
    quint64 uploadedCount,
    qint64 uploadMilliseconds,
    qint64 lastPresentedPts) const
{
    QMutexLocker locker(&m_mutex);
    qint64 catchupMilliseconds = m_catchupMilliseconds;
    if (m_catchupActive && m_catchupStartedMilliseconds >= 0) {
        const qint64 now = m_performanceMilliseconds;
        if (now > m_catchupStartedMilliseconds)
            catchupMilliseconds += now - m_catchupStartedMilliseconds;
    }
    const qint64 telemetryPts = lastPresentedPts >= 0
        ? lastPresentedPts : m_lastOutputPts;
    const qint64 avLag = telemetryPts >= 0
        ? audioPositionMilliseconds - telemetryPts : 0;
    return QString::fromLatin1(
        "WW:SOFT_STATS pts=%1 decoded=%2 pictures=%3 outputDrops=%4 "
        "presented=%5 uploaded=%6 decodeMs=%7 uploadMs=%8 "
        "convertMs=%9 repackCopyMs=%10 queueMutexWaitMs=%11 "
        "avLagMs=%12 queueDepth=%13 catchup=%14 "
        "catchupEnters=%15 catchupMs=%16")
        .arg(telemetryPts)
        .arg(m_decodedPictureCount)
        .arg(m_pictureCount)
        .arg(m_outputDropCount)
        .arg(static_cast<qulonglong>(presentedCount))
        .arg(static_cast<qulonglong>(uploadedCount))
        .arg(m_decodeMilliseconds)
        .arg(uploadMilliseconds)
        .arg(m_convertMilliseconds)
        .arg(m_repackCopyMilliseconds)
        .arg(m_outputMutexWaitMilliseconds)
        .arg(avLag)
        .arg(m_output.size())
        .arg(m_catchupActive ? 1 : 0)
        .arg(m_catchupEnterCount)
        .arg(catchupMilliseconds);
}

int FfmpegH264Decoder::error() const
{
    QMutexLocker locker(&m_mutex);
    return m_error;
}

bool FfmpegH264Decoder::takeFrame(
    qint64 audioPositionMilliseconds,
    QImage *frame,
    qint64 *timestampMilliseconds,
    int *serial)
{
    if (!frame || !serial)
        return false;
    OutputFrame output;
    if (!takeOutput(audioPositionMilliseconds, &output, serial)) {
        return false;
    }
    QImage converted = output.image;
    if (converted.isNull()) {
        if (!output.yuv420.isValid())
            return false;
        QTime convertClock;
        convertClock.start();
        if (!convertPackedYuv420Frame(output.yuv420, &converted))
            return false;
        const qint64 elapsed = convertClock.elapsed();
        QMutexLocker locker(&m_mutex);
        m_convertMilliseconds += elapsed;
    }
    *frame = converted;
    if (timestampMilliseconds)
        *timestampMilliseconds = output.pts;
    return true;
}

bool FfmpegH264Decoder::takeYuv420Frame(
    qint64 audioPositionMilliseconds,
    Yuv420Frame *frame,
    int *serial)
{
    if (!frame || !serial)
        return false;
    OutputFrame output;
    if (!takeOutput(audioPositionMilliseconds, &output, serial) ||
        !output.yuv420.isValid()) {
        return false;
    }
    *frame = output.yuv420;
    return true;
}

bool FfmpegH264Decoder::takeOutput(
    qint64 audioPositionMilliseconds,
    OutputFrame *output,
    int *serial)
{
    if (!output || !serial)
        return false;
    QTime mutexWaitClock;
    mutexWaitClock.start();
    QMutexLocker locker(&m_mutex);
    m_outputMutexWaitMilliseconds += mutexWaitClock.elapsed();
    m_audioPositionMilliseconds = audioPositionMilliseconds;
    if (m_output.isEmpty())
        return false;

    // Decoder output is already in presentation order. Select the newest
    // frame currently due against the audio clock, then remove all older due
    // frames in one batch. This keeps the queue small and fresh without the
    // old one-by-one late-drop feedback loop.
    int selected = -1;
    int severelyLate = 0;
    int index;
    for (index = 0; index < m_output.size(); ++index) {
        const OutputFrame &candidate = m_output.at(index);
        if (candidate.pts == AV_NOPTS_VALUE ||
            candidate.pts <= audioPositionMilliseconds +
                KOutputDueToleranceMilliseconds) {
            selected = index;
            if (candidate.pts != AV_NOPTS_VALUE &&
                candidate.pts + KOutputSeverelyLateMilliseconds <
                    audioPositionMilliseconds)
                ++severelyLate;
        } else {
            break;
        }
    }
    if (selected < 0)
        return false;
    const OutputFrame selectedOutput = m_output.at(selected);
    const int dropped = selected;
    m_output.remove(0, selected + 1);
    if (dropped > 0) {
        m_outputDropCount += dropped;
    }
    m_outputSpace.wakeOne();
    if (*serial == selectedOutput.serial)
        return false;
    *output = selectedOutput;
    *serial = selectedOutput.serial;
    return true;
}

void FfmpegH264Decoder::setFailure(int errorCode, const char *stage)
{
    QMutexLocker locker(&m_mutex);
    if (m_abort)
        return;
    m_error = errorCode == 0 ? -1 : errorCode;
    m_state = DecoderFailed;
    qDebug() << "WW:FFMPEG_SOFT_ERROR" << stage << m_error
             << m_pictureCount << m_input.size();
    m_inputReady.wakeAll();
    m_outputSpace.wakeAll();
}

bool FfmpegH264Decoder::enqueueOutput(
    const QImage &image,
    const Yuv420Frame &yuv420,
    qint64 pts)
{
    QMutexLocker locker(&m_mutex);
    // This wait is bounded-queue backpressure only; the decoder never sleeps
    // on a PTS or audio-clock deadline. The presenter remains the sole timing
    // authority and decides when to display or batch-drop output pictures.
    while (!m_abort && m_output.size() >= MaximumOutputFrames)
        m_outputSpace.wait(&m_mutex);
    if (m_abort)
        return false;
    OutputFrame output;
    output.image = image;
    output.yuv420 = yuv420;
    output.pts = pts;
    output.serial = ++m_nextSerial;
    output.yuv420.pts = pts;
    output.yuv420.serial = output.serial;
    m_output.append(output);
    ++m_pictureCount;
    m_lastOutputPts = pts;
    m_state = DecoderHasPictures;
    if (m_pictureCount == 1) {
        qDebug() << "WW:FFMPEG_SOFT_FIRST_FRAME"
                 << (yuv420.isValid()
                      ? QSize(yuv420.width, yuv420.height) : image.size())
                 << pts << m_codedWidth << m_codedHeight
                 << (m_yuv420Output ? "YUV420_GLES" : "RGB565_DEFERRED");
    }
    return true;
}

void FfmpegH264Decoder::run()
{
    avcodec_register_all();
    AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        setFailure(-1, "find-decoder");
        return;
    }
    AVCodecContext *context = avcodec_alloc_context3(codec);
    AVFrame *decoded = av_frame_alloc();
    if (!context || !decoded) {
        av_frame_free(&decoded);
        avcodec_free_context(&context);
        setFailure(-12, "allocate");
        return;
    }
    context->width = m_codedWidth;
    context->height = m_codedHeight;
    context->thread_count = 1;
    context->flags2 |= AV_CODEC_FLAG2_FAST;
    // PotatoStream's useful ARM11 lesson is to remove deblocking from the
    // steady-state software path, not to copy its 3DS-only Y2RU ABI. H.264
    // reference reconstruction remains intact; only the optional visual
    // post-filter is skipped. This is a deliberate speed/quality tradeoff for
    // 640x360 fallback on a single ARM1176 core.
    context->skip_frame = AVDISCARD_DEFAULT;
#ifdef WILIWILI_FFMPEG_SKIP_NONREF_LOOP_FILTER
    // A/B policy: deblock reference pictures normally, but skip the filter
    // for non-reference pictures. The latter still decode and remain
    // displayable; only their optional post-filter work is removed.
    context->skip_loop_filter = AVDISCARD_NONREF;
#else
    context->skip_loop_filter = AVDISCARD_DEFAULT;
#endif
    context->time_base.num = 1;
    context->time_base.den = 1000;
    context->pkt_timebase.num = 1;
    context->pkt_timebase.den = 1000;
    const int openError = avcodec_open2(context, codec, 0);
    if (openError < 0) {
        av_frame_free(&decoded);
        avcodec_free_context(&context);
        setFailure(openError, "open");
        return;
    }
    {
        QMutexLocker locker(&m_mutex);
        if (!m_abort)
            m_state = DecoderRunning;
    }
    qDebug() << "WW:FFMPEG_SOFT_READY" << codec->name
             << m_codedWidth << m_codedHeight
#ifdef WILIWILI_FFMPEG_ARMV6_ASM
#ifdef WILIWILI_ENABLE_FFMPEG_LATE_PRESENTATION_DROP
             << (m_yuv420Output
                 ? "ARM11_GENERIC_H264_GLES_YUV420_LATEDROP"
                 : "ARM11_GENERIC_H264_RGB565_LUT2X2_LATEDROP");
#else
             << (m_yuv420Output
                 ? "ARM11_GENERIC_H264_GLES_YUV420"
                 : "ARM11_GENERIC_H264_RGB565_LUT2X2");
#endif
#else
             << (m_yuv420Output ? "PURE_C_GLES_YUV420"
                                : "PURE_C_RGB565");
#endif
#ifdef WILIWILI_FFMPEG_SKIP_NONREF_LOOP_FILTER
    qDebug() << "WW:FFMPEG_SOFT_LOOP_FILTER" << "NONREF_ONLY";
#else
    qDebug() << "WW:FFMPEG_SOFT_LOOP_FILTER" << "ALL_FRAMES";
#endif

    QTime performanceClock;
    performanceClock.start();
    qint64 totalDecodeMilliseconds = 0;
    qint64 totalRepackCopyMilliseconds = 0;
    int lateConversionDrops = 0;
    bool catchUp = false;
    qint64 highLagSinceMilliseconds = -1;
    qint64 lowLagSinceMilliseconds = -1;
    bool draining = false;
    while (true) {
        InputUnit input;
        input.dts = AV_NOPTS_VALUE;
        input.pts = AV_NOPTS_VALUE;
        {
            QMutexLocker locker(&m_mutex);
            while (!m_abort && m_input.isEmpty() && !m_inputFinished)
                m_inputReady.wait(&m_mutex);
            if (m_abort)
                break;
            if (m_input.isEmpty() && m_inputFinished) {
                draining = true;
            } else {
                input = m_input.first();
                m_input.remove(0);
            }
        }

        QByteArray padded;
        AVPacket packet;
        av_init_packet(&packet);
        if (!draining) {
            const int payloadSize = input.bytes.size();
            padded = input.bytes;
            padded.append(QByteArray(AV_INPUT_BUFFER_PADDING_SIZE, '\0'));
            packet.data = reinterpret_cast<uint8_t *>(padded.data());
            packet.size = payloadSize;
            packet.dts = input.dts;
            packet.pts = input.pts;
        } else {
            packet.data = 0;
            packet.size = 0;
            packet.dts = AV_NOPTS_VALUE;
            packet.pts = AV_NOPTS_VALUE;
        }

        if (!draining) {
            qint64 audioPosition = 0;
            {
                QMutexLocker locker(&m_mutex);
                audioPosition = m_audioPositionMilliseconds;
            }
            const qint64 lag = audioPosition - input.dts;
            const qint64 nowMilliseconds = performanceClock.elapsed();
            if (!catchUp) {
                lowLagSinceMilliseconds = -1;
                if (lag > KCatchupEnterLagMilliseconds) {
                    if (highLagSinceMilliseconds < 0)
                        highLagSinceMilliseconds = nowMilliseconds;
                    if (nowMilliseconds - highLagSinceMilliseconds >=
                        KCatchupConfirmMilliseconds) {
                        catchUp = true;
                        lowLagSinceMilliseconds = -1;
                        {
                            QMutexLocker locker(&m_mutex);
                            m_catchupActive = true;
                            ++m_catchupEnterCount;
                            m_catchupStartedMilliseconds = nowMilliseconds;
                        }
                        context->skip_frame = AVDISCARD_NONREF;
#ifdef WILIWILI_FFMPEG_SKIP_NONREF_LOOP_FILTER
                        context->skip_loop_filter = AVDISCARD_NONREF;
#else
                        context->skip_loop_filter = AVDISCARD_DEFAULT;
#endif
                        qDebug() << "WW:FFMPEG_SOFT_CATCHUP" << true
                                 << lag << audioPosition << input.dts
                                 << "SKIP_NONREF";
                    }
                } else {
                    highLagSinceMilliseconds = -1;
                }
            } else {
                highLagSinceMilliseconds = -1;
                if (lag < KCatchupExitLagMilliseconds) {
                    if (lowLagSinceMilliseconds < 0)
                        lowLagSinceMilliseconds = nowMilliseconds;
                    if (nowMilliseconds - lowLagSinceMilliseconds >=
                        KCatchupConfirmMilliseconds) {
                        catchUp = false;
                        {
                            QMutexLocker locker(&m_mutex);
                            m_catchupActive = false;
                            if (m_catchupStartedMilliseconds >= 0 &&
                                nowMilliseconds >
                                    m_catchupStartedMilliseconds) {
                                m_catchupMilliseconds +=
                                    nowMilliseconds -
                                    m_catchupStartedMilliseconds;
                            }
                            m_catchupStartedMilliseconds = -1;
                        }
                        context->skip_frame = AVDISCARD_DEFAULT;
#ifdef WILIWILI_FFMPEG_SKIP_NONREF_LOOP_FILTER
                        context->skip_loop_filter = AVDISCARD_NONREF;
#else
                        context->skip_loop_filter = AVDISCARD_DEFAULT;
#endif
                        qDebug() << "WW:FFMPEG_SOFT_CATCHUP" << false
                                 << lag << audioPosition << input.dts
                                 << "NORMAL";
                    }
                } else {
                    lowLagSinceMilliseconds = -1;
                }
            }
        }

        int gotPicture = 0;
        QTime decodeClock;
        decodeClock.start();
        const int decodeResult = avcodec_decode_video2(
            context, decoded, &gotPicture, &packet);
        totalDecodeMilliseconds += decodeClock.elapsed();
        {
            QMutexLocker locker(&m_mutex);
            m_decodeMilliseconds = totalDecodeMilliseconds;
            m_performanceMilliseconds = performanceClock.elapsed();
        }
        if (decodeResult < 0) {
            setFailure(decodeResult, "decode");
            break;
        }
        if (gotPicture) {
            {
                QMutexLocker locker(&m_mutex);
                ++m_decodedPictureCount;
            }
            qint64 pts = decoded->best_effort_timestamp;
            if (pts == AV_NOPTS_VALUE)
                pts = decoded->pkt_pts;
            if (pts == AV_NOPTS_VALUE)
                pts = input.pts;

#ifdef WILIWILI_ENABLE_FFMPEG_LATE_PRESENTATION_DROP
            // Diagnosis-only control. A decoded reference picture may still be
            // required by later H.264 pictures, so this drops presentation work
            // only. Nokia 603 testing advanced the media clock but left about
            // 6-7 visible fps; normal ffmpegsoft2 builds therefore keep it off.
            qint64 audioPosition = 0;
            {
                QMutexLocker locker(&m_mutex);
                audioPosition = m_audioPositionMilliseconds;
            }
            if (audioPosition > 0 && pts != AV_NOPTS_VALUE &&
                pts + 180 < audioPosition) {
                ++lateConversionDrops;
                av_frame_unref(decoded);
                continue;
            }
#endif

            Yuv420Frame yuv420;
            QTime repackClock;
            repackClock.start();
            // Keep the decoder-side handoff cheap for both output modes.  In
            // the CPU RGB565 mode this is important: stale pictures are
            // discarded by takeOutput() before the selected YUV picture is
            // converted.  GLES retains the same packed-plane handoff.
            const bool outputReady = copyYuv420Planes(decoded, &yuv420);
            if (!outputReady) {
                setFailure(-5, "pixel-format");
                break;
            }
            totalRepackCopyMilliseconds += repackClock.elapsed();
            {
                QMutexLocker locker(&m_mutex);
                m_repackCopyMilliseconds = totalRepackCopyMilliseconds;
                m_performanceMilliseconds = performanceClock.elapsed();
            }
            if (!enqueueOutput(QImage(), yuv420, pts))
                break;
            {
                QMutexLocker locker(&m_mutex);
                m_performanceMilliseconds = performanceClock.elapsed();
            }
            const int pictures = pictureCount();
        }
        av_frame_unref(decoded);
        if (draining && !gotPicture) {
            QMutexLocker locker(&m_mutex);
            if (!m_abort)
                m_state = DecoderEnded;
            qDebug() << "WW:FFMPEG_SOFT_END"
                     << m_pictureCount << m_decodedPictureCount
                     << m_outputDropCount;
            break;
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        if (m_catchupActive && m_catchupStartedMilliseconds >= 0 &&
            m_performanceMilliseconds > m_catchupStartedMilliseconds) {
            m_catchupMilliseconds +=
                m_performanceMilliseconds - m_catchupStartedMilliseconds;
        }
        m_catchupActive = false;
        m_catchupStartedMilliseconds = -1;
    }

    avcodec_close(context);
    av_frame_free(&decoded);
    avcodec_free_context(&context);
}

} // namespace wiliwili
