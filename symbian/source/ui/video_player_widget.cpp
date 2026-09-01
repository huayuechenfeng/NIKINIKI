#include "ui/video_player_widget.h"
#include "platform/flv_live_demuxer.h"
#include "platform/mp4_avc_probe_reader.h"
#include "platform/video_playback_backend.h"
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
#include "platform/devvideo_direct_probe.h"
#endif

#include <QtCore/QTimerEvent>
#include <QtCore/QUrl>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QEvent>
#include <QtCore/QFile>
#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtGui/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QDesktopWidget>
#include <QtGui/QFontMetrics>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <qmediacontent.h>
#ifdef Q_OS_SYMBIAN
#include <e32std.h>
#include <eikenv.h>
#include <aknappui.h>
#include <apgtask.h>
#include <f32file.h>
#endif

namespace wiliwili {

// A growing local media file must opt into Symbian read/write sharing before
// MMF opens a second handle. QFile's default native share mode makes
// CVideoPlayerUtility2::OpenFileL() fail with KErrInUse (-14) while the
// download is still active.
class LocalDownloadWriter
{
public:
    LocalDownloadWriter()
#ifdef Q_OS_SYMBIAN
        : m_sessionConnected(false), m_open(false)
#endif
    {
    }

    ~LocalDownloadWriter()
    {
        close();
    }

    bool open(const QString &path)
    {
#ifdef Q_OS_SYMBIAN
        close();
        TInt error = m_fileSession.Connect();
        if (error != KErrNone)
            return false;
        m_sessionConnected = true;
        error = m_fileSession.ShareProtected();
        if (error != KErrNone) {
            close();
            return false;
        }
        const QString nativePath = QDir::toNativeSeparators(path);
        const TPtrC descriptor(
            reinterpret_cast<const TUint16 *>(nativePath.utf16()),
            nativePath.length());
        error = m_file.Replace(
            m_fileSession, descriptor,
            EFileWrite | EFileShareReadersOrWriters);
        if (error != KErrNone) {
            close();
            return false;
        }
        m_open = true;
        return true;
#else
        m_file.setFileName(path);
        return m_file.open(QIODevice::WriteOnly);
#endif
    }

    qint64 write(const QByteArray &data)
    {
#ifdef Q_OS_SYMBIAN
        if (!m_open)
            return -1;
        const TPtrC8 bytes(
            reinterpret_cast<const TUint8 *>(data.constData()),
            data.size());
        return m_file.Write(bytes) == KErrNone ? data.size() : -1;
#else
        return m_file.write(data);
#endif
    }

    bool flush()
    {
#ifdef Q_OS_SYMBIAN
        return m_open && m_file.Flush() == KErrNone;
#else
        return m_file.flush();
#endif
    }

    void close()
    {
#ifdef Q_OS_SYMBIAN
        if (m_open) {
            m_file.Close();
            m_open = false;
        }
        if (m_sessionConnected) {
            m_fileSession.Close();
            m_sessionConnected = false;
        }
#else
        m_file.close();
#endif
    }

private:
#ifdef Q_OS_SYMBIAN
    RFs m_fileSession;
    RFile m_file;
    bool m_sessionConnected;
    bool m_open;
#else
    QFile m_file;
#endif
};

static VideoOverlayWidget *g_persistentVideoOverlay = 0;

static const int KAvcProbeHeaderInitialBytes = 1536 * 1024;
static const int KAvcProbeHeaderMiddleBytes = 3 * 1024 * 1024;
static const int KAvcProbeHeaderMaximumBytes = 6 * 1024 * 1024;
static const int KAvcProbeStalledPollLimit = 20; // 10 seconds at 500 ms.
static const int KAvcProbeTotalPollLimit = 120;  // 60 seconds at 500 ms.
static const int KOverlayFrameIntervalMilliseconds = 40; // 25 fps ceiling.
static const int KSoftOverlayFrameIntervalMilliseconds = 33; // ~30 fps.
static const int KSoftPositionRefreshMilliseconds = 500;
// Nokia 603 can open the shared growing file at 2 MiB, but the decoder then
// catches the network writer during the first seconds. Eight MiB remains a
// small on-disk head start while avoiding that immediately visible stutter.
static const qint64 KOpenFileStreamingStartBytes = 8LL * 1024LL * 1024LL;
// AAC is much smaller than the original interleaved FLV.  Keep roughly
// several seconds of audio on disk before MMF opens the shared ADTS stream;
// video starts only after MMF has prepared and an IDR-led batch is ready.
static const qint64 KLiveAacStartBytes = 96LL * 1024LL;
static const qint64 KLiveAacMaximumBytes = 64LL * 1024LL * 1024LL;
static const int KLiveVideoStartUnits = 45;
static const int KLiveVideoPendingMaximumUnits = 900;

static QByteArray playbackMimeTypeForUrl(
    const QString &sourceUrl, int variant)
{
    const QString path = QUrl(sourceUrl).path().toLower();
    if (path.endsWith(QString::fromLatin1(".m3u8"))) {
        if (variant == 0)
            return QByteArray("application/x-mpegURL");
        if (variant == 1)
            return QByteArray("application/vnd.apple.mpegurl");
        return QByteArray();
    }
    if (path.endsWith(QString::fromLatin1(".flv"))) {
        if (variant == 0)
            return QByteArray("video/flv");
        if (variant == 1)
            return QByteArray("video/x-flv");
        return QByteArray();
    }
    return QByteArray();
}

#ifdef Q_OS_SYMBIAN
typedef TUint32 OverlayFastCounterValue;

static OverlayFastCounterValue overlayFastCounterNow()
{
    return User::FastCounter();
}

// The Symbian SR1 SDK used by the project exposes User::FastCounter(), but
// not a stable HAL import library in every installed SDK image. Calibrate its
// device-specific frequency once against the already available monotonic
// QTime clock; all subsequent stage measurements are derived from the fast
// counter and handle the 32-bit counter wrap by unsigned subtraction.
static qint64 overlayFastCounterElapsedMilliseconds(
    OverlayFastCounterValue start)
{
    static OverlayFastCounterValue calibrationStart = 0;
    static QTime calibrationClock;
    static TInt frequency = 0;
    if (frequency <= 0) {
        if (!calibrationClock.isValid()) {
            calibrationStart = User::FastCounter();
            calibrationClock.start();
        } else {
            const int wallMilliseconds = calibrationClock.elapsed();
            if (wallMilliseconds >= 200) {
                const TUint32 ticks =
                    User::FastCounter() - calibrationStart;
                if (ticks > 0) {
                    const quint64 estimated =
                        (static_cast<quint64>(ticks) * 1000) /
                        static_cast<quint64>(wallMilliseconds);
                    if (estimated > 0 && estimated <= 1000000000ULL)
                        frequency = static_cast<TInt>(estimated);
                }
            }
        }
    }
    if (frequency <= 0)
        return 0;
    const TUint32 ticks = User::FastCounter() - start;
    return static_cast<qint64>(
        (static_cast<quint64>(ticks) * 1000) /
        static_cast<quint64>(frequency));
}
#else
typedef qint64 OverlayFastCounterValue;

static OverlayFastCounterValue overlayFastCounterNow()
{
    return 0;
}

static qint64 overlayFastCounterElapsedMilliseconds(
    OverlayFastCounterValue start)
{
    Q_UNUSED(start);
    return 0;
}
#endif

// Keep the local FFmpeg decoder fed while the next HTTP Range reply is in
// flight.  Two-second batches were too shallow for the Nokia 603 network
// stack: a delayed reply drained the input queue and forced catch-up.  The
// reader still caps each request at 6 MiB and 480 samples.
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
static const int KAvcSoftBatchDurationMilliseconds = 16000;
static const int KAvcSoftPrefetchThresholdUnits = 420;
static const int KAvcProbePrefetchThresholdUnits =
    KAvcSoftPrefetchThresholdUnits;
#else
static const int KAvcProbePrefetchThresholdUnits = 60;
#endif

static QString playerTimeText(qint64 milliseconds)
{
    int seconds = static_cast<int>(qMax<qint64>(0, milliseconds) / 1000);
    const int hours = seconds / 3600;
    seconds %= 3600;
    const int minutes = seconds / 60;
    const int remainder = seconds % 60;
    if (hours > 0) {
        return QString::fromLatin1("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(remainder, 2, 10, QLatin1Char('0'));
    }
    return QString::fromLatin1("%1:%2")
        .arg(minutes)
        .arg(remainder, 2, 10, QLatin1Char('0'));
}

static void requestPlayerPlatformForeground()
{
#ifdef Q_OS_SYMBIAN
    CEikonEnv *environment = CEikonEnv::Static();
    if (!environment)
        return;
    TApaTask task(environment->WsSession());
    task.SetWgId(environment->RootWin().Identifier());
    if (task.Exists())
        task.BringToForeground();
#endif
}

static QString playerRectText(const QRect &rect)
{
    return QString::fromLatin1("%1,%2 %3x%4")
        .arg(rect.x())
        .arg(rect.y())
        .arg(rect.width())
        .arg(rect.height());
}

static int requestPlayerOrientation(bool landscape)
{
#ifdef Q_OS_SYMBIAN
    CEikonEnv *environment = CEikonEnv::Static();
    if (!environment || !environment->EikAppUi())
        return KErrNotReady;

    CAknAppUiBase *appUi =
        static_cast<CAknAppUiBase *>(environment->EikAppUi());
    if (!appUi->OrientationCanBeChanged())
        return KErrNotSupported;

    TInt error = KErrNone;
    const CAknAppUiBase::TAppUiOrientation orientation = landscape
        ? CAknAppUiBase::EAppUiOrientationLandscape
        : CAknAppUiBase::EAppUiOrientationPortrait;
    TRAP(error, appUi->SetOrientationL(orientation));
    return error;
#else
    Q_UNUSED(landscape);
    return -5;
#endif
}

// Software video has its own opaque native child window.  The persistent
// ARGB tool window remains above it and contains only danmaku and controls.
// Keeping RGB565 out of that transparent window avoids the expensive
// full-screen RGB565 -> ARGB composition measured on the Nokia 603.
class SoftVideoSurfaceWidget : public QWidget
{
public:
    explicit SoftVideoSurfaceWidget(QWidget *parent)
        : QWidget(parent), m_active(false), m_firstPaintPending(false),
          m_framePts(-1), m_lastPresentedPts(-1), m_frameSerial(0),
          m_paintedSerial(0), m_presentedCount(0),
          m_replacedBeforePaintCount(0), m_paintCount(0),
          m_paintMilliseconds(0), m_clearMilliseconds(0),
          m_videoDrawMilliseconds(0), m_painterEndMilliseconds(0),
          m_otherMilliseconds(0)
    {
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_QuitOnClose, false);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
        hide();
    }

    void activateSurface()
    {
        if (!m_active) {
            resetTelemetry();
            m_active = true;
            m_firstPaintPending = true;
        }
        show();
        raise();
        update();
        qDebug() << "WW:SOFT_SURFACE_ACTIVE"
                 << static_cast<void *>(this)
                 << geometry() << static_cast<void *>(winId());
    }

    void deactivateSurface()
    {
        m_active = false;
        m_frame = QImage();
        m_framePts = -1;
        ++m_frameSerial;
        hide();
    }

    bool isSurfaceActive() const
    {
        return m_active;
    }

    void setFrame(const QImage &frame, qint64 pts)
    {
        if (!m_active || frame.isNull())
            return;
        if (m_frameSerial != m_paintedSerial && !m_frame.isNull())
            ++m_replacedBeforePaintCount;
        m_frame = frame;
        m_framePts = pts;
        ++m_frameSerial;
        update();
    }

    quint64 presentedCount() const
    {
        return m_presentedCount;
    }

    qint64 lastPresentedPts() const
    {
        return m_lastPresentedPts;
    }

    QString telemetry() const
    {
        return QString::fromLatin1(
            " softSurfaceActive=%1 softSurfacePresented=%2 "
            "softSurfaceReplaced=%3 softSurfacePaintCount=%4 "
            "softSurfacePaintMs=%5 softSurfaceClearMs=%6 "
            "softSurfaceVideoDrawMs=%7 softSurfacePainterEndMs=%8 "
            "softSurfaceOtherMs=%9 softSurfaceLastPts=%10")
            .arg(m_active ? 1 : 0)
            .arg(static_cast<qulonglong>(m_presentedCount))
            .arg(static_cast<qulonglong>(m_replacedBeforePaintCount))
            .arg(static_cast<qulonglong>(m_paintCount))
            .arg(m_paintMilliseconds)
            .arg(m_clearMilliseconds)
            .arg(m_videoDrawMilliseconds)
            .arg(m_painterEndMilliseconds)
            .arg(m_otherMilliseconds)
            .arg(m_lastPresentedPts);
    }

protected:
    virtual void paintEvent(QPaintEvent *event)
    {
        Q_UNUSED(event);
        if (!m_active)
            return;

        const OverlayFastCounterValue paintStart =
            overlayFastCounterNow();
        QTime paintClock;
        paintClock.start();
        QPainter painter(this);
        if (!painter.isActive())
            return;

        if (m_firstPaintPending) {
            m_firstPaintPending = false;
            qDebug() << "WW:SOFT_SURFACE_FIRST_PAINT"
                     << size() << m_frame.size()
                     << static_cast<int>(m_frame.format());
        }

        const OverlayFastCounterValue clearStart =
            overlayFastCounterNow();
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        QRect destination;
        if (m_frame.isNull()) {
            painter.fillRect(rect(), Qt::black);
        } else {
            QSize fitted = m_frame.size();
            fitted.scale(size(), Qt::KeepAspectRatio);
            destination = QRect(
                (width() - fitted.width()) / 2,
                (height() - fitted.height()) / 2,
                fitted.width(), fitted.height());
            // Do not clear the full screen when a landscape RGB565 frame
            // already covers it. For letterboxed media, touch only the bars.
            if (destination.top() > 0)
                painter.fillRect(
                    QRect(0, 0, width(), destination.top()), Qt::black);
            if (destination.bottom() + 1 < height())
                painter.fillRect(
                    QRect(0, destination.bottom() + 1, width(),
                          height() - destination.bottom() - 1), Qt::black);
            if (destination.left() > 0)
                painter.fillRect(
                    QRect(0, destination.top(), destination.left(),
                          destination.height()), Qt::black);
            if (destination.right() + 1 < width())
                painter.fillRect(
                    QRect(destination.right() + 1, destination.top(),
                          width() - destination.right() - 1,
                          destination.height()), Qt::black);
        }
        const qint64 clearElapsed =
            overlayFastCounterElapsedMilliseconds(clearStart);
        m_clearMilliseconds += clearElapsed;

        const OverlayFastCounterValue videoStart =
            overlayFastCounterNow();
        if (!m_frame.isNull()) {
            painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
            painter.drawImage(destination, m_frame);
        }
        const qint64 videoElapsed =
            overlayFastCounterElapsedMilliseconds(videoStart);
        m_videoDrawMilliseconds += videoElapsed;

        const OverlayFastCounterValue endStart =
            overlayFastCounterNow();
        painter.end();
        const qint64 endElapsed =
            overlayFastCounterElapsedMilliseconds(endStart);
        m_painterEndMilliseconds += endElapsed;

        qint64 elapsed =
            overlayFastCounterElapsedMilliseconds(paintStart);
        if (elapsed <= 0)
            elapsed = paintClock.elapsed();
        ++m_paintCount;
        m_paintMilliseconds += elapsed;
        const qint64 stages = clearElapsed + videoElapsed + endElapsed;
        if (elapsed > stages)
            m_otherMilliseconds += elapsed - stages;

        if (!m_frame.isNull() && m_paintedSerial != m_frameSerial) {
            m_paintedSerial = m_frameSerial;
            m_lastPresentedPts = m_framePts;
            ++m_presentedCount;
        }
    }

private:
    void resetTelemetry()
    {
        m_frame = QImage();
        m_framePts = -1;
        m_lastPresentedPts = -1;
        m_frameSerial = 0;
        m_paintedSerial = 0;
        m_presentedCount = 0;
        m_replacedBeforePaintCount = 0;
        m_paintCount = 0;
        m_paintMilliseconds = 0;
        m_clearMilliseconds = 0;
        m_videoDrawMilliseconds = 0;
        m_painterEndMilliseconds = 0;
        m_otherMilliseconds = 0;
    }

    bool m_active;
    bool m_firstPaintPending;
    QImage m_frame;
    qint64 m_framePts;
    qint64 m_lastPresentedPts;
    int m_frameSerial;
    int m_paintedSerial;
    quint64 m_presentedCount;
    quint64 m_replacedBeforePaintCount;
    quint64 m_paintCount;
    qint64 m_paintMilliseconds;
    qint64 m_clearMilliseconds;
    qint64 m_videoDrawMilliseconds;
    qint64 m_painterEndMilliseconds;
    qint64 m_otherMilliseconds;
};

class VideoOverlayWidget : public QWidget
{
public:
    explicit VideoOverlayWidget(QWidget *lifetimeOwner)
        : QWidget(lifetimeOwner, Qt::Tool | Qt::FramelessWindowHint |
                          Qt::WindowStaysOnTopHint),
          m_owner(0), m_updateTimerId(0),
          m_updateIntervalMilliseconds(KOverlayFrameIntervalMilliseconds),
          m_nativeWindowPrepared(false), m_danmakuEnabled(true),
          m_nativeDanmakuActive(false), m_danmakuWasVisible(false),
          m_controlsVisible(true), m_controlsAge(0), m_pressed(false),
          m_controlsVisibleOnPress(true),
          m_draggingProgress(false), m_draggingVolume(false),
          m_dragPreviewPosition(0),
          m_speedIndex(0), m_qualityMenuVisible(false),
          m_firstPaintPending(false),
          m_decoderFrameSerial(0),
          m_framePositionMilliseconds(0), m_framePositionValid(false),
          m_positionCacheBaseMilliseconds(0),
          m_positionCacheRate(1.0), m_positionCacheValid(false),
          m_positionCachePlaying(false), m_positionCacheHitCount(0),
          m_timerEventCount(0), m_timerElapsedMilliseconds(0),
          m_timerMaxGapMilliseconds(0), m_paintEventCount(0),
          m_paintMilliseconds(0), m_positionQueryCount(0),
          m_positionMilliseconds(0),
          m_overlayClearMilliseconds(0),
          m_overlayVideoDrawMilliseconds(0),
          m_overlayIntermediateMilliseconds(0),
          m_overlayRotateMilliseconds(0),
          m_overlayDanmakuMilliseconds(0),
          m_overlayControlsMilliseconds(0),
          m_overlayPainterEndMilliseconds(0),
          m_overlayOtherMilliseconds(0),
          m_scrollingDanmakuFloorMilliseconds(0)
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
          , m_directProbeMode(false), m_directProbeTouchActive(false),
          m_directProbeTouchPoint(0, 0)
#endif
    {
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_QuitOnClose, false);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::StrongFocus);
        qDebug() << "WW:PLAYER_OVERLAY_PERSISTENT_NEW"
                 << static_cast<void *>(this);
    }

    virtual ~VideoOverlayWidget()
    {
        if (m_updateTimerId)
            killTimer(m_updateTimerId);
        m_updateTimerId = 0;
        m_owner = 0;
        if (g_persistentVideoOverlay == this)
            g_persistentVideoOverlay = 0;
        qDebug() << "WW:PLAYER_OVERLAY_PERSISTENT_DESTROY"
                 << static_cast<void *>(this);
    }

    void attachOwner(VideoPlayerWidget *owner)
    {
        if (!owner)
            return;
        if (m_owner == owner) {
            if (!m_updateTimerId)
                m_updateTimerId = startTimer(
                    m_updateIntervalMilliseconds);
            return;
        }
        // This top-level ARGB window belongs to the application, not to an
        // individual MMF session. Keeping its native backing store alive
        // avoids the Qt 4.7/WSERV destroy-create path that crashed before the
        // second session reached its delayed MMF rebuild.
        m_owner = owner;
        m_danmaku.clear();
        m_danmakuTextWidths.clear();
        m_danmakuDisplayStartMilliseconds.clear();
        m_nativeDanmakuActive = false;
        m_danmakuWasVisible = false;
        m_controlsVisible = true;
        m_controlsAge = 0;
        m_pressed = false;
        m_controlsVisibleOnPress = true;
        m_draggingProgress = false;
        m_draggingVolume = false;
        m_dragPreviewPosition = 0;
        m_speedIndex = 0;
        m_qualityMenuVisible = false;
        m_firstPaintPending = true;
        m_decoderFrameSerial = 0;
        m_framePositionMilliseconds = 0;
        m_framePositionValid = false;
        invalidatePositionCache();
        resetPacingTelemetry();
        m_scrollingDanmakuFloorMilliseconds = 0;
        m_updateIntervalMilliseconds = KOverlayFrameIntervalMilliseconds;
        if (!m_updateTimerId)
            m_updateTimerId = startTimer(
                m_updateIntervalMilliseconds);
        qDebug() << "WW:PLAYER_OVERLAY_ATTACH"
                 << static_cast<void *>(this)
                 << static_cast<void *>(owner);
    }

#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    void setDirectProbeMode(bool enabled)
    {
        m_directProbeMode = enabled;
        m_directProbeTouchActive = false;
        m_directProbeTouchPoint = QPoint(0, 0);
        if (enabled) {
            m_controlsVisible = true;
            m_controlsAge = 0;
            m_directProbeClock.start();
            qDebug() << "WW:DIRECT_OVERLAY_MODE" << "enabled";
        } else {
            qDebug() << "WW:DIRECT_OVERLAY_MODE" << "disabled";
        }
        update();
    }
#endif

    void setFrameInterval(int milliseconds)
    {
        const int interval = qMax(1, milliseconds);
        if (m_updateIntervalMilliseconds == interval && m_updateTimerId)
            return;
        m_updateIntervalMilliseconds = interval;
        if (m_updateTimerId) {
            killTimer(m_updateTimerId);
            m_updateTimerId = startTimer(m_updateIntervalMilliseconds);
        }
    }

    void detachOwner(VideoPlayerWidget *owner)
    {
        if (!owner || m_owner.data() != owner)
            return;
        if (m_updateTimerId)
            killTimer(m_updateTimerId);
        m_updateTimerId = 0;
        m_updateIntervalMilliseconds = KOverlayFrameIntervalMilliseconds;
        // Clear the guarded owner before hiding the native window. Any paint,
        // timer, or pointer event already queued by Qt can then return without
        // touching the parked VideoPlayerWidget while browsing resumes.
        m_owner = 0;
        m_danmaku.clear();
        m_danmakuTextWidths.clear();
        m_danmakuDisplayStartMilliseconds.clear();
        m_nativeDanmakuActive = false;
        m_danmakuWasVisible = false;
        m_pressed = false;
        m_draggingProgress = false;
        m_draggingVolume = false;
        m_qualityMenuVisible = false;
        m_decoderFrameSerial = 0;
        m_framePositionMilliseconds = 0;
        m_framePositionValid = false;
        invalidatePositionCache();
        resetPacingTelemetry();
        m_scrollingDanmakuFloorMilliseconds = 0;
        hide();
        qDebug() << "WW:PLAYER_OVERLAY_DETACH"
                 << static_cast<void *>(this)
                 << static_cast<void *>(owner);
    }

    void prepareNativeWindow()
    {
        // WA_TranslucentBackground selects an ARGB backing store before the
        // native handle is created.  Explicitly enable WSERV alpha blending as
        // well: the MMF video surface is a separate native RWindow and must be
        // allowed to show through every untouched overlay pixel.
#ifdef Q_OS_SYMBIAN
        if (m_nativeWindowPrepared) {
            qDebug() << "WW:PLAYER_OVERLAY_NATIVE_REUSE"
                     << static_cast<void *>(this);
            return;
        }
        CCoeControl *control = winId();
        TInt alphaError = KErrNotReady;
        if (control && control->OwnsWindow() && control->DrawableWindow()) {
            RWindow *window =
                static_cast<RWindow *>(control->DrawableWindow());
            alphaError = window->SetTransparencyAlphaChannel();
        }
        qDebug() << "WW:PLAYER_OVERLAY_ALPHA"
                 << static_cast<void *>(control) << alphaError;
        m_nativeWindowPrepared = control && alphaError == KErrNone;
#else
        winId();
        m_nativeWindowPrepared = true;
#endif
    }

    void setDanmaku(
        VideoPlayerWidget *owner,
        const QVector<DanmakuItemCompat> &items)
    {
        if (m_owner.data() != owner)
            return;
        m_danmaku = items;
        m_danmakuTextWidths.clear();
        m_danmakuDisplayStartMilliseconds.clear();
        QFont widthFont = QApplication::font();
        widthFont.setBold(true);
        int widthIndex;
        for (widthIndex = 0; widthIndex < m_danmaku.size(); ++widthIndex) {
            widthFont.setPixelSize(qBound(
                13, m_danmaku.at(widthIndex).fontSize * 2 / 3, 24));
            m_danmakuTextWidths.append(
                QFontMetrics(widthFont).width(
                    m_danmaku.at(widthIndex).text));
            m_danmakuDisplayStartMilliseconds.append(-1);
        }
        m_danmakuWasVisible = false;
        // Danmaku is fetched after media playback starts. Do not introduce
        // delayed network results halfway across the screen: scrolling items
        // older than this media position were never able to enter at x=right
        // and are intentionally skipped.
        m_scrollingDanmakuFloorMilliseconds =
            owner->m_player ? owner->m_player->position() : 0;
        qDebug() << "WW:DANMAKU_SCROLL_FLOOR"
                 << m_scrollingDanmakuFloorMilliseconds << m_danmaku.size();
        update();
    }

    void sourceChanged()
    {
        m_draggingProgress = false;
        m_draggingVolume = false;
        m_qualityMenuVisible = false;
        m_controlsVisible = true;
        m_controlsAge = 0;
        m_decoderFrameSerial = 0;
        m_framePositionMilliseconds = 0;
        m_framePositionValid = false;
        invalidatePositionCache();
        resetPacingTelemetry();
        if (m_owner && m_owner->m_player)
            m_scrollingDanmakuFloorMilliseconds =
                m_owner->m_player->position();
        int index;
        for (index = 0;
             index < m_danmakuDisplayStartMilliseconds.size(); ++index) {
            m_danmakuDisplayStartMilliseconds[index] = -1;
        }
        m_danmakuWasVisible = false;
        if (m_owner && m_owner->m_isLive)
            m_speedIndex = 0;
        if (m_owner)
            m_owner->updateVideoGeometry(true, false);
        update();
    }

    void seekDanmakuTo(qint64 mediaPositionMilliseconds)
    {
        m_framePositionValid = false;
        invalidatePositionCache();
        m_scrollingDanmakuFloorMilliseconds =
            qMax<qint64>(0, mediaPositionMilliseconds);
        int index;
        for (index = 0;
             index < m_danmakuDisplayStartMilliseconds.size(); ++index) {
            m_danmakuDisplayStartMilliseconds[index] = -1;
        }
        m_danmakuWasVisible = false;
        update();
    }

    void invalidatePositionCache()
    {
        m_positionCacheClock = QTime();
        m_positionCacheBaseMilliseconds = 0;
        m_positionCacheRate = 1.0;
        m_positionCacheValid = false;
        m_positionCachePlaying = false;
    }

    bool controlsVisible() const
    {
        return m_controlsVisible;
    }

    void revealControls()
    {
        m_controlsVisible = true;
        m_controlsAge = 0;
        if (m_owner)
            m_owner->updateVideoGeometry(true, m_qualityMenuVisible);
        update();
    }

    bool qualityMenuVisible() const
    {
        return m_qualityMenuVisible;
    }

    void toggleQualityMenu()
    {
        if (!m_owner || m_owner->m_qualities.isEmpty())
            return;
        m_qualityMenuVisible = !m_qualityMenuVisible;
        m_controlsVisible = true;
        m_controlsAge = 0;
        m_owner->updateVideoGeometry(true, m_qualityMenuVisible);
        update();
    }

    void toggleControlsFromVideo()
    {
        m_qualityMenuVisible = false;
        m_controlsVisible = !m_controlsVisible;
        m_controlsAge = 0;
        if (m_owner)
            m_owner->updateVideoGeometry(m_controlsVisible, false);
        update();
    }

    // PositionL() is a synchronous MMF call on Symbian.  During software
    // playback the timer, danmaku and controls used to query it independently
    // in one UI cycle.  Reuse the timer's clock sample for all three paths;
    // normal MMF playback keeps the old direct query behaviour.
    qint64 presentationPosition() const
    {
        if (!m_owner || !m_owner->m_player)
            return 0;
        if (m_framePositionValid)
            return m_framePositionMilliseconds;
        return m_owner->m_player->position();
    }

    bool hasVisibleDanmaku(qint64 position) const
    {
        if (m_nativeDanmakuActive || !m_danmakuEnabled ||
            m_danmaku.isEmpty()) {
            return false;
        }
        const qint64 earliest = position - 7500;
        int low = 0;
        int high = m_danmaku.size();
        while (low < high) {
            const int middle = low + (high - low) / 2;
            if (m_danmaku.at(middle).timeMilliseconds < earliest)
                low = middle + 1;
            else
                high = middle;
        }
        int index;
        for (index = low; index < m_danmaku.size(); ++index) {
            const qint64 timestamp =
                m_danmaku.at(index).timeMilliseconds;
            if (timestamp > position)
                break;
            if (timestamp >= m_scrollingDanmakuFloorMilliseconds)
                return true;
        }
        return false;
    }

    void resetPacingTelemetry()
    {
        m_timerClock = QTime();
        m_timerEventCount = 0;
        m_timerElapsedMilliseconds = 0;
        m_timerMaxGapMilliseconds = 0;
        m_paintEventCount = 0;
        m_paintMilliseconds = 0;
        m_positionQueryCount = 0;
        m_positionMilliseconds = 0;
        m_positionCacheHitCount = 0;
        m_overlayClearMilliseconds = 0;
        m_overlayVideoDrawMilliseconds = 0;
        m_overlayIntermediateMilliseconds = 0;
        m_overlayRotateMilliseconds = 0;
        m_overlayDanmakuMilliseconds = 0;
        m_overlayControlsMilliseconds = 0;
        m_overlayPainterEndMilliseconds = 0;
        m_overlayOtherMilliseconds = 0;
    }

    qint64 sampleSoftPosition()
    {
        if (!m_owner || !m_owner->m_player)
            return 0;

        const bool playing = m_owner->m_player->state() ==
            VideoPlaybackBackend::PlayingState;
        const qreal rate = m_owner->m_player->playbackRate();
        const int elapsed = m_positionCacheClock.isValid()
            ? m_positionCacheClock.elapsed() : 0;
        const bool rateChanged =
            qAbs(rate - m_positionCacheRate) > 0.001;
        if (m_positionCacheValid && !rateChanged &&
            playing == m_positionCachePlaying &&
            (!playing || elapsed < KSoftPositionRefreshMilliseconds)) {
            ++m_positionCacheHitCount;
            if (!playing)
                return m_positionCacheBaseMilliseconds;
            const qint64 projected = m_positionCacheBaseMilliseconds +
                static_cast<qint64>(elapsed * rate);
            const qint64 duration = m_owner->m_player->duration();
            return duration > 0 ? qMin(projected, duration) : projected;
        }

        QTime clock;
        clock.start();
        const qint64 position = m_owner->m_player->position();
        m_positionMilliseconds += clock.elapsed();
        ++m_positionQueryCount;
        m_positionCacheBaseMilliseconds = position;
        m_positionCacheRate = rate;
        m_positionCachePlaying = playing;
        m_positionCacheValid = true;
        m_positionCacheClock.start();
        return position;
    }

    QString pacingTelemetry() const
    {
        return QString::fromLatin1(
            " overlayPaintCount=%1 overlayPaintMs=%2 "
            "overlayPositionQueries=%3 overlayPositionMs=%4 "
            "overlayPositionCacheHits=%5 "
            "overlayTimerEvents=%6 overlayTimerElapsedMs=%7 "
            "overlayTimerMaxGapMs=%8 "
            "overlayClearMs=%9 overlayVideoDrawMs=%10 "
            "overlayIntermediateMs=%11 overlayRotateMs=%12 "
            "overlayDanmakuMs=%13 overlayControlsMs=%14 "
            "overlayPainterEndMs=%15 overlayOtherMs=%16")
            .arg(static_cast<qulonglong>(m_paintEventCount))
            .arg(m_paintMilliseconds)
            .arg(static_cast<qulonglong>(m_positionQueryCount))
            .arg(m_positionMilliseconds)
            .arg(static_cast<qulonglong>(m_positionCacheHitCount))
            .arg(static_cast<qulonglong>(m_timerEventCount))
            .arg(m_timerElapsedMilliseconds)
            .arg(m_timerMaxGapMilliseconds)
            .arg(m_overlayClearMilliseconds)
            .arg(m_overlayVideoDrawMilliseconds)
            .arg(m_overlayIntermediateMilliseconds)
            .arg(m_overlayRotateMilliseconds)
            .arg(m_overlayDanmakuMilliseconds)
            .arg(m_overlayControlsMilliseconds)
            .arg(m_overlayPainterEndMilliseconds)
            .arg(m_overlayOtherMilliseconds);
    }

    qint64 paintStageTotal() const
    {
        return m_overlayClearMilliseconds +
            m_overlayVideoDrawMilliseconds +
            m_overlayIntermediateMilliseconds +
            m_overlayRotateMilliseconds +
            m_overlayDanmakuMilliseconds +
            m_overlayControlsMilliseconds +
            m_overlayPainterEndMilliseconds;
    }

    void addPaintStageMilliseconds(
        qint64 *bucket,
        OverlayFastCounterValue start)
    {
        if (bucket)
            *bucket += overlayFastCounterElapsedMilliseconds(start);
    }

    void recordPaintElapsed(
        OverlayFastCounterValue start,
        const QTime &clock,
        qint64 stageTotalBefore)
    {
        ++m_paintEventCount;
        qint64 elapsed = overlayFastCounterElapsedMilliseconds(start);
        if (elapsed <= 0)
            elapsed = clock.elapsed();
        m_paintMilliseconds += elapsed;
        const qint64 stageDelta = paintStageTotal() - stageTotalBefore;
        if (elapsed > stageDelta)
            m_overlayOtherMilliseconds += elapsed - stageDelta;
    }

protected:
    virtual void timerEvent(QTimerEvent *event)
    {
        if (!event || event->timerId() != m_updateTimerId || !m_owner) {
            if (event)
                event->accept();
            return;
        }
        if (!isVisible()) {
            event->accept();
            return;
        }
        if (m_timerClock.isValid()) {
            const int gap = m_timerClock.elapsed();
            m_timerElapsedMilliseconds += gap;
            if (gap > m_timerMaxGapMilliseconds)
                m_timerMaxGapMilliseconds = gap;
        }
        m_timerClock.start();
        ++m_timerEventCount;
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
        if (m_directProbeMode) {
            // Continuous repaint drives the real ARGB backing store exactly
            // as moving danmaku and touch feedback do in production.
            update();
            event->accept();
            return;
        }
#endif
        bool controlsChanged = false;
        if (m_owner && m_owner->m_downloadReply) {
            m_controlsVisible = true;
            m_controlsAge = 0;
            m_owner->updateVideoGeometry(true, false);
        } else if (m_controlsVisible && !m_qualityMenuVisible &&
            ++m_controlsAge > 90) {
            m_controlsVisible = false;
            controlsChanged = true;
            if (m_owner)
                m_owner->updateVideoGeometry(false, false);
        }
        if (m_owner) {
            QImage hardwareFrame;
            Yuv420Frame yuv420Frame;
            qint64 timestamp = 0;
            const bool avcPlaybackActive = m_owner->m_player &&
                m_owner->m_player->isAvcHardwarePlaybackActive();
            if (m_owner->m_player && m_owner->m_avcProbeStage == 4 &&
                !avcPlaybackActive) {
                // Never leave the last decoded frame frozen above MMF after
                // a compatibility-input or decoder failure.
                if (m_owner->m_softVideoActive)
                    m_owner->clearSoftwareVideoSurface();
                if (m_owner->m_glesYuvActive && m_owner->m_delegate) {
                    m_owner->m_delegate->videoPlayerClearSoftwareVideo();
                }
                if (m_owner->m_glesYuvActive)
                    setFrameInterval(KOverlayFrameIntervalMilliseconds);
                m_owner->m_glesYuvActive = false;
            }
            m_framePositionValid = false;
#if defined(Q_OS_SYMBIAN) && \
    (defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER) || \
     defined(WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_DIAGNOSTIC))
            if (avcPlaybackActive) {
                m_framePositionMilliseconds =
                    sampleSoftPosition();
                m_framePositionValid = true;
            }
#endif
            if (m_owner->m_glesYuvActive && m_owner->m_player &&
                (m_framePositionValid
                    ? m_owner->m_player->takeAvcHardwareYuv420FrameAt(
                        m_framePositionMilliseconds, &yuv420Frame,
                        &m_decoderFrameSerial)
                    : m_owner->m_player->takeAvcHardwareYuv420Frame(
                        &yuv420Frame, &m_decoderFrameSerial))) {
                if (m_owner->m_delegate)
                    m_owner->m_delegate->videoPlayerPresentYuv420(
                        yuv420Frame);
            } else if (!m_owner->m_glesYuvActive && m_owner->m_player &&
                (m_framePositionValid
                    ? m_owner->m_player->takeAvcHardwareFrameAt(
                        m_framePositionMilliseconds, &hardwareFrame,
                        &timestamp, &m_decoderFrameSerial)
                    : m_owner->m_player->takeAvcHardwareFrame(
                        &hardwareFrame, &timestamp,
                        &m_decoderFrameSerial))) {
                m_owner->presentSoftwareFrame(hardwareFrame, timestamp);
            }
            m_nativeDanmakuActive = m_owner->updateNativeDanmaku(
                m_danmaku, m_danmakuEnabled);
        }
        bool danmakuVisible = false;
        if (!m_nativeDanmakuActive && m_danmakuEnabled &&
            !m_danmaku.isEmpty()) {
            danmakuVisible = hasVisibleDanmaku(
                presentationPosition());
        }
        const bool repaint = controlsChanged || m_controlsVisible ||
            m_qualityMenuVisible || downloadProgressVisible() ||
            danmakuVisible || m_danmakuWasVisible;
        m_danmakuWasVisible = danmakuVisible;
        if (repaint)
            update();
        event->accept();
    }

    virtual void paintEvent(QPaintEvent *event)
    {
        Q_UNUSED(event);
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
        if (m_directProbeMode) {
            paintDirectProbeOverlay();
            return;
        }
#endif
        if (!m_owner || !m_owner->m_player)
            return;
        const OverlayFastCounterValue paintStart =
            overlayFastCounterNow();
        QTime paintClock;
        paintClock.start();
        const qint64 stageTotalBefore = paintStageTotal();
        QPainter target(this);
        if (!target.isActive()) {
            recordPaintElapsed(
                paintStart, paintClock, stageTotalBefore);
            return;
        }
        if (m_firstPaintPending) {
            m_firstPaintPending = false;
            qDebug() << "WW:PLAYER_OVERLAY_FIRST_PAINT"
                     << static_cast<void *>(this)
                     << static_cast<void *>(m_owner.data())
                     << size() << "native-orientation";
        }
        // Clear the one full-screen ARGB overlay to transparent on every
        // frame.  This is intentionally one window, never one WSERV window per
        // comment.
        const OverlayFastCounterValue clearStart =
            overlayFastCounterNow();
        target.setCompositionMode(QPainter::CompositionMode_Source);
        target.fillRect(QWidget::rect(), QColor(0, 0, 0, 0));
        target.setCompositionMode(QPainter::CompositionMode_SourceOver);
        addPaintStageMilliseconds(
            &m_overlayClearMilliseconds, clearStart);
        // The RGB565 software picture lives in the opaque native sibling
        // below this window.  This ARGB surface now contains only danmaku and
        // controls, and therefore stays transparent everywhere else.
        paintLogicalCanvas(&target);
        const OverlayFastCounterValue targetEndStart =
            overlayFastCounterNow();
        target.end();
        addPaintStageMilliseconds(
            &m_overlayPainterEndMilliseconds, targetEndStart);
        recordPaintElapsed(
            paintStart, paintClock, stageTotalBefore);
    }

    virtual void mousePressEvent(QMouseEvent *event)
    {
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
        if (m_directProbeMode) {
            m_directProbeTouchActive = true;
            m_directProbeTouchPoint = event->pos();
            qDebug() << "WW:DIRECT_TOUCH" << "press"
                     << event->pos().x() << event->pos().y();
            update();
            event->accept();
            return;
        }
#endif
        if (!m_owner || !m_owner->m_player) {
            m_pressed = false;
            m_draggingProgress = false;
            event->ignore();
            return;
        }
        const QPoint position = logicalPosition(event->pos());
        m_pressed = true;
        m_pressPosition = position;
        m_controlsVisibleOnPress = m_controlsVisible;
        updateHitBoxes();
        m_draggingProgress = !m_owner->m_isLive &&
            m_progressBox.contains(position);
        m_draggingVolume = m_volumeBox.contains(position);
        if (m_draggingProgress)
            m_dragPreviewPosition = progressPosition(position.x());
        if (m_draggingVolume)
            setVolumeFromPosition(position.y());
        m_controlsVisible = true;
        m_controlsAge = 0;
        m_owner->updateVideoGeometry(true, m_qualityMenuVisible);
        update();
        event->accept();
    }

    virtual void mouseMoveEvent(QMouseEvent *event)
    {
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
        if (m_directProbeMode) {
            if (m_directProbeTouchActive) {
                m_directProbeTouchPoint = event->pos();
                update();
            }
            event->accept();
            return;
        }
#endif
        if (!m_owner || !m_owner->m_player) {
            m_pressed = false;
            m_draggingProgress = false;
            event->ignore();
            return;
        }
        const QPoint position = logicalPosition(event->pos());
        if (m_pressed && m_draggingProgress) {
            m_dragPreviewPosition = progressPosition(position.x());
            m_controlsVisible = true;
            m_controlsAge = 0;
            update();
        } else if (m_pressed && m_draggingVolume) {
            setVolumeFromPosition(position.y());
            m_controlsVisible = true;
            m_controlsAge = 0;
            update();
        }
        event->accept();
    }

    virtual void mouseReleaseEvent(QMouseEvent *event)
    {
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
        if (m_directProbeMode) {
            m_directProbeTouchPoint = event->pos();
            m_directProbeTouchActive = false;
            qDebug() << "WW:DIRECT_TOUCH" << "release"
                     << event->pos().x() << event->pos().y();
            update();
            event->accept();
            return;
        }
#endif
        if (!m_owner || !m_owner->m_player) {
            m_pressed = false;
            m_draggingProgress = false;
            event->ignore();
            return;
        }
        if (!m_pressed) {
            event->accept();
            return;
        }
        m_pressed = false;
        const QPoint position = logicalPosition(event->pos());
        if (m_draggingProgress) {
            m_dragPreviewPosition = progressPosition(position.x());
            m_owner->seekTo(m_dragPreviewPosition);
            m_draggingProgress = false;
            m_controlsAge = 0;
            update();
            event->accept();
            return;
        }
        if (m_draggingVolume) {
            setVolumeFromPosition(position.y());
            m_draggingVolume = false;
            m_controlsAge = 0;
            update();
            event->accept();
            return;
        }
        if ((position - m_pressPosition).manhattanLength() >= 18) {
            event->accept();
            return;
        }
        // If this press began while the MMF viewport filled the screen, the
        // first tap only reveals controls.  Do not let a button that appeared
        // under the finger during mousePress execute on the same release.
        if (!m_controlsVisibleOnPress) {
            m_qualityMenuVisible = false;
            m_controlsVisible = true;
            m_controlsAge = 0;
            m_owner->updateVideoGeometry(true, false);
            update();
            event->accept();
            return;
        }
        updateHitBoxes();
        const int selectedQuality = qualityIndexAt(position);
        if (m_qualityMenuVisible && selectedQuality >= 0) {
            const PlaybackQualityCompat quality =
                m_owner->m_qualities.at(selectedQuality);
            m_qualityMenuVisible = false;
            m_owner->requestQuality(quality.quality);
        } else if (m_backBox.contains(position)) {
            m_owner->closePlayer();
            // closePlayer() detaches and parks the application-lifetime
            // playback surface. Do not run the common geometry update below
            // against the now-hidden MMF/native window.
            event->accept();
            return;
        } else if (m_playBox.contains(position)) {
            m_owner->togglePlayback();
        } else if (!m_owner->m_isLive &&
                   m_progressBox.contains(position)) {
            const qreal ratio = qBound<qreal>(
                static_cast<qreal>(0.0),
                (position.x() - m_progressBox.left()) /
                    static_cast<qreal>(m_progressBox.width()),
                static_cast<qreal>(1.0));
            m_owner->seekTo(
                static_cast<qint64>(ratio * m_owner->m_player->duration()));
        } else if (!m_owner->m_isLive &&
                   m_danmakuBox.contains(position)) {
            m_danmakuEnabled = !m_danmakuEnabled;
        } else if (m_qualityBox.contains(position)) {
            toggleQualityMenu();
        } else if (!m_owner->m_isLive &&
                   m_speedBox.contains(position)) {
            static const qreal speeds[] = { 1.0, 1.25, 1.5, 2.0 };
            m_speedIndex = (m_speedIndex + 1) % 4;
            m_owner->m_player->setPlaybackRate(speeds[m_speedIndex]);
            invalidatePositionCache();
        } else {
            m_qualityMenuVisible = false;
            m_controlsVisible = !m_controlsVisibleOnPress;
        }
        m_controlsAge = 0;
        m_owner->updateVideoGeometry(
            m_controlsVisible, m_qualityMenuVisible);
        update();
        event->accept();
    }

    virtual void mouseDoubleClickEvent(QMouseEvent *event)
    {
        if (!m_owner || !m_owner->m_player) {
            event->ignore();
            return;
        }
        m_owner->togglePlayback();
        m_controlsVisible = true;
        m_controlsAge = 0;
        m_owner->updateVideoGeometry(true, m_qualityMenuVisible);
        event->accept();
    }

    virtual void keyPressEvent(QKeyEvent *event)
    {
        if (!m_owner) {
            event->ignore();
            return;
        }
        // A Qt::Tool can become the active native window simply by being
        // shown. Forward hardware keys to the current player even though the
        // overlay no longer forces activateWindow()/setFocus() each session.
        m_owner->keyPressEvent(event);
    }

    virtual void closeEvent(QCloseEvent *event)
    {
        if (m_owner)
            m_owner->closePlayer();
        else
            hide();
        event->ignore();
    }

private:
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    void paintDirectProbeOverlay()
    {
        QPainter painter(this);
        if (!painter.isActive())
            return;
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), QColor(0, 0, 0, 0));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        painter.fillRect(QRect(0, 0, width(), 48),
                         QColor(8, 8, 14, 178));
        painter.fillRect(QRect(0, height() - 46, width(), 46),
                         QColor(8, 8, 14, 190));
        QFont titleFont = QApplication::font();
        titleFont.setPixelSize(15);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.setPen(QColor(251, 114, 153));
        painter.drawText(QRect(12, 0, width() - 24, 48),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         QString::fromLatin1(
                            "DevVideo Direct Probe | ARGB overlay"));

        QFont textFont = QApplication::font();
        textFont.setPixelSize(13);
        textFont.setBold(true);
        painter.setFont(textFont);
        const int elapsed = m_directProbeClock.isValid()
            ? m_directProbeClock.elapsed() : 0;
        const QString moving = QString::fromLatin1(
            "moving danmaku / repaint / raise");
        const int textWidth = QFontMetrics(textFont).width(moving);
        const qreal progress = (elapsed % 6000) / 6000.0;
        const int movingX = width() - static_cast<int>(
            progress * (width() + textWidth));
        drawOutlinedText(
            &painter, QPointF(movingX, 104), moving, Qt::white);
        const qreal reverseProgress =
            ((elapsed + 2400) % 7000) / 7000.0;
        drawOutlinedText(
            &painter,
            QPointF(-textWidth + static_cast<int>(
                reverseProgress * (width() + textWidth)), 154),
            QString::fromLatin1("UI must remain above YUV video"),
            QColor(255, 220, 92));

        painter.setPen(QColor(238, 238, 244));
        painter.drawText(
            QRect(12, height() - 46, width() - 24, 46),
            Qt::AlignCenter,
            QString::fromLatin1(
                "Touch anywhere: feedback and DIRECT_TOUCH log"));
        if (m_directProbeTouchActive) {
            painter.setPen(QPen(QColor(251, 114, 153), 4));
            painter.setBrush(QColor(251, 114, 153, 80));
            painter.drawEllipse(m_directProbeTouchPoint, 22, 22);
        }
        painter.end();
    }
#endif

    void paintLogicalCanvas(QPainter *painter)
    {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
        if (downloadProgressVisible()) {
            drawDownloadProgress(painter);
        } else {
            const OverlayFastCounterValue danmakuStart =
                overlayFastCounterNow();
            drawDanmaku(painter);
            addPaintStageMilliseconds(
                &m_overlayDanmakuMilliseconds, danmakuStart);
        }
        if (m_controlsVisible) {
            const OverlayFastCounterValue controlsStart =
                overlayFastCounterNow();
            drawControls(painter);
            addPaintStageMilliseconds(
                &m_overlayControlsMilliseconds, controlsStart);
        }
    }

    int canvasWidth() const
    {
        return QWidget::width();
    }

    int canvasHeight() const
    {
        return QWidget::height();
    }

    QPoint logicalPosition(const QPoint &physical) const
    {
        return physical;
    }

    void updateHitBoxes()
    {
        m_backBox = QRect(0, 0, 64, 52);
        m_playBox = QRect(8, canvasHeight() - 56, 48, 48);
        if (canvasWidth() >= 560) {
            m_progressBox = QRect(62, canvasHeight() - 35,
                                qMax(1, canvasWidth() - 346), 24);
            m_volumeBox = QRect(canvasWidth() - 42, 66, 32, 148);
            m_qualityBox = QRect(canvasWidth() - 268,
                                canvasHeight() - 52, 82, 40);
            m_danmakuBox = QRect(canvasWidth() - 180,
                                canvasHeight() - 52, 82, 40);
            m_speedBox = QRect(canvasWidth() - 92,
                              canvasHeight() - 52, 84, 40);
        } else {
            m_progressBox = QRect(62, canvasHeight() - 35,
                                qMax(1, canvasWidth() - 340), 24);
            m_volumeBox = QRect(canvasWidth() - 40, 58, 30,
                               qMax(100, canvasHeight() - 132));
            m_qualityBox = QRect(canvasWidth() - 268,
                                canvasHeight() - 52, 82, 40);
            m_danmakuBox = QRect(canvasWidth() - 180,
                                canvasHeight() - 52, 82, 40);
            m_speedBox = QRect(canvasWidth() - 92,
                              canvasHeight() - 52, 84, 40);
        }
        m_volumeTrackBox = QRect(
            m_volumeBox.center().x() - 5,
            m_volumeBox.top() + 30,
            10,
            qMax(20, m_volumeBox.height() - 42));
        updateQualityItemBoxes();
    }

    void setVolumeFromPosition(int y)
    {
        if (!m_owner || m_volumeTrackBox.height() <= 0)
            return;
        const qreal ratio = qBound<qreal>(
            static_cast<qreal>(0.0),
            (m_volumeTrackBox.bottom() - y) /
                static_cast<qreal>(m_volumeTrackBox.height()),
            static_cast<qreal>(1.0));
        const int requested = qBound(0,
            static_cast<int>(ratio * 100.0 + 0.5), 100);
        m_owner->adjustPersistentVolume(requested - m_owner->m_volume);
    }

    void updateQualityItemBoxes()
    {
        m_qualityItemBoxes.clear();
        if (!m_owner || m_owner->m_qualities.isEmpty())
            return;
        const int count = m_owner->m_qualities.size();
        const int menuWidth = 128;
        const int bottom = canvasHeight() - 58;
        const int availableHeight = qMax(1, bottom - 54);
        const int rowHeight = qBound(
            16, availableHeight / qMax(1, count), 28);
        const int top = qMax(54, bottom - count * rowHeight);
        int index;
        for (index = 0; index < count; ++index) {
            m_qualityItemBoxes.append(QRect(
                canvasWidth() - menuWidth - 8,
                top + index * rowHeight,
                menuWidth,
                rowHeight));
        }
    }

    int qualityIndexAt(const QPoint &position) const
    {
        if (!m_qualityMenuVisible)
            return -1;
        int index;
        for (index = 0; index < m_qualityItemBoxes.size(); ++index) {
            if (m_qualityItemBoxes.at(index).contains(position))
                return index;
        }
        return -1;
    }

    qint64 progressPosition(int x) const
    {
        if (!m_owner || !m_owner->m_player ||
            m_progressBox.width() <= 0) {
            return 0;
        }
        const qreal ratio = qBound<qreal>(
            static_cast<qreal>(0.0),
            (x - m_progressBox.left()) /
                static_cast<qreal>(m_progressBox.width()),
            static_cast<qreal>(1.0));
        return static_cast<qint64>(
            ratio * m_owner->m_player->duration());
    }

    void drawOutlinedText(
        QPainter *painter,
        const QPointF &position,
        const QString &text,
        const QColor &color)
    {
        painter->setPen(QColor(0, 0, 0, 210));
        painter->drawText(position + QPointF(-1.0, 0.0), text);
        painter->drawText(position + QPointF(1.0, 0.0), text);
        painter->drawText(position + QPointF(0.0, -1.0), text);
        painter->drawText(position + QPointF(0.0, 1.0), text);
        painter->setPen(color);
        painter->drawText(position, text);
    }

    void drawDanmaku(QPainter *painter)
    {
        if (m_nativeDanmakuActive ||
            !m_danmakuEnabled || m_danmaku.isEmpty())
            return;
        const qint64 position = presentationPosition();
        const int laneHeight = qMax(18, canvasHeight() / 12);
        const int lanes = qMax(3, (canvasHeight() - 112) / laneHeight);

        // The XML list is sorted by timestamp by the parser. Locate the first
        // potentially visible entry instead of scanning all (up to 1200)
        // comments ten times per second on the phone CPU.
        const qint64 earliest = position - 7500;
        int low = 0;
        int high = m_danmaku.size();
        while (low < high) {
            const int middle = low + (high - low) / 2;
            if (m_danmaku.at(middle).timeMilliseconds < earliest)
                low = middle + 1;
            else
                high = middle;
        }
        int index;
        int rendered = 0;
        int lastFontSize = -1;
        QFont font = QApplication::font();
        font.setBold(true);
        for (index = low; index < m_danmaku.size(); ++index) {
            const DanmakuItemCompat &item = m_danmaku.at(index);
            if (item.timeMilliseconds > position)
                break;
            const bool scrolling = item.mode == 1 || item.mode == 6;
            if (scrolling && item.timeMilliseconds <
                m_scrollingDanmakuFloorMilliseconds) {
                continue;
            }
            qint64 elapsed = position - item.timeMilliseconds;
            const qint64 lifetime = scrolling ? 7500 : 4000;
            if (scrolling &&
                index < m_danmakuDisplayStartMilliseconds.size()) {
                qint64 &displayStart =
                    m_danmakuDisplayStartMilliseconds[index];
                // Anchor a scrolling comment when it is first actually
                // painted. A late network reply or a long UI frame can no
                // longer make its first visible frame appear mid-screen.
                if (displayStart < 0)
                    displayStart = position;
                elapsed = position - displayStart;
            }
            if (elapsed < 0 || elapsed > lifetime)
                continue;
            const int fontSize = qBound(
                13, item.fontSize * 2 / 3, 24);
            if (fontSize != lastFontSize) {
                font.setPixelSize(fontSize);
                painter->setFont(font);
                lastFontSize = fontSize;
            }
            const int textWidth = index < m_danmakuTextWidths.size()
                ? m_danmakuTextWidths.at(index)
                : QFontMetrics(font).width(item.text);
            const qreal progress = elapsed /
                static_cast<qreal>(lifetime);
            qreal x = (canvasWidth() - textWidth) / 2.0;
            qreal y = item.mode == 4
                ? canvasHeight() - 82.0 : 82.0;
            if (scrolling) {
                const int lane = index % lanes;
                x = item.mode == 6
                    ? -textWidth + progress *
                        (canvasWidth() + textWidth)
                    : canvasWidth() - progress *
                        (canvasWidth() + textWidth);
                y = 58.0 + lane * laneHeight;
            }
            drawOutlinedText(
                painter, QPointF(x, y), item.text,
                QColor::fromRgb(static_cast<QRgb>(item.color)));
            if (++rendered >= 18)
                break;
        }
    }

    bool downloadProgressVisible() const
    {
        return m_owner && m_owner->m_sessionActive &&
            (m_owner->m_isLive
                ? m_owner->m_downloadReply != 0
                : m_owner->m_playbackMode ==
                    VideoPlayerWidget::DownloadThenPlayback) &&
            !m_owner->m_localPlaybackActive &&
            !m_owner->m_policyRouteFailed;
    }

    QString downloadSizeText(qint64 bytes) const
    {
        if (bytes <= 0)
            return QString::fromLatin1("0 MiB");
        const qreal mib = bytes / static_cast<qreal>(1024 * 1024);
        return QString::fromLatin1("%1 MiB").arg(
            QString::number(mib, 'f', mib < 10.0 ? 1 : 0));
    }

    void drawDownloadProgress(QPainter *painter)
    {
        const qint64 downloaded = qMax<qint64>(
            0, m_owner->m_downloadBytes);
        // A live FLV response has no meaningful "file size".  The old panel
        // reused the VOD download wording and therefore made the bounded
        // startup buffer look like DownloadThenPlayback.  For the temporary
        // growing-live route, measure only the amount required for the next
        // local open and label it as buffering.
        const bool liveBuffering = m_owner->m_isLive &&
            (m_owner->m_openLocalWhileDownloading ||
             m_owner->m_liveFlvDemuxActive);
        const qint64 total = liveBuffering
            ? (m_owner->m_liveFlvDemuxActive
                ? KLiveAacStartBytes : KOpenFileStreamingStartBytes)
            : qMax<qint64>(0, m_owner->m_downloadTotalBytes);
        const qint64 progressBytes = m_owner->m_liveFlvDemuxActive
            ? m_owner->m_liveFlvAudioBytes : downloaded;
        const int percentage = total > 0
            ? qBound(0, static_cast<int>(
                progressBytes * 100 / total), 100)
            : 0;

        painter->fillRect(rect(), QColor(12, 12, 18, 232));
        const int panelWidth = qMax(260, qMin(440, canvasWidth() - 48));
        const int panelHeight = 154;
        const QRect panel(
            (canvasWidth() - panelWidth) / 2,
            (canvasHeight() - panelHeight) / 2,
            panelWidth, panelHeight);
        painter->setPen(QPen(QColor(251, 114, 153, 190), 1));
        painter->setBrush(QColor(35, 35, 45, 248));
        painter->drawRoundedRect(panel, 13, 13);

        QFont titleFont = QApplication::font();
        titleFont.setPixelSize(17);
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(QColor(248, 248, 250));
        painter->drawText(
            QRect(panel.left() + 18, panel.top() + 15,
                  panel.width() - 36, 27),
            Qt::AlignCenter,
            liveBuffering
                ? QString::fromUtf8("正在缓冲直播")
                : m_owner->m_downloadReply
                    ? QString::fromUtf8("正在下载视频")
                    : QString::fromUtf8("正在准备视频下载"));

        QFont detailFont = QApplication::font();
        detailFont.setPixelSize(11);
        painter->setFont(detailFont);
        painter->setPen(QColor(188, 188, 201));
        const QString detail = liveBuffering
            ? QString::fromUtf8("已缓冲 %1 / 起播缓冲 %2")
                .arg(downloadSizeText(progressBytes))
                .arg(downloadSizeText(total))
            : total > 0
            ? QString::fromUtf8("已下载 %1 / 文件大小 %2")
                .arg(downloadSizeText(downloaded))
                .arg(downloadSizeText(total))
            : QString::fromUtf8("已下载 %1 / 正在获取文件大小…")
                .arg(downloadSizeText(downloaded));
        painter->drawText(
            QRect(panel.left() + 18, panel.top() + 47,
                  panel.width() - 36, 22),
            Qt::AlignCenter, detail);

        const QRect progressBar(
            panel.left() + 24, panel.top() + 82,
            panel.width() - 48, 10);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(82, 82, 95));
        painter->drawRoundedRect(progressBar, 5, 5);
        if (total > 0 && percentage > 0) {
            QRect completed = progressBar;
            completed.setWidth(qMax(
                5, progressBar.width() * percentage / 100));
            painter->setBrush(QColor(251, 114, 153));
            painter->drawRoundedRect(completed, 5, 5);
        }

        QFont statusFont = QApplication::font();
        statusFont.setPixelSize(12);
        statusFont.setBold(total > 0);
        painter->setFont(statusFont);
        painter->setPen(total > 0
            ? QColor(251, 150, 178) : QColor(166, 166, 180));
        painter->drawText(
            QRect(panel.left() + 18, panel.top() + 102,
                  panel.width() - 36, 21),
            Qt::AlignCenter,
            total > 0
                ? QString::fromLatin1("%1%").arg(percentage)
                : QString::fromUtf8("连接中"));
        painter->setPen(QColor(145, 145, 158));
        statusFont.setBold(false);
        statusFont.setPixelSize(10);
        painter->setFont(statusFont);
        painter->drawText(
            QRect(panel.left() + 18, panel.top() + 128,
                  panel.width() - 36, 17),
            Qt::AlignCenter,
            liveBuffering
                ? QString::fromUtf8("达到起播缓冲后继续边下边播")
                : QString::fromUtf8("下载完成后将自动开始播放"));
    }

    void drawControls(QPainter *painter)
    {
        updateHitBoxes();
        painter->fillRect(QRect(0, 0, canvasWidth(), 52),
                          QColor(8, 8, 12, 178));
        painter->fillRect(QRect(0, canvasHeight() - 62,
                                canvasWidth(), 62),
                          QColor(8, 8, 12, 196));

        QFont titleFont = QApplication::font();
        titleFont.setPixelSize(15);
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(QColor(251, 114, 153));
        painter->drawText(m_backBox, Qt::AlignCenter,
                          QString::fromUtf8("‹ 返回"));
        painter->setPen(Qt::white);
        painter->drawText(QRect(68, 0, canvasWidth() - 218, 52),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          m_owner->m_title);

        QFont smallFont = QApplication::font();
        smallFont.setPixelSize(11);
        painter->setFont(smallFont);
        painter->setPen(QColor(190, 190, 202));
        painter->drawText(QRect(canvasWidth() - 150, 0, 142, 52),
                          Qt::AlignCenter,
                          QString::fromLatin1("MMF %1 / %2")
                              .arg(m_owner->qualityLabel())
                              .arg(m_owner->playbackStatus()));

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(251, 114, 153, 230));
        painter->drawEllipse(m_playBox.adjusted(5, 5, -5, -5));
        painter->setPen(Qt::white);
        QFont playFont = QApplication::font();
        playFont.setPixelSize(20);
        painter->setFont(playFont);
        painter->drawText(m_playBox, Qt::AlignCenter,
            m_owner->m_player->state() == VideoPlaybackBackend::PlayingState
                ? QString::fromLatin1("||") : QString::fromUtf8("▶"));

        const qint64 duration = m_owner->m_player->duration();
        const qint64 position = m_draggingProgress
            ? m_dragPreviewPosition
            : presentationPosition();
        const qreal ratio = duration > 0 && !m_owner->m_isLive
            ? qBound<qreal>(
                static_cast<qreal>(0.0),
                position / static_cast<qreal>(duration),
                static_cast<qreal>(1.0))
            : 0.0;
        const int lineY = m_progressBox.center().y();
        painter->setPen(QPen(QColor(105, 105, 116), 4));
        painter->drawLine(m_progressBox.left(), lineY,
                          m_progressBox.right(), lineY);
        painter->setPen(QPen(QColor(251, 114, 153), 4));
        if (m_owner->m_isLive) {
            painter->drawLine(m_progressBox.left(), lineY,
                              m_progressBox.right(), lineY);
        } else {
            painter->drawLine(m_progressBox.left(), lineY,
                              m_progressBox.left() +
                                  static_cast<int>(
                                      m_progressBox.width() * ratio),
                              lineY);
        }
        painter->setPen(QColor(230, 230, 236));
        painter->setFont(smallFont);
        painter->drawText(QRect(m_progressBox.left(),
                                canvasHeight() - 60,
                                m_progressBox.width(), 18),
                          Qt::AlignCenter,
                          m_owner->m_isLive
                              ? QString::fromUtf8("● 直播中")
                              : playerTimeText(position) +
                                  QString::fromLatin1(" / ") +
                                  playerTimeText(duration));

        // A narrow glass slider stays clear of the bottom transport row and
        // uses the same dark/pink palette as the rest of the player.
        painter->setPen(QPen(QColor(112, 112, 126, 180), 1));
        painter->setBrush(QColor(27, 27, 38, 218));
        painter->drawRoundedRect(m_volumeBox, 12, 12);
        painter->setPen(QColor(214, 214, 224));
        painter->setFont(smallFont);
        painter->drawText(
            QRect(m_volumeBox.left(), m_volumeBox.top() + 4,
                  m_volumeBox.width(), 20),
            Qt::AlignCenter,
            QString::fromLatin1("%1").arg(m_owner->m_volume));
        painter->setPen(QPen(QColor(104, 104, 118), 5,
                            Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(m_volumeTrackBox.center().x(),
                          m_volumeTrackBox.top(),
                          m_volumeTrackBox.center().x(),
                          m_volumeTrackBox.bottom());
        const int volumeY = m_volumeTrackBox.bottom() -
            m_owner->m_volume * m_volumeTrackBox.height() / 100;
        painter->setPen(QPen(QColor(251, 114, 153), 5,
                            Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(m_volumeTrackBox.center().x(), volumeY,
                          m_volumeTrackBox.center().x(),
                          m_volumeTrackBox.bottom());
        painter->setPen(QPen(QColor(255, 208, 222), 1));
        painter->setBrush(QColor(251, 114, 153));
        painter->drawEllipse(
            QPoint(m_volumeTrackBox.center().x(), volumeY), 6, 6);

        painter->setPen(QColor(230, 230, 236));
        painter->drawText(m_qualityBox, Qt::AlignCenter,
                          m_owner->qualityLabel());

        painter->setPen(!m_owner->m_isLive && m_danmakuEnabled
            ? QColor(251, 132, 166) : QColor(150, 150, 160));
        painter->drawText(m_danmakuBox, Qt::AlignCenter,
                          m_owner->m_isLive
                              ? QString::fromUtf8("弹幕 --")
                              : m_danmakuEnabled
                              ? QString::fromUtf8("弹幕 开")
                              : QString::fromUtf8("弹幕 关"));
        static const char *speedLabels[] = {
            "1.0x", "1.25x", "1.5x", "2.0x"
        };
        painter->setPen(QColor(230, 230, 236));
        painter->drawText(m_speedBox, Qt::AlignCenter,
                          m_owner->m_isLive
                              ? QString::fromUtf8("直播")
                              : QString::fromLatin1(
                                  speedLabels[m_speedIndex]));

        if (m_qualityMenuVisible &&
            !m_qualityItemBoxes.isEmpty()) {
            int index;
            for (index = 0;
                 index < m_qualityItemBoxes.size() &&
                 index < m_owner->m_qualities.size();
                 ++index) {
                const QRect box = m_qualityItemBoxes.at(index);
                const PlaybackQualityCompat &quality =
                    m_owner->m_qualities.at(index);
                const bool selected =
                    quality.quality == m_owner->m_quality;
                painter->fillRect(
                    box,
                    selected
                        ? QColor(251, 114, 153, 226)
                        : QColor(22, 22, 30, 238));
                painter->setPen(selected
                    ? Qt::white : QColor(226, 226, 234));
                painter->drawText(
                    box.adjusted(8, 0, -8, 0),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    quality.description.left(18));
            }
        }

        if (!m_owner->m_downloadReply &&
            m_owner->m_player->error() != VideoPlaybackBackend::NoError &&
            m_owner->m_sourceIndex + 1 >= m_owner->m_sourceUrls.size()) {
            const QRect errorBox(
                qMax(14, canvasWidth() / 8),
                qMax(64, canvasHeight() / 2 - 34),
                qMax(120, canvasWidth() * 3 / 4), 68);
            painter->fillRect(errorBox, QColor(16, 16, 22, 218));
            painter->setPen(QColor(251, 132, 166));
            painter->drawText(
                errorBox.adjusted(10, 6, -10, -6),
                Qt::AlignCenter | Qt::TextWordWrap,
                QString::fromUtf8("播放失败：") +
                    m_owner->m_player->errorString());
        }
    }

    QPointer<VideoPlayerWidget> m_owner;
    int m_updateTimerId;
    int m_updateIntervalMilliseconds;
    bool m_nativeWindowPrepared;
    QVector<DanmakuItemCompat> m_danmaku;
    QVector<int> m_danmakuTextWidths;
    bool m_danmakuEnabled;
    bool m_nativeDanmakuActive;
    bool m_danmakuWasVisible;
    bool m_controlsVisible;
    int m_controlsAge;
    bool m_pressed;
    bool m_controlsVisibleOnPress;
    QPoint m_pressPosition;
    bool m_draggingProgress;
    bool m_draggingVolume;
    qint64 m_dragPreviewPosition;
    int m_speedIndex;
    bool m_qualityMenuVisible;
    bool m_firstPaintPending;
    int m_decoderFrameSerial;
    qint64 m_framePositionMilliseconds;
    bool m_framePositionValid;
    QTime m_positionCacheClock;
    qint64 m_positionCacheBaseMilliseconds;
    qreal m_positionCacheRate;
    bool m_positionCacheValid;
    bool m_positionCachePlaying;
    quint64 m_positionCacheHitCount;
    QTime m_timerClock;
    quint64 m_timerEventCount;
    qint64 m_timerElapsedMilliseconds;
    int m_timerMaxGapMilliseconds;
    quint64 m_paintEventCount;
    qint64 m_paintMilliseconds;
    quint64 m_positionQueryCount;
    qint64 m_positionMilliseconds;
    qint64 m_overlayClearMilliseconds;
    qint64 m_overlayVideoDrawMilliseconds;
    qint64 m_overlayIntermediateMilliseconds;
    qint64 m_overlayRotateMilliseconds;
    qint64 m_overlayDanmakuMilliseconds;
    qint64 m_overlayControlsMilliseconds;
    qint64 m_overlayPainterEndMilliseconds;
    qint64 m_overlayOtherMilliseconds;
    qint64 m_scrollingDanmakuFloorMilliseconds;
    QVector<qint64> m_danmakuDisplayStartMilliseconds;
    QRect m_backBox;
    QRect m_playBox;
    QRect m_progressBox;
    QRect m_volumeBox;
    QRect m_volumeTrackBox;
    QRect m_qualityBox;
    QRect m_danmakuBox;
    QRect m_speedBox;
    QVector<QRect> m_qualityItemBoxes;
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    bool m_directProbeMode;
    bool m_directProbeTouchActive;
    QPoint m_directProbeTouchPoint;
    QTime m_directProbeClock;
#endif
};

static VideoOverlayWidget *persistentVideoOverlay(QWidget *lifetimeOwner)
{
    if (!g_persistentVideoOverlay)
        g_persistentVideoOverlay = new VideoOverlayWidget(lifetimeOwner);
    return g_persistentVideoOverlay;
}

VideoPlayerWidget::VideoPlayerWidget(
    QWidget *returnWidget,
    VideoPlayerDelegate *delegate)
    : QWidget(0, Qt::Window | Qt::FramelessWindowHint),
      m_returnWidget(returnWidget),
      m_delegate(delegate),
      m_player(0),
      m_downloadManager(new QNetworkAccessManager(this)),
      m_downloadReply(0), m_downloadFile(0),
      m_liveFlvDemuxer(new FlvLiveDemuxer()),
      m_avcProbeReader(new Mp4AvcProbeReader()),
      m_avcProbeReply(0), m_avcProbeRangeStart(0),
      m_avcProbeRangeEnd(0), m_avcProbeStage(0),
      m_avcProbeSampleCount(0), m_avcProbeFirstSample(0),
      m_avcProbeLastSampleExclusive(0), m_avcProbeNextSample(0),
      m_avcProbeSessionSerial(0), m_avcProbeLastPictures(0),
       m_avcProbeRequestPolls(0), m_avcProbeStalledPolls(0),
       m_avcProbeLastObservedBytes(0), m_avcProbeLastLoggedBytes(0),
      m_avcProbeResetDecoder(false), m_avcHeaderPreflightPending(false),
      m_avcProbeInputFinished(false),
      m_mmfSourceOpenDeferred(false),
      m_videoWidget(0), m_softVideoSurface(0),
      m_overlay(0),
      m_sourceIndex(-1), m_quality(0), m_lastReportedError(-1),
      m_pollTimerId(0), m_startTimerId(0),
      m_retryTimerId(0), m_orientationTimerId(0),
      m_screenAwakeTimerId(0),
      m_pendingSourceIndex(-1), m_sourcePass(0),
      m_liveMimeVariant(0), m_liveFlvRedirectCount(0),
      m_downloadSourceIndex(-1), m_downloadBytes(0),
      m_downloadTotalBytes(0), m_liveFlvAudioBytes(0),
      m_automaticFallbackTarget(0),
      m_playbackMode(UrlStreamingPlayback),
      m_decoderMode(AutomaticDecoder),
      m_volume(80),
      m_closing(false), m_isLive(false),
      m_streamPlaybackMode(true),
      m_localFallbackAttempted(false), m_localPlaybackActive(false),
      m_openLocalWhileDownloading(false),
      m_liveFlvDemuxActive(false), m_liveFlvAudioOpen(false),
      m_liveFlvVideoStarted(false), m_policyRouteFailed(false),
      m_softwareVideoRouteSelected(false),
      m_landscapeRequested(false), m_landscapeApplied(false),
      m_orientationStage(NativePortraitIdle),
      m_landscapeWorkAreaSeen(false), m_portraitWorkAreaSeen(false),
      m_restoreClosesSession(false),
      m_glesYuvActive(false), m_softVideoActive(false),
      m_sessionActive(false), m_sessionSerial(0)
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
      , m_directProbe(0), m_directProbeActive(false),
      m_directProbePhaseAPassed(false),
      m_directProbePhaseBPassed(false)
#endif
{
    setWindowTitle(QString::fromLatin1("NIKINIKI Player"));
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_QuitOnClose, false);
    // This persistent controller is the independent top-level used by the
    // verified app-shell native-landscape sequence. Only its native video host
    // and the separate ARGB overlay draw during playback. It is hidden, never
    // destroyed, between sessions so the proven persistent MMF graph remains
    // intact.
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    createVideoOutputWidget();
    m_overlay = persistentVideoOverlay(m_returnWidget);
    m_overlay->attachOwner(this);
    QDesktopWidget *desktop = QApplication::desktop();
    connect(desktop, SIGNAL(workAreaResized(int)),
            this, SLOT(onDesktopWorkAreaResized(int)));
    connect(desktop, SIGNAL(resized(int)),
            this, SLOT(onDesktopScreenResized(int)));
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    m_volume = qBound(
        0, settings.value(
            QString::fromLatin1("player/volume"), 80).toInt(), 100);
    // Bind the MMF video output only after this full-window child owns a native
    // Symbian window. Binding while it is still hidden produces the null
    // receiver/disconnect warnings seen in the 0.6.3 device log.
    m_pollTimerId = startTimer(500);
}

VideoPlayerWidget::~VideoPlayerWidget()
{
    qDebug() << "WW:PLAYER_SESSION_DESTROY_BEGIN"
             << static_cast<void *>(this);
    saveVolumePreference();
    if (m_startTimerId)
        killTimer(m_startTimerId);
    if (m_pollTimerId)
        killTimer(m_pollTimerId);
    if (m_retryTimerId)
        killTimer(m_retryTimerId);
    if (m_orientationTimerId)
        killTimer(m_orientationTimerId);
    if (m_screenAwakeTimerId)
        killTimer(m_screenAwakeTimerId);
    disconnect(QApplication::desktop(), 0, this, 0);
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    if (m_directProbe) {
        m_directProbe->stop();
        delete m_directProbe;
        m_directProbe = 0;
    }
    m_directProbeActive = false;
    if (m_overlay)
        m_overlay->setDirectProbeMode(false);
#endif
    if (m_landscapeApplied ||
        m_orientationStage == NativeWaitingLandscapeWorkArea ||
        m_orientationStage == NativeLandscapeVisible)
        requestPlayerOrientation(false);
    if (m_overlay)
        m_overlay->detachOwner(this);
    hideNativeDanmaku();
    if (m_player) {
        m_player->stop();
        m_player->clearMedia();
        delete m_player;
        m_player = 0;
    }
    cancelAvcHardwareProbe();
    delete m_avcProbeReader;
    m_avcProbeReader = 0;
    cancelLocalDownloadFallback(true);
    delete m_liveFlvDemuxer;
    m_liveFlvDemuxer = 0;
    qDebug() << "WW:PLAYER_SESSION_DESTROY_READY"
             << static_cast<void *>(this);
}

void VideoPlayerWidget::setPlaybackPreferences(
    int playbackMode, int decoderMode)
{
    m_playbackMode = qBound(
        static_cast<int>(UrlStreamingPlayback), playbackMode,
        static_cast<int>(DownloadThenPlayback));
    m_decoderMode = qBound(
        static_cast<int>(AutomaticDecoder), decoderMode,
        static_cast<int>(SoftwareOnlyDecoder));
}

void VideoPlayerWidget::startDevVideoDirectProbe()
{
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    if (m_sessionActive || m_directProbeActive)
        return;
    m_sessionActive = true;
    m_directProbeActive = true;
    m_directProbePhaseAPassed = false;
    m_directProbePhaseBPassed = false;
    ++m_sessionSerial;
    m_closing = false;
    m_restoreClosesSession = false;
    m_landscapeRequested = true;
    m_title = QString::fromLatin1("devvideodirectprobe1");
    m_quality = 16;
    m_format = QString::fromLatin1("static-yuv420p");
    m_isLive = false;
    m_sourceUrls.clear();
    m_qualities.clear();
    m_glesYuvActive = false;
    clearSoftwareVideoSurface();
    createVideoOutputWidget();
    m_overlay->attachOwner(this);
    m_overlay->setDirectProbeMode(true);
    qDebug() << "WW:DIRECT_PROBE_BEGIN"
             << "session" << m_sessionSerial
             << "host" << static_cast<void *>(m_returnWidget)
             << "surface" << static_cast<void *>(m_softVideoSurface)
             << "overlay" << static_cast<void *>(m_overlay);
    beginNativeLandscapeTransition();
#else
    qDebug() << "WW:DIRECT_PROBE_DISABLED";
#endif
}

void VideoPlayerWidget::onDevVideoDirectProbeFinished(
    bool phaseAPassed,
    bool phaseBPassed)
{
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    m_directProbePhaseAPassed = phaseAPassed;
    m_directProbePhaseBPassed = phaseBPassed;
    qDebug() << "WW:DIRECT_PROBE_FINISHED"
             << "phaseA" << (phaseAPassed ? "YES" : "NO")
             << "phaseB" << (phaseBPassed ? "YES" : "NO");
    // Leave the final overlay/result visible briefly, then tear down DSA and
    // restore portrait through the same verified player close path.
    QTimer::singleShot(1400, this, SLOT(closeDevVideoDirectProbe()));
#else
    Q_UNUSED(phaseAPassed);
    Q_UNUSED(phaseBPassed);
#endif
}

void VideoPlayerWidget::closeDevVideoDirectProbe()
{
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    if (!m_directProbeActive)
        return;
    if (m_directProbe)
        m_directProbe->stop();
    m_directProbeActive = false;
    if (m_overlay)
        m_overlay->setDirectProbeMode(false);
    qDebug() << "WW:DIRECT_PROBE_RESTORE"
             << "phaseA" << (m_directProbePhaseAPassed ? "YES" : "NO")
             << "phaseB" << (m_directProbePhaseBPassed ? "YES" : "NO");
    closePlayer();
#endif
}

void VideoPlayerWidget::openSource(
    const PlaybackSourceCompat &source,
    const QString &title,
    const QByteArray &cookieHeader)
{
    if (m_delegate)
        m_delegate->videoPlayerClearSoftwareVideo();
    m_glesYuvActive = false;
    clearSoftwareVideoSurface();
    cancelAvcHardwareProbe();
    m_sessionActive = true;
    setScreenAwakeEnabled(true);
    if (m_overlay)
        m_overlay->attachOwner(this);
    ++m_sessionSerial;
    qDebug() << "WW:PLAYER_SESSION_ACTIVE" << m_sessionSerial
             << static_cast<void *>(this)
             << static_cast<void *>(m_videoWidget)
             << static_cast<void *>(m_softVideoSurface)
             << static_cast<void *>(m_player);
    const bool wasVisible = isVisible();
    cancelLocalDownloadFallback(true);
    if (m_retryTimerId) {
        killTimer(m_retryTimerId);
        m_retryTimerId = 0;
    }
    if (m_orientationTimerId) {
        killTimer(m_orientationTimerId);
        m_orientationTimerId = 0;
    }
    m_sourceUrls.clear();
    if (!source.url.isEmpty())
        m_sourceUrls.append(source.url);
    int index;
    for (index = 0; index < source.backupUrls.size(); ++index) {
        if (!m_sourceUrls.contains(source.backupUrls.at(index)))
            m_sourceUrls.append(source.backupUrls.at(index));
    }
    const QStringList secureUrls = m_sourceUrls;
    m_sourceUrls.clear();
#ifdef Q_OS_SYMBIAN
    // The MMF player does not share RHTTP's working TLS 1.2 session. Prefer
    // plain upos URLs and keep HTTPS only as a final fallback. This avoids a
    // guaranteed Symbian -34 before every successful playback.
    for (index = 0; index < secureUrls.size(); ++index) {
        QUrl mediaUrl(secureUrls.at(index));
        if (mediaUrl.scheme().compare(
                QString::fromLatin1("https"), Qt::CaseInsensitive) == 0) {
            // The proprietary edge service on 4483 is TLS-only; changing its
            // scheme returns HTTP 400. Other upos mirrors serve byte ranges on
            // port 80 and are compatible with Belle MMF.
            if (mediaUrl.port() == 4483)
                continue;
            mediaUrl.setScheme(QString::fromLatin1("http"));
        }
        const QString plainUrl = mediaUrl.toString();
        if (!plainUrl.isEmpty() && !m_sourceUrls.contains(plainUrl))
            m_sourceUrls.append(plainUrl);
    }
    for (index = 0; index < secureUrls.size(); ++index) {
        const QString secure = secureUrls.at(index);
        if (!m_sourceUrls.contains(secure))
            m_sourceUrls.append(secure);
    }
#else
    for (index = 0; index < secureUrls.size(); ++index) {
        const QString secure = secureUrls.at(index);
        QString plain;
        if (secure.startsWith(QString::fromLatin1("https://"))) {
            plain = secure;
            plain.replace(0, 5, QString::fromLatin1("http"));
        }
#ifdef Q_OS_SYMBIAN
        // Belle MMF owns a separate, obsolete TLS stack. Live CDN
        // certificates can fail there even when application HTTPS works.
        if (source.live && !plain.isEmpty() &&
            !m_sourceUrls.contains(plain)) {
            m_sourceUrls.append(plain);
        }
#endif
        if (!m_sourceUrls.contains(secure))
            m_sourceUrls.append(secure);
        if (!plain.isEmpty()) {
            if (!m_sourceUrls.contains(plain))
                m_sourceUrls.append(plain);
        }
    }
#endif
    m_title = title;
    m_quality = source.quality;
    m_format = source.format;
    m_isLive = source.live;
    m_landscapeRequested = source.live ||
        (source.videoWidth > 0 && source.videoHeight > 0 &&
         source.videoWidth > source.videoHeight);
    m_qualities = source.qualities;
    m_referer = source.referer.isEmpty()
        ? QString::fromLatin1("https://www.bilibili.com/")
        : source.referer;
    m_cookieHeader = cookieHeader;
    m_sourceIndex = -1;
    m_pendingSourceIndex = -1;
    m_sourcePass = 0;
    m_liveMimeVariant = 0;
    m_liveFlvRedirectCount = 0;
    m_downloadSourceIndex = -1;
    m_downloadBytes = 0;
    m_downloadTotalBytes = 0;
    m_liveFlvAudioBytes = 0;
    m_liveFlvPendingUnits.clear();
    m_liveFlvPendingTimes.clear();
    m_liveFlvPendingPresentationTimes.clear();
    m_localFallbackAttempted = false;
    m_localPlaybackActive = false;
    m_openLocalWhileDownloading = false;
    m_liveFlvDemuxActive = false;
    m_liveFlvAudioOpen = false;
    m_liveFlvVideoStarted = false;
    m_policyRouteFailed = false;
    m_softwareVideoRouteSelected = false;
    m_lastReportedError = -1;
    m_closing = false;
    m_sourceClock.start();
    if (m_startTimerId) {
        killTimer(m_startTimerId);
        m_startTimerId = 0;
    }
    if (!wasVisible)
        m_automaticFallbackTarget = 0;
    const bool nativeOrientationChange =
        m_landscapeRequested != m_landscapeApplied;
    qDebug() << "WW:PLAYER_NATIVE_ORIENTATION"
             << m_landscapeRequested << nativeOrientationChange
             << (m_returnWidget ? m_returnWidget->size() : QSize());
    if (!m_videoWidget)
        createVideoOutputWidget();
    // Close the previous controller/media before any show/geometry update for
    // the new source. After a normal Back/re-enter cycle the playback windows
    // are still hidden here. This removes the old display binding while
    // retaining the facade, observer, utility and native window objects.
    recreateMediaPlayer(
        m_isLive || m_playbackMode == UrlStreamingPlayback);
    m_overlay->sourceChanged();
    m_restoreClosesSession = false;

    if (m_landscapeRequested) {
        if (m_landscapeApplied &&
            m_orientationStage == NativeLandscapeVisible) {
            // Quality switches reuse the already-visible native landscape
            // window and the complete persistent playback graph.
            startPlaybackPresentation();
        } else {
            beginNativeLandscapeTransition();
        }
    } else if (m_landscapeApplied ||
               m_orientationStage == NativeLandscapeVisible) {
        // Rare portrait media can return to native portrait without closing
        // the logical playback session.
        m_restoreClosesSession = false;
        QTimer::singleShot(0, this, SLOT(beginNativePortraitRestore()));
    } else {
        m_orientationStage = NativePortraitIdle;
        startPlaybackPresentation();
    }

    qDebug() << "WW:PLAYER_PAGE_ORIENTATION_PENDING"
             << source.videoWidth << source.videoHeight
             << m_landscapeRequested << nativeOrientationChange
             << "stage" << static_cast<int>(m_orientationStage);
    qDebug() << "WW:PLAYER_POLICY"
             << "playback" << m_playbackMode
             << "decoder" << m_decoderMode
             << "live" << m_isLive;
    if (m_isLive &&
        (m_playbackMode != UrlStreamingPlayback ||
         m_decoderMode == SoftwareOnlyDecoder)) {
        qDebug() << "WW:PLAYER_POLICY_LIVE_USES_OPENURL_MMF";
    }
}

void VideoPlayerWidget::setDanmaku(
    const QVector<DanmakuItemCompat> &items)
{
    if (m_overlay)
        m_overlay->setDanmaku(this, items);
}

void VideoPlayerWidget::adjustPersistentVolume(int delta)
{
    adjustVolume(delta);
    saveVolumePreference();
}

void VideoPlayerWidget::loadSourceAt(int index)
{
    if (!m_player || index < 0 || index >= m_sourceUrls.size())
        return;
    m_sourceIndex = index;

#if defined(Q_OS_SYMBIAN) && \
    (defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER) || \
     defined(WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_DIAGNOSTIC))
    // On Nokia 603, opening MMF and a second Qt Network Range request for the
    // same CDN URL at the same time can leave the Range reply at zero bytes.
    // That used to abandon classification after ten seconds and made the
    // software path unreachable. Probe the MP4 header and then one sync
    // access unit before MMF opens this first source. The actual DevVideo
    // header result, rather than an inferred AVC risk template, selects MMF
    // or the local software decoder.
    const bool canPreflightBeforeMmf = index == 0 && m_sourcePass == 0 &&
        m_avcProbeStage == 0 && !m_isLive &&
#ifdef WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_DIAGNOSTIC
        // The diagnostic needs one real Annex-B access unit for its own
        // DevVideo session even when the UI has been set to hardware-only.
        // It never redirects software-only selection into this route.
        m_decoderMode != SoftwareOnlyDecoder &&
#else
        m_decoderMode != HardwareOnlyDecoder &&
#endif
        m_format.startsWith(QString::fromLatin1("mp4"),
                            Qt::CaseInsensitive) &&
        m_avcProbeReader && !m_avcProbeReply;
    if (canPreflightBeforeMmf) {
        m_mmfSourceOpenDeferred = true;
        qDebug() << "WW:PLAYER_SOURCE_DEFER_MMF"
                 << (index + 1) << m_sourceUrls.size();
        startAvcHardwareProbeMetadata(index);
        return;
    }
#endif
    if (m_decoderMode == HardwareOnlyDecoder)
        qDebug() << "WW:PLAYER_DECODER_ROUTE" << "HARDWARE";
    openSelectedPlaybackSourceAt(index, "direct");
}

void VideoPlayerWidget::openSelectedPlaybackSourceAt(
    int index, const char *reason)
{
    if (!m_player || index < 0 || index >= m_sourceUrls.size())
        return;
    m_sourceIndex = index;
    m_mmfSourceOpenDeferred = false;
    m_player->setNativeVideoEnabled(!m_softwareVideoRouteSelected);
    if (m_isLive || m_playbackMode == UrlStreamingPlayback) {
        openMmfSourceAt(index, reason);
        return;
    }
    startLocalDownloadFallback(
        index, m_playbackMode == OpenFileStreamingPlayback);
    qDebug() << "WW:PLAYER_LOCAL_ROUTE" << reason
             << "mode" << m_playbackMode
             << (index + 1) << m_sourceUrls.size();
}

void VideoPlayerWidget::openMmfSourceAt(int index, const char *reason)
{
    if (!m_player || index < 0 || index >= m_sourceUrls.size())
        return;
    m_sourceIndex = index;
    m_mmfSourceOpenDeferred = false;
    QNetworkRequest request(QUrl(m_sourceUrls.at(index)));
    request.setRawHeader(
        "User-Agent",
        "Mozilla/5.0 (Symbian/3; Nokia603) NIKINIKI/"
        WILIWILI_SYMBIAN_VERSION_STR);
    request.setRawHeader("Referer", m_referer.toUtf8());
    request.setRawHeader("Origin", "https://www.bilibili.com");
    if (!m_cookieHeader.isEmpty())
        request.setRawHeader("Cookie", m_cookieHeader);
    m_sourceClock.restart();
    const QByteArray mimeType = playbackMimeTypeForUrl(
        m_sourceUrls.at(index), m_liveMimeVariant);
    m_player->setMedia(QMediaContent(request), mimeType);
    m_player->play();
    m_lastReportedError = -1;
    qDebug() << "WW:PLAYER_SOURCE"
             << reason
             << (index + 1) << m_sourceUrls.size()
             << "mimeVariant" << m_liveMimeVariant << mimeType
             << m_sourceUrls.at(index).startsWith(
                    QString::fromLatin1("https://"));
    m_overlay->update();
}

void VideoPlayerWidget::openDeferredMmfSource(const char *reason)
{
    if (!m_mmfSourceOpenDeferred)
        return;
    const int index = m_sourceIndex;
    qDebug() << "WW:PLAYER_SOURCE_DEFER_MMF_DONE" << reason
             << (index + 1) << m_sourceUrls.size();
    openSelectedPlaybackSourceAt(index, reason);
}

void VideoPlayerWidget::handleAvcProbeFailure(const char *reason)
{
    if (m_decoderMode != SoftwareOnlyDecoder) {
        openDeferredMmfSource(reason);
        return;
    }
    m_mmfSourceOpenDeferred = false;
    m_policyRouteFailed = true;
    qDebug() << "WW:PLAYER_SOFTWARE_ONLY_FAILED" << reason;
    if (m_overlay)
        m_overlay->update();
}

void VideoPlayerWidget::createVideoOutputWidget()
{
    if (!m_videoWidget) {
        m_videoWidget = new QWidget(this);
        m_videoWidget->setAttribute(Qt::WA_NativeWindow, true);
        m_videoWidget->setAttribute(Qt::WA_NoSystemBackground, true);
        m_videoWidget->setAutoFillBackground(false);
        // The sole transparent overlay owns all player input.
        m_videoWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }
    if (!m_softVideoSurface) {
        m_softVideoSurface = new SoftVideoSurfaceWidget(this);
        qDebug() << "WW:SOFT_SURFACE_CREATED"
                 << static_cast<void *>(m_softVideoSurface);
    }
}

void VideoPlayerWidget::presentSoftwareFrame(
    const QImage &frame,
    qint64 timestamp)
{
    if (!m_softVideoSurface || frame.isNull() || !m_sessionActive)
        return;
    if (!m_softVideoActive) {
        m_softVideoActive = true;
        if (m_videoWidget)
            m_videoWidget->hide();
        m_softVideoSurface->setGeometry(rect());
        m_softVideoSurface->activateSurface();
        if (m_overlay)
            m_overlay->raise();
        qDebug() << "WW:SOFT_SURFACE_FIRST_FRAME"
                 << frame.size() << static_cast<int>(frame.format())
                 << timestamp << m_softVideoSurface->geometry();
    }
    m_softVideoSurface->setFrame(frame, timestamp);
}

void VideoPlayerWidget::clearSoftwareVideoSurface()
{
    if (!m_softVideoActive)
        return;
    m_softVideoActive = false;
    if (m_softVideoSurface)
        m_softVideoSurface->deactivateSurface();
}

void VideoPlayerWidget::releasePlaybackSurfaceForOrientation()
{
    // Closing can be initiated inside the top-level overlay's mouse event.
    // Keep the controller, native QWidget/RWindow, MMF facade and observer
    // alive until application shutdown. Only stop playback and hide the host;
    // the next session reuses the exact same native object graph.
    if (m_player)
        m_player->stop();
    if (m_videoWidget)
        m_videoWidget->hide();
    clearSoftwareVideoSurface();
    qDebug() << "WW:PLAYER_NATIVE_SURFACE_PARKED";
}

void VideoPlayerWidget::restartOrientationTimeout()
{
    stopOrientationTimeout();
    m_orientationTimerId = startTimer(6000);
}

void VideoPlayerWidget::stopOrientationTimeout()
{
    if (!m_orientationTimerId)
        return;
    killTimer(m_orientationTimerId);
    m_orientationTimerId = 0;
}

bool VideoPlayerWidget::nativeOrientationTransitionActive() const
{
    return m_orientationStage == NativeWaitingPortraitChrome ||
        m_orientationStage == NativeWaitingLandscapeWorkArea ||
        m_orientationStage == NativeWaitingPortraitWorkArea ||
        m_orientationStage == NativeWaitingPortraitFullscreen;
}

void VideoPlayerWidget::beginNativeLandscapeTransition()
{
    if (!m_returnWidget) {
        failOrientationTransition("no-return-window");
        return;
    }

    stopOrientationTimeout();
    hide();
    if (m_videoWidget)
        m_videoWidget->hide();
    if (m_softVideoSurface)
        m_softVideoSurface->hide();
    if (m_overlay)
        m_overlay->hide();
    m_landscapeWorkAreaSeen = false;
    m_orientationStage = NativeWaitingPortraitChrome;
    restartOrientationTimeout();

    QDesktopWidget *desktop = QApplication::desktop();
    qDebug() << "WW:PLAYER_NATIVE_LANDSCAPE_BEGIN"
             << "available" << playerRectText(
                    desktop->availableGeometry(0))
             << "physical" << playerRectText(
                    desktop->screenGeometry(0))
             << "hostVisible" << m_returnWidget->isVisible();

    // The validated sequence first leaves Qt fullscreen while portrait is
    // stable. Constructed Avkon panes then produce workAreaResized(); only
    // that signal authorizes SetOrientationL().
    m_returnWidget->showMaximized();
    m_returnWidget->raise();
    m_returnWidget->activateWindow();
    QApplication::setActiveWindow(m_returnWidget);
    m_returnWidget->setFocus(Qt::ActiveWindowFocusReason);
    m_returnWidget->update();

    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    if (available.height() > available.width() &&
        physical == QRect(0, 0, 360, 640) &&
        available != QRect(0, 0, 360, 640)) {
        stopOrientationTimeout();
        QTimer::singleShot(
            0, this, SLOT(requestNativeLandscapeOrientation()));
    }
}

void VideoPlayerWidget::requestNativeLandscapeOrientation()
{
    if (m_orientationStage != NativeWaitingPortraitChrome ||
        m_closing)
        return;

    m_orientationStage = NativeWaitingLandscapeWorkArea;
    restartOrientationTimeout();
    const int error = requestPlayerOrientation(true);
    qDebug() << "WW:PLAYER_NATIVE_LANDSCAPE_REQUEST"
             << "error" << error;
    if (error != 0) {
        failOrientationTransition("landscape-request-failed");
        return;
    }

    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    if (available.width() > available.height() &&
        physical == QRect(0, 0, 640, 360)) {
        m_landscapeWorkAreaSeen = true;
        QTimer::singleShot(0, this, SLOT(commitNativeLandscapeWindow()));
    }
}

void VideoPlayerWidget::onDesktopWorkAreaResized(int screen)
{
    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(screen);
    const QRect physical = desktop->screenGeometry(screen);
    qDebug() << "WW:PLAYER_NATIVE_WORKAREA"
             << "stage" << static_cast<int>(m_orientationStage)
             << "screen" << screen
             << "available" << playerRectText(available)
             << "physical" << playerRectText(physical);

    if (m_orientationStage == NativeWaitingPortraitChrome &&
        available.height() > available.width() &&
        physical == QRect(0, 0, 360, 640) &&
        available != QRect(0, 0, 360, 640)) {
        stopOrientationTimeout();
        qDebug() << "WW:PLAYER_NATIVE_PORTRAIT_CHROME_READY";
        QTimer::singleShot(
            0, this, SLOT(requestNativeLandscapeOrientation()));
    } else if (m_orientationStage == NativeWaitingLandscapeWorkArea &&
               available.width() > available.height() &&
               physical == QRect(0, 0, 640, 360)) {
        m_landscapeWorkAreaSeen = true;
        qDebug() << "WW:PLAYER_NATIVE_LANDSCAPE_640X360_READY";
        QTimer::singleShot(
            0, this, SLOT(commitNativeLandscapeWindow()));
    } else if (m_orientationStage == NativeWaitingPortraitWorkArea &&
               available.height() > available.width() &&
               physical == QRect(0, 0, 360, 640)) {
        m_portraitWorkAreaSeen = true;
        QTimer::singleShot(
            0, this, SLOT(commitNativePortraitWindow()));
    } else if (m_orientationStage == NativeWaitingPortraitFullscreen &&
               available == QRect(0, 0, 360, 640) &&
               physical == QRect(0, 0, 360, 640)) {
        QTimer::singleShot(
            0, this, SLOT(completeNativePortraitFullscreen()));
    }
}

void VideoPlayerWidget::onDesktopScreenResized(int screen)
{
    QDesktopWidget *desktop = QApplication::desktop();
    qDebug() << "WW:PLAYER_NATIVE_SCREEN_RESIZED"
             << "stage" << static_cast<int>(m_orientationStage)
             << "screen" << screen
             << "available" << playerRectText(
                    desktop->availableGeometry(screen))
             << "physical" << playerRectText(
                    desktop->screenGeometry(screen));
}

void VideoPlayerWidget::commitNativeLandscapeWindow()
{
    if (m_orientationStage != NativeWaitingLandscapeWorkArea ||
        !m_landscapeWorkAreaSeen || m_closing)
        return;

    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    if (available.width() <= available.height() ||
        physical != QRect(0, 0, 640, 360))
        return;

    stopOrientationTimeout();
    m_orientationStage = NativeLandscapeVisible;
    m_landscapeApplied = true;
    qDebug() << "WW:PLAYER_NATIVE_LANDSCAPE_SHOW_BEGIN"
             << "available" << playerRectText(available)
             << "physical" << playerRectText(physical)
             << "hostVisible" << m_returnWidget->isVisible();

    showFullScreen();
    raise();
    activateWindow();
    QApplication::setActiveWindow(this);
    setFocus(Qt::OtherFocusReason);
    update();
    startPlaybackPresentation();

    qDebug() << "WW:PLAYER_NATIVE_LANDSCAPE_VISIBLE"
             << "geometry" << playerRectText(geometry())
             << "fullscreen" << isFullScreen()
             << "hostVisible" << m_returnWidget->isVisible();
}

void VideoPlayerWidget::startPlaybackPresentation()
{
    if (m_closing || !m_sessionActive)
        return;
    if (m_landscapeRequested &&
        m_orientationStage != NativeLandscapeVisible)
        return;

    if (m_startTimerId) {
        killTimer(m_startTimerId);
        m_startTimerId = 0;
    }
    // Both orientations now use their real screen coordinates. There is no
    // MMF, ARGB, danmaku, controls or input 90-degree transform.
    showFullScreen();
    raise();
    activateWindow();
    QApplication::setActiveWindow(this);
    setFocus(Qt::ActiveWindowFocusReason);
    winId();
    updateVideoGeometry(true, false);
    if (m_videoWidget && !m_softVideoActive) {
#ifdef WILIWILI_ENABLE_E7_MMF_ROOT_WINDOW_DIAGNOSTIC
        // The E7 diagnostic binds MMF to this top-level widget.  Keeping the
        // normal native child hidden rules out a child-window occlusion or
        // compositor routing failure while preserving the ARGB overlay.
        m_videoWidget->hide();
#else
        m_videoWidget->show();
        m_videoWidget->winId();
#endif
    }
    if (m_softVideoSurface && m_softVideoActive) {
        m_softVideoSurface->show();
        m_softVideoSurface->raise();
    }
    updateOverlayGeometry();
    if (m_overlay) {
#ifdef WILIWILI_ENABLE_E7_MMF_BARE_WINDOW_DIAGNOSTIC
        // Keep the overlay absent for this hardware-composition experiment.
        // Its timer then returns while hidden, so it also performs no video
        // position sampling or ARGB repaint work during MMF playback.
        m_overlay->hide();
        qDebug() << "WW:E7_MMF_BARE_WINDOW_OVERLAY_HIDDEN"
                 << "presentation";
#else
        m_overlay->show();
        m_overlay->prepareNativeWindow();
        m_overlay->raise();
#endif
    }
    requestPlayerPlatformForeground();
    m_startTimerId = startTimer(180);
    qDebug() << "WW:PLAYER_PAGE_PREPARED_NATIVE"
             << size()
             << (m_videoWidget ? m_videoWidget->geometry() : QRect())
             << isWindow() << isFullScreen()
             << "landscape" << m_landscapeApplied;
}

void VideoPlayerWidget::beginNativePortraitRestore()
{
    if (!m_sessionActive)
        return;

    stopOrientationTimeout();
    if (m_overlay)
        m_overlay->hide();
    if (m_videoWidget)
        m_videoWidget->hide();
    if (m_softVideoSurface)
        m_softVideoSurface->hide();
    hide();
    m_portraitWorkAreaSeen = false;
    m_orientationStage = NativeWaitingPortraitWorkArea;
    restartOrientationTimeout();
    const int error = requestPlayerOrientation(false);
    qDebug() << "WW:PLAYER_NATIVE_PORTRAIT_REQUEST"
             << "error" << error;
    if (error != 0) {
        failOrientationTransition("portrait-request-failed");
        return;
    }

    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    if (available.height() > available.width() &&
        physical == QRect(0, 0, 360, 640)) {
        m_portraitWorkAreaSeen = true;
        QTimer::singleShot(0, this, SLOT(commitNativePortraitWindow()));
    }
}

void VideoPlayerWidget::commitNativePortraitWindow()
{
    if (m_orientationStage != NativeWaitingPortraitWorkArea ||
        !m_portraitWorkAreaSeen || !m_returnWidget)
        return;

    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    if (available.height() <= available.width() ||
        physical != QRect(0, 0, 360, 640))
        return;

    stopOrientationTimeout();
    m_orientationStage = NativeWaitingPortraitFullscreen;
    m_returnWidget->showMaximized();
    m_returnWidget->raise();
    m_returnWidget->activateWindow();
    QApplication::setActiveWindow(m_returnWidget);
    m_returnWidget->setFocus(Qt::ActiveWindowFocusReason);
    m_returnWidget->update();
    restartOrientationTimeout();
    qDebug() << "WW:PLAYER_NATIVE_PORTRAIT_FULLSCREEN_REQUEST"
             << "available" << playerRectText(available)
             << "physical" << playerRectText(physical);
    m_returnWidget->showFullScreen();

    if (desktop->availableGeometry(0) == QRect(0, 0, 360, 640) &&
        desktop->screenGeometry(0) == QRect(0, 0, 360, 640)) {
        QTimer::singleShot(
            0, this, SLOT(completeNativePortraitFullscreen()));
    }
}

void VideoPlayerWidget::completeNativePortraitFullscreen()
{
    if (m_orientationStage != NativeWaitingPortraitFullscreen ||
        !m_returnWidget)
        return;

    QDesktopWidget *desktop = QApplication::desktop();
    const QRect available = desktop->availableGeometry(0);
    const QRect physical = desktop->screenGeometry(0);
    if (available != QRect(0, 0, 360, 640) ||
        physical != QRect(0, 0, 360, 640))
        return;

    stopOrientationTimeout();
    m_orientationStage = NativePortraitIdle;
    m_landscapeApplied = false;
    restoreMainWindow();
    qDebug() << "WW:PLAYER_NATIVE_PORTRAIT_FULLSCREEN_READY"
             << "available" << playerRectText(available)
             << "physical" << playerRectText(physical)
             << "hostVisible" << m_returnWidget->isVisible()
             << "fullscreen" << m_returnWidget->isFullScreen();

    if (m_restoreClosesSession)
        finishCloseAfterOrientation();
    else
        startPlaybackPresentation();
}

void VideoPlayerWidget::failOrientationTransition(const char *reason)
{
    qDebug() << "WW:PLAYER_NATIVE_ORIENTATION_FAILED"
             << reason << "stage" << static_cast<int>(m_orientationStage);
    stopOrientationTimeout();

    if (m_orientationStage == NativeWaitingPortraitWorkArea ||
        m_orientationStage == NativeWaitingPortraitFullscreen) {
        requestPlayerOrientation(false);
        m_orientationStage = NativePortraitIdle;
        m_landscapeApplied = false;
        restoreMainWindow();
        if (m_restoreClosesSession)
            finishCloseAfterOrientation();
        else
            startPlaybackPresentation();
        return;
    }

    // A failed entry never falls back to the costly portrait-window virtual
    // rotation. Abort this playback attempt and restore the known-safe main
    // window instead.
    m_closing = true;
    m_restoreClosesSession = true;
    // This abort path does not pass through closePlayer(). Stop the inactivity
    // reset immediately, including when the subsequent portrait restore never
    // reaches its completion callback.
    setScreenAwakeEnabled(false);
    QTimer::singleShot(0, this, SLOT(beginNativePortraitRestore()));
}

void VideoPlayerWidget::recreateMediaPlayer(bool streamPlayback)
{
    if (m_player) {
        m_player->stop();
        m_player->clearMedia();
        qDebug() << "WW:PLAYER_BACKEND_REUSE"
                  << static_cast<void *>(m_player)
                  << "native-no-rotation";
    } else {
        // Production binds MMF to a persistent native child.  E7 reports
        // audio with a black MMF image for a file that the system player can
        // render, so this opt-in diagnostic uses the already-visible player
        // top-level RWindow and removes that child composition layer.
#ifdef WILIWILI_ENABLE_E7_MMF_ROOT_WINDOW_DIAGNOSTIC
        m_player = new VideoPlaybackBackend(this);
#else
        m_player = new VideoPlaybackBackend(m_videoWidget);
#endif
        qDebug() << "WW:PLAYER_BACKEND_NEW"
                  << static_cast<void *>(m_player)
                  << "native-no-rotation"
#ifdef WILIWILI_ENABLE_E7_MMF_ROOT_WINDOW_DIAGNOSTIC
                  << "E7_MMF_ROOT_WINDOW"
#endif
                  ;
    }
    m_player->setNativeVideoEnabled(true);
    m_player->setVolume(m_volume);
    m_streamPlaybackMode = streamPlayback;
    qDebug() << "WW:PLAYER_BACKEND_NATIVE"
             << (streamPlayback ? 1 : 0)
             << "orientation" << m_landscapeRequested
             << "mmfRotation" << 0;
}

bool VideoPlayerWidget::updateNativeDanmaku(
    const QVector<DanmakuItemCompat> &items,
    bool enabled)
{
    Q_UNUSED(items);
    Q_UNUSED(enabled);
    hideNativeDanmaku();
    return false;
}

void VideoPlayerWidget::hideNativeDanmaku()
{
}

void VideoPlayerWidget::scheduleSourceAt(
    int index, int delayMilliseconds)
{
    if (index < 0 || index >= m_sourceUrls.size())
        return;
    if (m_retryTimerId) {
        killTimer(m_retryTimerId);
        m_retryTimerId = 0;
    }
    // MMF can keep a failed controller in KErrNotReady for the remainder of
    // the event-loop turn. Release the media first and give the native plugin
    // a short quiet period before opening the next CDN URL.
    if (m_player) {
        m_player->stop();
    }
    m_pendingSourceIndex = index;
    m_retryTimerId = startTimer(qMax(100, delayMilliseconds));
    qDebug() << "WW:PLAYER_SOURCE_SCHEDULE"
             << (index + 1) << m_sourceUrls.size()
             << delayMilliseconds << m_sourcePass;
}

int VideoPlayerWidget::nextLiveFlvSourceIndex(int startIndex) const
{
    int index;
    for (index = qMax(0, startIndex); index < m_sourceUrls.size(); ++index) {
        if (QUrl(m_sourceUrls.at(index)).path().toLower().endsWith(
                QString::fromLatin1(".flv"))) {
            return index;
        }
    }
    return -1;
}

void VideoPlayerWidget::startLiveFlvDemuxFallback(int sourceIndex)
{
    if (m_closing || !m_isLive || !m_liveFlvDemuxer ||
        sourceIndex < 0 || sourceIndex >= m_sourceUrls.size() ||
        !QUrl(m_sourceUrls.at(sourceIndex)).path().toLower().endsWith(
            QString::fromLatin1(".flv"))) {
        return;
    }

    cancelLocalDownloadFallback(true);
    m_liveFlvDemuxer->reset();
    m_liveFlvPendingUnits.clear();
    m_liveFlvPendingTimes.clear();
    m_liveFlvPendingPresentationTimes.clear();
    m_liveFlvAudioBytes = 0;
    m_liveFlvRedirectCount = 0;
    m_downloadSourceIndex = sourceIndex;
    m_downloadBytes = 0;
    m_downloadTotalBytes = 0;
    m_localFallbackAttempted = true;
    m_liveFlvDemuxActive = true;
    m_liveFlvAudioOpen = false;
    m_liveFlvVideoStarted = false;
    m_softwareVideoRouteSelected = true;

    QDir().mkpath(QDir::tempPath());
    m_downloadPath = QDir::tempPath() +
        QString::fromLatin1("/wiliwili_live_audio.aac");
    QFile::remove(m_downloadPath);
    m_downloadFile = new LocalDownloadWriter;
    if (!m_downloadFile->open(m_downloadPath)) {
        delete m_downloadFile;
        m_downloadFile = 0;
        m_liveFlvDemuxActive = false;
        m_policyRouteFailed = true;
        qDebug() << "WW:LIVE_FLV_AUDIO_FILE_ERROR" << m_downloadPath;
        if (m_overlay)
            m_overlay->update();
        return;
    }

    QNetworkRequest request(QUrl(m_sourceUrls.at(sourceIndex)));
    request.setRawHeader(
        "User-Agent",
        "Mozilla/5.0 (Symbian/3; Nokia603) NIKINIKI/"
        WILIWILI_SYMBIAN_VERSION_STR);
    request.setRawHeader("Referer", m_referer.toUtf8());
    request.setRawHeader("Origin", "https://www.bilibili.com");
    if (!m_cookieHeader.isEmpty())
        request.setRawHeader("Cookie", m_cookieHeader);
    m_downloadReply = m_downloadManager->get(request);
    qDebug() << "WW:LIVE_FLV_DEMUX_BEGIN"
             << (sourceIndex + 1) << m_sourceUrls.size()
             << m_sourceUrls.at(sourceIndex).startsWith(
                    QString::fromLatin1("https://"));
    if (m_overlay)
        m_overlay->update();
}

void VideoPlayerWidget::startLiveFlvAudioClock()
{
    if (!m_liveFlvDemuxActive || m_liveFlvAudioOpen || !m_downloadFile ||
        m_liveFlvAudioBytes < KLiveAacStartBytes || !m_player)
        return;
    if (!m_downloadFile->flush()) {
        retryLiveFlvDemuxFallback("audio-flush");
        return;
    }

    // The failed remote FLV controller and any decoder probe are released
    // before MMF selects its ordinary local AAC controller.  The writer keeps
    // the same read/write sharing mode already proven on Nokia 603.
    recreateMediaPlayer(false);
    m_player->setNativeVideoEnabled(false);
    m_player->setMedia(QMediaContent(
        QUrl::fromLocalFile(m_downloadPath)));
    m_player->play();
    m_liveFlvAudioOpen = true;
    m_localPlaybackActive = true;
    m_lastReportedError = -1;
    qDebug() << "WW:LIVE_FLV_AAC_OPEN"
             << m_liveFlvAudioBytes << m_downloadPath;
    if (m_overlay)
        m_overlay->update();
}

void VideoPlayerWidget::startLiveFlvVideoIfReady()
{
    if (!m_liveFlvDemuxActive || !m_liveFlvAudioOpen || !m_player ||
        m_player->mediaStatus() != VideoPlaybackBackend::LoadedMedia)
        return;

    m_player->setNativeVideoEnabled(false);
    if (!m_liveFlvVideoStarted) {
        if (m_liveFlvPendingUnits.size() < KLiveVideoStartUnits)
            return;
        m_player->setAvcHardwareYuv420OutputEnabled(false);
        if (!m_player->startAvcHardwarePlayback(
                m_liveFlvPendingUnits,
                m_liveFlvPendingTimes,
                m_liveFlvPendingPresentationTimes,
                m_liveFlvDemuxer->width(),
                m_liveFlvDemuxer->height())) {
            qDebug() << "WW:LIVE_FLV_VIDEO_START_ERROR"
                     << m_player->avcHardwareError();
            retryLiveFlvDemuxFallback("video-start");
            return;
        }
        qDebug() << "WW:LIVE_FLV_VIDEO_START"
                 << m_liveFlvPendingUnits.size()
                 << m_liveFlvDemuxer->width()
                 << m_liveFlvDemuxer->height();
        m_liveFlvPendingUnits.clear();
        m_liveFlvPendingTimes.clear();
        m_liveFlvPendingPresentationTimes.clear();
        m_liveFlvVideoStarted = true;
        m_avcProbeStage = 3;
        m_glesYuvActive = false;
        if (m_overlay) {
            m_overlay->sourceChanged();
            m_overlay->setFrameInterval(
                KOverlayFrameIntervalMilliseconds);
            m_overlay->show();
            m_overlay->raise();
        }
        if (m_softVideoSurface)
            m_softVideoSurface->setGeometry(rect());
        qDebug() << "WW:FFMPEG_SOFT_NATIVE_SURFACE_PENDING"
                 << (m_softVideoSurface
                     ? m_softVideoSurface->geometry() : QRect())
                 << "live-overlay-above";
        return;
    }

    if (!m_liveFlvPendingUnits.isEmpty() &&
        m_player->avcHardwareBufferedUnitCount() <
            KAvcProbePrefetchThresholdUnits) {
        if (!m_player->appendAvcHardwarePlayback(
                m_liveFlvPendingUnits,
                m_liveFlvPendingTimes,
                m_liveFlvPendingPresentationTimes)) {
            qDebug() << "WW:LIVE_FLV_VIDEO_APPEND_ERROR"
                     << m_player->avcHardwareError();
            retryLiveFlvDemuxFallback("video-append");
            return;
        }
        qDebug() << "WW:LIVE_FLV_VIDEO_APPEND"
                 << m_liveFlvPendingUnits.size()
                 << m_player->avcHardwareBufferedUnitCount();
        m_liveFlvPendingUnits.clear();
        m_liveFlvPendingTimes.clear();
        m_liveFlvPendingPresentationTimes.clear();
    }
}

void VideoPlayerWidget::retryLiveFlvDemuxFallback(const char *reason)
{
    if (!m_liveFlvDemuxActive)
        return;
    const int failedIndex = m_downloadSourceIndex;
    const int nextIndex = nextLiveFlvSourceIndex(failedIndex + 1);
    qDebug() << "WW:LIVE_FLV_DEMUX_FAILED"
             << reason << (failedIndex + 1) << m_downloadBytes
             << m_liveFlvAudioBytes
             << m_liveFlvPendingUnits.size();
    cancelLocalDownloadFallback(true);
    if (nextIndex >= 0) {
        qDebug() << "WW:LIVE_FLV_DEMUX_RETRY"
                 << (nextIndex + 1) << m_sourceUrls.size();
        startLiveFlvDemuxFallback(nextIndex);
    } else {
        m_localFallbackAttempted = true;
        m_policyRouteFailed = true;
        qDebug() << "WW:LIVE_FLV_DEMUX_EXHAUSTED"
                 << (failedIndex + 1) << m_sourceUrls.size();
        if (m_overlay)
            m_overlay->update();
    }
}

void VideoPlayerWidget::pollLiveFlvDemuxFallback()
{
    if (!m_liveFlvDemuxActive || !m_downloadReply ||
        !m_downloadFile || !m_liveFlvDemuxer)
        return;

    const QByteArray available = m_downloadReply->readAll();
    if (!available.isEmpty()) {
        QVector<QByteArray> units;
        QVector<qint64> times;
        QVector<qint64> presentationTimes;
        QByteArray audio;
        QString parseError;
        const bool hadVideoConfig =
            m_liveFlvDemuxer->hasVideoConfiguration();
        const bool hadAudioConfig =
            m_liveFlvDemuxer->hasAudioConfiguration();
        if (!m_liveFlvDemuxer->append(
                available, &units, &times,
                &presentationTimes, &audio, &parseError)) {
            qDebug() << "WW:LIVE_FLV_PARSE_ERROR" << parseError;
            retryLiveFlvDemuxFallback("parse");
            return;
        }
        m_downloadBytes += available.size();
        if (!audio.isEmpty()) {
            const qint64 written = m_downloadFile->write(audio);
            if (written != audio.size()) {
                qDebug() << "WW:LIVE_FLV_AUDIO_WRITE_ERROR"
                         << written << audio.size();
                retryLiveFlvDemuxFallback("audio-write");
                return;
            }
            m_liveFlvAudioBytes += written;
            if (m_liveFlvAudioBytes > KLiveAacMaximumBytes) {
                retryLiveFlvDemuxFallback("audio-file-limit");
                return;
            }
            if (m_liveFlvAudioOpen && !m_downloadFile->flush()) {
                retryLiveFlvDemuxFallback("audio-growing-flush");
                return;
            }
        }
        m_liveFlvPendingUnits += units;
        m_liveFlvPendingTimes += times;
        m_liveFlvPendingPresentationTimes += presentationTimes;
        if (!hadVideoConfig &&
            m_liveFlvDemuxer->hasVideoConfiguration()) {
            qDebug() << "WW:LIVE_FLV_AVC_CONFIG"
                     << m_liveFlvDemuxer->width()
                     << m_liveFlvDemuxer->height();
        }
        if (!hadAudioConfig &&
            m_liveFlvDemuxer->hasAudioConfiguration()) {
            qDebug() << "WW:LIVE_FLV_AAC_CONFIG"
                     << m_liveFlvDemuxer->audioSampleRate()
                     << m_liveFlvDemuxer->audioChannels();
        }
        if (m_liveFlvPendingUnits.size() >
            KLiveVideoPendingMaximumUnits) {
            retryLiveFlvDemuxFallback("video-queue-overflow");
            return;
        }
    }

    startLiveFlvAudioClock();
    startLiveFlvVideoIfReady();
    if (!m_liveFlvDemuxActive || !m_downloadReply)
        return;
    if (!m_downloadReply->isFinished()) {
        if (m_overlay)
            m_overlay->update();
        return;
    }

    const int status = m_downloadReply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QVariant redirect = m_downloadReply->attribute(
        QNetworkRequest::RedirectionTargetAttribute);
    if (redirect.isValid() && m_downloadBytes == 0 &&
        m_liveFlvRedirectCount < 3) {
        const QUrl redirected = m_downloadReply->url().resolved(
            redirect.toUrl());
        m_downloadReply->deleteLater();
        m_downloadReply = 0;
        ++m_liveFlvRedirectCount;
        QNetworkRequest request(redirected);
        request.setRawHeader(
            "User-Agent",
            "Mozilla/5.0 (Symbian/3; Nokia603) NIKINIKI/"
            WILIWILI_SYMBIAN_VERSION_STR);
        request.setRawHeader("Referer", m_referer.toUtf8());
        request.setRawHeader("Origin", "https://www.bilibili.com");
        if (!m_cookieHeader.isEmpty())
            request.setRawHeader("Cookie", m_cookieHeader);
        m_downloadReply = m_downloadManager->get(request);
        qDebug() << "WW:LIVE_FLV_REDIRECT"
                 << status << m_liveFlvRedirectCount
                 << redirected.scheme();
        return;
    }

    const int networkError = static_cast<int>(m_downloadReply->error());
    const QString networkText = m_downloadReply->errorString();
    qDebug() << "WW:LIVE_FLV_NETWORK_END"
             << status << networkError << networkText
             << m_downloadBytes;
    retryLiveFlvDemuxFallback("network-end");
}

void VideoPlayerWidget::startLocalDownloadFallback(
    int sourceIndex, bool openWhileDownloading)
{
    const bool liveFlv = m_isLive && sourceIndex >= 0 &&
        sourceIndex < m_sourceUrls.size() &&
        QUrl(m_sourceUrls.at(sourceIndex)).path().toLower().endsWith(
            QString::fromLatin1(".flv"));
    if (m_closing || (m_isLive && !liveFlv) || sourceIndex < 0 ||
        sourceIndex >= m_sourceUrls.size()) {
        return;
    }
    cancelLocalDownloadFallback(false);
    m_openLocalWhileDownloading = openWhileDownloading;
    m_localFallbackAttempted = true;
    m_downloadSourceIndex = sourceIndex;
    m_downloadBytes = 0;
    m_downloadTotalBytes = 0;
    if (m_downloadPath.isEmpty()) {
        QDir().mkpath(QDir::tempPath());
        m_downloadPath = QDir::tempPath() +
            (liveFlv
                ? QString::fromLatin1("/wiliwili_live_cache.flv")
                : QString::fromLatin1("/wiliwili_player_cache.mp4"));
    }
    QFile::remove(m_downloadPath);
    m_downloadFile = new LocalDownloadWriter;
    if (!m_downloadFile->open(m_downloadPath)) {
        qDebug() << "WW:PLAYER_LOCAL_OPEN_FAILED" << m_downloadPath;
        delete m_downloadFile;
        m_downloadFile = 0;
        m_policyRouteFailed = true;
        if (m_overlay)
            m_overlay->update();
        return;
    }

    QNetworkRequest request(QUrl(m_sourceUrls.at(sourceIndex)));
    request.setRawHeader(
        "User-Agent",
        "Mozilla/5.0 (Symbian/3; Nokia603) NIKINIKI/"
        WILIWILI_SYMBIAN_VERSION_STR);
    request.setRawHeader("Referer", m_referer.toUtf8());
    request.setRawHeader("Origin", "https://www.bilibili.com");
    if (!m_cookieHeader.isEmpty())
        request.setRawHeader("Cookie", m_cookieHeader);
    m_downloadReply = m_downloadManager->get(request);
    qDebug() << "WW:PLAYER_LOCAL_DOWNLOAD"
             << (sourceIndex + 1) << m_sourceUrls.size()
             << "progressive" << m_openLocalWhileDownloading
             << m_sourceUrls.at(sourceIndex).startsWith(
                    QString::fromLatin1("https://"));
    m_overlay->update();
}

void VideoPlayerWidget::openLocalDownloadForPlayback(const char *reason)
{
    if (!m_player || m_downloadPath.isEmpty() ||
        m_localPlaybackActive || m_downloadBytes < 4096) {
        return;
    }
    if (m_downloadFile)
        m_downloadFile->flush();
    recreateMediaPlayer(false);
    m_player->setNativeVideoEnabled(!m_softwareVideoRouteSelected);
    m_player->setMedia(QMediaContent(
        QUrl::fromLocalFile(m_downloadPath)));
    m_sourceClock.restart();
    m_lastReportedError = -1;
    m_localPlaybackActive = true;
    m_player->play();
    qDebug() << "WW:PLAYER_LOCAL_OPEN" << reason
             << m_downloadBytes << m_downloadPath;
    m_overlay->update();
}

void VideoPlayerWidget::pollLocalDownloadFallback()
{
    if (m_liveFlvDemuxActive) {
        pollLiveFlvDemuxFallback();
        return;
    }
    if (!m_downloadReply || !m_downloadFile)
        return;
    const QByteArray available = m_downloadReply->readAll();
    if (!available.isEmpty()) {
        const qint64 written = m_downloadFile->write(available);
        if (written != available.size()) {
            qDebug() << "WW:PLAYER_LOCAL_WRITE_FAILED"
                     << written << available.size();
            cancelLocalDownloadFallback(true);
            m_policyRouteFailed = true;
            if (m_overlay)
                m_overlay->update();
            return;
        }
        m_downloadBytes += written;
        if (m_localPlaybackActive && !m_downloadFile->flush()) {
            qDebug() << "WW:PLAYER_LOCAL_FLUSH_FAILED"
                     << m_downloadBytes;
            cancelLocalDownloadFallback(true);
            m_policyRouteFailed = true;
            if (m_overlay)
                m_overlay->update();
            return;
        }
    }

    const qint64 maximumBytes = (m_isLive ? 192LL : 96LL) *
        1024LL * 1024LL;
    const QVariant lengthHeader = m_downloadReply->header(
        QNetworkRequest::ContentLengthHeader);
    const qint64 declaredBytes = lengthHeader.isValid()
        ? lengthHeader.toLongLong() : 0;
    if (declaredBytes > 0)
        m_downloadTotalBytes = declaredBytes;
    if (declaredBytes > maximumBytes ||
        m_downloadBytes > maximumBytes) {
        qDebug() << "WW:PLAYER_LOCAL_TOO_LARGE"
                 << declaredBytes << m_downloadBytes;
        cancelLocalDownloadFallback(true);
        m_policyRouteFailed = true;
        if (m_overlay)
            m_overlay->update();
        return;
    }
    if (m_openLocalWhileDownloading && !m_localPlaybackActive &&
        m_downloadBytes >= KOpenFileStreamingStartBytes) {
        openLocalDownloadForPlayback("progressive-threshold");
    }
    if (!m_downloadReply->isFinished())
        return;

    const QNetworkReply::NetworkError error = m_downloadReply->error();
    const int status = m_downloadReply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorText = m_downloadReply->errorString();
    m_downloadFile->flush();
    m_downloadFile->close();
    delete m_downloadFile;
    m_downloadFile = 0;
    m_downloadReply->deleteLater();
    m_downloadReply = 0;

    if (error != QNetworkReply::NoError ||
        m_downloadBytes < 4096) {
        qDebug() << "WW:PLAYER_LOCAL_FAILED"
                 << status << static_cast<int>(error)
                 << errorText << m_downloadBytes;
        const bool retryProgressively = m_openLocalWhileDownloading;
        if (m_localPlaybackActive && m_player) {
            m_player->stop();
            m_player->clearMedia();
            m_localPlaybackActive = false;
        }
        QFile::remove(m_downloadPath);
        const int nextSourceIndex = m_isLive
            ? nextLiveFlvSourceIndex(m_downloadSourceIndex + 1)
            : m_downloadSourceIndex + 1;
        if (nextSourceIndex >= 0 && nextSourceIndex < m_sourceUrls.size()) {
            qDebug() << "WW:PLAYER_LOCAL_RETRY"
                     << (nextSourceIndex + 1) << m_sourceUrls.size();
            startLocalDownloadFallback(nextSourceIndex, retryProgressively);
        } else {
            m_policyRouteFailed = true;
            qDebug() << "WW:PLAYER_LOCAL_FALLBACK_EXHAUSTED"
                     << m_downloadSourceIndex + 1 << m_sourceUrls.size();
            if (m_overlay)
                m_overlay->update();
        }
        return;
    }

    if (!m_localPlaybackActive)
        openLocalDownloadForPlayback("download-complete");
    qDebug() << "WW:PLAYER_LOCAL_READY"
             << status << m_downloadBytes << m_downloadPath
             << "progressive" << m_openLocalWhileDownloading;
    m_overlay->update();
}

void VideoPlayerWidget::cancelLocalDownloadFallback(bool removeFile)
{
    if (removeFile && m_localPlaybackActive && m_player) {
        m_player->stop();
        m_player->clearMedia();
    }
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = 0;
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = 0;
    }
    if (removeFile && !m_downloadPath.isEmpty()) {
        QFile::remove(m_downloadPath);
        m_downloadPath.clear();
    }
    m_openLocalWhileDownloading = false;
    m_localPlaybackActive = false;
    m_liveFlvDemuxActive = false;
    m_liveFlvAudioOpen = false;
    m_liveFlvVideoStarted = false;
    m_liveFlvAudioBytes = 0;
    m_liveFlvPendingUnits.clear();
    m_liveFlvPendingTimes.clear();
    m_liveFlvPendingPresentationTimes.clear();
    if (m_liveFlvDemuxer)
        m_liveFlvDemuxer->reset();
}

void VideoPlayerWidget::startAvcHardwareProbeMetadata(int sourceIndex)
{
#ifndef Q_OS_SYMBIAN
    Q_UNUSED(sourceIndex);
    return;
#else
    if (m_closing || m_isLive ||
        !m_format.startsWith(QString::fromLatin1("mp4"),
                             Qt::CaseInsensitive) ||
        sourceIndex < 0 || sourceIndex >= m_sourceUrls.size() ||
        !m_avcProbeReader || m_avcProbeReply) {
        return;
    }

    m_avcProbeBytes.clear();
    m_avcProbeRangeStart = 0;
    m_avcProbeRangeEnd = 0;
    m_avcProbeSampleCount = 0;
    m_avcProbeFirstSample = 0;
    m_avcProbeLastSampleExclusive = 0;
    m_avcProbeNextSample = 0;
    m_avcProbeSessionSerial = static_cast<int>(m_sessionSerial);
    m_avcProbeLastPictures = 0;
    m_avcProbeResetDecoder = false;
    m_avcHeaderPreflightPending = false;
    m_avcProbeInputFinished = false;
    m_avcPendingUnits.clear();
    m_avcPendingTimes.clear();
    m_avcPendingPresentationTimes.clear();
    requestAvcHardwareProbeHeader(KAvcProbeHeaderInitialBytes);
#endif
}

void VideoPlayerWidget::requestAvcHardwareProbeHeader(int byteCount)
{
#ifndef Q_OS_SYMBIAN
    Q_UNUSED(byteCount);
    return;
#else
    if (m_closing || m_sourceIndex < 0 ||
        m_sourceIndex >= m_sourceUrls.size() || byteCount <= 0 ||
        byteCount > KAvcProbeHeaderMaximumBytes) {
        m_avcProbeStage = 4;
        qDebug() << "WW:DEVVIDEO_RANGE_HEADER_INVALID"
                 << byteCount << m_sourceIndex;
        return;
    }
    if (m_avcProbeReply) {
        m_avcProbeReply->abort();
        m_avcProbeReply->deleteLater();
        m_avcProbeReply = 0;
    }
    m_avcProbeBytes.clear();
    m_avcProbeRangeStart = 0;
    m_avcProbeRangeEnd = static_cast<quint64>(byteCount - 1);
    m_avcProbeStage = 1;
    m_avcProbeRequestPolls = 0;
    m_avcProbeStalledPolls = 0;
    m_avcProbeLastObservedBytes = 0;
    m_avcProbeLastLoggedBytes = 0;

    QNetworkRequest request(QUrl(m_sourceUrls.at(m_sourceIndex)));
    request.setRawHeader(
        "User-Agent",
        "Mozilla/5.0 (Symbian/3; Nokia603) NIKINIKI/"
        WILIWILI_SYMBIAN_VERSION_STR);
    request.setRawHeader("Referer", m_referer.toUtf8());
    request.setRawHeader("Origin", "https://www.bilibili.com");
    request.setRawHeader(
        "Range",
        QString::fromLatin1("bytes=0-%1")
            .arg(m_avcProbeRangeEnd).toLatin1());
    if (!m_cookieHeader.isEmpty())
        request.setRawHeader("Cookie", m_cookieHeader);
    m_avcProbeReply = m_downloadManager->get(request);
    qDebug() << "WW:DEVVIDEO_RANGE_HEADER_BEGIN"
             << m_avcProbeSessionSerial << m_sourceIndex << byteCount;
#endif
}

void VideoPlayerWidget::requestAvcHardwareProbeRange(
    quint64 firstByte,
    quint64 lastByte,
    int firstSample,
    int lastSampleExclusive,
    bool resetDecoder,
    bool headerPreflight)
{
    if (m_sourceIndex < 0 || m_sourceIndex >= m_sourceUrls.size() ||
        firstByte > lastByte || lastByte - firstByte >= 6 * 1024 * 1024 ||
        firstSample < 0 || lastSampleExclusive <= firstSample) {
        m_avcProbeStage = 4;
        qDebug() << "WW:DEVVIDEO_RANGE_SAMPLE_INVALID"
                 << firstByte << lastByte
                 << firstSample << lastSampleExclusive;
        if (m_player && m_player->isAvcHardwarePlaybackActive())
            m_player->stopAvcHardwarePlayback();
        return;
    }
    m_avcProbeBytes.clear();
    m_avcProbeRangeStart = firstByte;
    m_avcProbeRangeEnd = lastByte;
    m_avcProbeFirstSample = firstSample;
    m_avcProbeLastSampleExclusive = lastSampleExclusive;
    m_avcProbeSampleCount = lastSampleExclusive - firstSample;
    m_avcProbeResetDecoder = resetDecoder;
    m_avcHeaderPreflightPending = headerPreflight;
    m_avcProbeStage = 2;
    m_avcProbeRequestPolls = 0;
    m_avcProbeStalledPolls = 0;
    m_avcProbeLastObservedBytes = 0;
    m_avcProbeLastLoggedBytes = 0;

    QNetworkRequest request(QUrl(m_sourceUrls.at(m_sourceIndex)));
    request.setRawHeader(
        "User-Agent",
        "Mozilla/5.0 (Symbian/3; Nokia603) NIKINIKI/"
        WILIWILI_SYMBIAN_VERSION_STR);
    request.setRawHeader("Referer", m_referer.toUtf8());
    request.setRawHeader("Origin", "https://www.bilibili.com");
    request.setRawHeader(
        "Range",
        QString::fromLatin1("bytes=%1-%2")
            .arg(firstByte).arg(lastByte).toLatin1());
    if (!m_cookieHeader.isEmpty())
        request.setRawHeader("Cookie", m_cookieHeader);
    m_avcProbeReply = m_downloadManager->get(request);
    qDebug() << "WW:DEVVIDEO_PLAYER_RANGE_BEGIN"
             << firstByte << lastByte
             << firstSample << lastSampleExclusive
             << resetDecoder << headerPreflight;
}

void VideoPlayerWidget::requestAvcHardwareSampleBatch(
    int firstSample,
    bool resetDecoder,
    bool headerPreflight)
{
    if (!m_avcProbeReader || !m_avcProbeReader->isValid() ||
        firstSample < 0 || firstSample >= m_avcProbeReader->sampleCount()) {
        m_avcProbeStage = 4;
        handleAvcProbeFailure("probe-no-sample");
        return;
    }
    quint64 firstByte = 0;
    quint64 lastByte = 0;
    int lastSampleExclusive = 0;
    QString rangeError;
    if (!m_avcProbeReader->sampleByteRange(
            firstSample,
            headerPreflight ? 1 :
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
            KAvcSoftBatchDurationMilliseconds,
#else
            5000,
#endif
            6 * 1024 * 1024,
            &firstByte, &lastByte, &lastSampleExclusive, &rangeError)) {
        qDebug() << "WW:DEVVIDEO_PLAYER_RANGE_ERROR" << rangeError;
        m_avcProbeStage = 4;
        if (m_player && m_player->isAvcHardwarePlaybackActive())
            m_player->stopAvcHardwarePlayback();
        handleAvcProbeFailure("probe-range-invalid");
        return;
    }
    requestAvcHardwareProbeRange(
        firstByte, lastByte, firstSample, lastSampleExclusive,
        resetDecoder, headerPreflight);
    if (headerPreflight) {
        qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_RANGE"
                 << firstByte << lastByte
                 << firstSample << lastSampleExclusive;
    }
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
    if (!resetDecoder) {
        qDebug() << "WW:DEVVIDEO_PLAYER_PREFETCH"
                 << KAvcSoftBatchDurationMilliseconds
                 << KAvcSoftPrefetchThresholdUnits
                 << firstSample << lastSampleExclusive;
    }
#endif
}

void VideoPlayerWidget::handOffAvcHardwareProbe(
    const QVector<QByteArray> &accessUnits,
    const QVector<qint64> &decodingTimesMilliseconds,
    const QVector<qint64> &presentationTimesMilliseconds,
    bool resetDecoder)
{
    if (m_avcProbeSessionSerial != static_cast<int>(m_sessionSerial) ||
        m_closing || !m_sessionActive || !m_player || accessUnits.isEmpty() ||
        accessUnits.size() != decodingTimesMilliseconds.size() ||
        accessUnits.size() != presentationTimesMilliseconds.size()) {
        m_avcProbeStage = 4;
        handleAvcProbeFailure("probe-handoff-invalid");
        return;
    }
    qDebug() << "WW:DEVVIDEO_PLAYER_HANDOFF"
             << accessUnits.size()
             << decodingTimesMilliseconds.first()
             << decodingTimesMilliseconds.last()
             << presentationTimesMilliseconds.first()
             << presentationTimesMilliseconds.last()
             << resetDecoder << m_quality;

    // The initial H.264 bytes were deliberately acquired before MMF opened
    // the same remote URL. Start MMF now so it can prepare AAC while the
    // decoder waits below for LoadedMedia.
    if (resetDecoder) {
#ifdef WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_SOLO_DIAGNOSTIC
        // Hardware-resource control case: keep the range-fed AVC batch, but
        // never open CVideoPlayerUtility2.  The deliberate absence of AAC is
        // what lets InitComplete distinguish an MMF-held hardware resource
        // from a public DevVideo memory-output limitation.
        m_mmfSourceOpenDeferred = false;
        qDebug() << "WW:E7_DEVVIDEO_MEMORY_SOLO_MMF_BYPASSED";
#else
        openDeferredMmfSource("soft-prefetch-ready");
#endif
    }

    // MMF must finish Prepare before SetVideoEnabledL(false) can release its
    // native video track. Keep the batch while AAC initialization completes.
    if (resetDecoder &&
#ifndef WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_SOLO_DIAGNOSTIC
        m_player->mediaStatus() != VideoPlaybackBackend::LoadedMedia) {
#else
        false) {
#endif
        m_avcPendingUnits = accessUnits;
        m_avcPendingTimes = decodingTimesMilliseconds;
        m_avcPendingPresentationTimes = presentationTimesMilliseconds;
        m_avcProbeResetDecoder = true;
        m_avcProbeStage = 5;
        qDebug() << "WW:DEVVIDEO_PLAYER_WAIT_MMF";
        return;
    }
    if (resetDecoder && m_softwareVideoRouteSelected &&
        !m_player->isNativeVideoPolicyApplied()) {
        // Some Belle MMF controllers reject SetVideoEnabledL(false) even
        // though the independent FFmpeg video surface can run normally. The
        // opaque software surface hides the MMF window, so this controller
        // optimization must not become a software-decoder admission gate.
        qDebug() << "WW:PLAYER_SOFTWARE_MMF_VIDEO_DISABLE_BEST_EFFORT"
                 << "decoder" << m_decoderMode;
    }

    // The measured three-plane GLES path is not viable on the Nokia 603:
    // texture upload plus QGLWidget presentation takes roughly 300 ms per
    // frame.  Keep the existing CPU YUV420P -> RGB565 QImage route as the
    // default soft renderer.  The decoder and timing pipeline are unchanged.
    const bool requestGlesYuv = false;
    if (resetDecoder)
        m_player->setAvcHardwareYuv420OutputEnabled(requestGlesYuv);
    const bool accepted = resetDecoder
        ? m_player->startAvcHardwarePlayback(
              accessUnits, decodingTimesMilliseconds,
              presentationTimesMilliseconds,
              m_avcProbeReader->width(), m_avcProbeReader->height())
        : m_player->appendAvcHardwarePlayback(
              accessUnits, decodingTimesMilliseconds,
              presentationTimesMilliseconds);
    if (!accepted) {
        m_avcProbeStage = 4;
        qDebug() << "WW:DEVVIDEO_PLAYER_HANDOFF_ERROR"
                 << m_player->avcHardwareError();
        if (m_player->isAvcHardwarePlaybackActive())
            m_player->stopAvcHardwarePlayback();
        if (m_decoderMode == SoftwareOnlyDecoder) {
            m_policyRouteFailed = true;
            if (m_overlay)
                m_overlay->update();
        }
        return;
    }
    if (resetDecoder) {
        m_glesYuvActive = requestGlesYuv;
        if (m_glesYuvActive) {
            // Reuse the application's one long-lived EGL surface. Hide the
            // full-screen native player controller and its MMF child so the
            // QGLWidget is visible below the independent ARGB overlay.
            m_overlay->sourceChanged();
            if (m_delegate)
                m_delegate->videoPlayerClearSoftwareVideo();
            if (m_videoWidget)
                m_videoWidget->hide();
            hide();
            if (m_returnWidget) {
                m_returnWidget->showFullScreen();
                m_returnWidget->raise();
                m_returnWidget->update();
            }
            m_overlay->show();
            m_overlay->raise();
            m_overlay->setFrameInterval(
                KSoftOverlayFrameIntervalMilliseconds);
            qDebug() << "WW:FFMPEG_GLES_FRAME_PACING"
                     << KSoftOverlayFrameIntervalMilliseconds;
            qDebug() << "WW:FFMPEG_GLES_SURFACE_ACTIVE"
                     << m_landscapeApplied
                     << (m_returnWidget ? m_returnWidget->size() : QSize());
        } else if (m_overlay) {
            // Reset the pacing counters exactly where software presentation
            // starts. RGB565 frames will be handed to the opaque native child;
            // this ARGB window now renders danmaku and controls only.
            m_overlay->sourceChanged();
            m_overlay->setFrameInterval(KOverlayFrameIntervalMilliseconds);
            if (m_softVideoSurface)
                m_softVideoSurface->setGeometry(rect());
            m_overlay->show();
            m_overlay->raise();
            qDebug() << "WW:FFMPEG_SOFT_NATIVE_SURFACE_PENDING"
                     << (m_softVideoSurface
                         ? m_softVideoSurface->geometry() : QRect())
                     << "overlay-above";
        }
    }
    m_avcProbeStage = 3;
    m_player->pumpAvcHardwarePlayback();
    m_overlay->update();
}

void VideoPlayerWidget::pollAvcHardwareProbe()
{
    // Live AVC units arrive from the incremental FLV demuxer rather than the
    // finite MP4 Range reader.  Pump/present the same decoder, but never ask
    // Mp4AvcProbeReader for a nonexistent next sample.
    if (m_liveFlvDemuxActive) {
        if (m_player && m_liveFlvVideoStarted)
            m_player->pumpAvcHardwarePlayback();
        return;
    }
    if (m_avcProbeStage == 5 && m_player &&
        m_player->mediaStatus() == VideoPlaybackBackend::LoadedMedia &&
        !m_avcPendingUnits.isEmpty()) {
        const QVector<QByteArray> units = m_avcPendingUnits;
        const QVector<qint64> times = m_avcPendingTimes;
        const QVector<qint64> presentationTimes =
            m_avcPendingPresentationTimes;
        m_avcPendingUnits.clear();
        m_avcPendingTimes.clear();
        m_avcPendingPresentationTimes.clear();
        handOffAvcHardwareProbe(
            units, times, presentationTimes, true);
    }
    const bool hardwareRunning = m_player &&
        (m_avcProbeStage == 3 ||
         (m_avcProbeStage == 2 &&
          m_player->isAvcHardwarePlaybackActive()));
    if (hardwareRunning) {
        m_player->pumpAvcHardwarePlayback();
        if (m_player->avcHardwareError() != 0) {
            qDebug() << "WW:DEVVIDEO_PLAYER_FAILED"
                     << m_player->avcHardwarePictureCount()
                     << m_player->avcHardwareError();
            m_avcProbeStage = 4;
            m_player->stopAvcHardwarePlayback();
        } else if (m_avcProbeStage == 3 && !m_avcProbeReply &&
                   !m_avcProbeInputFinished &&
                   m_player->avcHardwareBufferedUnitCount() <
                       KAvcProbePrefetchThresholdUnits) {
            if (m_avcProbeNextSample <
                m_avcProbeReader->sampleCount()) {
                requestAvcHardwareSampleBatch(
                    m_avcProbeNextSample, false);
            } else {
                m_avcProbeInputFinished = true;
                m_player->finishAvcHardwareInput();
                qDebug() << "WW:DEVVIDEO_PLAYER_ALL_SAMPLES_QUEUED"
                         << m_avcProbeNextSample;
            }
        }
    }
    if (!m_avcProbeReply)
        return;
    if (m_avcProbeSessionSerial != static_cast<int>(m_sessionSerial)) {
        cancelAvcHardwareProbe();
        return;
    }

    m_avcProbeBytes.append(m_avcProbeReply->readAll());
    ++m_avcProbeRequestPolls;
    const int status = m_avcProbeReply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const quint64 requestedLength =
        m_avcProbeRangeEnd - m_avcProbeRangeStart + 1;
    const quint64 neededWhenRangeIgnored = m_avcProbeRangeEnd + 1;
    const quint64 received = static_cast<quint64>(m_avcProbeBytes.size());
    if (m_avcProbeBytes.size() != m_avcProbeLastObservedBytes) {
        m_avcProbeLastObservedBytes = m_avcProbeBytes.size();
        m_avcProbeStalledPolls = 0;
    } else if (!m_avcProbeReply->isFinished()) {
        ++m_avcProbeStalledPolls;
    }
    if (received > 10 * 1024 * 1024 ||
        (status == 200 && neededWhenRangeIgnored > 10 * 1024 * 1024)) {
        qDebug() << "WW:DEVVIDEO_RANGE_LIMIT"
                 << m_avcProbeStage << status << received
                 << neededWhenRangeIgnored;
        m_avcProbeReply->abort();
        m_avcProbeReply->deleteLater();
        m_avcProbeReply = 0;
        m_avcProbeBytes.clear();
        m_avcProbeStage = 4;
        if (m_player && m_player->isAvcHardwarePlaybackActive())
            m_player->stopAvcHardwarePlayback();
        handleAvcProbeFailure("probe-range-limit");
        return;
    }
    const bool enoughWithoutFinish =
        (status == 206 && received >= requestedLength) ||
        (status == 200 && received >= neededWhenRangeIgnored);
    if (!enoughWithoutFinish && !m_avcProbeReply->isFinished() &&
        (m_avcProbeStalledPolls >= KAvcProbeStalledPollLimit ||
         m_avcProbeRequestPolls >= KAvcProbeTotalPollLimit)) {
        qDebug() << "WW:DEVVIDEO_RANGE_TIMEOUT"
                 << m_avcProbeStage << status
                 << m_avcProbeBytes.size()
                 << m_avcProbeRequestPolls << m_avcProbeStalledPolls;
        m_avcProbeReply->abort();
        m_avcProbeReply->deleteLater();
        m_avcProbeReply = 0;
        m_avcProbeBytes.clear();
        m_avcProbeStage = 4;
        if (m_player && m_player->isAvcHardwarePlaybackActive())
            m_player->stopAvcHardwarePlayback();
        handleAvcProbeFailure("probe-range-timeout");
        return;
    }
    if (!m_avcProbeReply->isFinished() && !enoughWithoutFinish)
        return;

    const int completedStage = m_avcProbeStage;
    const QNetworkReply::NetworkError networkError =
        m_avcProbeReply->error();
    const QString networkErrorText = m_avcProbeReply->errorString();
    if (!m_avcProbeReply->isFinished())
        m_avcProbeReply->abort();
    m_avcProbeReply->deleteLater();
    m_avcProbeReply = 0;

    if (networkError != QNetworkReply::NoError && !enoughWithoutFinish) {
        qDebug() << "WW:DEVVIDEO_RANGE_ERROR"
                 << completedStage << status
                 << static_cast<int>(networkError) << networkErrorText
                 << m_avcProbeBytes.size();
        m_avcProbeStage = 4;
        if (m_player && m_player->isAvcHardwarePlaybackActive())
            m_player->stopAvcHardwarePlayback();
        handleAvcProbeFailure("probe-range-error");
        return;
    }

    const quint64 actualBase = status == 206 ? m_avcProbeRangeStart : 0;
    if (completedStage == 1) {
        QString parseError;
        if (!m_avcProbeReader->parseHeader(m_avcProbeBytes, &parseError)) {
            const int completedBytes = static_cast<int>(
                m_avcProbeRangeEnd + 1);
            int retryBytes = 0;
            if (parseError.contains(QString::fromLatin1("moov"))) {
                if (completedBytes < KAvcProbeHeaderMiddleBytes)
                    retryBytes = KAvcProbeHeaderMiddleBytes;
                else if (completedBytes < KAvcProbeHeaderMaximumBytes)
                    retryBytes = KAvcProbeHeaderMaximumBytes;
            }
            qDebug() << "WW:DEVVIDEO_MP4_HEADER_ERROR"
                     << status << m_avcProbeBytes.size() << parseError;
            if (retryBytes > 0) {
                qDebug() << "WW:DEVVIDEO_RANGE_HEADER_RETRY"
                         << completedBytes << retryBytes;
                requestAvcHardwareProbeHeader(retryBytes);
                return;
            }
            m_avcProbeStage = 4;
            handleAvcProbeFailure("probe-header-invalid");
            return;
        }
        qDebug() << "WW:DEVVIDEO_MP4_AVC"
                 << m_avcProbeReader->width()
                 << m_avcProbeReader->height()
                 << m_avcProbeReader->profile()
                 << m_avcProbeReader->compatibility()
                 << m_avcProbeReader->level()
                 << m_avcProbeReader->nalLengthSize()
                 << m_avcProbeReader->timescale()
                 << m_avcProbeReader->maxReferenceFrames()
                 << m_avcProbeReader->maxReorderFrames()
                 << m_avcProbeReader->maxDecodedFrameBuffering()
                 << m_avcProbeReader->weightedPrediction()
                 << m_avcProbeReader->weightedBiPrediction();
        const int firstSample = m_avcProbeReader->syncSampleForTime(
            m_player ? m_player->position() : 0);
        if (firstSample < 0) {
            qDebug() << "WW:DEVVIDEO_PLAYER_NO_SYNC_SAMPLE";
            m_avcProbeStage = 4;
            handleAvcProbeFailure("probe-no-sync");
            return;
        }
        // Auto mode submits the smallest legal header batch to the read-only
        // Broadcom preflight. Software-only skips that hardware decision and
        // immediately asks for a normal FFmpeg prefetch batch from this IDR.
#ifdef WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_DIAGNOSTIC
        // The diagnostic owns its own decoder session, so both Automatic and
        // Hardware-only use the real header as that session's admission gate.
        const bool headerPreflight = true;
#else
        const bool headerPreflight =
            m_decoderMode == AutomaticDecoder;
#endif
        if (!headerPreflight)
            m_softwareVideoRouteSelected = true;
        qDebug() << "WW:PLAYER_DECODER_ROUTE"
                 << (headerPreflight ? "AUTO_PREFLIGHT" : "SOFTWARE");
        requestAvcHardwareSampleBatch(
            firstSample, true, headerPreflight);
        return;
    }

    if (completedStage == 2) {
        QString convertError;
        QVector<Mp4AvcProbeReader::AccessUnit> parsedUnits;
        if (!m_avcProbeReader->makeAnnexBAccessUnits(
            m_avcProbeBytes,
            actualBase,
            m_avcProbeFirstSample,
            m_avcProbeLastSampleExclusive,
            m_avcProbeResetDecoder,
            &parsedUnits,
            &convertError)) {
            qDebug() << "WW:DEVVIDEO_MP4_CONVERT_ERROR" << convertError;
            m_avcProbeStage = 4;
            if (m_player && m_player->isAvcHardwarePlaybackActive())
                m_player->stopAvcHardwarePlayback();
            handleAvcProbeFailure("probe-convert-error");
            return;
        }
        QVector<QByteArray> units;
        QVector<qint64> times;
        QVector<qint64> presentationTimes;
        units.reserve(parsedUnits.size());
        times.reserve(parsedUnits.size());
        presentationTimes.reserve(parsedUnits.size());
        int unitIndex;
        for (unitIndex = 0; unitIndex < parsedUnits.size(); ++unitIndex) {
            units.append(parsedUnits.at(unitIndex).annexB);
            times.append(
                parsedUnits.at(unitIndex).decodingTimeMilliseconds);
            presentationTimes.append(
                parsedUnits.at(unitIndex).presentationTimeMilliseconds);
        }
        const bool headerPreflight = m_avcHeaderPreflightPending;
        m_avcHeaderPreflightPending = false;
        const bool resetDecoder = m_avcProbeResetDecoder;
        if (headerPreflight) {
#ifdef WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_DIAGNOSTIC
            // Do not make a throwaway DevVideo HeaderInformation call here.
            // The next batch is delivered to the one decoder session that
            // will itself parse the real SPS/PPS, ConfigureDecoderL(), and
            // consume the subsequent access units.  This keeps the diagnosis
            // independent of any state a separate temporary session might
            // leave in the E7 multimedia stack.
            qDebug() << "WW:E7_DEVVIDEO_MEMORY_SESSION_ROUTE"
                     << "REAL_HEADER_CONFIGURE" << units.first().size();
            m_softwareVideoRouteSelected = true;
            requestAvcHardwareSampleBatch(
                m_avcProbeFirstSample, true, false);
            return;
#else
            if (!m_player) {
                qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_ROUTE"
                         << "MMF" << "no-player";
                m_avcProbeStage = 4;
                handleAvcProbeFailure("probe-header-no-player");
                return;
            }
            int headerError = 0;
            const VideoPlaybackBackend::AvcHeaderProbeResult result =
                m_player->probeAvcHardwareHeader(units.first(), &headerError);
            if (result == VideoPlaybackBackend::AvcHeaderProbeAccepted) {
                qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_ROUTE"
                         << "MMF" << headerError;
                m_avcProbeStage = 4;
                openDeferredMmfSource("probe-header-accepted");
                return;
            }

            // A rejected header is the only normal route into FFmpeg. Start
            // a real prefetch batch from the same IDR rather than reusing the
            // tiny header sample: this preserves the established queue and
            // audio/MMF handoff behaviour of the software path.
            qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_ROUTE"
                     << "FFMPEG" << headerError;
            m_softwareVideoRouteSelected = true;
            requestAvcHardwareSampleBatch(
                m_avcProbeFirstSample, true, false);
            return;
#endif
        }
        m_avcProbeNextSample = m_avcProbeLastSampleExclusive;
        handOffAvcHardwareProbe(
            units, times, presentationTimes, resetDecoder);
    }
}

void VideoPlayerWidget::cancelAvcHardwareProbe()
{
    if (m_avcProbeReply) {
        m_avcProbeReply->abort();
        m_avcProbeReply->deleteLater();
        m_avcProbeReply = 0;
    }
    m_avcProbeBytes.clear();
    m_avcProbeRangeStart = 0;
    m_avcProbeRangeEnd = 0;
    m_avcProbeStage = 0;
    m_avcProbeSampleCount = 0;
    m_avcProbeFirstSample = 0;
    m_avcProbeLastSampleExclusive = 0;
    m_avcProbeNextSample = 0;
    m_avcProbeSessionSerial = 0;
    m_avcProbeLastPictures = 0;
    m_avcProbeRequestPolls = 0;
    m_avcProbeStalledPolls = 0;
    m_avcProbeLastObservedBytes = 0;
    m_avcProbeLastLoggedBytes = 0;
    m_avcPendingUnits.clear();
    m_avcPendingTimes.clear();
    m_avcPendingPresentationTimes.clear();
    m_avcProbeResetDecoder = false;
    m_avcHeaderPreflightPending = false;
    m_avcProbeInputFinished = false;
    m_mmfSourceOpenDeferred = false;
    if (m_player)
        m_player->stopAvcHardwarePlayback();
    if (m_delegate)
        m_delegate->videoPlayerClearSoftwareVideo();
    if (m_overlay)
        m_overlay->setFrameInterval(KOverlayFrameIntervalMilliseconds);
    m_glesYuvActive = false;
    clearSoftwareVideoSurface();
}

void VideoPlayerWidget::togglePlayback()
{
    if (!m_player)
        return;
    if (m_player->state() == VideoPlaybackBackend::PlayingState)
        m_player->pause();
    else
        m_player->play();
    if (m_overlay)
        m_overlay->invalidatePositionCache();
    m_overlay->update();
}

void VideoPlayerWidget::adjustVolume(int delta)
{
    const int adjusted = qBound(0, m_volume + delta, 100);
    if (adjusted != m_volume) {
        m_volume = adjusted;
        if (m_player)
            m_player->setVolume(m_volume);
        qDebug() << "WW:PLAYER_VOLUME" << m_volume;
    }
    if (m_overlay)
        m_overlay->revealControls();
}

void VideoPlayerWidget::saveVolumePreference() const
{
    QSettings settings(
        QSettings::IniFormat, QSettings::UserScope,
        QString::fromLatin1("wiliwili"),
        QString::fromLatin1("wiliwili_symbian"));
    settings.setValue(QString::fromLatin1("player/volume"), m_volume);
}

void VideoPlayerWidget::seekBy(qint64 deltaMilliseconds)
{
    if (m_isLive || !m_player)
        return;
    const qint64 target = qBound<qint64>(
        0, m_player->position() + deltaMilliseconds,
        qMax<qint64>(0, m_player->duration()));
    seekTo(target);
}

void VideoPlayerWidget::seekTo(qint64 milliseconds)
{
    if (m_isLive || !m_player)
        return;
    const qint64 target = qBound<qint64>(
        0, milliseconds, qMax<qint64>(0, m_player->duration()));
    m_player->setPosition(target);
    if (m_overlay)
        m_overlay->seekDanmakuTo(target);

    if (!m_avcProbeReader || !m_avcProbeReader->isValid() ||
        (m_avcProbeStage != 2 && m_avcProbeStage != 3 &&
         m_avcProbeStage != 5)) {
        return;
    }
    if (m_avcProbeReply) {
        m_avcProbeReply->abort();
        m_avcProbeReply->deleteLater();
        m_avcProbeReply = 0;
    }
    m_avcProbeBytes.clear();
    m_avcPendingUnits.clear();
    m_avcPendingTimes.clear();
    m_avcPendingPresentationTimes.clear();
    m_avcProbeInputFinished = false;
    m_player->stopAvcHardwarePlayback();
    if (m_overlay)
        m_overlay->sourceChanged();
    const int firstSample =
        m_avcProbeReader->syncSampleForTime(target);
    qDebug() << "WW:DEVVIDEO_PLAYER_SEEK"
             << target << firstSample;
    if (firstSample >= 0)
        requestAvcHardwareSampleBatch(firstSample, true);
    else
        m_avcProbeStage = 4;
}

void VideoPlayerWidget::requestQuality(int quality)
{
    if (quality <= 0 || quality == m_quality || !m_delegate)
        return;
    m_automaticFallbackTarget = 0;
    qDebug() << "WW:PLAYER_QUALITY_REQUEST" << quality << m_isLive;
    m_delegate->videoPlayerRequestQuality(quality);
}

QString VideoPlayerWidget::qualityLabel() const
{
    int index;
    for (index = 0; index < m_qualities.size(); ++index) {
        if (m_qualities.at(index).quality == m_quality)
            return m_qualities.at(index).description;
    }
    return QString::fromLatin1("Q%1").arg(m_quality);
}

QString VideoPlayerWidget::playbackStatus() const
{
    if (m_policyRouteFailed) {
        if (m_isLive)
            return QString::fromLatin1("LIVEERR");
        return m_decoderMode == SoftwareOnlyDecoder
            ? QString::fromLatin1("SWERR")
            : QString::fromLatin1("DLERR");
    }
    if (m_downloadReply) {
        if (m_isLive &&
            (m_openLocalWhileDownloading || m_liveFlvDemuxActive)) {
            const qint64 buffered = m_liveFlvDemuxActive
                ? m_liveFlvAudioBytes : m_downloadBytes;
            const qint64 target = m_liveFlvDemuxActive
                ? KLiveAacStartBytes : KOpenFileStreamingStartBytes;
            return QString::fromLatin1("BUF%1%")
                .arg(qBound(0, static_cast<int>(
                    buffered * 100 / target), 100));
        }
        if (m_downloadTotalBytes > 0) {
            return QString::fromLatin1("DL%1%")
                .arg(qBound(0, static_cast<int>(
                    m_downloadBytes * 100 / m_downloadTotalBytes), 100));
        }
        return QString::fromLatin1("DL%1M")
            .arg(m_downloadBytes / (1024 * 1024));
    }
    if (!m_player)
        return QString::fromLatin1("ROTATE");
    if (m_player->error() != VideoPlaybackBackend::NoError)
        return QString::fromLatin1("ERR%1").arg(m_player->error());
    if (m_player->mediaStatus() == VideoPlaybackBackend::BufferingMedia ||
        m_player->mediaStatus() == VideoPlaybackBackend::StalledMedia) {
        return QString::fromLatin1("BUF%1").arg(m_player->bufferStatus());
    }
    if (m_player->state() == VideoPlaybackBackend::PlayingState)
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
        return m_player->isAvcHardwarePlaybackActive()
            ? QString::fromLatin1("SW")
            : !m_isLive && !m_player->isVideoAvailable()
            ? QString::fromLatin1("AUDIO")
            : QString::fromLatin1("PLAY");
#else
        return m_player->isAvcHardwarePlaybackActive()
            ? QString::fromLatin1("HW")
            : !m_isLive && !m_player->isVideoAvailable()
            ? QString::fromLatin1("AUDIO")
            : QString::fromLatin1("PLAY");
#endif
    if (m_player->state() == VideoPlaybackBackend::PausedState)
        return QString::fromLatin1("PAUSE");
    return QString::fromLatin1("LOAD");
}

void VideoPlayerWidget::setScreenAwakeEnabled(bool enabled)
{
    if (enabled) {
        if (!m_screenAwakeTimerId) {
            m_screenAwakeTimerId = startTimer(5000);
            qDebug() << "WW:SCREEN_AWAKE_ON";
        }
    } else if (m_screenAwakeTimerId) {
        killTimer(m_screenAwakeTimerId);
        m_screenAwakeTimerId = 0;
        qDebug() << "WW:SCREEN_AWAKE_OFF";
    }
}

void VideoPlayerWidget::closePlayer()
{
    if (m_closing)
        return;
    saveVolumePreference();
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    if (m_directProbeActive) {
        if (m_directProbe)
            m_directProbe->stop();
        m_directProbeActive = false;
        if (m_overlay)
            m_overlay->setDirectProbeMode(false);
        qDebug() << "WW:DIRECT_PROBE_MANUAL_STOP";
    }
#endif
    m_closing = true;
    setScreenAwakeEnabled(false);
    if (m_startTimerId) {
        killTimer(m_startTimerId);
        m_startTimerId = 0;
    }
    if (m_retryTimerId) {
        killTimer(m_retryTimerId);
        m_retryTimerId = 0;
    }
    stopOrientationTimeout();
    m_pendingSourceIndex = -1;
    hideNativeDanmaku();
    if (m_overlay)
        m_overlay->detachOwner(this);
    if (m_videoWidget)
        m_videoWidget->hide();
    clearSoftwareVideoSurface();
    if (m_delegate)
        m_delegate->videoPlayerClearSoftwareVideo();
    m_glesYuvActive = false;
    cancelAvcHardwareProbe();
    cancelLocalDownloadFallback(true);
    hide();
    releasePlaybackSurfaceForOrientation();
    m_restoreClosesSession = true;

    QDesktopWidget *desktop = QApplication::desktop();
    const bool nativeLandscapeNow = m_landscapeApplied ||
        m_orientationStage == NativeLandscapeVisible ||
        m_orientationStage == NativeWaitingLandscapeWorkArea ||
        desktop->screenGeometry(0) == QRect(0, 0, 640, 360);
    if (nativeLandscapeNow ||
        m_orientationStage == NativeWaitingPortraitChrome) {
        // closePlayer() can run inside the overlay's pointer event. Defer all
        // native window/orientation work until that event has unwound.
        QTimer::singleShot(0, this, SLOT(beginNativePortraitRestore()));
    } else {
        m_orientationStage = NativePortraitIdle;
        m_landscapeApplied = false;
        restoreMainWindow();
        finishCloseAfterOrientation();
    }
}

void VideoPlayerWidget::finishCloseAfterOrientation()
{
    if (!m_sessionActive)
        return;
    // Keep session teardown self-contained. Normal Back already disables this
    // in closePlayer(); orientation-abort and future teardown paths must not
    // depend on having entered through that function.
    setScreenAwakeEnabled(false);
    m_sessionActive = false;
    qDebug() << "WW:PLAYER_SESSION_PARKED" << m_sessionSerial
             << static_cast<void *>(this)
             << static_cast<void *>(m_videoWidget)
             << static_cast<void *>(m_softVideoSurface)
             << static_cast<void *>(m_player)
             << "orientation" << static_cast<int>(m_orientationStage);
    requestPlayerPlatformForeground();
    if (m_delegate)
        m_delegate->videoPlayerDidClose();
    m_closing = false;
    m_restoreClosesSession = false;
}

void VideoPlayerWidget::restoreMainWindow()
{
    if (m_returnWidget) {
        // Dynamic fullscreen is the device-verified way to reclaim the whole
        // 360x640 work area while keeping Avkon panes constructed for the next
        // orientation transition.
        m_returnWidget->showFullScreen();
        m_returnWidget->raise();
        m_returnWidget->activateWindow();
        QApplication::setActiveWindow(m_returnWidget);
        m_returnWidget->setFocus(Qt::ActiveWindowFocusReason);
        m_returnWidget->update();
    }
}

void VideoPlayerWidget::updateVideoGeometry(
    bool controlsVisible, bool qualityMenuVisible)
{
    Q_UNUSED(controlsVisible);
    Q_UNUSED(qualityMenuVisible);
    if (!m_videoWidget)
        return;
    // Video fills the actual native-orientation window. Controls and danmaku
    // are true overlays and no longer steal a safe-area strip.
    const QRect targetRect = rect();
    if (m_videoWidget->geometry() != targetRect)
        m_videoWidget->setGeometry(targetRect);
    if (m_softVideoSurface &&
        m_softVideoSurface->geometry() != targetRect)
        m_softVideoSurface->setGeometry(targetRect);
    // Once GLES or the opaque RGB565 surface owns presentation, the native
    // MMF video window is hidden and only AAC remains active. Re-entering the
    // synchronous MMF geometry API from overlay/UI events adds avoidable
    // event-loop stalls on the 603.
    if (m_player && !m_glesYuvActive && !m_softVideoActive)
        m_player->updateVideoWindow();
}

void VideoPlayerWidget::updateOverlayGeometry()
{
    if (!m_overlay)
        return;
    const QPoint globalTopLeft = mapToGlobal(QPoint(0, 0));
    m_overlay->setGeometry(QRect(globalTopLeft, size()));
}

bool VideoPlayerWidget::isPlayerVisible() const
{
    return isVisible();
}

QString VideoPlayerWidget::softPlaybackTelemetry(
    quint64 presentedCount,
    quint64 uploadedCount,
    qint64 uploadMilliseconds,
    qint64 lastPresentedPts) const
{
    quint64 effectivePresented = presentedCount;
    qint64 effectiveLastPts = lastPresentedPts;
    if (m_softVideoActive && m_softVideoSurface) {
        effectivePresented = m_softVideoSurface->presentedCount();
        effectiveLastPts = m_softVideoSurface->lastPresentedPts();
    }
    const QString decoderTelemetry = m_player
        ? m_player->softPlaybackTelemetry(
              effectivePresented, uploadedCount,
              uploadMilliseconds, effectiveLastPts)
        : QString();
    if (decoderTelemetry.isEmpty() || !m_overlay)
        return decoderTelemetry;
    const QString surfaceTelemetry = m_softVideoSurface
        ? m_softVideoSurface->telemetry() : QString();
    return decoderTelemetry + surfaceTelemetry +
        m_overlay->pacingTelemetry();
}

bool VideoPlayerWidget::ownsForeground() const
{
    return m_sessionActive;
}

void VideoPlayerWidget::handleForegroundKey(QKeyEvent *event)
{
    if (event)
        keyPressEvent(event);
}

void VideoPlayerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // The final app-shell probe proved that an opaque raster top-level can be
    // painted safely after the 640x360 work-area notification. Keep exposed
    // regions deterministic while MMF or the software frame is starting.
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
}

void VideoPlayerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // The validated orientation sequence never mutates the player window tree
    // from a resize callback. Geometry is committed only after the matching
    // workAreaResized() state has been observed.
    if (nativeOrientationTransitionActive())
        return;
    updateOverlayGeometry();
    updateVideoGeometry(
        m_overlay->controlsVisible(),
        m_overlay->qualityMenuVisible());
    if (m_softVideoActive && m_softVideoSurface)
        m_softVideoSurface->raise();
    else if (m_videoWidget)
        m_videoWidget->raise();
    if (m_overlay && m_overlay->isVisible())
        m_overlay->raise();
}

void VideoPlayerWidget::mousePressEvent(QMouseEvent *event)
{
    event->accept();
}

void VideoPlayerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // Fallback if WSERV sends a tap to the native video host instead of the
    // top-level ARGB overlay.
    if (m_overlay)
        m_overlay->toggleControlsFromVideo();
    event->accept();
}

void VideoPlayerWidget::keyPressEvent(QKeyEvent *event)
{
    bool handled = true;
    if (event->key() == Qt::Key_Back ||
        event->key() == Qt::Key_Escape ||
        event->key() == Qt::Key_Backspace) {
        closePlayer();
    } else if (event->key() == Qt::Key_Left) {
        seekBy(-10000);
    } else if (event->key() == Qt::Key_Right) {
        seekBy(10000);
    } else if (event->key() == Qt::Key_Menu) {
        m_overlay->toggleQualityMenu();
    } else if ((event->key() == Qt::Key_Up ||
                event->key() == Qt::Key_VolumeUp) && m_player) {
        adjustVolume(5);
    } else if ((event->key() == Qt::Key_Down ||
                event->key() == Qt::Key_VolumeDown) && m_player) {
        adjustVolume(-5);
    } else if (event->key() == Qt::Key_Space ||
               event->key() == Qt::Key_Select ||
               event->key() == Qt::Key_Enter ||
               event->key() == Qt::Key_Return) {
        togglePlayback();
    } else {
        handled = false;
        QWidget::keyPressEvent(event);
    }
    if (handled)
        event->accept();
}

void VideoPlayerWidget::closeEvent(QCloseEvent *event)
{
    closePlayer();
    event->ignore();
}

void VideoPlayerWidget::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_screenAwakeTimerId) {
#ifdef Q_OS_SYMBIAN
        // Reset the OS inactivity timer so the screen saver/backlight timeout
        // cannot engage while a playback session is open. This capability-free
        // mechanism was the standard Symbian approach for games and players.
        if (!m_closing && m_sessionActive && isVisible())
            User::ResetInactivityTime();
#endif
        event->accept();
        return;
    }
    if (event->timerId() == m_orientationTimerId) {
        killTimer(m_orientationTimerId);
        m_orientationTimerId = 0;
        QDesktopWidget *desktop = QApplication::desktop();
        qDebug() << "WW:PLAYER_NATIVE_ORIENTATION_TIMEOUT"
                 << "stage" << static_cast<int>(m_orientationStage)
                 << "available" << playerRectText(
                        desktop->availableGeometry(0))
                 << "physical" << playerRectText(
                        desktop->screenGeometry(0));
        failOrientationTransition("timeout");
        event->accept();
        return;
    }
    if (event->timerId() == m_startTimerId) {
        killTimer(m_startTimerId);
        m_startTimerId = 0;
        if (!m_closing) {
            qDebug() << "WW:PLAYER_REBUILD_BEGIN"
                     << m_landscapeApplied << size() << isVisible();
            // Orientation and fullscreen geometry were committed before this
            // delayed media start. Resolve the persistent native child only
            // after that stable workAreaResized() boundary.
            winId();
            qDebug() << "WW:PLAYER_NATIVE_ROOT_READY"
                     << size() << winId() << isWindow() << isFullScreen();
            raise();
            setFocus(Qt::ActiveWindowFocusReason);
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
            if (m_directProbeActive) {
                if (!m_softVideoSurface)
                    createVideoOutputWidget();
                updateOverlayGeometry();
                updateVideoGeometry(true, false);
                if (m_videoWidget)
                    m_videoWidget->hide();
                m_softVideoActive = true;
                m_softVideoSurface->setGeometry(rect());
                m_softVideoSurface->activateSurface();
                m_overlay->show();
                m_overlay->prepareNativeWindow();
                m_overlay->raise();
                requestPlayerPlatformForeground();
                if (!m_directProbe) {
                    m_directProbe = new DevVideoDirectProbe(
                        m_softVideoSurface, m_overlay, this);
                    connect(
                        m_directProbe,
                        SIGNAL(finished(bool,bool)),
                        this,
                        SLOT(onDevVideoDirectProbeFinished(bool,bool)));
                }
                QTimer::singleShot(
                    0, m_directProbe, SLOT(start()));
                qDebug() << "WW:DIRECT_PROBE_NATIVE_READY"
                         << size() << m_softVideoSurface->geometry()
                         << m_overlay->geometry();
                event->accept();
                return;
            }
#endif
            const bool surfaceWasReleased = !m_videoWidget;
            if (!m_videoWidget)
                createVideoOutputWidget();
            updateOverlayGeometry();
            updateVideoGeometry(
                m_overlay->controlsVisible(),
                m_overlay->qualityMenuVisible());
            if (m_softVideoActive && m_softVideoSurface) {
                m_softVideoSurface->show();
                m_softVideoSurface->raise();
            } else {
#ifdef WILIWILI_ENABLE_E7_MMF_ROOT_WINDOW_DIAGNOSTIC
                // The root-window diagnostic must leave this full-screen Qt
                // native child hidden.  Showing it here after the delayed
                // orientation start would cover MMF's root RWindow output and
                // invalidate the experiment.
                m_videoWidget->hide();
                qDebug() << "WW:E7_MMF_ROOT_WINDOW_CHILD_HIDDEN"
                         << m_videoWidget->geometry();
#else
                m_videoWidget->show();
                m_videoWidget->winId();
#endif
            }
            if (!m_player)
                recreateMediaPlayer(true);
#ifndef WILIWILI_ENABLE_E7_MMF_ROOT_WINDOW_DIAGNOSTIC
            if (!m_softVideoActive)
                m_videoWidget->raise();
#endif
#ifdef WILIWILI_ENABLE_E7_MMF_BARE_WINDOW_DIAGNOSTIC
            // Do not let the delayed media-start path resurrect the ARGB
            // overlay after startPlaybackPresentation() hid it.
            m_overlay->hide();
            qDebug() << "WW:E7_MMF_BARE_WINDOW_OVERLAY_HIDDEN"
                     << "media-start";
#else
            m_overlay->show();
            m_overlay->prepareNativeWindow();
            m_overlay->raise();
#endif
            requestPlayerPlatformForeground();
            if (surfaceWasReleased)
                qDebug() << "WW:PLAYER_NATIVE_HOST_REBUILT"
                         << size() << m_videoWidget->geometry();
            qDebug() << "WW:PLAYER_NATIVE_READY"
                     << size() << m_videoWidget->geometry();
            loadSourceAt(0);
        }
        event->accept();
        return;
    }
    if (event->timerId() == m_retryTimerId) {
        killTimer(m_retryTimerId);
        m_retryTimerId = 0;
        const int sourceIndex = m_pendingSourceIndex;
        m_pendingSourceIndex = -1;
        if (!m_closing && isVisible())
            loadSourceAt(sourceIndex);
        event->accept();
        return;
    }
    if (event->timerId() != m_pollTimerId) {
        QWidget::timerEvent(event);
        return;
    }
    pollLocalDownloadFallback();
    pollAvcHardwareProbe();
    if (isVisible() && m_player && m_localPlaybackActive &&
        m_player->error() != VideoPlaybackBackend::NoError &&
        m_lastReportedError != static_cast<int>(m_player->error())) {
        m_lastReportedError = static_cast<int>(m_player->error());
        qDebug() << "WW:PLAYER_LOCAL_ERROR"
                 << m_lastReportedError << m_player->errorString();
        if (m_liveFlvDemuxActive) {
            retryLiveFlvDemuxFallback("mmf-audio");
        } else if (m_isLive) {
            const int failedSourceIndex = m_downloadSourceIndex;
            const int nextSourceIndex = nextLiveFlvSourceIndex(
                failedSourceIndex + 1);
            cancelLocalDownloadFallback(true);
            if (nextSourceIndex >= 0) {
                qDebug() << "WW:PLAYER_LOCAL_RETRY"
                         << (nextSourceIndex + 1) << m_sourceUrls.size();
                startLocalDownloadFallback(nextSourceIndex, true);
            } else {
                m_policyRouteFailed = true;
                qDebug() << "WW:PLAYER_LOCAL_FALLBACK_EXHAUSTED"
                         << (failedSourceIndex + 1) << m_sourceUrls.size();
                if (m_overlay)
                    m_overlay->update();
            }
        }
    }
    if (isVisible() && m_player && !m_retryTimerId &&
        !m_downloadReply && !m_localPlaybackActive &&
        m_player->error() != VideoPlaybackBackend::NoError &&
        m_lastReportedError != static_cast<int>(m_player->error())) {
        m_lastReportedError = static_cast<int>(m_player->error());
        qDebug() << "WW:PLAYER_ERROR"
                 << m_lastReportedError << m_player->errorString()
                 << (m_sourceIndex + 1) << m_sourceUrls.size();
        if (m_isLive && m_liveMimeVariant < 2) {
            ++m_liveMimeVariant;
            recreateMediaPlayer(true);
            qDebug() << "WW:PLAYER_LIVE_MIME_FALLBACK"
                     << m_liveMimeVariant << (m_sourceIndex + 1);
            scheduleSourceAt(m_sourceIndex, 300);
        } else if (m_isLive && !m_localFallbackAttempted &&
                   nextLiveFlvSourceIndex(m_sourceIndex) == m_sourceIndex) {
            // The Nokia 603 trace proves that explicit FLV MIME types are
            // rejected by OpenUrlL and the sniffed URL reaches Prepare only
            // to fail there. Do not repeat that firmware-level result across
            // every CDN and protocol. Move forward once to on-device FLV
            // demux; any subsequent CDN retries remain on that path.
            qDebug() << "WW:PLAYER_LIVE_DEMUX_FALLBACK"
                     << (m_sourceIndex + 1) << "after-direct";
            startLiveFlvDemuxFallback(m_sourceIndex);
        } else if (m_sourceIndex + 1 < m_sourceUrls.size()) {
            m_liveMimeVariant = 0;
            scheduleSourceAt(m_sourceIndex + 1, 300);
        } else if (!m_isLive && m_sourcePass < 1 &&
                   !m_sourceUrls.isEmpty()) {
            // CDN and the Belle media service are both observed to recover on
            // a later open. One bounded second pass handles that transient
            // case without leaving the player in an endless spinner.
            ++m_sourcePass;
            m_liveMimeVariant = 0;
            recreateMediaPlayer(false);
            qDebug() << "WW:PLAYER_BACKEND_FALLBACK";
            scheduleSourceAt(0, 800);
        } else if (!m_localFallbackAttempted && m_isLive) {
            const int flvIndex = nextLiveFlvSourceIndex(0);
            if (flvIndex >= 0) {
                qDebug() << "WW:PLAYER_LIVE_DEMUX_FALLBACK"
                         << (flvIndex + 1);
                startLiveFlvDemuxFallback(flvIndex);
            } else {
                m_localFallbackAttempted = true;
                qDebug() << "WW:PLAYER_LIVE_DEMUX_UNAVAILABLE";
            }
        } else if (!m_localFallbackAttempted && !m_isLive) {
            qDebug() << "WW:PLAYER_SOURCES_EXHAUSTED"
                     << m_sourceUrls.size() << m_sourcePass;
            startLocalDownloadFallback(0);
        } else {
            qDebug() << "WW:PLAYER_FALLBACK_EXHAUSTED"
                     << m_sourceUrls.size() << m_sourcePass;
        }
    }
    if (isVisible() && m_player && !m_isLive && m_delegate &&
        m_player->state() == VideoPlaybackBackend::PlayingState &&
        !m_player->isVideoAvailable() &&
        m_avcProbeStage != 1 && m_avcProbeStage != 2 &&
        m_avcProbeStage != 3 && m_avcProbeStage != 5 &&
        m_sourceClock.isValid() && m_sourceClock.elapsed() > 6000) {
        int lowerQuality = 0;
        int qualityIndex;
        for (qualityIndex = 0;
             qualityIndex < m_qualities.size(); ++qualityIndex) {
            const int candidate = m_qualities.at(qualityIndex).quality;
            if (candidate > 0 && candidate < m_quality &&
                candidate > lowerQuality) {
                lowerQuality = candidate;
            }
        }
        if (lowerQuality > 0 &&
            lowerQuality != m_automaticFallbackTarget) {
            m_automaticFallbackTarget = lowerQuality;
            qDebug() << "WW:PLAYER_VIDEO_MISSING_FALLBACK"
                     << m_quality << lowerQuality;
            m_delegate->videoPlayerRequestQuality(lowerQuality);
        }
    }
    m_overlay->update();
    event->accept();
}

} // namespace wiliwili
