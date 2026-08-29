#ifndef WILIWILI_SYMBIAN_VIDEO_PLAYER_WIDGET_H
#define WILIWILI_SYMBIAN_VIDEO_PLAYER_WIDGET_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTime>
#include <QtCore/QVector>
#include <QtGui/QWidget>

#include "model/playback_source.h"

class QCloseEvent;
class QEvent;
class QFile;
class QKeyEvent;
class QMouseEvent;
class QNetworkAccessManager;
class QNetworkReply;
class QImage;
class QPaintEvent;
class QResizeEvent;

namespace wiliwili {

class Mp4AvcProbeReader;
class SoftVideoSurfaceWidget;
class VideoOverlayWidget;
class VideoPlaybackBackend;
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
class DevVideoDirectProbe;
#endif
struct Yuv420Frame;

class VideoPlayerDelegate
{
public:
    virtual ~VideoPlayerDelegate() {}
    virtual void videoPlayerRequestQuality(int quality) = 0;
    virtual void videoPlayerDidClose() = 0;
    virtual bool videoPlayerCanPresentYuv420() const = 0;
    virtual void videoPlayerPresentYuv420(
        const Yuv420Frame &frame) = 0;
    virtual void videoPlayerClearSoftwareVideo() = 0;
};

class VideoPlayerWidget : public QWidget
{
    Q_OBJECT

public:
    VideoPlayerWidget(
        QWidget *returnWidget,
        VideoPlayerDelegate *delegate);
    virtual ~VideoPlayerWidget();

    void openSource(
        const PlaybackSourceCompat &source,
        const QString &title,
        const QByteArray &cookieHeader);
    void setDanmaku(const QVector<DanmakuItemCompat> &items);
    void closePlayer();

    bool isPlayerVisible() const;
    bool ownsForeground() const;
    QString softPlaybackTelemetry(
        quint64 presentedCount,
        quint64 uploadedCount,
        qint64 uploadMilliseconds,
        qint64 lastPresentedPts) const;
    void handleForegroundKey(QKeyEvent *event);

public slots:
    void startDevVideoDirectProbe();

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void closeEvent(QCloseEvent *event);
    virtual void timerEvent(QTimerEvent *event);

private slots:
    void requestNativeLandscapeOrientation();
    void onDesktopWorkAreaResized(int screen);
    void onDesktopScreenResized(int screen);
    void commitNativeLandscapeWindow();
    void beginNativePortraitRestore();
    void commitNativePortraitWindow();
    void completeNativePortraitFullscreen();
    void onDevVideoDirectProbeFinished(
        bool phaseAPassed, bool phaseBPassed);
    void closeDevVideoDirectProbe();

private:
    VideoPlayerWidget(const VideoPlayerWidget &);
    VideoPlayerWidget &operator=(const VideoPlayerWidget &);
    friend class VideoOverlayWidget;

    enum NativeOrientationStage {
        NativePortraitIdle = 0,
        NativeWaitingPortraitChrome = 1,
        NativeWaitingLandscapeWorkArea = 2,
        NativeLandscapeVisible = 3,
        NativeWaitingPortraitWorkArea = 4,
        NativeWaitingPortraitFullscreen = 5
    };

    void togglePlayback();
    void seekBy(qint64 deltaMilliseconds);
    void seekTo(qint64 milliseconds);
    void requestQuality(int quality);
    void createVideoOutputWidget();
    void presentSoftwareFrame(const QImage &frame, qint64 timestamp);
    void clearSoftwareVideoSurface();
    void releasePlaybackSurfaceForOrientation();
    void recreateMediaPlayer(bool streamPlayback);
    void loadSourceAt(int index);
    void openMmfSourceAt(int index, const char *reason);
    void openDeferredMmfSource(const char *reason);
    void scheduleSourceAt(int index, int delayMilliseconds);
    void startLocalDownloadFallback(int sourceIndex);
    void pollLocalDownloadFallback();
    void cancelLocalDownloadFallback(bool removeFile);
    void startAvcHardwareProbeMetadata(int sourceIndex);
    void pollAvcHardwareProbe();
    void cancelAvcHardwareProbe();
    void requestAvcHardwareProbeHeader(int byteCount);
    void requestAvcHardwareProbeRange(
        quint64 firstByte,
        quint64 lastByte,
        int firstSample,
        int lastSampleExclusive,
        bool resetDecoder,
        bool headerPreflight);
    void handOffAvcHardwareProbe(
        const QVector<QByteArray> &accessUnits,
        const QVector<qint64> &decodingTimesMilliseconds,
        const QVector<qint64> &presentationTimesMilliseconds,
        bool resetDecoder);
    void requestAvcHardwareSampleBatch(
        int firstSample,
        bool resetDecoder,
        bool headerPreflight = false);
    bool updateNativeDanmaku(
        const QVector<DanmakuItemCompat> &items,
        bool enabled);
    void hideNativeDanmaku();
    void beginNativeLandscapeTransition();
    void startPlaybackPresentation();
    void finishCloseAfterOrientation();
    void failOrientationTransition(const char *reason);
    void restartOrientationTimeout();
    void stopOrientationTimeout();
    bool nativeOrientationTransitionActive() const;
    void restoreMainWindow();
    void updateVideoGeometry(bool controlsVisible, bool qualityMenuVisible);
    void updateOverlayGeometry();
    QString playbackStatus() const;
    QString qualityLabel() const;
    void setScreenAwakeEnabled(bool enabled);

    QWidget *m_returnWidget;
    VideoPlayerDelegate *m_delegate;
    VideoPlaybackBackend *m_player;
    QNetworkAccessManager *m_downloadManager;
    QNetworkReply *m_downloadReply;
    QFile *m_downloadFile;
    Mp4AvcProbeReader *m_avcProbeReader;
    QNetworkReply *m_avcProbeReply;
    QByteArray m_avcProbeBytes;
    quint64 m_avcProbeRangeStart;
    quint64 m_avcProbeRangeEnd;
    int m_avcProbeStage;
    int m_avcProbeSampleCount;
    int m_avcProbeFirstSample;
    int m_avcProbeLastSampleExclusive;
    int m_avcProbeNextSample;
    int m_avcProbeSessionSerial;
    int m_avcProbeLastPictures;
    int m_avcProbeRequestPolls;
    int m_avcProbeStalledPolls;
    int m_avcProbeLastObservedBytes;
    int m_avcProbeLastLoggedBytes;
    QVector<QByteArray> m_avcPendingUnits;
    QVector<qint64> m_avcPendingTimes;
    QVector<qint64> m_avcPendingPresentationTimes;
    bool m_avcProbeResetDecoder;
    bool m_avcHeaderPreflightPending;
    bool m_avcProbeInputFinished;
    bool m_mmfSourceOpenDeferred;
    QWidget *m_videoWidget;
    SoftVideoSurfaceWidget *m_softVideoSurface;
    VideoOverlayWidget *m_overlay;
    QStringList m_sourceUrls;
    QString m_title;
    QString m_referer;
    QByteArray m_cookieHeader;
    int m_sourceIndex;
    int m_quality;
    QString m_format;
    QVector<PlaybackQualityCompat> m_qualities;
    int m_lastReportedError;
    int m_pollTimerId;
    int m_startTimerId;
    int m_retryTimerId;
    int m_orientationTimerId;
    int m_screenAwakeTimerId;
    int m_pendingSourceIndex;
    int m_sourcePass;
    int m_downloadSourceIndex;
    qint64 m_downloadBytes;
    QString m_downloadPath;
    QTime m_sourceClock;
    int m_automaticFallbackTarget;
    bool m_closing;
    bool m_isLive;
    bool m_streamPlaybackMode;
    bool m_localFallbackAttempted;
    bool m_localPlaybackActive;
    bool m_landscapeRequested;
    bool m_landscapeApplied;
    NativeOrientationStage m_orientationStage;
    bool m_landscapeWorkAreaSeen;
    bool m_portraitWorkAreaSeen;
    bool m_restoreClosesSession;
    bool m_glesYuvActive;
    bool m_softVideoActive;
    bool m_sessionActive;
    unsigned int m_sessionSerial;
#ifdef WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    DevVideoDirectProbe *m_directProbe;
    bool m_directProbeActive;
    bool m_directProbePhaseAPassed;
    bool m_directProbePhaseBPassed;
#endif
};

} // namespace wiliwili

#endif
