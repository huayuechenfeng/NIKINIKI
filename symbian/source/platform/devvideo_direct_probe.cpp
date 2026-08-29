#include "platform/devvideo_direct_probe.h"

#if defined(Q_OS_SYMBIAN) && \
    defined(WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE)

#include <QtCore/QDebug>
#include <QtCore/QPoint>

#include <coecntrl.h>
#include <eikenv.h>

namespace wiliwili {

namespace {

const int KProbeWidth = 640;
const int KProbeHeight = 360;
const int KProbeLumaBytes = KProbeWidth * KProbeHeight;
const int KProbeChromaBytes = (KProbeWidth / 2) * (KProbeHeight / 2);
const int KProbePictureBytes = KProbeLumaBytes + 2 * KProbeChromaBytes;
const int KPhaseAHiddenMilliseconds = 1500;
const int KPhaseATotalMilliseconds = 8500;
const int KPhaseBMilliseconds = 10000;
const int KDeadlockMilliseconds = 1600;
const int KMinimumUsefulRegionPercent = 80;
const int KAbortStormLimit = 12;
const int KFrameIntervalMilliseconds = 40;

const TUid KPostProcessors[] = {
    { 0x2003162A },
    { 0x10273417 }
};

static QString rectText(const TRect &rect)
{
    return QString::fromLatin1("%1,%2 %3x%4")
        .arg(rect.iTl.iX)
        .arg(rect.iTl.iY)
        .arg(rect.Width())
        .arg(rect.Height());
}

static void setYuvSample(
    TUint8 *yPlane,
    TUint8 *uPlane,
    TUint8 *vPlane,
    int x,
    int y,
    TUint8 luma,
    TUint8 u,
    TUint8 v)
{
    yPlane[y * KProbeWidth + x] = luma;
    if ((x & 1) == 0 && (y & 1) == 0) {
        const int chroma = (y / 2) * (KProbeWidth / 2) + x / 2;
        uPlane[chroma] = u;
        vPlane[chroma] = v;
    }
}

} // namespace

DevVideoDirectProbe::PictureSlot::PictureSlot()
    : memory(0), descriptor(0, 0, 0), busy(false),
      ptsMicroseconds(-1)
{
    Mem::FillZ(&picture, sizeof(picture));
}

DevVideoDirectProbe::PictureSlot::~PictureSlot()
{
    delete [] memory;
    memory = 0;
}

void DevVideoDirectProbe::PictureSlot::allocateL()
{
    if (!memory)
        memory = new (ELeave) TUint8[KProbePictureBytes];
    descriptor.Set(memory, KProbePictureBytes, KProbePictureBytes);
    descriptor.SetLength(KProbePictureBytes);
    reset();
}

void DevVideoDirectProbe::PictureSlot::reset()
{
    busy = false;
    ptsMicroseconds = -1;
    Mem::FillZ(&picture, sizeof(picture));
    if (memory) {
        picture.iData.iDataFormat = EYuvRawData;
        picture.iData.iDataSize = TSize(KProbeWidth, KProbeHeight);
        picture.iData.iRawData = &descriptor;
        picture.iOptions = TVideoPicture::ETimestamp;
    }
}

DevVideoDirectProbe::DevVideoDirectProbe(
    QWidget *videoSurface,
    QWidget *argbOverlay,
    QObject *parent)
    : QObject(parent),
      m_surface(videoSurface), m_overlay(argbOverlay),
      m_stage(ProbeIdle), m_dsa(0), m_devVideo(0),
      m_postProcessorId(0), m_candidateIndex(-1),
      m_nextFrameIndex(0), m_phaseBAbortBase(0),
      m_phaseBRestartBase(0), m_pendingFailureError(KErrNone),
      m_started(false), m_overlayExpectedVisible(false),
      m_visibleRegionLogged(false),
      m_aborted(false), m_phaseAPassed(false),
      m_phaseBPassed(false), m_devInitializing(false),
      m_devInitialized(false), m_pipelineRunning(false),
      m_candidateFailurePending(false), m_fatalSeen(false),
      m_hiddenRegionArea(0), m_visibleRegionArea(0),
      m_directRegionArea(0), m_directSubmitted(0),
      m_directReturned(0), m_directBusyDrops(0),
      m_directWriteMilliseconds(0),
      m_directReturnLatencyMilliseconds(0),
      m_directReturnLatencyLastMilliseconds(0),
      m_directReturnLatencyMaximumMilliseconds(0),
      m_directAbortCount(0), m_directRestartCount(0),
      m_directLastPts(-1), m_directDevDisplayed(0),
      m_directDevSkipped(0), m_directDevDecoded(0),
      m_directDevTotal(0), m_directDevPlaybackPts(0)
{
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTick()));
}

DevVideoDirectProbe::~DevVideoDirectProbe()
{
    stop();
}

void DevVideoDirectProbe::start()
{
    if (m_started || !m_surface || !m_overlay)
        return;

    m_started = true;
    m_stage = PhaseAOverlayHidden;
    m_overlayExpectedVisible = false;
    m_visibleRegionLogged = false;
    m_overlay->hide();
    m_surface->show();
    m_surface->raise();
    m_surface->winId();

    TInt error = KErrNone;
    TRAP(error, createDsaL(true));
    qDebug() << "WW:DIRECT_PHASE_A_BEGIN"
             << "version" << "devvideodirectprobe1"
             << "dsaError" << error
             << "surface" << m_surface->geometry();
    if (error != KErrNone || !m_dsa) {
        m_phaseAPassed = false;
        finishProbe(false, "phase-a-dsa-create");
        return;
    }

    m_stageClock.start();
    m_statsClock.start();
    logRegion(false);
    m_hiddenRegionArea = m_directRegionArea;
    m_timer.start(100);
}

void DevVideoDirectProbe::stop()
{
    m_timer.stop();
    m_pipelineRunning = false;
    cleanupDevVideo();
    cleanupDsa();
    m_started = false;
}

void DevVideoDirectProbe::createDsaL(bool regionTrackingOnly)
{
    CEikonEnv *environment = CEikonEnv::Static();
    RWindow *window = resolveSurfaceWindow();
    if (!environment || !environment->ScreenDevice() || !window)
        User::Leave(KErrNotReady);
    m_dsa = CDirectScreenAccess::NewL(
        environment->WsSession(),
        *environment->ScreenDevice(),
        *window,
        *this,
        regionTrackingOnly ? ETrue : EFalse);
    m_dsa->StartL();
    // Region-tracking-only mode is allowed to omit the drawing GC/screen
    // objects. Phase A needs only the DrawingRegion; Phase B deliberately
    // recreates a full DSA object before handing its screen device to
    // DevVideo.
    if (!m_dsa->DrawingRegion())
        User::Leave(KErrNotReady);
}

RWindow *DevVideoDirectProbe::resolveSurfaceWindow() const
{
    if (!m_surface)
        return 0;
    m_surface->setAttribute(Qt::WA_NativeWindow, true);
    CCoeControl *control = m_surface->winId();
    if (!control || !control->OwnsWindow() ||
        !control->DrawableWindow()) {
        return 0;
    }
    return static_cast<RWindow *>(control->DrawableWindow());
}

TRect DevVideoDirectProbe::videoRect() const
{
    const QPoint topLeft = m_surface
        ? m_surface->mapToGlobal(QPoint(0, 0)) : QPoint(0, 0);
    const int width = m_surface ? m_surface->width() : KProbeWidth;
    const int height = m_surface ? m_surface->height() : KProbeHeight;
    return TRect(
        TPoint(topLeft.x(), topLeft.y()),
        TSize(width, height));
}

qint64 DevVideoDirectProbe::regionArea(const TRegion *region) const
{
    if (!region || region->CheckError() || region->IsEmpty())
        return 0;
    qint64 area = 0;
    int index;
    for (index = 0; index < region->Count(); ++index) {
        const TRect &rect = (*region)[index];
        if (rect.Width() > 0 && rect.Height() > 0)
            area += static_cast<qint64>(rect.Width()) * rect.Height();
    }
    return area;
}

void DevVideoDirectProbe::logRegion(bool overlayVisible)
{
    const TRegion *region = (!m_aborted && m_dsa)
        ? m_dsa->DrawingRegion() : 0;
    const int count = region ? region->Count() : 0;
    const TRect bounds = region && !region->IsEmpty()
        ? region->BoundingRect() : TRect(0, 0, 0, 0);
    m_directRegionArea = regionArea(region);
    if (overlayVisible)
        m_visibleRegionArea = m_directRegionArea;
    qDebug() << "WW:DIRECT_REGION"
             << (overlayVisible ? "overlay=1" : "overlay=0")
             << "rects" << count
             << "bounds" << rectText(bounds)
             << "area" << m_directRegionArea
             << "aborted" << m_aborted;
}

void DevVideoDirectProbe::onTick()
{
    if (!m_started)
        return;

    if (m_stage == PhaseAOverlayHidden) {
        if (m_stageClock.elapsed() >= KPhaseAHiddenMilliseconds) {
            m_overlayExpectedVisible = true;
            m_overlay->show();
            m_overlay->raise();
            m_overlay->update();
            m_stage = PhaseAOverlayVisible;
            qDebug() << "WW:DIRECT_OVERLAY_SHOW"
                     << m_overlay->geometry()
                     << static_cast<void *>(m_overlay->winId());
        }
        return;
    }

    if (m_stage == PhaseAOverlayVisible) {
        // Exercise the same operations used by controls, touch feedback and
        // moving danmaku instead of a single hide/show sample.
        m_overlay->update();
        if ((m_stageClock.elapsed() / 500) % 2 == 0)
            m_overlay->raise();
        if (m_stageClock.elapsed() >= KPhaseAHiddenMilliseconds + 500 &&
            !m_visibleRegionLogged) {
            logRegion(true);
            m_visibleRegionLogged = true;
        }
        if (m_stageClock.elapsed() >= KPhaseATotalMilliseconds)
            evaluatePhaseA();
        return;
    }

    if (m_stage == PhaseBRunning) {
        submitPatternFrame();
        if (!m_statsClock.isValid() || m_statsClock.elapsed() >= 1000) {
            sampleDevVideoTelemetry();
            logTelemetry();
            m_statsClock.restart();
        }

        const int outstanding = outstandingPictures();
        const bool returnsStopped = outstanding == 3 &&
            m_directSubmitted >= 2 && m_lastReturnClock.isValid() &&
            m_lastReturnClock.elapsed() >= KDeadlockMilliseconds;
        const bool progressStopped = m_lastProgressClock.isValid() &&
            m_lastProgressClock.elapsed() >= KDeadlockMilliseconds;
        if (returnsStopped && progressStopped) {
            qDebug() << "WW:DIRECT_DEADLOCK"
                     << "submitted" << m_directSubmitted
                     << "returned" << m_directReturned
                     << "outstanding" << outstanding
                     << "displayed" << m_directDevDisplayed
                     << "playbackPts" << m_directDevPlaybackPts;
            scheduleCandidateFailure(KErrTimedOut, "pipeline-deadlock");
            return;
        }

        if (m_aborted && m_lastProgressClock.isValid() &&
            m_lastProgressClock.elapsed() >= 2000) {
            finishProbe(false, "phase-b-dsa-aborted");
            return;
        }
        if (static_cast<int>(m_directAbortCount) - m_phaseBAbortBase >
                KAbortStormLimit ||
            static_cast<int>(m_directRestartCount) - m_phaseBRestartBase >
                KAbortStormLimit) {
            finishProbe(false, "phase-b-abort-restart-storm");
            return;
        }

        if (m_stageClock.elapsed() >= KPhaseBMilliseconds) {
            sampleDevVideoTelemetry();
            logTelemetry();
            const bool flowPassed = m_directSubmitted >= 25 &&
                m_directReturned > 0 &&
                !m_aborted && !m_fatalSeen;
            if (!flowPassed) {
                scheduleCandidateFailure(
                    KErrTimedOut, "phase-b-flow-gate");
                return;
            }
            // Do not require an empty submission pool at the arbitrary
            // ten-second boundary. A renderer may legally retain the current
            // display picture and may also have accepted recently queued
            // pictures. A real stall has already been handled above by the
            // joint three-busy/no-return/no-device-progress hard gate.
            m_phaseBPassed = true;
            qDebug() << "WW:DIRECT_PHASE_B" << "YES"
                     << "uid" << QString::number(
                            KPostProcessors[m_candidateIndex].iUid, 16)
                     << "manual" << "visual-touch-overlay-required";
            finishProbe(true, "completed");
        }
    }
}

void DevVideoDirectProbe::evaluatePhaseA()
{
    logRegion(true);
    const qint64 videoArea = static_cast<qint64>(
        qMax(1, m_surface->width())) * qMax(1, m_surface->height());
    const qint64 minimumArea =
        videoArea * KMinimumUsefulRegionPercent / 100;
    const bool hiddenPassed = m_hiddenRegionArea >= minimumArea;
    const bool visiblePassed = m_visibleRegionArea >= minimumArea;
    const bool storm = m_directAbortCount > KAbortStormLimit ||
        m_directRestartCount > KAbortStormLimit;
    m_phaseAPassed = hiddenPassed && visiblePassed &&
        !storm && !m_aborted;
    qDebug() << "WW:DIRECT_PHASE_A"
             << (m_phaseAPassed ? "YES" : "NO")
             << "hiddenArea" << m_hiddenRegionArea
             << "visibleArea" << m_visibleRegionArea
             << "minimumArea" << minimumArea
             << "aborts" << m_directAbortCount
             << "restarts" << m_directRestartCount
             << "aborted" << m_aborted;
    if (!m_phaseAPassed) {
        // Hard gate: no DevVideo object has been created at this point.
        finishProbe(false, "phase-a-overlay-dsa-conflict");
        return;
    }
    beginPhaseB();
}

void DevVideoDirectProbe::beginPhaseB()
{
    m_stage = PhaseBInitializing;
    m_candidateIndex = -1;
    m_phaseBAbortBase = static_cast<int>(m_directAbortCount);
    m_phaseBRestartBase = static_cast<int>(m_directRestartCount);
    qDebug() << "WW:DIRECT_PHASE_B_BEGIN"
             << "size" << KProbeWidth << KProbeHeight
             << "bytes" << KProbePictureBytes
             << "slots" << 3;
    // Phase A was intentionally region-tracking-only. DevVideo's DSA API
    // additionally requires CFbsScreenDevice, so recreate the same DSA
    // contract in full mode while the validated overlay remains present.
    cleanupDsa();
    TInt dsaError = KErrNone;
    TRAP(dsaError, createDsaL(false));
    qDebug() << "WW:DIRECT_DSA_MODE"
             << "phase" << "B"
             << "trackingOnly" << 0
             << "error" << dsaError
             << "screenDevice"
             << static_cast<void *>(
                    m_dsa ? m_dsa->ScreenDevice() : 0);
    if (dsaError != KErrNone || !m_dsa ||
        !m_dsa->ScreenDevice() || !m_dsa->DrawingRegion()) {
        finishProbe(false, "phase-b-full-dsa-create");
        return;
    }
    logRegion(true);
    tryNextPostProcessor();
}

TUncompressedVideoFormat DevVideoDirectProbe::inputFormat(
    int attempt) const
{
    TUncompressedVideoFormat format;
    Mem::FillZ(&format, sizeof(format));
    format.iDataFormat = EYuvRawData;
    format.iYuvFormat.iDataLayout = EYuvDataPlanar;
    format.iYuvFormat.iYuv2RgbMatrix = 0;
    format.iYuvFormat.iRgb2YuvMatrix = 0;
    format.iYuvFormat.iAspectRatioNum = 1;
    format.iYuvFormat.iAspectRatioDenom = 1;
    if (attempt == 0) {
        format.iYuvFormat.iCoefficients = EYuvBt709Range0;
        format.iYuvFormat.iPattern = EYuv420Chroma1;
    } else if (attempt == 1) {
        format.iYuvFormat.iCoefficients = EYuvBt601Range0;
        format.iYuvFormat.iPattern = EYuv420Chroma1;
    } else {
        format.iYuvFormat.iCoefficients = EYuvBt709Range1;
        format.iYuvFormat.iPattern = EYuv420Chroma3;
    }
    return format;
}

void DevVideoDirectProbe::tryNextPostProcessor()
{
    cleanupDevVideo();
    m_candidateFailurePending = false;
    m_pendingFailureError = KErrNone;
    m_pendingFailureStage.clear();
    ++m_candidateIndex;
    if (m_candidateIndex >= 2) {
        qDebug() << "WW:DIRECT_PHASE_B" << "NO"
                 << "reason" << "all-postprocessors-failed";
        finishProbe(false, "all-postprocessors-failed");
        return;
    }

    const TUid uid = KPostProcessors[m_candidateIndex];
    qDebug() << "WW:DIRECT_PP"
             << "uid" << QString::number(uid.iUid, 16)
             << "candidate" << (m_candidateIndex + 1);

    TInt error = KErrNone;
    TRAP(error, m_devVideo = CMMFDevVideoPlay::NewL(*this));
    if (error != KErrNone || !m_devVideo) {
        scheduleCandidateFailure(
            error == KErrNone ? KErrNoMemory : error,
            "new");
        return;
    }

    TRAP(error,
        m_postProcessorId = m_devVideo->SelectPostProcessorL(uid));
    if (error != KErrNone) {
        scheduleCandidateFailure(error, "select-postprocessor");
        return;
    }

    bool inputAccepted = false;
    int attempt;
    for (attempt = 0; attempt < 3; ++attempt) {
        const TUncompressedVideoFormat format = inputFormat(attempt);
        qDebug() << "WW:DIRECT_INPUT_TRY"
                 << "uid" << QString::number(uid.iUid, 16)
                 << "attempt" << attempt
                 << "coefficients" << static_cast<TUint32>(
                        format.iYuvFormat.iCoefficients)
                 << "pattern" << static_cast<TUint32>(
                        format.iYuvFormat.iPattern)
                 << "layout" << static_cast<TUint32>(
                        format.iYuvFormat.iDataLayout)
                 << "size" << KProbeWidth << KProbeHeight;
        TRAP(error,
            m_devVideo->SetInputFormatL(m_postProcessorId, format));
        if (error == KErrNone) {
            inputAccepted = true;
            qDebug() << "WW:DIRECT_INPUT_OK"
                     << "uid" << QString::number(uid.iUid, 16)
                     << "attempt" << attempt;
            break;
        }
        qDebug() << "WW:DIRECT_INPUT_FAIL"
                 << "uid" << QString::number(uid.iUid, 16)
                 << "attempt" << attempt
                 << "error" << error;
        if (error != KErrNotSupported)
            break;
    }
    if (!inputAccepted) {
        scheduleCandidateFailure(error, "input-format");
        return;
    }

    logOutputFormats(m_postProcessorId);
    TRAP(error, m_devVideo->SetVideoDestScreenL(ETrue));
    qDebug() << "WW:DIRECT_DEST_SCREEN"
             << "uid" << QString::number(uid.iUid, 16)
             << "error" << error;
    if (error != KErrNone) {
        scheduleCandidateFailure(error, "dest-screen");
        return;
    }

    m_devInitializing = true;
    m_fatalSeen = false;
    m_devVideo->Initialize();
    qDebug() << "WW:DIRECT_INIT_SENT"
             << "uid" << QString::number(uid.iUid, 16);
}

void DevVideoDirectProbe::logOutputFormats(THwDeviceId deviceId)
{
    if (!m_devVideo)
        return;
    RArray<TUncompressedVideoFormat> formats;
    TInt error = KErrNone;
    TRAP(error, m_devVideo->GetOutputFormatListL(deviceId, formats));
    qDebug() << "WW:DIRECT_OUTPUT_LIST"
             << "error" << error << "count" << formats.Count();
    if (error == KErrNone) {
        int index;
        for (index = 0; index < formats.Count(); ++index) {
            const TUncompressedVideoFormat &format = formats[index];
            if (format.iDataFormat == EYuvRawData) {
                qDebug() << "WW:DIRECT_OUTPUT"
                         << index << "YUV"
                         << static_cast<TUint32>(
                                format.iYuvFormat.iCoefficients)
                         << static_cast<TUint32>(
                                format.iYuvFormat.iPattern)
                         << static_cast<TUint32>(
                                format.iYuvFormat.iDataLayout);
            } else if (format.iDataFormat == ERgbRawData ||
                       format.iDataFormat == ERgbFbsBitmap) {
                qDebug() << "WW:DIRECT_OUTPUT"
                         << index << "RGB"
                         << static_cast<TUint32>(format.iRgbFormat);
            } else {
                qDebug() << "WW:DIRECT_OUTPUT"
                         << index << "DATA"
                         << static_cast<TUint32>(format.iDataFormat);
            }
        }
    }
    formats.Close();
}

void DevVideoDirectProbe::startInitializedPipelineL()
{
    if (!m_devVideo || !m_dsa || !m_dsa->ScreenDevice() ||
        !m_dsa->DrawingRegion() || m_aborted)
        User::Leave(KErrNotReady);

    int index;
    for (index = 0; index < 3; ++index)
        m_slots[index].allocateL();

    // Each post-processor candidate has an independent flow gate. Do not let
    // a few callbacks from a rejected first device make the second device
    // appear to have submitted, returned or displayed those pictures.
    m_directSubmitted = 0;
    m_directReturned = 0;
    m_directBusyDrops = 0;
    m_directWriteMilliseconds = 0;
    m_directReturnLatencyMilliseconds = 0;
    m_directReturnLatencyLastMilliseconds = 0;
    m_directReturnLatencyMaximumMilliseconds = 0;
    m_directLastPts = -1;
    m_directDevDisplayed = 0;
    m_directDevSkipped = 0;
    m_directDevDecoded = 0;
    m_directDevTotal = 0;
    m_directDevPlaybackPts = 0;

    m_devVideo->SetPauseOnClipFail(ETrue);
    m_devVideo->StartDirectScreenAccessL(
        videoRect(),
        *m_dsa->ScreenDevice(),
        *m_dsa->DrawingRegion());
    m_devVideo->Start();

    m_pipelineRunning = true;
    m_stage = PhaseBRunning;
    m_nextFrameIndex = 0;
    m_stageClock.restart();
    m_statsClock.restart();
    m_lastReturnClock.start();
    m_lastProgressClock.start();
    m_timer.start(KFrameIntervalMilliseconds);
    qDebug() << "WW:DIRECT_PIPELINE_STARTED"
             << "uid" << QString::number(
                    KPostProcessors[m_candidateIndex].iUid, 16)
             << "rect" << rectText(videoRect())
             << "regionArea" << m_directRegionArea;
}

void DevVideoDirectProbe::submitPatternFrame()
{
    if (!m_pipelineRunning || !m_devVideo || m_aborted ||
        m_candidateFailurePending)
        return;
    PictureSlot *slot = 0;
    int index;
    for (index = 0; index < 3; ++index) {
        if (!m_slots[index].busy) {
            slot = &m_slots[index];
            break;
        }
    }
    if (!slot) {
        ++m_directBusyDrops;
        return;
    }

    fillPattern(slot, m_nextFrameIndex);
    const qint64 pts = static_cast<qint64>(m_nextFrameIndex) *
        KFrameIntervalMilliseconds * 1000;
    slot->picture.iTimestamp = TTimeIntervalMicroSeconds(pts);
    slot->ptsMicroseconds = pts;
    slot->busy = true;
    slot->submitClock.start();
    ++m_directSubmitted;
    m_directLastPts = pts;

    QTime writeClock;
    writeClock.start();
    TInt error = KErrNone;
    TRAP(error, m_devVideo->WritePictureL(&slot->picture));
    m_directWriteMilliseconds += writeClock.elapsed();
    if (error != KErrNone) {
        slot->busy = false;
        if (m_directSubmitted > 0)
            --m_directSubmitted;
        qDebug() << "WW:DIRECT_WRITE_FAIL"
                 << "error" << error
                 << "frame" << m_nextFrameIndex;
        scheduleCandidateFailure(error, "write-picture");
        return;
    }
    ++m_nextFrameIndex;
}

void DevVideoDirectProbe::fillPattern(
    PictureSlot *slot,
    int frameIndex)
{
    if (!slot || !slot->memory)
        return;
    TUint8 *yPlane = slot->memory;
    TUint8 *uPlane = yPlane + KProbeLumaBytes;
    TUint8 *vPlane = uPlane + KProbeChromaBytes;

    if (frameIndex < 50) {
        Mem::Fill(yPlane, KProbeLumaBytes, 16);
        Mem::Fill(uPlane, KProbeChromaBytes, 128);
        Mem::Fill(vPlane, KProbeChromaBytes, 128);
        return;
    }
    if (frameIndex < 100) {
        Mem::Fill(yPlane, KProbeLumaBytes, 235);
        Mem::Fill(uPlane, KProbeChromaBytes, 128);
        Mem::Fill(vPlane, KProbeChromaBytes, 128);
        return;
    }

    static const TUint8 yValues[8] = {
        235, 210, 170, 145, 106, 81, 41, 16
    };
    static const TUint8 uValues[8] = {
        128, 16, 166, 54, 202, 90, 240, 128
    };
    static const TUint8 vValues[8] = {
        128, 146, 16, 34, 222, 240, 110, 128
    };
    int y;
    for (y = 0; y < KProbeHeight; ++y) {
        int x;
        for (x = 0; x < KProbeWidth; ++x) {
            const int bar = qMin(7, x * 8 / KProbeWidth);
            setYuvSample(
                yPlane, uPlane, vPlane, x, y,
                yValues[bar], uValues[bar], vValues[bar]);
        }
    }

    const int scanY = (frameIndex * 3) % KProbeHeight;
    int row;
    for (row = qMax(0, scanY - 2);
         row <= qMin(KProbeHeight - 1, scanY + 2); ++row) {
        int x;
        for (x = 0; x < KProbeWidth; ++x)
            setYuvSample(yPlane, uPlane, vPlane, x, row,
                         235, 128, 128);
    }
}

int DevVideoDirectProbe::outstandingPictures() const
{
    int outstanding = 0;
    int index;
    for (index = 0; index < 3; ++index) {
        if (m_slots[index].busy)
            ++outstanding;
    }
    return outstanding;
}

void DevVideoDirectProbe::sampleDevVideoTelemetry()
{
    if (!m_devVideo || !m_devInitialized || m_fatalSeen)
        return;
    CMMFDevVideoPlay::TPictureCounters counters;
    m_devVideo->GetPictureCounters(counters);
    m_directDevDisplayed += counters.iPicturesDisplayed;
    m_directDevSkipped += counters.iPicturesSkipped;
    m_directDevDecoded += counters.iPicturesDecoded;
    m_directDevTotal += counters.iTotalPictures;
    const qint64 previousPlayback = m_directDevPlaybackPts;
    m_directDevPlaybackPts = static_cast<qint64>(
        m_devVideo->PlaybackPosition().Int64());
    if (counters.iPicturesDisplayed > 0 ||
        m_directDevPlaybackPts > previousPlayback) {
        m_lastProgressClock.restart();
    }
}

void DevVideoDirectProbe::logTelemetry()
{
    const QString line = QString::fromLatin1(
        "directSubmitted=%1 directReturned=%2 directOutstanding=%3 "
        "directBusyDrops=%4 directWriteMs=%5 "
        "directReturnLatencyMs=%6 directReturnLatencyLastMs=%7 "
        "directReturnLatencyMaxMs=%8 directAbortCount=%9 "
        "directRestartCount=%10 directRegionArea=%11 directLastPts=%12 "
        "directDevDisplayed=%13 directDevSkipped=%14 "
        "directDevDecoded=%15 directDevTotal=%16 "
        "directDevPlaybackPts=%17")
        .arg(static_cast<qulonglong>(m_directSubmitted))
        .arg(static_cast<qulonglong>(m_directReturned))
        .arg(outstandingPictures())
        .arg(static_cast<qulonglong>(m_directBusyDrops))
        .arg(m_directWriteMilliseconds)
        .arg(m_directReturnLatencyMilliseconds)
        .arg(m_directReturnLatencyLastMilliseconds)
        .arg(m_directReturnLatencyMaximumMilliseconds)
        .arg(static_cast<qulonglong>(m_directAbortCount))
        .arg(static_cast<qulonglong>(m_directRestartCount))
        .arg(m_directRegionArea)
        .arg(m_directLastPts)
        .arg(static_cast<qulonglong>(m_directDevDisplayed))
        .arg(static_cast<qulonglong>(m_directDevSkipped))
        .arg(static_cast<qulonglong>(m_directDevDecoded))
        .arg(static_cast<qulonglong>(m_directDevTotal))
        .arg(m_directDevPlaybackPts);
    qDebug() << "WW:DIRECT_STATS" << qPrintable(line);
}

void DevVideoDirectProbe::scheduleCandidateFailure(
    TInt error,
    const char *stage)
{
    if (m_candidateFailurePending || m_stage == ProbeFinished)
        return;
    m_candidateFailurePending = true;
    m_pipelineRunning = false;
    m_pendingFailureError = error;
    m_pendingFailureStage = QString::fromLatin1(stage ? stage : "unknown");
    qDebug() << "WW:DIRECT_CANDIDATE_FAIL"
             << "uid" << (m_candidateIndex >= 0
                    ? QString::number(
                        KPostProcessors[m_candidateIndex].iUid, 16)
                    : QString::fromLatin1("none"))
             << "stage" << m_pendingFailureStage
             << "error" << error;
    QTimer::singleShot(0, this, SLOT(handleCandidateFailure()));
}

void DevVideoDirectProbe::handleCandidateFailure()
{
    if (!m_candidateFailurePending || m_stage == ProbeFinished)
        return;
    qDebug() << "WW:DIRECT_PP_REJECTED"
             << "uid" << QString::number(
                    KPostProcessors[m_candidateIndex].iUid, 16)
             << "stage" << m_pendingFailureStage
             << "error" << m_pendingFailureError;
    tryNextPostProcessor();
}

void DevVideoDirectProbe::finishProbe(
    bool phaseBPassed,
    const char *reason)
{
    if (m_stage == ProbeFinished)
        return;
    m_phaseBPassed = phaseBPassed;
    m_stage = ProbeFinished;
    logTelemetry();
    qDebug() << "WW:DIRECT_RESULT"
             << "phaseA" << (m_phaseAPassed ? "YES" : "NO")
             << "phaseB" << (m_phaseBPassed ? "YES" : "NO")
             << "reason" << (reason ? reason : "unknown")
             << "manual" << "orientation-color-scanline-overlay-touch";
    m_timer.stop();
    m_pipelineRunning = false;
    cleanupDevVideo();
    cleanupDsa();
    m_started = false;
    emit finished(m_phaseAPassed, m_phaseBPassed);
}

void DevVideoDirectProbe::cleanupDevVideo()
{
    m_pipelineRunning = false;
    m_devInitializing = false;
    if (m_devVideo) {
        if (m_devInitialized && !m_fatalSeen) {
            m_devVideo->Stop();
            m_devVideo->AbortDirectScreenAccess();
        }
        delete m_devVideo;
        m_devVideo = 0;
    }
    m_devInitialized = false;
    m_postProcessorId = 0;
    int index;
    for (index = 0; index < 3; ++index)
        m_slots[index].reset();
}

void DevVideoDirectProbe::cleanupDsa()
{
    delete m_dsa;
    m_dsa = 0;
    m_aborted = false;
    m_directRegionArea = 0;
}

void DevVideoDirectProbe::AbortNow(
    RDirectScreenAccess::TTerminationReasons reason)
{
    ++m_directAbortCount;
    m_aborted = true;
    m_pipelineRunning = false;
    // Documented as safe/required from AbortNow(). No Window Server API is
    // touched in this callback.
    if (m_devVideo && m_devInitialized)
        m_devVideo->AbortDirectScreenAccess();
    qDebug() << "WW:DIRECT_ABORT"
             << "reason" << static_cast<int>(reason)
             << "count" << m_directAbortCount;
}

void DevVideoDirectProbe::Restart(
    RDirectScreenAccess::TTerminationReasons reason)
{
    ++m_directRestartCount;
    TInt dsaError = KErrNotReady;
    if (m_dsa)
        TRAP(dsaError, m_dsa->StartL());
    if (dsaError == KErrNone) {
        m_aborted = false;
        logRegion(m_overlayExpectedVisible);
    }

    TInt devVideoError = KErrNone;
    if (dsaError == KErrNone && m_devVideo && m_devInitialized) {
        if (!m_dsa->ScreenDevice() || !m_dsa->DrawingRegion()) {
            devVideoError = KErrNotReady;
        } else {
            TRAP(devVideoError,
                m_devVideo->StartDirectScreenAccessL(
                    videoRect(),
                    *m_dsa->ScreenDevice(),
                    *m_dsa->DrawingRegion()));
        }
    }
    const TInt error = dsaError != KErrNone ? dsaError : devVideoError;
    if (error != KErrNone) {
        m_aborted = true;
        m_pipelineRunning = false;
    } else if (m_stage == PhaseBRunning) {
        m_pipelineRunning = true;
        m_lastProgressClock.restart();
    }
    const TRegion *region = (!m_aborted && m_dsa)
        ? m_dsa->DrawingRegion() : 0;
    const TRect bounds = region && !region->IsEmpty()
        ? region->BoundingRect() : TRect(0, 0, 0, 0);
    qDebug() << "WW:DIRECT_RESTART"
             << "reason" << static_cast<int>(reason)
             << "error" << error
             << "rects" << (region ? region->Count() : 0)
             << "bounds" << rectText(bounds)
             << "area" << regionArea(region)
             << "count" << m_directRestartCount;
}

void DevVideoDirectProbe::MdvpoNewBuffers()
{
}

void DevVideoDirectProbe::MdvpoReturnPicture(TVideoPicture *picture)
{
    int index;
    for (index = 0; index < 3; ++index) {
        PictureSlot &slot = m_slots[index];
        if (&slot.picture != picture)
            continue;
        if (!slot.busy) {
            qDebug() << "WW:DIRECT_RETURN_DUPLICATE" << index;
            return;
        }
        slot.busy = false;
        ++m_directReturned;
        m_directReturnLatencyLastMilliseconds =
            slot.submitClock.isValid() ? slot.submitClock.elapsed() : 0;
        m_directReturnLatencyMilliseconds +=
            m_directReturnLatencyLastMilliseconds;
        m_directReturnLatencyMaximumMilliseconds = qMax(
            m_directReturnLatencyMaximumMilliseconds,
            m_directReturnLatencyLastMilliseconds);
        m_lastReturnClock.restart();
        m_lastProgressClock.restart();
        return;
    }
    qDebug() << "WW:DIRECT_RETURN_UNKNOWN"
             << static_cast<void *>(picture);
}

void DevVideoDirectProbe::MdvpoSupplementalInformation(
    const TDesC8 &,
    const TTimeIntervalMicroSeconds &,
    const TPictureId &)
{
}

void DevVideoDirectProbe::MdvpoPictureLoss()
{
    qDebug() << "WW:DIRECT_PICTURE_LOSS";
}

void DevVideoDirectProbe::MdvpoPictureLoss(
    const TArray<TPictureId> &pictures)
{
    qDebug() << "WW:DIRECT_PICTURE_LOSS_LIST" << pictures.Count();
}

void DevVideoDirectProbe::MdvpoSliceLoss(
    TUint firstMacroblock,
    TUint macroblocks,
    const TPictureId &)
{
    qDebug() << "WW:DIRECT_SLICE_LOSS"
             << firstMacroblock << macroblocks;
}

void DevVideoDirectProbe::MdvpoReferencePictureSelection(const TDesC8 &)
{
}

void DevVideoDirectProbe::MdvpoTimedSnapshotComplete(
    TInt,
    TPictureData *,
    const TTimeIntervalMicroSeconds &,
    const TPictureId &)
{
}

void DevVideoDirectProbe::MdvpoNewPictures()
{
    qDebug() << "WW:DIRECT_UNEXPECTED_MEMORY_PICTURE";
}

void DevVideoDirectProbe::MdvpoFatalError(TInt error)
{
    // Never destroy CMMFDevVideoPlay in this callback stack.
    m_fatalSeen = true;
    qDebug() << "WW:DIRECT_FATAL" << "error" << error;
    scheduleCandidateFailure(error, "fatal-callback");
}

void DevVideoDirectProbe::MdvpoInitComplete(TInt error)
{
    m_devInitializing = false;
    qDebug() << "WW:DIRECT_INIT_COMPLETE"
             << "uid" << QString::number(
                    KPostProcessors[m_candidateIndex].iUid, 16)
             << "error" << error;
    if (error != KErrNone || !m_devVideo) {
        scheduleCandidateFailure(
            error == KErrNone ? KErrNotReady : error,
            "initialize");
        return;
    }
    m_devInitialized = true;
    TInt startError = KErrNone;
    TRAP(startError, startInitializedPipelineL());
    if (startError != KErrNone) {
        scheduleCandidateFailure(startError, "start-dsa");
        return;
    }
}

void DevVideoDirectProbe::MdvpoStreamEnd()
{
    qDebug() << "WW:DIRECT_STREAM_END";
}

} // namespace wiliwili

#endif
