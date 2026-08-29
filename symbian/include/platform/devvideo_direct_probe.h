#ifndef WILIWILI_DEVVIDEO_DIRECT_PROBE_H
#define WILIWILI_DEVVIDEO_DIRECT_PROBE_H

#include <QtCore/QObject>

#if defined(Q_OS_SYMBIAN) && \
    defined(WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE)

#include <QtCore/QString>
#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtGui/QWidget>

#include <e32base.h>
#include <w32std.h>
#include <mmf/devvideo/devvideoplay.h>

namespace wiliwili {

// Diagnosis-only, static-picture proof of the external-YUV -> DevVideo
// post-processor -> DSA architecture. It deliberately has no dependency on
// FFmpeg, MMF audio, Range fetching or the production software-frame queue.
class DevVideoDirectProbe : public QObject,
                            public MDirectScreenAccess,
                            public MMMFDevVideoPlayObserver
{
    Q_OBJECT

public:
    DevVideoDirectProbe(
        QWidget *videoSurface,
        QWidget *argbOverlay,
        QObject *parent = 0);
    virtual ~DevVideoDirectProbe();

public slots:
    void start();
    void stop();

signals:
    void finished(bool phaseAPassed, bool phaseBPassed);

private slots:
    void onTick();
    void handleCandidateFailure();

private:
    enum ProbeStage {
        ProbeIdle = 0,
        PhaseAOverlayHidden,
        PhaseAOverlayVisible,
        PhaseBInitializing,
        PhaseBRunning,
        ProbeFinished
    };

    struct PictureSlot {
        PictureSlot();
        ~PictureSlot();
        void allocateL();
        void reset();

        TUint8 *memory;
        TPtr8 descriptor;
        TVideoPicture picture;
        bool busy;
        QTime submitClock;
        qint64 ptsMicroseconds;
    };

    DevVideoDirectProbe(const DevVideoDirectProbe &);
    DevVideoDirectProbe &operator=(const DevVideoDirectProbe &);

    void createDsaL(bool regionTrackingOnly);
    RWindow *resolveSurfaceWindow() const;
    TRect videoRect() const;
    qint64 regionArea(const TRegion *region) const;
    void logRegion(bool overlayVisible);
    void evaluatePhaseA();
    void beginPhaseB();
    void tryNextPostProcessor();
    TUncompressedVideoFormat inputFormat(int attempt) const;
    void logOutputFormats(THwDeviceId deviceId);
    void startInitializedPipelineL();
    void submitPatternFrame();
    void fillPattern(PictureSlot *slot, int frameIndex);
    int outstandingPictures() const;
    void sampleDevVideoTelemetry();
    void logTelemetry();
    void scheduleCandidateFailure(TInt error, const char *stage);
    void finishProbe(bool phaseBPassed, const char *reason);
    void cleanupDevVideo();
    void cleanupDsa();

    // MDirectScreenAccess. AbortNow deliberately performs no Window Server
    // calls; DevVideo's documented abort hook is the only native API used.
    virtual void AbortNow(
        RDirectScreenAccess::TTerminationReasons reason);
    virtual void Restart(
        RDirectScreenAccess::TTerminationReasons reason);

    // MMMFDevVideoPlayObserver.
    virtual void MdvpoNewBuffers();
    virtual void MdvpoReturnPicture(TVideoPicture *picture);
    virtual void MdvpoSupplementalInformation(
        const TDesC8 &data,
        const TTimeIntervalMicroSeconds &timestamp,
        const TPictureId &pictureId);
    virtual void MdvpoPictureLoss();
    virtual void MdvpoPictureLoss(const TArray<TPictureId> &pictures);
    virtual void MdvpoSliceLoss(
        TUint firstMacroblock,
        TUint macroblocks,
        const TPictureId &picture);
    virtual void MdvpoReferencePictureSelection(
        const TDesC8 &selectionData);
    virtual void MdvpoTimedSnapshotComplete(
        TInt error,
        TPictureData *pictureData,
        const TTimeIntervalMicroSeconds &timestamp,
        const TPictureId &pictureId);
    virtual void MdvpoNewPictures();
    virtual void MdvpoFatalError(TInt error);
    virtual void MdvpoInitComplete(TInt error);
    virtual void MdvpoStreamEnd();

    QWidget *m_surface;
    QWidget *m_overlay;
    QTimer m_timer;
    QTime m_stageClock;
    QTime m_statsClock;
    QTime m_lastReturnClock;
    QTime m_lastProgressClock;
    ProbeStage m_stage;
    CDirectScreenAccess *m_dsa;
    CMMFDevVideoPlay *m_devVideo;
    THwDeviceId m_postProcessorId;
    PictureSlot m_slots[3];
    int m_candidateIndex;
    int m_nextFrameIndex;
    int m_phaseBAbortBase;
    int m_phaseBRestartBase;
    TInt m_pendingFailureError;
    QString m_pendingFailureStage;
    bool m_started;
    bool m_overlayExpectedVisible;
    bool m_visibleRegionLogged;
    bool m_aborted;
    bool m_phaseAPassed;
    bool m_phaseBPassed;
    bool m_devInitializing;
    bool m_devInitialized;
    bool m_pipelineRunning;
    bool m_candidateFailurePending;
    bool m_fatalSeen;
    qint64 m_hiddenRegionArea;
    qint64 m_visibleRegionArea;
    qint64 m_directRegionArea;
    quint64 m_directSubmitted;
    quint64 m_directReturned;
    quint64 m_directBusyDrops;
    qint64 m_directWriteMilliseconds;
    qint64 m_directReturnLatencyMilliseconds;
    qint64 m_directReturnLatencyLastMilliseconds;
    qint64 m_directReturnLatencyMaximumMilliseconds;
    quint64 m_directAbortCount;
    quint64 m_directRestartCount;
    qint64 m_directLastPts;
    quint64 m_directDevDisplayed;
    quint64 m_directDevSkipped;
    quint64 m_directDevDecoded;
    quint64 m_directDevTotal;
    qint64 m_directDevPlaybackPts;
};

} // namespace wiliwili

#endif

#endif
