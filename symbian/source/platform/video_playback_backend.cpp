#include "platform/video_playback_backend.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QTime>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QWidget>

#include <qmediacontent.h>

#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
#include "platform/ffmpeg_h264_decoder.h"
#endif

#ifdef Q_OS_SYMBIAN
#include <coecntrl.h>
#include <eikenv.h>
#include <f32file.h>
#include <mmf/common/mmfbase.h>
#include <mmf/common/mmferrors.h>
#include <mmf/common/mmfvideo.h>
#ifdef WILIWILI_ENABLE_E7_MMF_HELIX_DIAGNOSTIC
#include <mmf/common/mmfcontrollerpluginresolver.h>
#endif
#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
#include <platform/graphics/surface.h>
#include <surfaceeventhandler.h>
#endif
#include <mmf/devvideo/devvideoplay.h>
#include <videoplayer2.h>
#endif

#if defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER) && \
    defined(WILIWILI_ENABLE_UNSUPPORTED_BCM_DEVVIDEO)
#error FFmpeg software and unsupported BCM DevVideo paths are mutually exclusive
#endif

namespace wiliwili {

#ifdef Q_OS_SYMBIAN
namespace {

class DevVideoProbeObserver : public MMMFDevVideoPlayObserver
{
public:
    virtual void MdvpoNewBuffers() {}
    virtual void MdvpoReturnPicture(TVideoPicture *) {}
    virtual void MdvpoSupplementalInformation(
        const TDesC8 &,
        const TTimeIntervalMicroSeconds &,
        const TPictureId &) {}
    virtual void MdvpoPictureLoss() {}
    virtual void MdvpoPictureLoss(const TArray<TPictureId> &) {}
    virtual void MdvpoSliceLoss(
        TUint, TUint, const TPictureId &) {}
    virtual void MdvpoReferencePictureSelection(const TDesC8 &) {}
    virtual void MdvpoTimedSnapshotComplete(
        TInt,
        TPictureData *,
        const TTimeIntervalMicroSeconds &,
        const TPictureId &) {}
    virtual void MdvpoNewPictures() {}
    virtual void MdvpoFatalError(TInt error)
    {
        qDebug() << "WW:DEVVIDEO_PROBE_FATAL" << error;
    }
    virtual void MdvpoInitComplete(TInt error)
    {
        qDebug() << "WW:DEVVIDEO_PROBE_INIT" << error;
    }
    virtual void MdvpoStreamEnd() {}
};

static QString symbianText(const TDesC &value)
{
    return QString::fromUtf16(
        reinterpret_cast<const ushort *>(value.Ptr()),
        value.Length());
}

static QString symbianText8(const TDesC8 &value)
{
    return QString::fromLatin1(
        reinterpret_cast<const char *>(value.Ptr()),
        value.Length());
}

// DevVideo accepts an Annex-B elementary stream, but a failed header probe
// does not tell us whether the problem is the container conversion or the
// decoder's H.264 feature limits. Keep a compact NAL type trace in the debug
// log so a device run can answer that question without a debugger.
static QString annexBNalTypes(const QByteArray &unit)
{
    QString result;
    int cursor = 0;
    int count = 0;
    while (cursor + 3 < unit.size() && count < 24) {
        int start = -1;
        int prefix = 0;
        int index;
        for (index = cursor; index + 2 < unit.size(); ++index) {
            const unsigned char b0 = static_cast<unsigned char>(unit.at(index));
            const unsigned char b1 = static_cast<unsigned char>(unit.at(index + 1));
            const unsigned char b2 = static_cast<unsigned char>(unit.at(index + 2));
            if (b0 == 0 && b1 == 0 && b2 == 1) {
                start = index;
                prefix = 3;
                break;
            }
            if (index + 3 < unit.size() && b0 == 0 && b1 == 0 &&
                b2 == 0 && static_cast<unsigned char>(unit.at(index + 3)) == 1) {
                start = index;
                prefix = 4;
                break;
            }
        }
        if (start < 0 || start + prefix >= unit.size())
            break;
        const int type = static_cast<unsigned char>(unit.at(start + prefix)) & 0x1f;
        if (!result.isEmpty())
            result += QLatin1String(",");
        result += QString::number(type);
        ++count;
        cursor = start + prefix + 1;
    }
    return result;
}

// This is intentionally less ambitious than the retired DevVideo playback
// experiment. The result answers one routing question only: does the real
// Nokia/Broadcom H.264 plugin accept this stream's SPS/PPS and first coded
// picture? No decoder configuration, output format, clock, DSA, or frame
// submission is touched here, so this probe cannot alter the persistent MMF
// player/display graph used by normal playback.
static VideoPlaybackBackend::AvcHeaderProbeResult
probeBroadcomAvcHeader(
    const QByteArray &annexBAccessUnit,
    int *errorCode)
{
    if (errorCode)
        *errorCode = KErrArgument;
    if (annexBAccessUnit.isEmpty()) {
        qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_REJECT"
                 << "input" << KErrArgument;
        return VideoPlaybackBackend::AvcHeaderProbeRejected;
    }

    const TUid decoderUid = TUid::Uid(0x10204C21);
    DevVideoProbeObserver observer;
    CMMFDevVideoPlay *probe = 0;
    THwDeviceId decoderId = 0;
    TInt stageError = KErrNone;

    qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_BEGIN"
             << decoderUid.iUid << annexBAccessUnit.size()
             << annexBNalTypes(annexBAccessUnit);

    TRAP(stageError, probe = CMMFDevVideoPlay::NewL(observer));
    qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_STAGE"
             << "new" << stageError;
    if (stageError != KErrNone || !probe) {
        if (errorCode)
            *errorCode = stageError == KErrNone ? KErrNoMemory : stageError;
        qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_REJECT"
                 << "new" << (errorCode ? *errorCode : stageError);
        return VideoPlaybackBackend::AvcHeaderProbeRejected;
    }

    TRAP(stageError, decoderId = probe->SelectDecoderL(decoderUid));
    qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_STAGE"
             << "select" << stageError << static_cast<TUint>(decoderId);
    if (stageError != KErrNone) {
        delete probe;
        if (errorCode)
            *errorCode = stageError;
        qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_REJECT"
                 << "select" << stageError;
        return VideoPlaybackBackend::AvcHeaderProbeRejected;
    }

    CCompressedVideoFormat *format = 0;
    TRAP(stageError,
        format = CCompressedVideoFormat::NewL(_L8("video/h264")));
    qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_STAGE"
             << "format" << stageError;
    if (stageError != KErrNone || !format) {
        delete probe;
        if (errorCode)
            *errorCode = stageError == KErrNone ? KErrNoMemory : stageError;
        qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_REJECT"
                 << "format" << (errorCode ? *errorCode : stageError);
        return VideoPlaybackBackend::AvcHeaderProbeRejected;
    }

    TRAP(stageError,
        probe->SetInputFormatL(
            decoderId, *format, EDuCodedPicture,
            EDuElementaryStream, ETrue));
    delete format;
    qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_STAGE"
             << "input" << stageError;
    if (stageError != KErrNone) {
        delete probe;
        if (errorCode)
            *errorCode = stageError;
        qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_REJECT"
                 << "input" << stageError;
        return VideoPlaybackBackend::AvcHeaderProbeRejected;
    }

    TVideoInputBuffer input;
    input.iData.Set(
        reinterpret_cast<TUint8 *>(
            const_cast<char *>(annexBAccessUnit.constData())),
        annexBAccessUnit.size(), annexBAccessUnit.size());
    TVideoPictureHeader *header = 0;
    TRAP(stageError,
        header = probe->GetHeaderInformationL(
            EDuCodedPicture, EDuElementaryStream, &input));
    qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_STAGE"
             << "header" << stageError << (header != 0);
    if (stageError == KErrNone && header) {
        qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_ACCEPT"
                 << header->iProfile << header->iLevel
                 << header->iSizeInMemory.iWidth
                 << header->iSizeInMemory.iHeight
                 << header->iDisplayedRect.Width()
                 << header->iDisplayedRect.Height();
        probe->ReturnHeader(header);
        delete probe;
        if (errorCode)
            *errorCode = KErrNone;
        return VideoPlaybackBackend::AvcHeaderProbeAccepted;
    }

    if (header)
        probe->ReturnHeader(header);
    delete probe;
    if (stageError == KErrNone)
        stageError = KErrUnderflow;
    if (errorCode)
        *errorCode = stageError;
    qDebug() << "WW:DEVVIDEO_HEADER_PREFLIGHT_REJECT"
             << "header" << stageError;
    return VideoPlaybackBackend::AvcHeaderProbeRejected;
}

static int clampByte(int value)
{
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

static QRgb yuvToRgb(unsigned char y, unsigned char u, unsigned char v)
{
    const int c = qMax(0, static_cast<int>(y) - 16);
    const int d = static_cast<int>(u) - 128;
    const int e = static_cast<int>(v) - 128;
    return qRgb(
        clampByte((298 * c + 409 * e + 128) >> 8),
        clampByte((298 * c - 100 * d - 208 * e + 128) >> 8),
        clampByte((298 * c + 516 * d + 128) >> 8));
}

static bool copyDevVideoPicture(
    const TVideoPicture &picture,
    const TUncompressedVideoFormat &format,
    QImage *frame)
{
    if (!frame || !picture.iData.iRawData)
        return false;
    const TDesC8 &raw = *picture.iData.iRawData;
    const int sourceWidth = picture.iData.iDataSize.iWidth;
    const int sourceHeight = picture.iData.iDataSize.iHeight;
    if (sourceWidth <= 0 || sourceHeight <= 0 || raw.Length() <= 0)
        return false;

    QRect crop(0, 0, sourceWidth, sourceHeight);
    if ((picture.iOptions & TVideoPicture::ECropRect) != 0 &&
        picture.iCropRect.Width() > 0 && picture.iCropRect.Height() > 0) {
        crop = QRect(
            picture.iCropRect.iTl.iX,
            picture.iCropRect.iTl.iY,
            picture.iCropRect.Width(),
            picture.iCropRect.Height()).intersected(
                QRect(0, 0, sourceWidth, sourceHeight));
    }
    if (crop.isEmpty())
        return false;

    // The overlay's logical framebuffer is 640x360. Convert only as many
    // pixels as can actually be displayed; this keeps CPU cost bounded while
    // the BCM2727 still performs the expensive H.264 decode in hardware.
    QSize targetSize = crop.size();
    targetSize.scale(QSize(640, 360), Qt::KeepAspectRatio);
    if (targetSize.isEmpty())
        return false;
    QImage converted(targetSize, QImage::Format_RGB32);
    if (converted.isNull())
        return false;

    const unsigned char *bytes = raw.Ptr();
    const int length = raw.Length();
    const bool yuv = format.iDataFormat == EYuvRawData;
    const bool rgb = format.iDataFormat == ERgbRawData;
    if (!yuv && !rgb)
        return false;

    const int yPlaneSize = sourceWidth * sourceHeight;
    const bool is420 = yuv &&
        (format.iYuvFormat.iPattern == EYuv420Chroma1 ||
         format.iYuvFormat.iPattern == EYuv420Chroma2 ||
         format.iYuvFormat.iPattern == EYuv420Chroma3);
    const bool planar = yuv &&
        format.iYuvFormat.iDataLayout == EYuvDataPlanar;
    const bool semiPlanar = yuv &&
        format.iYuvFormat.iDataLayout == EYuvDataSemiPlanar;
    if (yuv && is420 && length < yPlaneSize + yPlaneSize / 2)
        return false;

    int dy;
    for (dy = 0; dy < converted.height(); ++dy) {
        QRgb *out = reinterpret_cast<QRgb *>(converted.scanLine(dy));
        const int sy = crop.top() +
            dy * crop.height() / converted.height();
        int dx;
        for (dx = 0; dx < converted.width(); ++dx) {
            const int sx = crop.left() +
                dx * crop.width() / converted.width();
            if (yuv && is420 && (planar || semiPlanar)) {
                const int yOffset = sy * sourceWidth + sx;
                const int chromaWidth = (sourceWidth + 1) / 2;
                const int chromaOffset = (sy / 2) * chromaWidth + sx / 2;
                unsigned char u;
                unsigned char v;
                if (planar) {
                    const int chromaPlaneSize = chromaWidth *
                        ((sourceHeight + 1) / 2);
                    u = bytes[yPlaneSize + chromaOffset];
                    v = bytes[yPlaneSize + chromaPlaneSize + chromaOffset];
                } else {
                    u = bytes[yPlaneSize + chromaOffset * 2];
                    v = bytes[yPlaneSize + chromaOffset * 2 + 1];
                }
                out[dx] = yuvToRgb(bytes[yOffset], u, v);
            } else if (yuv &&
                       (format.iYuvFormat.iPattern == EYuv422Chroma1 ||
                        format.iYuvFormat.iPattern == EYuv422Chroma2)) {
                const int pairOffset = (sy * sourceWidth + (sx & ~1)) * 2;
                if (pairOffset + 3 >= length)
                    return false;
                unsigned char yy;
                unsigned char u;
                unsigned char v;
                if (format.iYuvFormat.iDataLayout ==
                    EYuvDataInterleavedLE) {
                    // DevVideo defines LE byte order as Y1,V,Y0,U.
                    u = bytes[pairOffset + 3];
                    yy = bytes[pairOffset + (sx & 1 ? 0 : 2)];
                    v = bytes[pairOffset + 1];
                } else {
                    // BE byte order is U,Y0,V,Y1.
                    u = bytes[pairOffset];
                    yy = bytes[pairOffset + (sx & 1 ? 3 : 1)];
                    v = bytes[pairOffset + 2];
                }
                out[dx] = yuvToRgb(yy, u, v);
            } else if (rgb && format.iRgbFormat == ERgb16bit565) {
                const int offset = (sy * sourceWidth + sx) * 2;
                if (offset + 1 >= length)
                    return false;
                const quint16 value = static_cast<quint16>(bytes[offset]) |
                    (static_cast<quint16>(bytes[offset + 1]) << 8);
                out[dx] = qRgb(
                    ((value >> 11) & 31) * 255 / 31,
                    ((value >> 5) & 63) * 255 / 63,
                    (value & 31) * 255 / 31);
            } else if (rgb && format.iRgbFormat == ERgb32bit888) {
                const int offset = (sy * sourceWidth + sx) * 4;
                if (offset + 3 >= length)
                    return false;
                out[dx] = qRgb(
                    bytes[offset + 2], bytes[offset + 1], bytes[offset]);
            } else {
                return false;
            }
        }
    }
    *frame = converted;
    return true;
}

static CVideoDecoderInfo *takeDecoderInfoL(
    CMMFDevVideoPlay *probe, TUid uid)
{
    CVideoDecoderInfo *info = probe->VideoDecoderInfoLC(uid);
    CleanupStack::Pop(info);
    return info;
}

static CPostProcessorInfo *takePostProcessorInfoL(
    CMMFDevVideoPlay *probe, TUid uid)
{
    CPostProcessorInfo *info = probe->PostProcessorInfoLC(uid);
    CleanupStack::Pop(info);
    return info;
}

static void probeLocalAvcDecoders()
{
    static bool completed = false;
    if (completed)
        return;
    completed = true;

    DevVideoProbeObserver observer;
    CMMFDevVideoPlay *probe = 0;
    TRAPD(createError, probe = CMMFDevVideoPlay::NewL(observer));
    qDebug() << "WW:DEVVIDEO_PROBE_CREATE"
             << createError << static_cast<void *>(probe);
    if (createError != KErrNone || !probe)
        return;

    RArray<TUid> mimeDecoders;
    TRAPD(findError,
        probe->FindDecodersL(
            _L8("video/avc"), 0, mimeDecoders, EFalse));
    qDebug() << "WW:DEVVIDEO_AVC_DECODERS"
             << findError << mimeDecoders.Count();
    mimeDecoders.Close();

    RArray<TUid> h264Decoders;
    TRAPD(findH264Error,
        probe->FindDecodersL(
            _L8("video/h264"), 0, h264Decoders, EFalse));
    qDebug() << "WW:DEVVIDEO_H264_DECODERS"
             << findH264Error << h264Decoders.Count();
    for (TInt h264Index = 0;
         h264Index < h264Decoders.Count(); ++h264Index) {
        qDebug() << "WW:DEVVIDEO_H264_UID"
                 << h264Decoders[h264Index].iUid;
    }
    h264Decoders.Close();

    // MIME matching depends on the ECom registration strings chosen by the
    // phone firmware.  A failed "video/avc" lookup does not prove that the
    // decoder registry is empty, so always enumerate every DevVideo decoder
    // and inspect its advertised formats before rejecting the local path.
    RArray<TUid> decoders;
    TRAPD(listError, probe->GetDecoderListL(decoders));
    qDebug() << "WW:DEVVIDEO_ALL_DECODERS"
             << listError << decoders.Count();

    TInt index;
    for (index = 0; index < decoders.Count(); ++index) {
        const TUid uid = decoders[index];
        CVideoDecoderInfo *info = 0;
        TRAPD(infoError,
            info = takeDecoderInfoL(probe, uid));
        if (infoError != KErrNone || !info) {
            qDebug() << "WW:DEVVIDEO_DECODER_INFO_ERROR"
                     << uid.iUid << infoError;
            continue;
        }

        const TSize maximum = info->MaxPictureSize();
        qDebug() << "WW:DEVVIDEO_DECODER"
                 << uid.iUid
                 << symbianText(info->Manufacturer())
                 << symbianText(info->Identifier())
                 << static_cast<bool>(info->Accelerated())
                 << static_cast<bool>(info->SupportsDirectDisplay())
                 << maximum.iWidth << maximum.iHeight
                 << info->MaxBitrate()
                 << info->CodingStandardSpecificInfo().Length();

        const RPointerArray<CCompressedVideoFormat> &formats =
            info->SupportedFormats();
        TInt formatIndex;
        for (formatIndex = 0;
             formatIndex < formats.Count(); ++formatIndex) {
            qDebug() << "WW:DEVVIDEO_FORMAT"
                     << uid.iUid << formatIndex
                     << symbianText8(formats[formatIndex]->MimeType())
                     << formats[formatIndex]->OptionalData().Length();
        }
        delete info;
    }

    decoders.Close();

    // Decoder output still needs either a direct-display post-processor or a
    // memory format that the Qt/GLES layer can upload.  Enumerating these
    // capabilities is read-only and lets us choose that path without trying
    // speculative hardware initialization on the user's phone.
    RArray<TUid> postProcessors;
    TRAPD(postListError,
        probe->GetPostProcessorListL(postProcessors));
    qDebug() << "WW:DEVVIDEO_ALL_POSTPROCESSORS"
             << postListError << postProcessors.Count();
    for (index = 0; index < postProcessors.Count(); ++index) {
        const TUid uid = postProcessors[index];
        CPostProcessorInfo *info = 0;
        TRAPD(infoError,
            info = takePostProcessorInfoL(probe, uid));
        if (infoError != KErrNone || !info) {
            qDebug() << "WW:DEVVIDEO_POSTPROCESSOR_INFO_ERROR"
                     << uid.iUid << infoError;
            continue;
        }

        qDebug() << "WW:DEVVIDEO_POSTPROCESSOR"
                 << uid.iUid
                 << symbianText(info->Manufacturer())
                 << symbianText(info->Identifier())
                 << static_cast<bool>(info->Accelerated())
                 << static_cast<bool>(info->SupportsDirectDisplay())
                 << info->SupportedRotations()
                 << static_cast<bool>(info->SupportsArbitraryScaling())
                 << static_cast<bool>(info->AntiAliasedScaling());

        const RArray<TUint32> &combinations =
            info->SupportedCombinations();
        bool supportsYuvToRgb = false;
        for (TInt combinationIndex = 0;
             combinationIndex < combinations.Count(); ++combinationIndex) {
            if ((combinations[combinationIndex] & EPpYuvToRgb) != 0)
                supportsYuvToRgb = true;
            qDebug() << "WW:DEVVIDEO_POSTPROCESSOR_COMBINATION"
                     << uid.iUid << combinationIndex
                     << combinations[combinationIndex];
        }

        const RArray<TUncompressedVideoFormat> &formats =
            info->SupportedFormats();
        for (TInt formatIndex = 0;
             formatIndex < formats.Count(); ++formatIndex) {
            const TUncompressedVideoFormat &format = formats[formatIndex];
            qDebug() << "WW:DEVVIDEO_POSTPROCESSOR_FORMAT"
                     << uid.iUid << formatIndex
                     << static_cast<TUint32>(format.iDataFormat);
            if (format.iDataFormat == EYuvRawData) {
                qDebug() << "WW:DEVVIDEO_POSTPROCESSOR_YUV"
                         << uid.iUid << formatIndex
                         << static_cast<TUint32>(
                                format.iYuvFormat.iCoefficients)
                         << static_cast<TUint32>(
                                format.iYuvFormat.iPattern)
                         << static_cast<TUint32>(
                                format.iYuvFormat.iDataLayout)
                         << format.iYuvFormat.iAspectRatioNum
                         << format.iYuvFormat.iAspectRatioDenom;
            } else if (format.iDataFormat == ERgbRawData ||
                       format.iDataFormat == ERgbFbsBitmap) {
                qDebug() << "WW:DEVVIDEO_POSTPROCESSOR_RGB"
                         << uid.iUid << formatIndex
                         << static_cast<TUint32>(format.iRgbFormat);
            }
        }

        if (supportsYuvToRgb) {
            const TYuvToRgbCapabilities &yuvToRgb =
                info->YuvToRgbCapabilities();
            qDebug() << "WW:DEVVIDEO_POSTPROCESSOR_YUVRGB"
                     << uid.iUid
                     << yuvToRgb.iSamplingPatterns
                     << yuvToRgb.iCoefficients
                     << yuvToRgb.iRgbFormats
                     << static_cast<bool>(yuvToRgb.iLightnessControl)
                     << static_cast<bool>(yuvToRgb.iSaturationControl)
                     << static_cast<bool>(yuvToRgb.iContrastControl)
                     << static_cast<bool>(yuvToRgb.iGammaCorrection)
                     << yuvToRgb.iDitherTypes;
        }

        const RArray<TScaleFactor> &scaleFactors =
            info->SupportedScaleFactors();
        for (TInt scaleIndex = 0;
             scaleIndex < scaleFactors.Count(); ++scaleIndex) {
            qDebug() << "WW:DEVVIDEO_POSTPROCESSOR_SCALE"
                     << uid.iUid << scaleIndex
                     << scaleFactors[scaleIndex].iScaleNum
                     << scaleFactors[scaleIndex].iScaleDenom;
        }
        delete info;
    }
    postProcessors.Close();

    delete probe;
    qDebug() << "WW:DEVVIDEO_PROBE_DONE";
}

} // namespace
#endif

class VideoPlaybackBackend::Impl
#ifdef Q_OS_SYMBIAN
    : public MVideoPlayerUtilityObserver,
      public MMMFDevVideoPlayObserver
#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
      , public MMMFSurfaceEventHandler
#endif
#endif
{
public:
    explicit Impl(QWidget *videoHost)
        : host(videoHost), currentState(StoppedState),
           currentStatus(NoMedia), currentError(NoError), currentDuration(0),
           currentVolume(80), mediaSessionSerial(0),
           requestedRate(1.0), playWhenReady(false),
           videoAvailable(false), nativeVideoEnabledRequested(true),
          nativeVideoPolicyApplied(true), nativeVideoPolicyError(0),
          devVideoProbeState(0),
          devVideoProbeError(0), devVideoProbePictures(0),
          devVideoFrameTimestamp(0), devVideoFrameSerial(0),
          devVideoCodedWidth(0), devVideoCodedHeight(0),
          devVideoYuv420Output(false)
#ifdef Q_OS_SYMBIAN
          , sharedFileSessionConnected(false), sharedMediaFileOpen(false),
          player(0), displayWindow(0), displayAdded(false),
          geometryApplied(false), rotationApplied(false),
          scaleApplied(false),
#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
          surfaceDisplayAdded(false), surfaceBackgroundAttached(false),
          surfaceDisplayId(0), surfaceId(TSurfaceId::CreateNullId()),
#endif
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
          ffmpegDecoder(0),
#endif
          devVideo(0), devVideoClock(0), devVideoDecoderId(0),
          devVideoUnitIndex(0),
          devVideoSequence(0), devVideoInitialized(false),
          devVideoInputEnded(false), devVideoInputFinal(false),
          devVideoOutputValid(false), devVideoPaused(false)
#endif
    {
    }

    ~Impl()
    {
        destroyNativePlayer();
    }

    void resetState()
    {
        currentState = StoppedState;
        currentStatus = NoMedia;
        currentError = NoError;
        currentErrorText.clear();
        currentDuration = 0;
        playWhenReady = false;
        videoAvailable = false;
    }

    void fail(int errorCode, const char *stage)
    {
        currentState = StoppedState;
        currentStatus = InvalidMedia;
        currentError = errorCode == 0 ? -1 : errorCode;
        currentErrorText = QString::fromLatin1("Symbian:%1").arg(errorCode);
        videoAvailable = false;
        qDebug() << "WW:NATIVE_MMF_ERROR" << stage << errorCode;
    }

#ifdef Q_OS_SYMBIAN
    RWindow *resolveDisplayWindow()
    {
        if (!host)
            return 0;
        host->setAttribute(Qt::WA_NativeWindow, true);
        CCoeControl *control = host->winId();
        if (!control || !control->OwnsWindow()) {
            qDebug() << "WW:NATIVE_MMF_NO_WINDOW"
                     << static_cast<void *>(control)
                     << (control ? control->OwnsWindow() : false);
            return 0;
        }
        RDrawableWindow *drawableWindow = control->DrawableWindow();
        if (!drawableWindow)
            return 0;

        // QWidget creates an owned RWindow for WA_NativeWindow widgets. Window()
        // is protected in the Symbian Qt 4.7 headers; DrawableWindow() exposes
        // the same native handle through the public CCoeControl API.
        return static_cast<RWindow *>(drawableWindow);
    }

    TRect displayRect() const
    {
        return TRect(
            TPoint(0, 0),
            TSize(qMax(1, host ? host->width() : 1),
                  qMax(1, host ? host->height() : 1)));
    }

    bool ensureNativePlayer()
    {
        if (player) {
            qDebug() << "WW:NATIVE_MMF_UTILITY_REUSE"
                     << static_cast<void *>(player)
                     << static_cast<void *>(displayWindow);
            return true;
        }
        qDebug() << "WW:NATIVE_MMF_STAGE" << 10 << "resolve-window";
        CEikonEnv *environment = CEikonEnv::Static();
        displayWindow = resolveDisplayWindow();
        qDebug() << "WW:NATIVE_MMF_STAGE" << 11 << "window-resolved"
                 << static_cast<void *>(environment)
                 << static_cast<void *>(displayWindow)
                 << (host ? host->size() : QSize());
        if (!environment || !environment->ScreenDevice() || !displayWindow) {
            fail(KErrNotReady, "window");
            return false;
        }

        // This RWindow is created, activated and owned by QWidget. Calling
        // RWindow::Activate() a second time triggers a WSERV debug trap on the
        // Nokia 603 before MMF is even constructed. Never mutate the Qt-owned
        // window lifecycle here; AddDisplayWindowL only needs its live handle.
        environment->WsSession().Flush();
        qDebug() << "WW:NATIVE_MMF_STAGE" << 13 << "qt-window-ready";

#ifdef WILIWILI_ENABLE_E7_MMF_USERENVIRONMENT_DIAGNOSTIC
        // Log the capability held by the installed EXE, rather than relying
        // solely on the generated MMP/SIS metadata. This is the published
        // baseline requirement for CVideoPlayerUtility video playback.
        const TBool hasUserEnvironment =
            RProcess().HasCapability(ECapabilityUserEnvironment);
        qDebug() << "WW:E7_MMF_USERENVIRONMENT_CAPABILITY"
                 << static_cast<bool>(hasUserEnvironment);
#endif

        // This is diagnostic-only.  CVideoPlayerUtility does not need a
        // DevVideo object to select its controller, so E7 tests can compile
        // it out to isolate MMF from any earlier decoder-plugin activity.
#ifndef WILIWILI_DISABLE_NATIVE_DEVVIDEO_PROBE
        probeLocalAvcDecoders();
#else
        qDebug() << "WW:NATIVE_MMF_DEVVIDEO_PROBE_SKIPPED";
#endif

        // Qt Mobility's S60 MMF implementation creates its video utility with
        // priority 0 / PreferenceNone.  EMdaPriorityNormal is also 0, but the
        // normal NIKINIKI route requests TimeAndQuality; preserve that normal
        // request while making the Qt-Mobility comparison exact in the opt-in
        // controller diagnostic.
#ifdef WILIWILI_ENABLE_E7_MMF_HELIX_DIAGNOSTIC
        const TInt playerPriority = 0;
        const TInt playerPreference = EMdaPriorityPreferenceNone;
#else
        const TInt playerPriority = EMdaPriorityNormal;
        const TInt playerPreference = EMdaPriorityPreferenceTimeAndQuality;
#endif
        qDebug() << "WW:NATIVE_MMF_STAGE" << 20 << "new-player-before"
                 << "priority" << playerPriority
                 << "preference" << playerPreference;
#ifdef WILIWILI_ENABLE_E7_MMF_LEGACY_UTILITY_DIAGNOSTIC
        const TRect initialRect = displayRect();
        TRAPD(createError,
            player = CVideoPlayerUtility::NewL(
                *this,
                playerPriority,
                playerPreference,
                environment->WsSession(),
                *environment->ScreenDevice(),
                *displayWindow,
                initialRect,
                initialRect);
        );
#else
        TRAPD(createError,
            player = CVideoPlayerUtility2::NewL(
                *this,
                playerPriority,
                playerPreference);
        );
#endif
        qDebug() << "WW:NATIVE_MMF_STAGE" << 21 << "new-player-after"
                  << createError << static_cast<void *>(player);
        if (createError != KErrNone || !player) {
            fail(createError, "create");
            return false;
        }
#ifdef WILIWILI_ENABLE_E7_MMF_LEGACY_UTILITY_DIAGNOSTIC
        displayAdded = true;
        qDebug() << "WW:E7_MMF_LEGACY_UTILITY_WINDOW"
                 << host->size() << host->winId()
                 << "bound-at-new";
#endif

        return true;
    }

    void logOpenedController()
    {
#ifdef WILIWILI_ENABLE_E7_MMF_HELIX_DIAGNOSTIC
        if (!player)
            return;
        TRAPD(controllerInfoError,
            const CMMFControllerImplementationInformation &info =
                player->ControllerImplementationInformationL();
            qDebug() << "WW:E7_MMF_HELIX_CONTROLLER_OPENED"
                     << info.Uid().iUid
                     << symbianText(info.DisplayName())
                     << symbianText(info.Supplier())
                     << info.Version();
        );
        qDebug() << "WW:E7_MMF_HELIX_CONTROLLER_QUERY"
                 << controllerInfoError;
#endif
    }

    void closeSharedMediaFile()
    {
        if (sharedMediaFileOpen) {
            sharedMediaFile.Close();
            sharedMediaFileOpen = false;
        }
        if (sharedFileSessionConnected) {
            sharedFileSession.Close();
            sharedFileSessionConnected = false;
        }
    }

    TInt openSharedMediaFile(const TDesC &path)
    {
        closeSharedMediaFile();
        TInt error = sharedFileSession.Connect();
        if (error != KErrNone)
            return error;
        sharedFileSessionConnected = true;
        error = sharedFileSession.ShareProtected();
        if (error == KErrNone) {
            error = sharedMediaFile.Open(
                sharedFileSession, path,
                EFileRead | EFileShareReadersOrWriters);
        }
        if (error != KErrNone) {
            closeSharedMediaFile();
            return error;
        }
        sharedMediaFileOpen = true;
        return KErrNone;
    }

    bool addDisplayWindow()
    {
        if (displayAdded)
            return true;
        CEikonEnv *environment = CEikonEnv::Static();
        if (!player || !environment || !environment->ScreenDevice() ||
            !displayWindow) {
            fail(KErrNotReady, "display-prerequisite");
            return false;
        }

#ifdef WILIWILI_ENABLE_E7_MMF_LEGACY_UTILITY_DIAGNOSTIC
        Q_UNUSED(environment);
        displayAdded = true;
        qDebug() << "WW:E7_MMF_LEGACY_UTILITY_DISPLAY_READY"
                 << host->size() << host->winId();
        return true;
#else

        // CVideoPlayerUtility2 does not own a selected controller until
        // MvpuoOpenComplete(KErrNone). Nokia 603 returns KErrNotReady when a
        // display is added immediately after NewL, before OpenUrl/OpenFile has
        // completed. Bind the Qt-owned RWindow only from the open callback.
        const TRect rect = displayRect();
        qDebug() << "WW:NATIVE_MMF_STAGE" << 30 << "add-display-before"
                 << host->size();
        TRAPD(displayError,
            player->AddDisplayWindowL(
                environment->WsSession(),
                *environment->ScreenDevice(),
                *displayWindow,
                rect,
                rect));
        qDebug() << "WW:NATIVE_MMF_STAGE" << 31 << "add-display-after"
                 << displayError;
        if (displayError != KErrNone) {
            fail(displayError, "display");
            return false;
        }
        displayAdded = true;
        qDebug() << "WW:NATIVE_MMF_WINDOW"
                 << host->size() << host->winId() << "native-no-rotation";
        return true;
#endif
    }

#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
    bool addGraphicsSurfaceDisplay()
    {
        if (surfaceDisplayAdded)
            return true;
        CEikonEnv *environment = CEikonEnv::Static();
        if (!player || !environment || !environment->ScreenDevice() ||
            !displayWindow) {
            fail(KErrNotReady, "surface-display-prerequisite");
            return false;
        }

        // CVideoPlayerUtility2 registers its surface with WSERV before this
        // object's MmsehSurfaceCreated callback.  Unlike AddDisplayWindowL,
        // this is the public window-less graphics-surface output API.
        surfaceDisplayId = environment->ScreenDevice()->GetScreenNumber();
        qDebug() << "WW:E7_MMF_SURFACE_ADD_BEFORE" << surfaceDisplayId;
        TRAPD(surfaceDisplayError,
            player->AddDisplayL(
                environment->WsSession(), surfaceDisplayId, *this));
        qDebug() << "WW:E7_MMF_SURFACE_ADD_AFTER" << surfaceDisplayError
                 << surfaceDisplayId;
        if (surfaceDisplayError != KErrNone) {
            fail(surfaceDisplayError, "surface-display");
            return false;
        }
        surfaceDisplayAdded = true;
        return true;
    }

    void removeGraphicsSurfaceDisplay()
    {
        if (surfaceBackgroundAttached && displayWindow) {
            displayWindow->RemoveBackgroundSurface(ETrue);
            qDebug() << "WW:E7_MMF_SURFACE_BACKGROUND_REMOVED";
        }
        surfaceBackgroundAttached = false;
        surfaceId = TSurfaceId::CreateNullId();
        if (player && surfaceDisplayAdded) {
            player->RemoveDisplay(surfaceDisplayId);
            qDebug() << "WW:E7_MMF_SURFACE_DISPLAY_REMOVED"
                     << surfaceDisplayId;
        }
        surfaceDisplayAdded = false;
        surfaceDisplayId = 0;
    }
#endif

    void closeMedia()
    {
        if (player) {
            player->Stop();
#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
            removeGraphicsSurfaceDisplay();
#endif
#ifndef WILIWILI_ENABLE_E7_MMF_LEGACY_UTILITY_DIAGNOSTIC
            if (displayAdded && displayWindow)
                player->RemoveDisplayWindow(*displayWindow);
#endif
            player->Close();
        }
        closeSharedMediaFile();
        displayAdded = false;
        geometryApplied = false;
        rotationApplied = false;
        scaleApplied = false;
        resetState();
        qDebug() << "WW:NATIVE_MMF_MEDIA_CLOSED"
                  << static_cast<void *>(player)
                  << static_cast<void *>(displayWindow)
                  << "session" << mediaSessionSerial
                  << "volume-percent" << currentVolume;
    }

    void applyNativeVolume(const char *stage)
    {
        if (!player)
            return;
        const TInt maximum = player->MaxVolume();
        const TInt target = maximum > 0
            ? maximum * currentVolume / 100 : 0;
        TRAPD(setError, player->SetVolumeL(target));
        const TInt actual = player->Volume();
        qDebug() << "WW:NATIVE_MMF_AUDIO_VOLUME"
                 << stage
                 << "session" << mediaSessionSerial
                 << "percent" << currentVolume
                 << "max" << maximum
                 << "target" << target
                 << "set-error" << setError
                 << "actual" << actual;
    }

    void applyNativeVideoPolicy(TBool *nativeVideoEnabled)
    {
        nativeVideoPolicyError = KErrNone;
        nativeVideoPolicyApplied = nativeVideoEnabledRequested;
        if (!player || nativeVideoEnabledRequested)
            return;
        TRAP(nativeVideoPolicyError,
             player->SetVideoEnabledL(EFalse));
        nativeVideoPolicyApplied =
            nativeVideoPolicyError == KErrNone ||
            (nativeVideoEnabled && !*nativeVideoEnabled);
        if (nativeVideoPolicyError == KErrNone && nativeVideoEnabled)
            *nativeVideoEnabled = EFalse;
        qDebug() << "WW:NATIVE_MMF_VIDEO_POLICY"
                 << "enabled" << nativeVideoEnabledRequested
                 << "error" << nativeVideoPolicyError
                 << "applied" << nativeVideoPolicyApplied;
    }

    void logNativeAudioState(const char *stage)
    {
        if (!player)
            return;
        TBool audioEnabled = EFalse;
        TRAPD(audioEnabledError,
              audioEnabled = player->AudioEnabledL());
        qDebug() << "WW:NATIVE_MMF_AUDIO_STATE"
                 << stage
                 << "session" << mediaSessionSerial
                 << "enabled-error" << audioEnabledError
                 << "enabled" << static_cast<bool>(audioEnabled)
                 << "percent" << currentVolume
                 << "max" << player->MaxVolume()
                 << "actual" << player->Volume();
    }

    void destroyNativePlayer()
    {
        destroyDevVideoProbe();
        closeMedia();
        delete player;
        player = 0;
        displayWindow = 0;
        qDebug() << "WW:NATIVE_MMF_UTILITY_DESTROYED";
    }

    void configureDisplay()
    {
        if (!player || !displayWindow)
            return;
        if (!rotationApplied) {
            qDebug() << "WW:NATIVE_MMF_STAGE" << 60 << "rotation-before"
                     << "native-none";
            TRAPD(rotationError,
#ifdef WILIWILI_ENABLE_E7_MMF_LEGACY_UTILITY_DIAGNOSTIC
                player->SetRotationL(EVideoRotationNone));
#else
                player->SetRotationL(
                    *displayWindow, EVideoRotationNone));
#endif
            qDebug() << "WW:NATIVE_MMF_ROTATION"
                     << "native-none" << rotationError;
            rotationApplied = true;
        }
        if (!scaleApplied) {
            qDebug() << "WW:NATIVE_MMF_STAGE" << 61 << "scale-before";
            TRAPD(scaleError,
#ifdef WILIWILI_ENABLE_E7_MMF_LEGACY_UTILITY_DIAGNOSTIC
                player->SetAutoScaleL(EAutoScaleBestFit));
#else
                player->SetAutoScaleL(
                    *displayWindow, EAutoScaleBestFit));
#endif
            qDebug() << "WW:NATIVE_MMF_SCALE" << scaleError;
            scaleApplied = scaleError == KErrNone;
        }
        updateWindow();
    }

    void updateWindow()
    {
        if (!player || !displayWindow || !displayAdded)
            return;
#ifdef WILIWILI_ENABLE_E7_MMF_LEGACY_UTILITY_DIAGNOSTIC
        // The legacy utility received this stable 640x360 RWindow plus its
        // extent and clip in NewL().  Do not rebind it after controller open.
        geometryApplied = true;
        return;
#else
        const TRect rect = displayRect();
        if (geometryApplied &&
            lastGeometryRect.iTl.iX == rect.iTl.iX &&
            lastGeometryRect.iTl.iY == rect.iTl.iY &&
            lastGeometryRect.iBr.iX == rect.iBr.iX &&
            lastGeometryRect.iBr.iY == rect.iBr.iY) {
            return;
        }
#ifndef WILIWILI_SYMBIAN_RELEASE_PLAYER_LOG
        qDebug() << "WW:NATIVE_MMF_STAGE" << 62 << "extent-before"
                 << (host ? host->size() : QSize());
#endif
        TRAPD(extentError,
            player->SetVideoExtentL(*displayWindow, rect));
#ifndef WILIWILI_SYMBIAN_RELEASE_PLAYER_LOG
        qDebug() << "WW:NATIVE_MMF_STAGE" << 63 << "clip-before"
                 << extentError;
#endif
        TRAPD(clipError,
            player->SetWindowClipRectL(*displayWindow, rect));
#ifndef WILIWILI_SYMBIAN_RELEASE_PLAYER_LOG
        qDebug() << "WW:NATIVE_MMF_STAGE" << 64 << "geometry-after"
                 << extentError << clipError;
#endif
        if (extentError != KErrNone || clipError != KErrNone) {
            qDebug() << "WW:NATIVE_MMF_GEOMETRY_ERROR"
                     << extentError << clipError;
        }
        if (extentError == KErrNone && clipError == KErrNone) {
            lastGeometryRect = rect;
            geometryApplied = true;
        } else {
            geometryApplied = false;
        }
#endif
    }

    virtual void MvpuoOpenComplete(TInt error)
    {
        qDebug() << "WW:NATIVE_MMF_OPEN_COMPLETE" << error;
        if (error != KErrNone) {
            fail(error, "open");
            return;
        }
        logOpenedController();
#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
        // The surface route must not bind CVideoPlayerUtility2's ordinary
        // display window before Prepare().  Qt Mobility registers AddDisplayL
        // only after its prepare callback, then attaches the callback surface
        // to a native window.  This keeps the final E7 test genuinely distinct
        // from every preceding AddDisplayWindowL experiment.
        currentStatus = LoadingMedia;
        qDebug() << "WW:E7_MMF_SURFACE_PREPARE_WITHOUT_DISPLAY_WINDOW";
        player->Prepare();
        return;
#endif
#ifdef WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_DIAGNOSTIC
        // This diagnostic keeps the MMF controller only for AAC and the
        // playback clock.  Do not attach an MMF display renderer, which could
        // reserve the BCM video path before the application-owned DevVideo
        // memory-output decoder is configured below.
        if (!nativeVideoEnabledRequested) {
            qDebug() << "WW:E7_DEVVIDEO_MEMORY_MMF_DISPLAY_SKIPPED"
                     << mediaSessionSerial;
            TRAPD(disableBeforePrepareError,
                player->SetVideoEnabledL(EFalse));
            qDebug() << "WW:E7_DEVVIDEO_MEMORY_MMF_VIDEO_OFF_PREPARE"
                     << disableBeforePrepareError;
        } else {
            if (!addDisplayWindow())
                return;
            qDebug() << "WW:NATIVE_MMF_STAGE" << 65 << "configure-before";
            configureDisplay();
            qDebug() << "WW:NATIVE_MMF_STAGE" << 66 << "configure-after";
        }
#else
        if (!addDisplayWindow())
            return;
        qDebug() << "WW:NATIVE_MMF_STAGE" << 65 << "configure-before";
        configureDisplay();
        qDebug() << "WW:NATIVE_MMF_STAGE" << 66 << "configure-after";
#endif
        currentStatus = LoadingMedia;
        qDebug() << "WW:NATIVE_MMF_STAGE" << 67 << "prepare-before";
        player->Prepare();
        qDebug() << "WW:NATIVE_MMF_STAGE" << 68 << "prepare-after";
    }

    void logSelectedTracks()
    {
#ifdef WILIWILI_ENABLE_E7_MMF_CONTROLLER_TRACE
        if (!player)
            return;
        TRAPD(trackInfoError,
            TSize videoSize;
            player->VideoFrameSizeL(videoSize);
            const TReal32 videoFrameRate = player->VideoFrameRateL();
            const TDesC8 &videoMime = player->VideoFormatMimeType();
            const TInt videoBitRate = player->VideoBitRateL();
            const TInt audioBitRate = player->AudioBitRateL();
            TFourCC audioType = player->AudioTypeL();
            qDebug() << "WW:NATIVE_MMF_TRACK_INFO"
                     << symbianText8(videoMime)
                     << videoSize.iWidth << videoSize.iHeight
                     << videoFrameRate << videoBitRate
                     << audioType.FourCC() << audioBitRate;
        );
        qDebug() << "WW:NATIVE_MMF_TRACK_INFO_QUERY" << trackInfoError;
#endif
    }

    virtual void MvpuoPrepareComplete(TInt error)
    {
        qDebug() << "WW:NATIVE_MMF_PREPARE_COMPLETE" << error;
        // KErrMMPartialPlayback is a usable MMF result, not a failed prepare:
        // one of the audio/video tracks could be initialized. Belle commonly
        // reports it for current Bilibili MP4 files even when the H.264 video
        // track is playable. Treating it as fatal prevented Play() entirely
        // and caused every CDN URL to be reopened in rapid succession.
        const bool partialPlayback = error == KErrMMPartialPlayback;
        if (error != KErrNone && !partialPlayback) {
            fail(error, "prepare");
            return;
        }

        TBool nativeVideoEnabled = EFalse;
        TBool nativeAudioEnabled = EFalse;
        TRAPD(videoEnabledError,
            nativeVideoEnabled = player->VideoEnabledL());
        TRAPD(audioEnabledError,
            nativeAudioEnabled = player->AudioEnabledL());
        qDebug() << "WW:NATIVE_MMF_TRACKS"
                 << partialPlayback
                 << videoEnabledError << static_cast<bool>(nativeVideoEnabled)
                 << audioEnabledError << static_cast<bool>(nativeAudioEnabled);
        logSelectedTracks();
#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
        if (nativeVideoEnabledRequested &&
            videoEnabledError == KErrNone && nativeVideoEnabled) {
            if (!addGraphicsSurfaceDisplay())
                return;
        } else {
            qDebug() << "WW:E7_MMF_SURFACE_SKIPPED"
                     << "requested" << nativeVideoEnabledRequested
                     << "video-query" << videoEnabledError
                     << "video-enabled" << static_cast<bool>(nativeVideoEnabled);
        }
#endif
        applyNativeVideoPolicy(&nativeVideoEnabled);
        qDebug() << "WW:NATIVE_MMF_STAGE" << 70 << "duration-before";
        TTimeIntervalMicroSeconds nativeDuration;
        TRAPD(durationError, nativeDuration = player->DurationL());
        qDebug() << "WW:NATIVE_MMF_STAGE" << 71 << "duration-after"
                 << durationError;
        if (durationError == KErrNone)
            currentDuration = static_cast<qint64>(nativeDuration.Int64()) / 1000;
        qDebug() << "WW:NATIVE_MMF_STAGE" << 72 << "volume-before";
        applyNativeVolume("prepare");
        qDebug() << "WW:NATIVE_MMF_STAGE" << 73 << "volume-after";
        if (requestedRate != 1.0)
            TRAP_IGNORE(player->SetPlayVelocityL(
                static_cast<TInt>(requestedRate * 100.0)));
        currentStatus = LoadedMedia;
        currentError = NoError;
        currentErrorText.clear();
        videoAvailable = videoEnabledError == KErrNone
            ? nativeVideoEnabled : !partialPlayback;
        if (playWhenReady)
            startPlayback();
    }

    virtual void MvpuoFrameReady(CFbsBitmap &frame, TInt error)
    {
        Q_UNUSED(frame);
        if (error != KErrNone)
            qDebug() << "WW:NATIVE_MMF_FRAME_ERROR" << error;
    }

    virtual void MvpuoPlayComplete(TInt error)
    {
        qDebug() << "WW:NATIVE_MMF_PLAY_COMPLETE" << error;
        currentState = StoppedState;
        currentStatus = error == KErrNone ? EndOfMedia : InvalidMedia;
        if (error != KErrNone)
            fail(error, "play");
    }

    virtual void MvpuoEvent(const TMMFEvent &event)
    {
        qDebug() << "WW:NATIVE_MMF_EVENT"
                 << event.iEventType.iUid << event.iErrorCode;
        if (event.iErrorCode != KErrNone)
            fail(event.iErrorCode, "event");
    }

#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
    virtual void MmsehSurfaceCreated(
        TInt aDisplayId,
        const TSurfaceId &aId,
        const TRect &aCropRect,
        TVideoAspectRatio aAspectRatio)
    {
        Q_UNUSED(aAspectRatio);
        surfaceId = aId;
        const TInt attachError = displayWindow
            ? displayWindow->SetBackgroundSurface(aId) : KErrNotReady;
        surfaceBackgroundAttached = attachError == KErrNone;
        CEikonEnv *environment = CEikonEnv::Static();
        if (environment)
            environment->WsSession().Flush();
        qDebug() << "WW:E7_MMF_SURFACE_CREATED"
                 << aDisplayId
                 << static_cast<quint32>(aId.iInternal[0])
                 << static_cast<quint32>(aId.iInternal[1])
                 << static_cast<quint32>(aId.iInternal[2])
                 << static_cast<quint32>(aId.iInternal[3])
                 << aCropRect.iTl.iX << aCropRect.iTl.iY
                 << aCropRect.iBr.iX << aCropRect.iBr.iY
                 << "attach" << attachError;
    }

    virtual void MmsehSurfaceParametersChanged(
        const TSurfaceId &aId,
        const TRect &aCropRect,
        TVideoAspectRatio aAspectRatio)
    {
        Q_UNUSED(aAspectRatio);
        qDebug() << "WW:E7_MMF_SURFACE_PARAMETERS"
                 << static_cast<quint32>(aId.iInternal[3])
                 << aCropRect.iTl.iX << aCropRect.iTl.iY
                 << aCropRect.iBr.iX << aCropRect.iBr.iY;
    }

    virtual void MmsehRemoveSurface(const TSurfaceId &aId)
    {
        const bool matchesCurrentSurface = aId == surfaceId;
        if (matchesCurrentSurface && surfaceBackgroundAttached && displayWindow)
            displayWindow->RemoveBackgroundSurface(ETrue);
        if (matchesCurrentSurface) {
            surfaceBackgroundAttached = false;
            surfaceId = TSurfaceId::CreateNullId();
        }
        qDebug() << "WW:E7_MMF_SURFACE_REMOVED"
                 << static_cast<quint32>(aId.iInternal[3])
                 << "current" << matchesCurrentSurface;
    }
#endif

    void startPlayback()
    {
        if (!player || currentStatus != LoadedMedia)
            return;
        qDebug() << "WW:NATIVE_MMF_STAGE" << 80 << "play-before";
        player->Play();
        qDebug() << "WW:NATIVE_MMF_STAGE" << 81 << "play-after";
        currentState = PlayingState;
        playWhenReady = false;
        qDebug() << "WW:NATIVE_MMF_PLAY";
        logNativeAudioState("play");
    }

    void destroyDevVideoProbe()
    {
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
        if (ffmpegDecoder) {
            ffmpegDecoder->stopDecoder();
            delete ffmpegDecoder;
            ffmpegDecoder = 0;
        }
#endif
        if (devVideo && devVideoInitialized)
            devVideo->Stop();
        delete devVideo;
        devVideo = 0;
        delete devVideoClock;
        devVideoClock = 0;
        devVideoUnits.clear();
        devVideoTimes.clear();
        devVideoPresentationTimes.clear();
        devVideoSubmittedTimes.clear();
        devVideoDecoderId = 0;
        devVideoUnitIndex = 0;
        devVideoSequence = 0;
        devVideoInitialized = false;
        devVideoInputEnded = false;
        devVideoInputFinal = false;
        devVideoOutputValid = false;
        devVideoPaused = false;
        devVideoProbeState = 0;
        devVideoProbeError = 0;
        devVideoProbePictures = 0;
        devVideoFrame = QImage();
        devVideoFrameClock = QTime();
        devVideoSessionClock = QTime();
        devVideoFrameTimestamp = 0;
        videoAvailable = false;
        ++devVideoFrameSerial;
    }

    void failDevVideoPlayback(TInt error, const char *stage)
    {
        devVideoProbeState = 4;
        devVideoProbeError = error;
        videoAvailable = false;
        devVideoFrame = QImage();
        ++devVideoFrameSerial;
        qDebug() << "WW:DEVVIDEO_PLAYER_FATAL_STAGE"
                 << stage << error << devVideoProbePictures
                 << devVideoUnitIndex;
    }

#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
    bool startFfmpegPlayback(
        const QVector<QByteArray> &units,
        const QVector<qint64> &times,
        const QVector<qint64> &presentationTimes,
        int codedWidth,
        int codedHeight)
    {
        destroyDevVideoProbe();
        ffmpegDecoder = new FfmpegH264Decoder();
        if (!ffmpegDecoder) {
            devVideoProbeError = KErrNoMemory;
            devVideoProbeState = 4;
            return false;
        }
        qDebug() << "WW:FFMPEG_SOFT_MMF_AUDIO_RETAINED"
                 << static_cast<void *>(player)
                 << currentState << currentStatus << displayAdded
                 << audioPositionMilliseconds();
        if (!ffmpegDecoder->startDecoder(
                units, times, presentationTimes, codedWidth, codedHeight,
                devVideoYuv420Output)) {
            delete ffmpegDecoder;
            ffmpegDecoder = 0;
            devVideoProbeError = KErrArgument;
            devVideoProbeState = 4;
            return false;
        }
        videoAvailable = false;
        devVideoProbeState = 1;
        devVideoProbeError = 0;
        qDebug() << "WW:FFMPEG_SOFT_BEGIN"
                 << units.size() << units.first().size()
                 << codedWidth << codedHeight
                 << times.first() << presentationTimes.first()
                 << (devVideoYuv420Output ? "GLES_YUV420" : "RGB565");
        return true;
    }

    bool appendFfmpegPlayback(
        const QVector<QByteArray> &units,
        const QVector<qint64> &times,
        const QVector<qint64> &presentationTimes)
    {
        return ffmpegDecoder &&
            ffmpegDecoder->append(units, times, presentationTimes);
    }
#endif

    void setupDevVideoProbeL()
    {
        if (devVideoUnits.isEmpty() ||
            devVideoUnits.size() != devVideoTimes.size() ||
            devVideoUnits.size() != devVideoPresentationTimes.size()) {
            User::Leave(KErrArgument);
        }
        if (devVideoCodedWidth <= 0 || devVideoCodedHeight <= 0 ||
            devVideoCodedWidth > 1280 || devVideoCodedHeight > 720) {
            User::Leave(KErrNotSupported);
        }
        const TUid selectedDecoder = TUid::Uid(0x10204C21);
        const char *decoderName = "BCM_0x10204C21";
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "new-before";
        TRAPD(newError, devVideo = CMMFDevVideoPlay::NewL(*this));
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "new-after" << newError;
        if (newError != KErrNone)
            User::Leave(newError);

        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "clock-before";
        TRAPD(clockError, devVideoClock = CSystemClockSource::NewL());
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "clock-after" << clockError;
        if (clockError != KErrNone)
            User::Leave(clockError);

        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "select-before"
                 << decoderName;
        TRAPD(selectError,
            devVideoDecoderId = devVideo->SelectDecoderL(selectedDecoder));
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "select-after"
                 << selectError << static_cast<TUint>(devVideoDecoderId);
        if (selectError != KErrNone)
            User::Leave(selectError);

        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "format-new-before"
                 << "video/h264";
        CCompressedVideoFormat *format =
            CCompressedVideoFormat::NewL(_L8("video/h264"));
        CleanupStack::PushL(format);
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "set-input-before"
                 << "video/h264" << static_cast<int>(EDuCodedPicture)
                 << static_cast<int>(EDuElementaryStream) << true;
        TRAPD(inputError,
            devVideo->SetInputFormatL(
                devVideoDecoderId,
                *format,
                EDuCodedPicture,
                EDuElementaryStream,
                ETrue));
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "set-input-after"
                 << inputError;
        CleanupStack::PopAndDestroy(format);
        if (inputError != KErrNone)
            User::Leave(inputError);

        TVideoInputBuffer headerInput;
        QByteArray &firstUnit = devVideoUnits[0];
        qDebug() << "WW:DEVVIDEO_DIAG_HEADER_INPUT"
                 << firstUnit.size() << annexBNalTypes(firstUnit)
                 << firstUnit.left(64).toHex();
        headerInput.iData.Set(
            reinterpret_cast<TUint8 *>(firstUnit.data()),
            firstUnit.size(),
            firstUnit.size());
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "header-before"
                 << firstUnit.size();
        TVideoPictureHeader *header = 0;
        TRAPD(headerError,
            header = devVideo->GetHeaderInformationL(
                EDuCodedPicture,
                EDuElementaryStream,
                &headerInput));
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "header-after"
                 << headerError << (header != 0);
        if (headerError != KErrNone)
            User::Leave(headerError);
        if (!header)
            User::Leave(KErrUnderflow);
        qDebug() << "WW:DEVVIDEO_PLAYER_HEADER"
                 << header->iProfile << header->iLevel
                 << header->iSizeInMemory.iWidth
                 << header->iSizeInMemory.iHeight
                 << header->iDisplayedRect.Width()
                 << header->iDisplayedRect.Height();
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "configure-before";
        TRAPD(configureError, devVideo->ConfigureDecoderL(*header));
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "configure-after"
                 << configureError;
        devVideo->ReturnHeader(header);
        if (configureError != KErrNone)
            User::Leave(configureError);

        RArray<TUncompressedVideoFormat> outputFormats;
        CleanupClosePushL(outputFormats);
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "output-list-before";
        TRAPD(outputListError,
            devVideo->GetOutputFormatListL(
                devVideoDecoderId, outputFormats));
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "output-list-after"
                 << outputListError << outputFormats.Count();
        if (outputListError != KErrNone) {
            CleanupStack::PopAndDestroy(&outputFormats);
            User::Leave(outputListError);
        }
        if (outputFormats.Count() == 0)
            User::Leave(KErrNotSupported);
        TInt selected = -1;
        TInt selectedScore = 100;
        TInt formatIndex;
        for (formatIndex = 0; formatIndex < outputFormats.Count();
             ++formatIndex) {
            const TUncompressedVideoFormat &candidate =
                outputFormats[formatIndex];
            TInt score = 100;
            if (candidate.iDataFormat == EYuvRawData) {
                const bool yuv420 =
                    candidate.iYuvFormat.iPattern == EYuv420Chroma1 ||
                    candidate.iYuvFormat.iPattern == EYuv420Chroma2 ||
                    candidate.iYuvFormat.iPattern == EYuv420Chroma3;
                if (yuv420 && candidate.iYuvFormat.iDataLayout ==
                    EYuvDataPlanar) {
                    score = 0;
                } else if (yuv420 && candidate.iYuvFormat.iDataLayout ==
                           EYuvDataSemiPlanar) {
                    score = 1;
                } else if (candidate.iYuvFormat.iPattern ==
                               EYuv422Chroma1 ||
                           candidate.iYuvFormat.iPattern ==
                               EYuv422Chroma2) {
                    score = 4;
                }
                qDebug() << "WW:DEVVIDEO_PLAYER_OUTPUT_OPTION"
                         << formatIndex
                         << static_cast<TUint32>(candidate.iDataFormat)
                         << static_cast<TUint32>(
                                candidate.iYuvFormat.iPattern)
                         << static_cast<TUint32>(
                                candidate.iYuvFormat.iDataLayout)
                         << score;
            } else if (candidate.iDataFormat == ERgbRawData) {
                if (candidate.iRgbFormat == ERgb16bit565)
                    score = 2;
                else if (candidate.iRgbFormat == ERgb32bit888)
                    score = 3;
                qDebug() << "WW:DEVVIDEO_PLAYER_OUTPUT_OPTION"
                         << formatIndex
                         << static_cast<TUint32>(candidate.iDataFormat)
                         << static_cast<TUint32>(candidate.iRgbFormat)
                         << score;
            }
            if (score < selectedScore) {
                selected = formatIndex;
                selectedScore = score;
            }
        }
        if (selected < 0 || selectedScore >= 100)
            User::Leave(KErrNotSupported);
        devVideoOutputFormat = outputFormats[selected];
        devVideoOutputValid = true;
        qDebug() << "WW:DEVVIDEO_PLAYER_OUTPUT_SELECTED"
                 << selected << selectedScore
                 << static_cast<TUint32>(
                        devVideoOutputFormat.iDataFormat);
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "output-set-before"
                 << selected;
        TRAPD(outputSetError,
            devVideo->SetOutputFormatL(
                devVideoDecoderId, devVideoOutputFormat));
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "output-set-after"
                 << outputSetError;
        CleanupStack::PopAndDestroy(&outputFormats);
        if (outputSetError != KErrNone)
            User::Leave(outputSetError);

        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "screen-dest-before";
        TRAPD(screenError, devVideo->SetVideoDestScreenL(EFalse));
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "screen-dest-after"
                 << screenError;
        if (screenError != KErrNone)
            User::Leave(screenError);
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "clock-set-before";
        devVideo->SetClockSource(devVideoClock);
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "clock-set-after";
        devVideo->SynchronizeDecoding(ETrue);
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "initialize-before";
        devVideo->Initialize();
        qDebug() << "WW:DEVVIDEO_DIAG_STAGE" << "initialize-sent";
    }

    bool startDevVideoPlayback(
        const QVector<QByteArray> &units,
        const QVector<qint64> &times,
        const QVector<qint64> &presentationTimes,
        int codedWidth,
        int codedHeight)
    {
        if (units.isEmpty() || units.size() != times.size() ||
            units.size() != presentationTimes.size()) {
            return false;
        }
        destroyDevVideoProbe();
        devVideoUnits = units;
        devVideoTimes = times;
        devVideoPresentationTimes = presentationTimes;
        devVideoCodedWidth = codedWidth;
        devVideoCodedHeight = codedHeight;
        devVideoSessionClock.start();
        // MMF's VideoEnabledL() is true even for the confirmed audio-only
        // controller result. Treat video as unavailable until DevVideo hands
        // us a real memory picture, so the 360p safety fallback remains live.
        videoAvailable = false;
        // The historical DevVideo experiment closed MMF before starting its
        // own decoder.  E7 memory-output diagnosis intentionally does the
        // opposite: MMF stays alive as AAC and the common playback clock, but
        // has had its video track disabled before Prepare().
#if defined(WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_DIAGNOSTIC) && \
    !defined(WILIWILI_ENABLE_E7_DEVVIDEO_MEMORY_SOLO_DIAGNOSTIC)
        qDebug() << "WW:E7_DEVVIDEO_MEMORY_MMF_RETAINED"
                 << static_cast<void *>(player)
                 << currentState << currentStatus << displayAdded
                 << audioPositionMilliseconds();
        logNativeAudioState("e7-devvideo-before");
#else
        qDebug() << "WW:E7_DEVVIDEO_MEMORY_MMF_ABSENT"
                 << static_cast<void *>(player)
                 << currentState << currentStatus << displayAdded;
        if (player) {
            qDebug() << "WW:DEVVIDEO_DIAG_MMF_CLOSE_BEFORE"
                     << currentState << currentStatus << displayAdded;
            closeMedia();
            User::After(200000);
            qDebug() << "WW:DEVVIDEO_DIAG_MMF_CLOSE_AFTER"
                     << currentState << currentStatus << displayAdded;
        }
#endif
        devVideoProbeState = 1;
        qDebug() << "WW:DEVVIDEO_PLAYER_BEGIN"
                 << devVideoUnits.size()
                 << devVideoUnits.first().size()
                  << devVideoCodedWidth << devVideoCodedHeight
                  << "BCM_0x10204C21"
                  << devVideoTimes.first()
                  << devVideoPresentationTimes.first();
        TRAPD(setupError, setupDevVideoProbeL());
        if (setupError != KErrNone) {
            destroyDevVideoProbe();
            devVideoProbeState = 4;
            devVideoProbeError = setupError;
            qDebug() << "WW:DEVVIDEO_PLAYER_SETUP_ERROR" << setupError;
            return false;
        }
        return true;
    }

    bool appendDevVideoPlayback(
        const QVector<QByteArray> &units,
        const QVector<qint64> &times,
        const QVector<qint64> &presentationTimes)
    {
        if (!devVideo || units.isEmpty() || units.size() != times.size() ||
            units.size() != presentationTimes.size() ||
            devVideoInputFinal || devVideoProbeState == 4) {
            return false;
        }
        devVideoUnits += units;
        devVideoTimes += times;
        devVideoPresentationTimes += presentationTimes;
        qDebug() << "WW:DEVVIDEO_PLAYER_APPEND"
                 << units.size()
                 << (devVideoUnits.size() - devVideoUnitIndex);
        feedDevVideoBuffers();
        return true;
    }

    qint64 audioPositionMilliseconds() const
    {
        if (!player)
            return 0;
        TTimeIntervalMicroSeconds value;
        TRAPD(error, value = player->PositionL());
        return error == KErrNone
            ? static_cast<qint64>(value.Int64()) / 1000 : 0;
    }

    void feedDevVideoBuffers()
    {
        if (!devVideo || !devVideoInitialized || devVideoInputEnded ||
            devVideoProbeState == 4 || devVideoPaused)
            return;
        int written = 0;
        while (devVideoUnitIndex < devVideoUnits.size() && written < 4) {
            const QByteArray &unit = devVideoUnits.at(devVideoUnitIndex);
            TVideoInputBuffer *buffer = 0;
            TRAPD(bufferError,
                buffer = devVideo->GetBufferL(
                    static_cast<TUint>(unit.size())));
            if (bufferError != KErrNone) {
                qDebug() << "WW:DEVVIDEO_PLAYER_BUFFER_ERROR"
                         << bufferError << devVideoUnitIndex;
                failDevVideoPlayback(bufferError, "buffer");
                return;
            }
            if (!buffer)
                break;
            buffer->iData.Copy(TPtrC8(
                reinterpret_cast<const TUint8 *>(unit.constData()),
                unit.size()));
            buffer->iOptions = TVideoInputBuffer::ESequenceNumber |
                TVideoInputBuffer::EDecodingTimestamp |
                TVideoInputBuffer::EPresentationTimestamp;
            buffer->iSequenceNumber = devVideoSequence++;
            const qint64 decodingTime =
                devVideoTimes.at(devVideoUnitIndex);
            buffer->iDecodingTimestamp = TTimeIntervalMicroSeconds(
                static_cast<TInt64>(decodingTime) * 1000);
            const qint64 presentationTime =
                devVideoPresentationTimes.at(devVideoUnitIndex);
            buffer->iPresentationTimestamp = TTimeIntervalMicroSeconds(
                static_cast<TInt64>(presentationTime) * 1000);
            buffer->iPreRoll = EFalse;
            buffer->iError = EFalse;
            TRAPD(writeError, devVideo->WriteCodedDataL(buffer));
            if (writeError != KErrNone) {
                qDebug() << "WW:DEVVIDEO_PLAYER_WRITE_ERROR"
                         << writeError << devVideoUnitIndex;
                failDevVideoPlayback(writeError, "write");
                return;
            }
            devVideoSubmittedTimes.append(presentationTime);
            ++devVideoUnitIndex;
            ++written;
        }
        // WriteCodedDataL receives a DevVideo-owned buffer containing a copy,
        // so the QByteArray source can be released as soon as it has been
        // submitted. Without compaction, a long progressive MP4 retained the
        // complete compressed video in RAM even though Range input is batched.
        if (devVideoUnitIndex > 0 &&
            (devVideoUnitIndex >= 60 ||
             devVideoUnitIndex == devVideoUnits.size())) {
            devVideoUnits.remove(0, devVideoUnitIndex);
            devVideoTimes.remove(0, devVideoUnitIndex);
            devVideoPresentationTimes.remove(0, devVideoUnitIndex);
            devVideoUnitIndex = 0;
        }
        if (devVideoInputFinal &&
            devVideoUnitIndex >= devVideoUnits.size() &&
            !devVideoInputEnded) {
            devVideoInputEnded = true;
            devVideo->InputEnd();
            qDebug() << "WW:DEVVIDEO_PLAYER_INPUT_END"
                     << devVideoUnitIndex << devVideoSequence;
        }
    }

    // MMMFDevVideoPlayObserver. Pictures are copied into the persistent ARGB
    // player layer and returned immediately so the hardware decoder can reuse
    // its small output pool.
    virtual void MdvpoNewBuffers()
    {
        feedDevVideoBuffers();
    }

    virtual void MdvpoReturnPicture(TVideoPicture *) {}

    virtual void MdvpoSupplementalInformation(
        const TDesC8 &,
        const TTimeIntervalMicroSeconds &,
        const TPictureId &) {}

    virtual void MdvpoPictureLoss()
    {
        qDebug() << "WW:DEVVIDEO_PLAYER_PICTURE_LOSS";
    }

    virtual void MdvpoPictureLoss(const TArray<TPictureId> &pictures)
    {
        qDebug() << "WW:DEVVIDEO_PLAYER_PICTURE_LOSS_LIST"
                 << pictures.Count();
    }

    virtual void MdvpoSliceLoss(
        TUint firstMacroblock,
        TUint macroblocks,
        const TPictureId &)
    {
        qDebug() << "WW:DEVVIDEO_PLAYER_SLICE_LOSS"
                 << firstMacroblock << macroblocks;
    }

    virtual void MdvpoReferencePictureSelection(const TDesC8 &) {}

    virtual void MdvpoTimedSnapshotComplete(
        TInt,
        TPictureData *,
        const TTimeIntervalMicroSeconds &,
        const TPictureId &) {}

    virtual void MdvpoNewPictures()
    {
        if (!devVideo)
            return;
        TUint available = 0;
        TTimeIntervalMicroSeconds earliest;
        TTimeIntervalMicroSeconds latest;
        devVideo->GetNewPictureInfo(available, earliest, latest);
        TUint index;
        for (index = 0; index < available; ++index) {
            TVideoPicture *picture = 0;
            TRAPD(error, picture = devVideo->NextPictureL());
            if (error != KErrNone || !picture) {
                qDebug() << "WW:DEVVIDEO_PLAYER_PICTURE_ERROR" << error;
                break;
            }
            ++devVideoProbePictures;
            QImage converted;
            const bool frameDue = !devVideoFrameClock.isValid() ||
                devVideoFrameClock.elapsed() >= 45;
            if (frameDue && devVideoOutputValid &&
                copyDevVideoPicture(
                    *picture, devVideoOutputFormat, &converted)) {
                devVideoFrame = converted;
                devVideoFrameClock.restart();
                if ((picture->iOptions & TVideoPicture::ETimestamp) != 0) {
                    devVideoFrameTimestamp = static_cast<qint64>(
                        picture->iTimestamp.Int64()) / 1000;
                } else if (devVideoProbePictures - 1 <
                           devVideoSubmittedTimes.size()) {
                    devVideoFrameTimestamp = devVideoSubmittedTimes.at(
                        devVideoProbePictures - 1);
                }
                ++devVideoFrameSerial;
                videoAvailable = true;
            }
            if (devVideoProbePictures == 1) {
                const TInt dataBytes = picture->iData.iRawData
                    ? picture->iData.iRawData->Length() : 0;
                qDebug() << "WW:DEVVIDEO_PLAYER_FIRST_PICTURE"
                         << picture->iData.iDataSize.iWidth
                         << picture->iData.iDataSize.iHeight
                         << static_cast<TUint32>(picture->iData.iDataFormat)
                         << dataBytes
                         << picture->iCropRect.Width()
                         << picture->iCropRect.Height()
                         << !devVideoFrame.isNull()
                         << devVideoFrame.size()
                         << (devVideoSessionClock.isValid()
                             ? devVideoSessionClock.elapsed() : -1)
                         << devVideoFrameTimestamp
                         << audioPositionMilliseconds();
            }
            devVideo->ReturnPicture(picture);
        }
        if (devVideoProbePictures > 0)
            devVideoProbeState = 3;
    }

    virtual void MdvpoFatalError(TInt error)
    {
        qDebug() << "WW:DEVVIDEO_PLAYER_FATAL"
                 << error << devVideoProbePictures << devVideoUnitIndex;
        failDevVideoPlayback(error, "callback");
    }

    virtual void MdvpoInitComplete(TInt error)
    {
        qDebug() << "WW:DEVVIDEO_PLAYER_INIT" << error;
        if (error != KErrNone || !devVideo) {
            failDevVideoPlayback(
                error == KErrNone ? KErrNotReady : error, "init");
            return;
        }
        devVideoInitialized = true;
        devVideoProbeState = 2;
        if (devVideoClock) {
            devVideoClock->Reset(TTimeIntervalMicroSeconds(
                static_cast<TInt64>(audioPositionMilliseconds()) * 1000));
        }
        devVideo->Start();
        if (currentState == PausedState) {
            devVideo->Pause();
            if (devVideoClock)
                devVideoClock->Suspend();
            devVideoPaused = true;
        }
        feedDevVideoBuffers();
    }

    virtual void MdvpoStreamEnd()
    {
        if (!devVideo)
            return;
        CMMFDevVideoPlay::TPictureCounters counters;
        devVideo->GetPictureCounters(counters);
        devVideoProbeState = 5;
        qDebug() << "WW:DEVVIDEO_PLAYER_RESULT"
                 << devVideoProbeState
                 << devVideoProbePictures
                 << counters.iPicturesDecoded
                 << counters.iPicturesDisplayed
                 << counters.iPicturesSkipped
                 << counters.iTotalPictures;
    }
#else
    void closeMedia()
    {
        resetState();
    }

    void destroyNativePlayer()
    {
        resetState();
    }

    void destroyDevVideoProbe() {}

    bool startDevVideoPlayback(
        const QVector<QByteArray> &,
        const QVector<qint64> &,
        const QVector<qint64> &,
        int,
        int)
    {
        return false;
    }

    bool appendDevVideoPlayback(
        const QVector<QByteArray> &,
        const QVector<qint64> &,
        const QVector<qint64> &)
    {
        return false;
    }

    void feedDevVideoBuffers() {}
#endif

    QWidget *host;
    State currentState;
    MediaStatus currentStatus;
    int currentError;
    QString currentErrorText;
    qint64 currentDuration;
    int currentVolume;
    int mediaSessionSerial;
    qreal requestedRate;
    bool playWhenReady;
    bool videoAvailable;
    bool nativeVideoEnabledRequested;
    bool nativeVideoPolicyApplied;
    int nativeVideoPolicyError;
    int devVideoProbeState;
    int devVideoProbeError;
    int devVideoProbePictures;
    QImage devVideoFrame;
    QTime devVideoFrameClock;
    QTime devVideoSessionClock;
    qint64 devVideoFrameTimestamp;
    int devVideoFrameSerial;
    int devVideoCodedWidth;
    int devVideoCodedHeight;
    bool devVideoYuv420Output;

#ifdef Q_OS_SYMBIAN
    RFs sharedFileSession;
    RFile sharedMediaFile;
    bool sharedFileSessionConnected;
    bool sharedMediaFileOpen;
#ifdef WILIWILI_ENABLE_E7_MMF_LEGACY_UTILITY_DIAGNOSTIC
    CVideoPlayerUtility *player;
#else
    CVideoPlayerUtility2 *player;
#endif
    RWindow *displayWindow;
    bool displayAdded;
    bool geometryApplied;
    TRect lastGeometryRect;
    bool rotationApplied;
    bool scaleApplied;
#ifdef WILIWILI_ENABLE_E7_MMF_GRAPHICS_SURFACE_DIAGNOSTIC
    bool surfaceDisplayAdded;
    bool surfaceBackgroundAttached;
    TInt surfaceDisplayId;
    TSurfaceId surfaceId;
#endif
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
    FfmpegH264Decoder *ffmpegDecoder;
#endif
    CMMFDevVideoPlay *devVideo;
    CSystemClockSource *devVideoClock;
    THwDeviceId devVideoDecoderId;
    QVector<QByteArray> devVideoUnits;
    QVector<qint64> devVideoTimes;
    QVector<qint64> devVideoPresentationTimes;
    QVector<qint64> devVideoSubmittedTimes;
    int devVideoUnitIndex;
    TUint devVideoSequence;
    bool devVideoInitialized;
    bool devVideoInputEnded;
    bool devVideoInputFinal;
    TUncompressedVideoFormat devVideoOutputFormat;
    bool devVideoOutputValid;
    bool devVideoPaused;
#endif
};

VideoPlaybackBackend::VideoPlaybackBackend(QWidget *videoHost)
    : m_impl(new Impl(videoHost))
{
}

VideoPlaybackBackend::~VideoPlaybackBackend()
{
    delete m_impl;
}

void VideoPlaybackBackend::setMedia(
    const QMediaContent &media,
    const QByteArray &mimeType)
{
    const QUrl url = media.canonicalUrl();
    qDebug() << "WW:NATIVE_MMF_STAGE" << 40 << "set-media-enter"
             << url.scheme();
    // Keep the observer and CVideoPlayerUtility2 object alive for the whole
    // application lifetime. On Symbian, deleting the MMF observer immediately
    // after leaving playback can leave already-queued framework callbacks
    // targeting freed memory. Close only the selected controller/media here;
    // ensureNativePlayer() below reuses the utility with the persistent native
    // QWidget/RWindow.
    m_impl->destroyDevVideoProbe();
    m_impl->closeMedia();
    if (url.isEmpty())
        return;
#ifdef Q_OS_SYMBIAN
    ++m_impl->mediaSessionSerial;
    qDebug() << "WW:NATIVE_MMF_SESSION"
             << m_impl->mediaSessionSerial
             << "volume-percent" << m_impl->currentVolume;
    qDebug() << "WW:NATIVE_MMF_STAGE" << 41 << "ensure-player-before";
    if (!m_impl->ensureNativePlayer())
        return;
    qDebug() << "WW:NATIVE_MMF_STAGE" << 42 << "ensure-player-after";
    m_impl->currentStatus = LoadingMedia;
    m_impl->currentError = NoError;
    m_impl->currentErrorText.clear();
    const bool localFile =
        url.scheme().compare(QString::fromLatin1("file"), Qt::CaseInsensitive) == 0;
    const QString value = localFile
        ? QDir::toNativeSeparators(url.toLocalFile())
        : url.toString();
    const TPtrC descriptor(
        reinterpret_cast<const TUint16 *>(value.utf16()), value.length());
#ifdef WILIWILI_ENABLE_E7_MMF_HELIX_DIAGNOSTIC
    // This is the UID hard-coded by Qt Mobility's public S60 MMF player
    // backend.  CVideoPlayerUtility's optional controller parameter is the
    // supported way to request it; KNullUid retains NIKINIKI's normal resolver
    // behaviour in every other build.
    const TUid controllerUid = TUid::Uid(0x101F8514);
    qDebug() << "WW:E7_MMF_HELIX_CONTROLLER_REQUEST"
             << controllerUid.iUid << "local" << localFile;
#else
    const TUid controllerUid = KNullUid;
#endif
    TInt openError = KErrNone;
    if (localFile) {
        qDebug() << "WW:NATIVE_MMF_STAGE" << 50 << "open-file-before";
#ifdef WILIWILI_ENABLE_E7_MMF_PATH_OPEN_DIAGNOSTIC
        // A fully downloaded MP4 need not remain open for a writer.  Match
        // the normal system-player input form and retain the filename for the
        // controller resolver while diagnosing E7 partial playback.
        qDebug() << "WW:NATIVE_MMF_FILE_PATH" << "complete-local";
        TRAP(openError,
             m_impl->player->OpenFileL(descriptor, controllerUid));
#else
        openError = m_impl->openSharedMediaFile(descriptor);
        qDebug() << "WW:NATIVE_MMF_FILE_HANDLE"
                 << "share-read-write" << openError;
        if (openError == KErrNone) {
            TRAP(openError,
                 m_impl->player->OpenFileL(
                     m_impl->sharedMediaFile, controllerUid));
        }
#endif
    } else {
        qDebug() << "WW:NATIVE_MMF_STAGE" << 50 << "open-url-before";
        const TPtrC8 mimeDescriptor(
            reinterpret_cast<const TUint8 *>(mimeType.constData()),
            mimeType.size());
        TRAP(openError,
            m_impl->player->OpenUrlL(
                descriptor, KUseDefaultIap, mimeDescriptor, controllerUid));
    }
    qDebug() << "WW:NATIVE_MMF_STAGE" << 51 << "open-call-after"
             << openError;
    qDebug() << "WW:NATIVE_MMF_OPEN"
             << localFile << url.scheme() << mimeType << openError;
    if (openError != KErrNone)
        m_impl->fail(openError, "open-call");
#else
    Q_UNUSED(url);
    Q_UNUSED(mimeType);
    m_impl->fail(-5, "unsupported-platform");
#endif
}

void VideoPlaybackBackend::clearMedia()
{
    m_impl->destroyDevVideoProbe();
    m_impl->closeMedia();
}

void VideoPlaybackBackend::play()
{
#ifdef Q_OS_SYMBIAN
    if (m_impl->currentStatus == LoadedMedia) {
        m_impl->startPlayback();
    } else if (m_impl->currentStatus == LoadingMedia) {
        m_impl->playWhenReady = true;
    }
    if (m_impl->devVideo && m_impl->devVideoInitialized &&
        m_impl->devVideoPaused) {
        if (m_impl->devVideoClock)
            m_impl->devVideoClock->Resume();
        m_impl->devVideo->Resume();
        m_impl->devVideoPaused = false;
        m_impl->feedDevVideoBuffers();
    }
#endif
}

void VideoPlaybackBackend::pause()
{
#ifdef Q_OS_SYMBIAN
    if (!m_impl->player || m_impl->currentState != PlayingState)
        return;
    TRAPD(error, m_impl->player->PauseL());
    if (error == KErrNone) {
        m_impl->currentState = PausedState;
        if (m_impl->devVideo && m_impl->devVideoInitialized &&
            !m_impl->devVideoPaused) {
            m_impl->devVideo->Pause();
            if (m_impl->devVideoClock)
                m_impl->devVideoClock->Suspend();
            m_impl->devVideoPaused = true;
        }
    } else {
        qDebug() << "WW:NATIVE_MMF_PAUSE_ERROR" << error;
    }
#endif
}

void VideoPlaybackBackend::stop()
{
#ifdef Q_OS_SYMBIAN
    if (m_impl->player)
        m_impl->player->Stop();
#endif
    m_impl->currentState = StoppedState;
    m_impl->playWhenReady = false;
}

void VideoPlaybackBackend::setPosition(qint64 milliseconds)
{
#ifdef Q_OS_SYMBIAN
    if (!m_impl->player)
        return;
    const TInt64 micros = static_cast<TInt64>(qMax<qint64>(0, milliseconds)) * 1000;
    TRAPD(error,
        m_impl->player->SetPositionL(TTimeIntervalMicroSeconds(micros)));
    if (error != KErrNone)
        qDebug() << "WW:NATIVE_MMF_SEEK_ERROR" << error;
#else
    Q_UNUSED(milliseconds);
#endif
}

qint64 VideoPlaybackBackend::position() const
{
#ifdef Q_OS_SYMBIAN
    if (!m_impl->player)
        return 0;
    TTimeIntervalMicroSeconds value;
    TRAPD(error, value = m_impl->player->PositionL());
    if (error == KErrNone)
        return static_cast<qint64>(value.Int64()) / 1000;
#endif
    return 0;
}

qint64 VideoPlaybackBackend::duration() const
{
    return m_impl->currentDuration;
}

void VideoPlaybackBackend::setPlaybackRate(qreal rate)
{
    m_impl->requestedRate = qBound<qreal>(0.5, rate, 2.0);
#ifdef Q_OS_SYMBIAN
    if (m_impl->player && m_impl->currentStatus == LoadedMedia) {
        TRAPD(error, m_impl->player->SetPlayVelocityL(
            static_cast<TInt>(m_impl->requestedRate * 100.0)));
        if (error != KErrNone)
            qDebug() << "WW:NATIVE_MMF_RATE_ERROR" << error;
    }
#endif
}

qreal VideoPlaybackBackend::playbackRate() const
{
    return m_impl->requestedRate;
}

void VideoPlaybackBackend::setVolume(int volume)
{
    m_impl->currentVolume = qBound(0, volume, 100);
#ifdef Q_OS_SYMBIAN
    if (m_impl->player) {
        m_impl->applyNativeVolume("user");
    }
#endif
}

int VideoPlaybackBackend::volume() const
{
    return m_impl->currentVolume;
}

VideoPlaybackBackend::State VideoPlaybackBackend::state() const
{
    return m_impl->currentState;
}

VideoPlaybackBackend::MediaStatus VideoPlaybackBackend::mediaStatus() const
{
    return m_impl->currentStatus;
}

int VideoPlaybackBackend::bufferStatus() const
{
    return m_impl->currentStatus == LoadedMedia ? 100 : 0;
}

int VideoPlaybackBackend::error() const
{
    return m_impl->currentError;
}

QString VideoPlaybackBackend::errorString() const
{
    return m_impl->currentErrorText;
}

bool VideoPlaybackBackend::isVideoAvailable() const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    if (m_impl->ffmpegDecoder && m_impl->ffmpegDecoder->pictureCount() > 0)
        return true;
#endif
    return m_impl->videoAvailable;
}

void VideoPlaybackBackend::setNativeVideoEnabled(bool enabled)
{
    m_impl->nativeVideoEnabledRequested = enabled;
    m_impl->nativeVideoPolicyApplied = enabled;
    m_impl->nativeVideoPolicyError = 0;
#ifdef Q_OS_SYMBIAN
    if (m_impl->currentStatus == LoadedMedia) {
        TBool nativeVideoEnabled = ETrue;
        TRAPD(videoEnabledError,
              nativeVideoEnabled = m_impl->player->VideoEnabledL());
        if (videoEnabledError != KErrNone)
            nativeVideoEnabled = ETrue;
        m_impl->applyNativeVideoPolicy(&nativeVideoEnabled);
        if (!enabled && m_impl->nativeVideoPolicyApplied)
            m_impl->videoAvailable = false;
    }
#endif
}

bool VideoPlaybackBackend::isNativeVideoPolicyApplied() const
{
    return m_impl->nativeVideoPolicyApplied;
}

VideoPlaybackBackend::AvcHeaderProbeResult
VideoPlaybackBackend::probeAvcHardwareHeader(
    const QByteArray &annexBAccessUnit,
    int *errorCode)
{
#ifdef Q_OS_SYMBIAN
    return probeBroadcomAvcHeader(annexBAccessUnit, errorCode);
#else
    Q_UNUSED(annexBAccessUnit);
    if (errorCode)
        *errorCode = -1;
    return AvcHeaderProbeRejected;
#endif
}

bool VideoPlaybackBackend::startAvcHardwarePlayback(
    const QVector<QByteArray> &accessUnits,
    const QVector<qint64> &decodingTimesMilliseconds,
    const QVector<qint64> &presentationTimesMilliseconds,
    int codedWidth,
    int codedHeight)
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    return m_impl->startFfmpegPlayback(
        accessUnits, decodingTimesMilliseconds,
        presentationTimesMilliseconds, codedWidth, codedHeight);
#else
    return m_impl->startDevVideoPlayback(
        accessUnits, decodingTimesMilliseconds,
        presentationTimesMilliseconds, codedWidth, codedHeight);
#endif
}

bool VideoPlaybackBackend::appendAvcHardwarePlayback(
    const QVector<QByteArray> &accessUnits,
    const QVector<qint64> &decodingTimesMilliseconds,
    const QVector<qint64> &presentationTimesMilliseconds)
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    return m_impl->appendFfmpegPlayback(
        accessUnits, decodingTimesMilliseconds,
        presentationTimesMilliseconds);
#else
    return m_impl->appendDevVideoPlayback(
        accessUnits, decodingTimesMilliseconds,
        presentationTimesMilliseconds);
#endif
}

void VideoPlaybackBackend::finishAvcHardwareInput()
{
#ifdef Q_OS_SYMBIAN
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
    if (m_impl->ffmpegDecoder)
        m_impl->ffmpegDecoder->finishInput();
#else
    m_impl->devVideoInputFinal = true;
    m_impl->feedDevVideoBuffers();
#endif
#endif
}

void VideoPlaybackBackend::stopAvcHardwarePlayback()
{
    m_impl->destroyDevVideoProbe();
}

void VideoPlaybackBackend::pumpAvcHardwarePlayback()
{
#ifdef Q_OS_SYMBIAN
#ifndef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
    if (m_impl->devVideoClock && m_impl->devVideoInitialized &&
        !m_impl->devVideoPaused) {
        const qint64 audioTime = m_impl->audioPositionMilliseconds();
        const qint64 videoTime = static_cast<qint64>(
            m_impl->devVideoClock->Time().Int64()) / 1000;
        if (qAbs(audioTime - videoTime) > 150) {
            m_impl->devVideoClock->Reset(TTimeIntervalMicroSeconds(
                static_cast<TInt64>(audioTime) * 1000));
            qDebug() << "WW:DEVVIDEO_PLAYER_CLOCK_RESYNC"
                     << audioTime << videoTime;
        }
    }
#endif
#endif
#ifndef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
    m_impl->feedDevVideoBuffers();
#endif
}

bool VideoPlaybackBackend::isAvcHardwarePlaybackActive() const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    return m_impl->ffmpegDecoder && m_impl->ffmpegDecoder->isActive();
#else
    return m_impl->devVideoProbeState == 1 ||
           m_impl->devVideoProbeState == 2 ||
           m_impl->devVideoProbeState == 3;
#endif
}

int VideoPlaybackBackend::avcHardwareBufferedUnitCount() const
{
#ifdef Q_OS_SYMBIAN
#ifdef WILIWILI_ENABLE_FFMPEG_SOFT_DECODER
    return m_impl->ffmpegDecoder
        ? m_impl->ffmpegDecoder->bufferedUnitCount() : 0;
#else
    return qMax(0,
        m_impl->devVideoUnits.size() - m_impl->devVideoUnitIndex);
#endif
#else
    return 0;
#endif
}

int VideoPlaybackBackend::avcHardwarePictureCount() const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    return m_impl->ffmpegDecoder ? m_impl->ffmpegDecoder->pictureCount() : 0;
#else
    return m_impl->devVideoProbePictures;
#endif
}

int VideoPlaybackBackend::avcHardwareError() const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    return m_impl->ffmpegDecoder ? m_impl->ffmpegDecoder->error()
                                 : m_impl->devVideoProbeError;
#else
    return m_impl->devVideoProbeError;
#endif
}

void VideoPlaybackBackend::setAvcHardwareYuv420OutputEnabled(bool enabled)
{
    m_impl->devVideoYuv420Output = enabled;
}

bool VideoPlaybackBackend::takeAvcHardwareFrame(
    QImage *frame,
    qint64 *timestampMilliseconds,
    int *serial) const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    // Preserve short-circuiting for ordinary MMF playback: when no software
    // decoder exists, do not perform an unnecessary synchronous PositionL().
    return m_impl->ffmpegDecoder &&
        m_impl->ffmpegDecoder->takeFrame(
            m_impl->audioPositionMilliseconds(), frame,
            timestampMilliseconds, serial);
#else
    if (!frame || !serial || m_impl->devVideoFrame.isNull() ||
        *serial == m_impl->devVideoFrameSerial) {
        return false;
    }
    *frame = m_impl->devVideoFrame;
    if (timestampMilliseconds)
        *timestampMilliseconds = m_impl->devVideoFrameTimestamp;
    *serial = m_impl->devVideoFrameSerial;
    return true;
#endif
}

bool VideoPlaybackBackend::takeAvcHardwareFrameAt(
    qint64 audioPositionMilliseconds,
    QImage *frame,
    qint64 *timestampMilliseconds,
    int *serial) const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    return m_impl->ffmpegDecoder &&
        m_impl->ffmpegDecoder->takeFrame(
            audioPositionMilliseconds, frame,
            timestampMilliseconds, serial);
#else
    Q_UNUSED(audioPositionMilliseconds);
    if (!frame || !serial || m_impl->devVideoFrame.isNull() ||
        *serial == m_impl->devVideoFrameSerial) {
        return false;
    }
    *frame = m_impl->devVideoFrame;
    if (timestampMilliseconds)
        *timestampMilliseconds = m_impl->devVideoFrameTimestamp;
    *serial = m_impl->devVideoFrameSerial;
    return true;
#endif
}

bool VideoPlaybackBackend::takeAvcHardwareYuv420Frame(
    Yuv420Frame *frame,
    int *serial) const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    return m_impl->ffmpegDecoder &&
        m_impl->ffmpegDecoder->takeYuv420Frame(
            m_impl->audioPositionMilliseconds(), frame, serial);
#else
    Q_UNUSED(frame);
    Q_UNUSED(serial);
    return false;
#endif
}

bool VideoPlaybackBackend::takeAvcHardwareYuv420FrameAt(
    qint64 audioPositionMilliseconds,
    Yuv420Frame *frame,
    int *serial) const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    return m_impl->ffmpegDecoder &&
        m_impl->ffmpegDecoder->takeYuv420Frame(
            audioPositionMilliseconds, frame, serial);
#else
    Q_UNUSED(audioPositionMilliseconds);
    Q_UNUSED(frame);
    Q_UNUSED(serial);
    return false;
#endif
}

QString VideoPlaybackBackend::softPlaybackTelemetry(
    quint64 presentedCount,
    quint64 uploadedCount,
    qint64 uploadMilliseconds,
    qint64 lastPresentedPts) const
{
#if defined(Q_OS_SYMBIAN) && defined(WILIWILI_ENABLE_FFMPEG_SOFT_DECODER)
    if (!m_impl->ffmpegDecoder)
        return QString();
    return m_impl->ffmpegDecoder->telemetry(
        m_impl->audioPositionMilliseconds(), presentedCount,
        uploadedCount, uploadMilliseconds, lastPresentedPts);
#else
    Q_UNUSED(presentedCount);
    Q_UNUSED(uploadedCount);
    Q_UNUSED(uploadMilliseconds);
    Q_UNUSED(lastPresentedPts);
    return QString();
#endif
}

void VideoPlaybackBackend::updateVideoWindow()
{
#ifdef Q_OS_SYMBIAN
    m_impl->updateWindow();
#endif
}

} // namespace wiliwili
