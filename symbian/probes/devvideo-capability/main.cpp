#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtGui/QApplication>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

#ifdef Q_OS_SYMBIAN
#include <e32base.h>
#include <hash.h>
#include <mmf/devvideo/devvideoplay.h>
#else
#error This research probe must only be built for Symbian.
#endif

namespace {

const TInt KNotRun = -1000001;
const TInt KPending = -1000002;
const TInt KInitializeTimeoutMs = 10000;
const TInt KDecodeTimeoutMs = 20000;
const TInt KMaximumAccessUnits = 100;
const TUid KBroadcomDecoderUid = { 0x10204C21 };

struct CaseSpec
{
    CaseSpec()
        : h264Size(0), profile(0), level(0), refs(0), dpb(0), reorder(0),
          weightP(0), weightB(0), accessUnits(0), eosNal(0),
          runtimeReady(false), headerH264Size(0), headerRefs(0)
    {
    }

    QString id;
    QString group;
    QString h264File;
    qint64 h264Size;
    QByteArray h264Sha256;
    QByteArray goldenSha256;
    TInt profile;
    TInt level;
    TInt refs;
    TInt dpb;
    TInt reorder;
    TInt weightP;
    TInt weightB;
    TInt accessUnits;
    TInt eosNal;
    bool runtimeReady;
    QString headerH264File;
    qint64 headerH264Size;
    QByteArray headerH264Sha256;
    TInt headerRefs;
};

struct CaseResult
{
    CaseResult()
    {
        reset();
    }

    void reset()
    {
        fileError = KNotRun;
        hashError = KNotRun;
        headerFileError = KNotRun;
        headerHashError = KNotRun;
        createError = KNotRun;
        infoError = KNotRun;
        selectError = KNotRun;
        inputError = KNotRun;
        headerError = KNotRun;
        configureError = KNotRun;
        outputListError = KNotRun;
        outputSetError = KNotRun;
        destinationError = KNotRun;
        initializeError = KNotRun;
        bufferError = KNotRun;
        writeError = KNotRun;
        firstPictureError = KNotRun;
        accelerated = false;
        directDisplay = false;
        headerWidth = 0;
        headerHeight = 0;
        outputFormats = 0;
        outputIndex = -1;
        outputDataFormat = 0;
        outputPattern = 0;
        outputLayout = 0;
        accessUnitsParsed = 0;
        accessUnitsTarget = 0;
        accessUnitsWritten = 0;
        eosNalCount = 0;
        picturesOutput = 0;
        pictureLoss = 0;
        sliceLoss = 0;
        fatalError = KNotRun;
        streamEnd = false;
        firstPictureBytes = 0;
        firstPictureWidth = 0;
        firstPictureHeight = 0;
        firstPictureFormat = 0;
        firstPictureCrc = 0;
        rollingCrc = 0;
        picturesDecoded = 0;
        picturesDisplayed = 0;
        picturesSkipped = 0;
        picturesTotal = 0;
        packetsLost = 0;
        packetsTotal = 0;
        preDecodeBufferSize = 0;
        maxPostDecodeBufferSize = 0;
        maxInputBufferSize = 0;
        minInputBuffers = 0;
        finishReason.clear();
    }

    TInt fileError;
    TInt hashError;
    TInt headerFileError;
    TInt headerHashError;
    TInt createError;
    TInt infoError;
    TInt selectError;
    TInt inputError;
    TInt headerError;
    TInt configureError;
    TInt outputListError;
    TInt outputSetError;
    TInt destinationError;
    TInt initializeError;
    TInt bufferError;
    TInt writeError;
    TInt firstPictureError;
    bool accelerated;
    bool directDisplay;
    TInt headerWidth;
    TInt headerHeight;
    TInt outputFormats;
    TInt outputIndex;
    TUint32 outputDataFormat;
    TUint32 outputPattern;
    TUint32 outputLayout;
    TInt accessUnitsParsed;
    TInt accessUnitsTarget;
    TInt accessUnitsWritten;
    TInt eosNalCount;
    TInt picturesOutput;
    TInt pictureLoss;
    TInt sliceLoss;
    TInt fatalError;
    bool streamEnd;
    TInt firstPictureBytes;
    TInt firstPictureWidth;
    TInt firstPictureHeight;
    TUint32 firstPictureFormat;
    quint32 firstPictureCrc;
    quint32 rollingCrc;
    TUint picturesDecoded;
    TUint picturesDisplayed;
    TUint picturesSkipped;
    TUint picturesTotal;
    TUint packetsLost;
    TUint packetsTotal;
    TUint preDecodeBufferSize;
    TUint maxPostDecodeBufferSize;
    TUint maxInputBufferSize;
    TUint minInputBuffers;
    QString finishReason;
};

static QString descriptorText(const TDesC &value)
{
    return QString::fromUtf16(
        reinterpret_cast<const ushort *>(value.Ptr()), value.Length());
}

static QString stageText(TInt error)
{
    if (error == KNotRun)
        return QString::fromLatin1("NOT_RUN");
    if (error == KPending)
        return QString::fromLatin1("PENDING");
    if (error == KErrNone)
        return QString::fromLatin1("OK");
    return QString::number(error);
}

static bool stageFailed(TInt error)
{
    return error != KNotRun && error != KPending && error != KErrNone;
}

static QString crcText(quint32 value)
{
    return QString::fromLatin1("%1").arg(value, 8, 16, QLatin1Char('0')).toUpper();
}

static quint32 crc32Update(quint32 state, const unsigned char *data, int size)
{
    quint32 crc = state;
    int index;
    for (index = 0; index < size; ++index) {
        crc ^= data[index];
        int bit;
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0U);
    }
    return crc;
}

static QByteArray sha256File(const QString &path, TInt *error)
{
    if (error)
        *error = KErrNone;
    CSHA2 *digest = 0;
    TRAPD(createError, digest = CSHA2::NewL(E256Bit));
    if (createError != KErrNone || !digest) {
        if (error)
            *error = createError != KErrNone ? createError : KErrNoMemory;
        return QByteArray();
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        delete digest;
        if (error)
            *error = KErrNotFound;
        return QByteArray();
    }

    while (!file.atEnd()) {
        const QByteArray chunk = file.read(64 * 1024);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            file.close();
            delete digest;
            if (error)
                *error = KErrGeneral;
            return QByteArray();
        }
        if (!chunk.isEmpty()) {
            const TPtrC8 view(
                reinterpret_cast<const TUint8 *>(chunk.constData()),
                chunk.size());
            digest->Update(view);
        }
    }
    file.close();

    const TPtrC8 result = digest->Final();
    const QByteArray bytes(
        reinterpret_cast<const char *>(result.Ptr()), result.Length());
    delete digest;
    return bytes.toHex().toUpper();
}

static CVideoDecoderInfo *takeDecoderInfoL(CMMFDevVideoPlay *devVideo)
{
    CVideoDecoderInfo *info = devVideo->VideoDecoderInfoLC(KBroadcomDecoderUid);
    CleanupStack::Pop(info);
    return info;
}

static int startCodeLength(const QByteArray &data, int offset)
{
    if (offset + 4 <= data.size() && data.at(offset) == 0 &&
        data.at(offset + 1) == 0 && data.at(offset + 2) == 0 &&
        data.at(offset + 3) == 1) {
        return 4;
    }
    if (offset + 3 <= data.size() && data.at(offset) == 0 &&
        data.at(offset + 1) == 0 && data.at(offset + 2) == 1) {
        return 3;
    }
    return 0;
}

static int countNalType(const QByteArray &data, int expectedType)
{
    int count = 0;
    int offset = 0;
    while (offset + 3 <= data.size()) {
        const int prefix = startCodeLength(data, offset);
        if (prefix == 0) {
            ++offset;
            continue;
        }
        if (offset + prefix >= data.size())
            break;
        const int nalType = static_cast<unsigned char>(
            data.at(offset + prefix)) & 31;
        if (nalType == expectedType)
            ++count;
        offset += prefix;
    }
    return count;
}

static bool validCaseId(const QString &value)
{
    if (value.isEmpty() || value.size() > 32)
        return false;
    int index;
    for (index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (!character.isLetterOrNumber() && character != QLatin1Char('_') &&
            character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

static bool validLocalFileName(const QString &value)
{
    return !value.isEmpty() && QFileInfo(value).fileName() == value &&
        !value.contains(QLatin1Char(':')) &&
        !value.contains(QString::fromLatin1(".."));
}

static bool splitAudAccessUnits(
    const QByteArray &data,
    QVector<QByteArray> *units,
    QString *errorText)
{
    if (!units)
        return false;
    units->clear();
    QVector<int> audOffsets;
    int offset = 0;
    while (offset + 3 <= data.size()) {
        const int prefix = startCodeLength(data, offset);
        if (prefix == 0) {
            ++offset;
            continue;
        }
        if (offset + prefix >= data.size())
            break;
        const int nalType = static_cast<unsigned char>(data.at(offset + prefix)) & 31;
        if (nalType == 9)
            audOffsets.append(offset);
        offset += prefix;
    }
    if (audOffsets.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("No AUD NAL unit found");
        return false;
    }
    int index;
    for (index = 0; index < audOffsets.size(); ++index) {
        const int begin = index == 0 ? 0 : audOffsets.at(index);
        const int end = index + 1 < audOffsets.size()
            ? audOffsets.at(index + 1) : data.size();
        if (end <= begin) {
            if (errorText)
                *errorText = QString::fromLatin1("Invalid AUD boundary");
            units->clear();
            return false;
        }
        units->append(data.mid(begin, end - begin));
    }
    return true;
}

static TInt outputScore(const TUncompressedVideoFormat &format)
{
    if (format.iDataFormat == EYuvRawData) {
        const bool yuv420 = format.iYuvFormat.iPattern == EYuv420Chroma1 ||
            format.iYuvFormat.iPattern == EYuv420Chroma2 ||
            format.iYuvFormat.iPattern == EYuv420Chroma3;
        if (yuv420 && format.iYuvFormat.iDataLayout == EYuvDataPlanar)
            return 0;
        if (yuv420 && format.iYuvFormat.iDataLayout == EYuvDataSemiPlanar)
            return 1;
        if (format.iYuvFormat.iPattern == EYuv422Chroma1 ||
            format.iYuvFormat.iPattern == EYuv422Chroma2) {
            return 4;
        }
    } else if (format.iDataFormat == ERgbRawData) {
        if (format.iRgbFormat == ERgb16bit565)
            return 2;
        if (format.iRgbFormat == ERgb32bit888)
            return 3;
    }
    return 100;
}

} // namespace

class DevVideoCapabilityProbe : public QWidget,
                                public MMMFDevVideoPlayObserver
{
    Q_OBJECT

public:
    DevVideoCapabilityProbe()
        : QWidget(0), m_status(0), m_startButton(0), m_devVideo(0),
          m_decoderId(0), m_caseIndex(-1), m_schema(0), m_stage(Idle),
          m_nextUnit(0), m_finishScheduled(false), m_initialized(false),
          m_inputEnded(false), m_crcState(0xFFFFFFFFU)
    {
        setWindowTitle(QString::fromLatin1("NIKINIKI H264 HwCap Probe"));

        QLabel *title = new QLabel(
            QString::fromLatin1("Direct DevVideo H.264 capability probe"), this);
        QFont titleFont = title->font();
        titleFont.setBold(true);
        titleFont.setPointSize(14);
        title->setFont(titleFont);
        title->setAlignment(Qt::AlignCenter);

        QLabel *description = new QLabel(
            QString::fromLatin1(
                "Research-only UID 0xE000B11D. Reads local REF1-REF8 files, "
                "selects decoder 0x10204C21, and writes staged results. "
                "It does not open NIKINIKI, MMF player, network or audio."),
            this);
        description->setWordWrap(true);

        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        m_status->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
        m_status->setMinimumHeight(130);

        m_startButton = new QPushButton(
            QString::fromLatin1("Run capability matrix"), this);
        QPushButton *quitButton = new QPushButton(
            QString::fromLatin1("Exit probe"), this);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);
        layout->addWidget(title);
        layout->addWidget(description);
        layout->addWidget(m_status, 1);
        layout->addWidget(m_startButton);
        layout->addWidget(quitButton);

        m_watchdog.setSingleShot(true);
        connect(m_startButton, SIGNAL(clicked()), this, SLOT(startRun()));
        connect(quitButton, SIGNAL(clicked()), qApp, SLOT(quit()));
        connect(&m_watchdog, SIGNAL(timeout()), this, SLOT(onWatchdog()));
        setStatus(QString::fromLatin1(
            "Copy the matrix directory to F:, E: or C:, then tap Run."));
    }

    ~DevVideoCapabilityProbe()
    {
        m_watchdog.stop();
        cleanupDevVideo();
        if (m_log.isOpen())
            m_log.close();
    }

private slots:
    void startRun()
    {
        if (m_stage != Idle && m_stage != Complete)
            return;
        QString errorText;
        if (!loadCases(&errorText)) {
            setStatus(QString::fromLatin1("Cannot start: %1").arg(errorText));
            qDebug() << "WW:HWCAP_LOAD_FAIL" << errorText;
            return;
        }
        if (!openRunLog(&errorText)) {
            setStatus(QString::fromLatin1("Cannot create result log: %1").arg(errorText));
            return;
        }
        m_startButton->setEnabled(false);
        m_caseIndex = -1;
        m_stage = LoadingCase;
        writeEvent(QString::fromLatin1("RUN_BEGIN"), QString::fromLatin1(
            "schema=%1\tmatrix=%2\tdecoder_uid=0x10204C21\tcase_count=%3\tinput=%4")
            .arg(m_schema).arg(m_matrixName)
            .arg(m_cases.size()).arg(m_baseDirectory));
        QTimer::singleShot(0, this, SLOT(startNextCase()));
    }

    void startNextCase()
    {
        cleanupDevVideo();
        ++m_caseIndex;
        if (m_caseIndex >= m_cases.size()) {
            m_stage = Complete;
            writeEvent(QString::fromLatin1("RUN_END"),
                       QString::fromLatin1("status=COMPLETE"));
            if (m_log.isOpen()) {
                m_log.flush();
                m_log.close();
            }
            setStatus(QString::fromLatin1(
                "Completed %1 cases.\nResults: %2\n\nNo corruption verdict is "
                "made until PC normalization compares the frame dumps.")
                .arg(m_cases.size()).arg(m_runDirectory));
            m_startButton->setEnabled(true);
            return;
        }

        m_result.reset();
        m_units.clear();
        m_headerUnits.clear();
        m_nextUnit = 0;
        m_finishScheduled = false;
        m_finishReason.clear();
        m_initialized = false;
        m_inputEnded = false;
        m_crcState = 0xFFFFFFFFU;
        Mem::FillZ(&m_outputFormat, sizeof(m_outputFormat));
        m_stage = LoadingCase;
        runCurrentCase();
    }

    void finishCurrentCase()
    {
        if (m_stage == Complete || m_stage == Idle)
            return;
        m_watchdog.stop();
        m_stage = FinishingCase;
        m_result.finishReason = m_finishReason;
        collectCounters();
        m_result.rollingCrc = m_crcState ^ 0xFFFFFFFFU;
        writeSummary();
        cleanupDevVideo();
        m_units.clear();
        m_headerUnits.clear();
        m_stage = LoadingCase;
        QTimer::singleShot(350, this, SLOT(startNextCase()));
    }

    void onWatchdog()
    {
        const QString reason = m_stage == Initializing
            ? QString::fromLatin1("initialize-timeout")
            : QString::fromLatin1("decode-timeout");
        if (m_stage == Initializing && m_result.initializeError == KPending)
            m_result.initializeError = KErrTimedOut;
        scheduleFinish(reason);
    }

private:
    enum Stage {
        Idle = 0,
        LoadingCase,
        Initializing,
        Decoding,
        FinishingCase,
        Complete
    };

    bool loadCases(QString *errorText)
    {
        const QStringList candidates = QStringList()
            << QString::fromLatin1("F:/Data/NIKINIKI/hwcap/cases.ini")
            << QString::fromLatin1("E:/Data/NIKINIKI/hwcap/cases.ini")
            << QString::fromLatin1("C:/Data/NIKINIKI/hwcap/cases.ini");
        QString settingsPath;
        int index;
        for (index = 0; index < candidates.size(); ++index) {
            if (QFileInfo(candidates.at(index)).isFile()) {
                settingsPath = candidates.at(index);
                break;
            }
        }
        if (settingsPath.isEmpty()) {
            if (errorText)
                *errorText = QString::fromLatin1(
                    "cases.ini not found in F:/Data/NIKINIKI/hwcap, "
                    "E:/Data/NIKINIKI/hwcap or C:/Data/NIKINIKI/hwcap");
            return false;
        }

        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        // QSettings exposes the INI [General] section as root keys. Calling
        // beginGroup("General") would instead address the escaped [%General]
        // section and return empty values on Qt 4.7.
        const int schema = settings.value(QString::fromLatin1("schema")).toInt();
        const QString matrix = settings.value(QString::fromLatin1("matrix")).toString();
        const int count = settings.value(QString::fromLatin1("count")).toInt();
        QStringList order;
        if (schema == 1 && matrix == QString::fromLatin1("H264_REF_1_TO_8") &&
            count == 8) {
            order << QString::fromLatin1("REF4")
                  << QString::fromLatin1("REF1")
                  << QString::fromLatin1("REF2")
                  << QString::fromLatin1("REF3")
                  << QString::fromLatin1("REF5")
                  << QString::fromLatin1("REF6")
                  << QString::fromLatin1("REF7")
                  << QString::fromLatin1("REF8");
        } else if (((schema == 2 &&
                     matrix == QString::fromLatin1("H264_BOUNDARY_R2") &&
                     count > 0 && count <= 32)) ||
                   (schema == 3 &&
                    matrix == QString::fromLatin1("H264_SPS_FAKE_AB_R3") &&
                    count == 2) ||
                   (schema == 4 &&
                    matrix == QString::fromLatin1("H264_HEADER_SUBMIT_SPLIT_R4") &&
                    count == 4)) {
            const QVariant orderValue = settings.value(QString::fromLatin1("order"));
            order = orderValue.toStringList();
            if (order.size() == 1 && order.at(0).contains(QLatin1Char(','))) {
                order = order.at(0).split(
                    QLatin1Char(','), QString::SkipEmptyParts);
            }
            if (order.size() != count) {
                order.clear();
                int indexedCase;
                for (indexedCase = 0; indexedCase < count; ++indexedCase) {
                    const QString key = QString::fromLatin1("case%1").arg(indexedCase);
                    const QString id = settings.value(key).toString().trimmed();
                    if (!id.isEmpty())
                        order.append(id);
                }
            }
            if (order.size() != count && schema == 2 && count == 11) {
                order.clear();
                order << QString::fromLatin1("R6")
                      << QString::fromLatin1("R7")
                      << QString::fromLatin1("D4")
                      << QString::fromLatin1("D5")
                      << QString::fromLatin1("D6")
                      << QString::fromLatin1("D7")
                      << QString::fromLatin1("D8")
                      << QString::fromLatin1("WP4_OFF")
                      << QString::fromLatin1("WP4_ON")
                      << QString::fromLatin1("WP6_OFF")
                      << QString::fromLatin1("WP6_ON");
            } else if (order.size() != count && schema == 3 && count == 2) {
                order.clear();
                order << QString::fromLatin1("ORIGINAL_R7")
                      << QString::fromLatin1("FAKE_REF3");
            } else if (order.size() != count && schema == 4 && count == 4) {
                order.clear();
                order << QString::fromLatin1("R6_NATIVE")
                      << QString::fromLatin1("R7_NATIVE")
                      << QString::fromLatin1("FAKE_HEADER_ORIGINAL_R7")
                      << QString::fromLatin1("FAKE_HEADER_FAKE_REF3");
            }
            int orderIndex;
            for (orderIndex = 0; orderIndex < order.size(); ++orderIndex)
                order[orderIndex] = order.at(orderIndex).trimmed();
        }
        if (order.size() != count) {
            if (errorText)
                *errorText = QString::fromLatin1(
                    "Unsupported cases.ini: schema=%1 matrix=%2 count=%3 order=%4")
                    .arg(schema).arg(matrix).arg(count).arg(order.size());
            return false;
        }

        QVector<CaseSpec> loaded;
        for (index = 0; index < order.size(); ++index) {
            const QString id = order.at(index);
            if (!validCaseId(id) || order.count(id) != 1) {
                if (errorText)
                    *errorText = QString::fromLatin1("Invalid or duplicate case ID: %1").arg(id);
                return false;
            }
            settings.beginGroup(id);
            CaseSpec spec;
            spec.id = id;
            spec.group = settings.value(QString::fromLatin1("group"),
                QString::fromLatin1("R")).toString();
            spec.h264File = settings.value(QString::fromLatin1("h264")).toString();
            spec.h264Size = settings.value(QString::fromLatin1("h264_size")).toLongLong();
            spec.h264Sha256 = settings.value(
                QString::fromLatin1("h264_sha256")).toByteArray().toUpper();
            spec.goldenSha256 = settings.value(
                QString::fromLatin1("golden_sha256")).toByteArray().toUpper();
            spec.profile = settings.value(QString::fromLatin1("profile")).toInt();
            spec.level = settings.value(QString::fromLatin1("level")).toInt();
            spec.refs = settings.value(QString::fromLatin1("refs")).toInt();
            spec.dpb = settings.value(QString::fromLatin1("dpb")).toInt();
            spec.reorder = settings.value(QString::fromLatin1("reorder")).toInt();
            spec.weightP = settings.value(QString::fromLatin1("weightp")).toInt();
            spec.weightB = settings.value(QString::fromLatin1("weightb")).toInt();
            spec.accessUnits = settings.value(
                QString::fromLatin1("access_units")).toInt();
            spec.eosNal = settings.value(QString::fromLatin1("eos_nal"), 0).toInt();
            spec.runtimeReady = settings.value(
                QString::fromLatin1("runtime_decode_eligible")).toBool();
            spec.headerH264File = settings.value(
                QString::fromLatin1("header_h264"), spec.h264File).toString();
            spec.headerH264Size = settings.value(
                QString::fromLatin1("header_h264_size"), spec.h264Size).toLongLong();
            spec.headerH264Sha256 = settings.value(
                QString::fromLatin1("header_h264_sha256"),
                spec.h264Sha256).toByteArray().toUpper();
            spec.headerRefs = settings.value(
                QString::fromLatin1("header_refs"), spec.refs).toInt();
            settings.endGroup();
            bool metadataValid = validLocalFileName(spec.h264File) &&
                validLocalFileName(spec.headerH264File) &&
                (schema == 4 || spec.h264File == id + QString::fromLatin1(".h264")) &&
                spec.h264Size > 0 && spec.h264Sha256.size() == 64 &&
                spec.headerH264Size > 0 && spec.headerH264Sha256.size() == 64 &&
                spec.headerRefs > 0 && spec.headerRefs <= 16 &&
                spec.goldenSha256.size() == 64 &&
                spec.profile == 100 && spec.level == 30 &&
                spec.refs > 0 && spec.refs <= 16 &&
                spec.dpb >= spec.refs && spec.dpb <= 16 &&
                spec.reorder == 0 && spec.weightP >= 0 && spec.weightP <= 1 &&
                spec.weightB >= 0 && spec.weightB <= 2 &&
                spec.accessUnits >= KMaximumAccessUnits;
            if (schema == 1) {
                metadataValid = metadataValid && spec.refs == id.mid(3).toInt() &&
                    spec.weightP == 0 && spec.weightB == 0 && spec.eosNal == 0 &&
                    spec.runtimeReady;
            } else if (schema == 2 &&
                       matrix == QString::fromLatin1("H264_BOUNDARY_R2") &&
                       spec.group == QString::fromLatin1("R")) {
                metadataValid = metadataValid && id.startsWith(QLatin1Char('R')) &&
                    spec.refs == id.mid(1).toInt() && spec.dpb == spec.refs &&
                    spec.weightP == 0 && spec.weightB == 0 && spec.eosNal == 1 &&
                    spec.runtimeReady;
            } else if (schema == 2 &&
                       matrix == QString::fromLatin1("H264_BOUNDARY_R2") &&
                       spec.group == QString::fromLatin1("D")) {
                metadataValid = metadataValid && spec.refs == 4 &&
                    spec.dpb >= 4 && spec.dpb <= 8 && spec.weightP == 0 &&
                    spec.weightB == 0 && spec.eosNal == 1 && spec.runtimeReady;
            } else if (schema == 2 &&
                       matrix == QString::fromLatin1("H264_BOUNDARY_R2") &&
                       spec.group == QString::fromLatin1("WP")) {
                metadataValid = metadataValid &&
                    (spec.refs == 4 || spec.refs == 6) && spec.weightB == 0 &&
                    spec.eosNal == 1 && spec.runtimeReady;
            } else if (schema == 3 &&
                       matrix == QString::fromLatin1("H264_SPS_FAKE_AB_R3") &&
                       spec.group == QString::fromLatin1("AB_ORIGINAL")) {
                metadataValid = metadataValid && id == QString::fromLatin1("ORIGINAL_R7") &&
                    spec.refs == 7 && spec.dpb == 7 && spec.weightP == 0 &&
                    spec.weightB == 0 && spec.eosNal == 1 && spec.runtimeReady;
            } else if (schema == 3 &&
                       matrix == QString::fromLatin1("H264_SPS_FAKE_AB_R3") &&
                       spec.group == QString::fromLatin1("AB_FAKE")) {
                metadataValid = metadataValid && id == QString::fromLatin1("FAKE_REF3") &&
                    spec.refs == 3 && spec.dpb == 7 && spec.weightP == 0 &&
                    spec.weightB == 0 && spec.eosNal == 1 && !spec.runtimeReady;
            } else if (schema == 4 &&
                       matrix == QString::fromLatin1("H264_HEADER_SUBMIT_SPLIT_R4") &&
                       spec.group == QString::fromLatin1("NATIVE_R6")) {
                metadataValid = metadataValid && id == QString::fromLatin1("R6_NATIVE") &&
                    spec.h264File == QString::fromLatin1("R6.h264") &&
                    spec.headerH264File == spec.h264File && spec.refs == 6 &&
                    spec.headerRefs == 6 && spec.dpb == 6 && spec.eosNal == 1 &&
                    spec.runtimeReady;
            } else if (schema == 4 &&
                       matrix == QString::fromLatin1("H264_HEADER_SUBMIT_SPLIT_R4") &&
                       spec.group == QString::fromLatin1("NATIVE_R7")) {
                metadataValid = metadataValid && id == QString::fromLatin1("R7_NATIVE") &&
                    spec.h264File == QString::fromLatin1("ORIGINAL_R7.h264") &&
                    spec.headerH264File == spec.h264File && spec.refs == 7 &&
                    spec.headerRefs == 7 && spec.dpb == 7 && spec.eosNal == 1 &&
                    spec.runtimeReady;
            } else if (schema == 4 &&
                       matrix == QString::fromLatin1("H264_HEADER_SUBMIT_SPLIT_R4") &&
                       spec.group == QString::fromLatin1("SPLIT_R7")) {
                metadataValid = metadataValid &&
                    id == QString::fromLatin1("FAKE_HEADER_ORIGINAL_R7") &&
                    spec.h264File == QString::fromLatin1("ORIGINAL_R7.h264") &&
                    spec.headerH264File == QString::fromLatin1("FAKE_REF3.h264") &&
                    spec.refs == 7 && spec.headerRefs == 3 && spec.dpb == 7 &&
                    spec.eosNal == 1 && spec.runtimeReady;
            } else if (schema == 4 &&
                       matrix == QString::fromLatin1("H264_HEADER_SUBMIT_SPLIT_R4") &&
                       spec.group == QString::fromLatin1("FAKE_CONTROL")) {
                metadataValid = metadataValid &&
                    id == QString::fromLatin1("FAKE_HEADER_FAKE_REF3") &&
                    spec.h264File == QString::fromLatin1("FAKE_REF3.h264") &&
                    spec.headerH264File == spec.h264File && spec.refs == 3 &&
                    spec.headerRefs == 3 && spec.dpb == 7 && spec.eosNal == 1 &&
                    !spec.runtimeReady;
            } else {
                metadataValid = false;
            }
            if (!metadataValid) {
                if (errorText)
                    *errorText = QString::fromLatin1("Invalid case metadata: %1").arg(id);
                return false;
            }
            loaded.append(spec);
        }
        m_cases = loaded;
        m_schema = schema;
        m_matrixName = matrix;
        m_baseDirectory = QFileInfo(settingsPath).absolutePath();
        return true;
    }

    bool openRunLog(QString *errorText)
    {
        const QString runId = QDateTime::currentDateTime().toString(
            QString::fromLatin1("yyyyMMdd-HHmmss"));
        m_runDirectory = QDir(m_baseDirectory).filePath(
            QString::fromLatin1("results/%1").arg(runId));
        if (!QDir().mkpath(m_runDirectory)) {
            if (errorText)
                *errorText = QString::fromLatin1("Cannot create %1").arg(m_runDirectory);
            return false;
        }
        m_log.setFileName(QDir(m_runDirectory).filePath(
            QString::fromLatin1("events.tsv")));
        if (!m_log.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorText)
                *errorText = QString::fromLatin1("Cannot open events.tsv");
            return false;
        }
        m_log.write("utc\tcase\tevent\tfields\n");
        m_log.flush();
        return true;
    }

    void runCurrentCase()
    {
        const CaseSpec &spec = m_cases.at(m_caseIndex);
        setStatus(QString::fromLatin1(
            "Case %1/%2: %3\nLoading and hashing local stream...")
            .arg(m_caseIndex + 1).arg(m_cases.size()).arg(spec.id));
        const QString path = QDir(m_baseDirectory).filePath(spec.h264File);
        QFile input(path);
        if (!input.open(QIODevice::ReadOnly)) {
            m_result.fileError = KErrNotFound;
            stage(QString::fromLatin1("FILE"), m_result.fileError, path);
            scheduleFinish(QString::fromLatin1("file-open"));
            return;
        }
        if (input.size() != spec.h264Size) {
            input.close();
            m_result.fileError = KErrCorrupt;
            stage(QString::fromLatin1("FILE"), m_result.fileError,
                  QString::fromLatin1("size=%1 expected=%2")
                  .arg(input.size()).arg(spec.h264Size));
            scheduleFinish(QString::fromLatin1("file-size"));
            return;
        }
        m_streamData = input.readAll();
        input.close();
        m_result.fileError = m_streamData.size() == spec.h264Size
            ? KErrNone : KErrCorrupt;
        stage(QString::fromLatin1("FILE"), m_result.fileError,
              QString::fromLatin1("bytes=%1").arg(m_streamData.size()));
        if (m_result.fileError != KErrNone) {
            scheduleFinish(QString::fromLatin1("file-read"));
            return;
        }

        TInt digestError = KErrNone;
        const QByteArray actualDigest = sha256File(path, &digestError);
        m_result.hashError = digestError == KErrNone &&
            actualDigest == spec.h264Sha256 ? KErrNone :
            (digestError != KErrNone ? digestError : KErrCorrupt);
        stage(QString::fromLatin1("SHA256"), m_result.hashError,
              QString::fromLatin1("actual=%1 expected=%2")
              .arg(QString::fromLatin1(actualDigest))
              .arg(QString::fromLatin1(spec.h264Sha256)));
        if (m_result.hashError != KErrNone) {
            scheduleFinish(QString::fromLatin1("sha256"));
            return;
        }

        QString splitError;
        if (!splitAudAccessUnits(m_streamData, &m_units, &splitError) ||
            m_units.size() != spec.accessUnits) {
            m_result.fileError = KErrCorrupt;
            stage(QString::fromLatin1("AUD_SPLIT"), m_result.fileError,
                  QString::fromLatin1("parsed=%1 expected=%2 detail=%3")
                  .arg(m_units.size()).arg(spec.accessUnits).arg(splitError));
            scheduleFinish(QString::fromLatin1("aud-split"));
            return;
        }
        m_result.accessUnitsParsed = m_units.size();
        m_result.accessUnitsTarget = qMin(KMaximumAccessUnits, m_units.size());
        m_result.eosNalCount = countNalType(m_streamData, 11);
        if (m_result.eosNalCount != spec.eosNal) {
            m_result.fileError = KErrCorrupt;
            stage(QString::fromLatin1("EOS_NAL"), m_result.fileError,
                  QString::fromLatin1("parsed=%1 expected=%2")
                  .arg(m_result.eosNalCount).arg(spec.eosNal));
            scheduleFinish(QString::fromLatin1("eos-nal"));
            return;
        }
        stage(QString::fromLatin1("AUD_SPLIT"), KErrNone,
              QString::fromLatin1("parsed=%1 target=%2 eos_nal=%3")
              .arg(m_units.size()).arg(m_result.accessUnitsTarget)
              .arg(m_result.eosNalCount));

        const QString headerPath = QDir(m_baseDirectory).filePath(
            spec.headerH264File);
        if (spec.headerH264File == spec.h264File &&
            spec.headerH264Size == spec.h264Size &&
            spec.headerH264Sha256 == spec.h264Sha256) {
            m_headerStreamData = m_streamData;
            m_headerUnits = m_units;
            m_result.headerFileError = KErrNone;
            m_result.headerHashError = KErrNone;
            stage(QString::fromLatin1("ADMISSION_SOURCE"), KErrNone,
                  QString::fromLatin1(
                      "mode=native file=%1 sha=%2 refs=%3 parsed_au=%4")
                  .arg(spec.headerH264File)
                  .arg(QString::fromLatin1(spec.headerH264Sha256))
                  .arg(spec.headerRefs).arg(m_headerUnits.size()));
        } else {
            QFile headerInput(headerPath);
            if (!headerInput.open(QIODevice::ReadOnly) ||
                headerInput.size() != spec.headerH264Size) {
                m_result.headerFileError = headerInput.isOpen()
                    ? KErrCorrupt : KErrNotFound;
                stage(QString::fromLatin1("ADMISSION_FILE"),
                      m_result.headerFileError,
                      QString::fromLatin1("file=%1 size=%2 expected=%3")
                      .arg(spec.headerH264File)
                      .arg(headerInput.isOpen() ? headerInput.size() : -1)
                      .arg(spec.headerH264Size));
                if (headerInput.isOpen())
                    headerInput.close();
                scheduleFinish(QString::fromLatin1("admission-file"));
                return;
            }
            m_headerStreamData = headerInput.readAll();
            headerInput.close();
            m_result.headerFileError =
                m_headerStreamData.size() == spec.headerH264Size
                ? KErrNone : KErrCorrupt;
            stage(QString::fromLatin1("ADMISSION_FILE"),
                  m_result.headerFileError,
                  QString::fromLatin1("file=%1 bytes=%2")
                  .arg(spec.headerH264File).arg(m_headerStreamData.size()));
            if (m_result.headerFileError != KErrNone) {
                scheduleFinish(QString::fromLatin1("admission-read"));
                return;
            }

            TInt headerDigestError = KErrNone;
            const QByteArray headerDigest = sha256File(
                headerPath, &headerDigestError);
            m_result.headerHashError = headerDigestError == KErrNone &&
                headerDigest == spec.headerH264Sha256 ? KErrNone :
                (headerDigestError != KErrNone ? headerDigestError : KErrCorrupt);
            stage(QString::fromLatin1("ADMISSION_SHA256"),
                  m_result.headerHashError,
                  QString::fromLatin1("actual=%1 expected=%2")
                  .arg(QString::fromLatin1(headerDigest))
                  .arg(QString::fromLatin1(spec.headerH264Sha256)));
            if (m_result.headerHashError != KErrNone) {
                scheduleFinish(QString::fromLatin1("admission-sha256"));
                return;
            }

            QString headerSplitError;
            if (!splitAudAccessUnits(
                    m_headerStreamData, &m_headerUnits, &headerSplitError) ||
                m_headerUnits.size() != spec.accessUnits) {
                m_result.headerFileError = KErrCorrupt;
                stage(QString::fromLatin1("ADMISSION_AUD_SPLIT"),
                      m_result.headerFileError,
                      QString::fromLatin1("parsed=%1 expected=%2 detail=%3")
                      .arg(m_headerUnits.size()).arg(spec.accessUnits)
                      .arg(headerSplitError));
                scheduleFinish(QString::fromLatin1("admission-aud-split"));
                return;
            }
            stage(QString::fromLatin1("ADMISSION_SOURCE"), KErrNone,
                  QString::fromLatin1(
                      "mode=split file=%1 sha=%2 refs=%3 parsed_au=%4 submit_file=%5")
                  .arg(spec.headerH264File)
                  .arg(QString::fromLatin1(spec.headerH264Sha256))
                  .arg(spec.headerRefs).arg(m_headerUnits.size())
                  .arg(spec.h264File));
        }
        setupDevVideo();
    }

    void setupDevVideo()
    {
        const CaseSpec &spec = m_cases.at(m_caseIndex);
        TRAPD(createError, m_devVideo = CMMFDevVideoPlay::NewL(*this));
        m_result.createError = createError;
        stage(QString::fromLatin1("CREATE"), createError);
        if (createError != KErrNone || !m_devVideo) {
            scheduleFinish(QString::fromLatin1("create"));
            return;
        }

        CVideoDecoderInfo *info = 0;
        TRAPD(infoError, info = takeDecoderInfoL(m_devVideo));
        m_result.infoError = infoError;
        if (infoError == KErrNone && info) {
            m_result.accelerated = info->Accelerated();
            m_result.directDisplay = info->SupportsDirectDisplay();
            const TSize maxSize = info->MaxPictureSize();
            stage(QString::fromLatin1("DECODER_INFO"), KErrNone,
                  QString::fromLatin1(
                      "uid=0x%1 accelerated=%2 direct=%3 max=%4x%5 bitrate=%6 manufacturer=%7 identifier=%8")
                  .arg(static_cast<TUint>(info->Uid().iUid), 0, 16)
                  .arg(info->Accelerated() ? 1 : 0)
                  .arg(info->SupportsDirectDisplay() ? 1 : 0)
                  .arg(maxSize.iWidth).arg(maxSize.iHeight)
                  .arg(info->MaxBitrate())
                  .arg(descriptorText(info->Manufacturer()))
                  .arg(descriptorText(info->Identifier())));
        } else {
            stage(QString::fromLatin1("DECODER_INFO"), infoError);
        }
        delete info;
        if (infoError != KErrNone || !m_result.accelerated) {
            if (infoError == KErrNone)
                m_result.infoError = KErrNotSupported;
            scheduleFinish(QString::fromLatin1("decoder-info"));
            return;
        }

        TRAPD(selectError,
              m_decoderId = m_devVideo->SelectDecoderL(KBroadcomDecoderUid));
        m_result.selectError = selectError;
        stage(QString::fromLatin1("SELECT"), selectError,
              QString::fromLatin1("hwdevice=%1").arg(static_cast<TUint>(m_decoderId)));
        if (selectError != KErrNone) {
            scheduleFinish(QString::fromLatin1("select"));
            return;
        }

        CCompressedVideoFormat *format = 0;
        TRAPD(formatError,
              format = CCompressedVideoFormat::NewL(_L8("video/h264")));
        if (formatError != KErrNone || !format) {
            m_result.inputError = formatError != KErrNone ? formatError : KErrNoMemory;
            stage(QString::fromLatin1("INPUT_FORMAT"), m_result.inputError);
            scheduleFinish(QString::fromLatin1("input-format-create"));
            return;
        }
        TRAPD(inputError,
              m_devVideo->SetInputFormatL(
                  m_decoderId, *format, EDuCodedPicture,
                  EDuElementaryStream, ETrue));
        delete format;
        m_result.inputError = inputError;
        stage(QString::fromLatin1("INPUT_FORMAT"), inputError,
              QString::fromLatin1("mime=video/h264 unit=coded-picture encapsulation=elementary in-order=1"));
        if (inputError != KErrNone) {
            scheduleFinish(QString::fromLatin1("input-format"));
            return;
        }

        TVideoInputBuffer headerInput;
        QByteArray &firstUnit = m_headerUnits[0];
        headerInput.iData.Set(
            reinterpret_cast<TUint8 *>(firstUnit.data()),
            firstUnit.size(), firstUnit.size());
        TVideoPictureHeader *header = 0;
        TRAPD(headerError,
              header = m_devVideo->GetHeaderInformationL(
                  EDuCodedPicture, EDuElementaryStream, &headerInput));
        if (headerError == KErrNone && !header)
            m_result.headerError = KErrUnderflow;
        else
            m_result.headerError = headerError;
        if (header) {
            m_result.headerWidth = header->iSizeInMemory.iWidth;
            m_result.headerHeight = header->iSizeInMemory.iHeight;
        }
        stage(QString::fromLatin1("HEADER"), m_result.headerError,
              QString::fromLatin1(
                  "bytes=%1 header_refs=%2 submit_refs=%3 dpb=%4 size=%5x%6 header_file=%7 submit_file=%8")
              .arg(firstUnit.size()).arg(spec.headerRefs).arg(spec.refs).arg(spec.dpb)
              .arg(m_result.headerWidth).arg(m_result.headerHeight)
              .arg(spec.headerH264File).arg(spec.h264File));
        if (m_result.headerError != KErrNone) {
            if (header)
                m_devVideo->ReturnHeader(header);
            scheduleFinish(QString::fromLatin1("header"));
            return;
        }

        TRAPD(configureError, m_devVideo->ConfigureDecoderL(*header));
        m_devVideo->ReturnHeader(header);
        m_result.configureError = configureError;
        stage(QString::fromLatin1("CONFIGURE"), configureError);
        if (configureError != KErrNone) {
            scheduleFinish(QString::fromLatin1("configure"));
            return;
        }

        RArray<TUncompressedVideoFormat> formats;
        TRAPD(outputListError,
              m_devVideo->GetOutputFormatListL(m_decoderId, formats));
        m_result.outputListError = outputListError;
        m_result.outputFormats = formats.Count();
        TInt selected = -1;
        TInt selectedScore = 100;
        TInt index;
        for (index = 0; index < formats.Count(); ++index) {
            const TInt score = outputScore(formats[index]);
            stage(QString::fromLatin1("OUTPUT_OPTION"), KErrNone,
                  QString::fromLatin1("index=%1 data=%2 pattern=%3 layout=%4 rgb=%5 score=%6")
                  .arg(index)
                  .arg(static_cast<TUint32>(formats[index].iDataFormat))
                  .arg(static_cast<TUint32>(formats[index].iYuvFormat.iPattern))
                  .arg(static_cast<TUint32>(formats[index].iYuvFormat.iDataLayout))
                  .arg(static_cast<TUint32>(formats[index].iRgbFormat))
                  .arg(score));
            if (score < selectedScore) {
                selected = index;
                selectedScore = score;
            }
        }
        stage(QString::fromLatin1("OUTPUT_LIST"), outputListError,
              QString::fromLatin1("count=%1 selected=%2 score=%3")
              .arg(formats.Count()).arg(selected).arg(selectedScore));
        if (outputListError != KErrNone || selected < 0 || selectedScore >= 100) {
            formats.Close();
            if (m_result.outputListError == KErrNone)
                m_result.outputListError = KErrNotSupported;
            scheduleFinish(QString::fromLatin1("output-list"));
            return;
        }
        m_outputFormat = formats[selected];
        m_result.outputIndex = selected;
        m_result.outputDataFormat = static_cast<TUint32>(m_outputFormat.iDataFormat);
        m_result.outputPattern = static_cast<TUint32>(m_outputFormat.iYuvFormat.iPattern);
        m_result.outputLayout = static_cast<TUint32>(m_outputFormat.iYuvFormat.iDataLayout);
        TRAPD(outputSetError,
              m_devVideo->SetOutputFormatL(m_decoderId, m_outputFormat));
        formats.Close();
        m_result.outputSetError = outputSetError;
        stage(QString::fromLatin1("OUTPUT_SET"), outputSetError,
              QString::fromLatin1("index=%1 data=%2 pattern=%3 layout=%4")
              .arg(selected).arg(m_result.outputDataFormat)
              .arg(m_result.outputPattern).arg(m_result.outputLayout));
        if (outputSetError != KErrNone) {
            scheduleFinish(QString::fromLatin1("output-set"));
            return;
        }

        CMMFDevVideoPlay::TBufferOptions options;
        m_devVideo->GetBufferOptions(options);
        m_result.preDecodeBufferSize = options.iPreDecodeBufferSize;
        m_result.maxPostDecodeBufferSize = options.iMaxPostDecodeBufferSize;
        m_result.maxInputBufferSize = options.iMaxInputBufferSize;
        m_result.minInputBuffers = options.iMinNumInputBuffers;
        stage(QString::fromLatin1("BUFFER_OPTIONS"), KErrNone,
              QString::fromLatin1("pre=%1 max-post=%2 max-input=%3 min-input=%4 pre-us=%5 post-us=%6")
              .arg(options.iPreDecodeBufferSize)
              .arg(options.iMaxPostDecodeBufferSize)
              .arg(options.iMaxInputBufferSize)
              .arg(options.iMinNumInputBuffers)
              .arg(static_cast<qint64>(options.iPreDecoderBufferPeriod.Int64()))
              .arg(static_cast<qint64>(options.iPostDecoderBufferPeriod.Int64())));

        TRAPD(destinationError, m_devVideo->SetVideoDestScreenL(EFalse));
        m_result.destinationError = destinationError;
        stage(QString::fromLatin1("DESTINATION"), destinationError,
              QString::fromLatin1("memory=1"));
        if (destinationError != KErrNone) {
            scheduleFinish(QString::fromLatin1("destination"));
            return;
        }
        m_devVideo->SynchronizeDecoding(EFalse);
        stage(QString::fromLatin1("SYNCHRONIZE"), KErrNone,
              QString::fromLatin1("enabled=0 clock=none"));

        m_result.initializeError = KPending;
        m_stage = Initializing;
        setStatus(QString::fromLatin1(
            "Case %1/%2: %3\nHeader and Configure passed. Waiting for Initialize...")
            .arg(m_caseIndex + 1).arg(m_cases.size()).arg(spec.id));
        stage(QString::fromLatin1("INITIALIZE_SENT"), KErrNone);
        m_watchdog.start(KInitializeTimeoutMs);
        m_devVideo->Initialize();
    }

    void feedInput()
    {
        if (!m_devVideo || !m_initialized || m_inputEnded ||
            m_finishScheduled || m_stage != Decoding) {
            return;
        }
        while (m_nextUnit < m_result.accessUnitsTarget) {
            const QByteArray &unit = m_units.at(m_nextUnit);
            TVideoInputBuffer *buffer = 0;
            TRAPD(bufferError,
                  buffer = m_devVideo->GetBufferL(static_cast<TUint>(unit.size())));
            if (bufferError != KErrNone) {
                m_result.bufferError = bufferError;
                stage(QString::fromLatin1("GET_BUFFER"), bufferError,
                      QString::fromLatin1("au=%1 bytes=%2")
                      .arg(m_nextUnit).arg(unit.size()));
                scheduleFinish(QString::fromLatin1("get-buffer"));
                return;
            }
            if (!buffer)
                break;
            buffer->iData.Copy(TPtrC8(
                reinterpret_cast<const TUint8 *>(unit.constData()), unit.size()));
            buffer->iOptions = TVideoInputBuffer::ESequenceNumber |
                TVideoInputBuffer::EDecodingTimestamp |
                TVideoInputBuffer::EPresentationTimestamp;
            buffer->iSequenceNumber = static_cast<TUint>(m_nextUnit);
            const TInt64 timestamp = static_cast<TInt64>(m_nextUnit) * 1000000 / 30;
            buffer->iDecodingTimestamp = TTimeIntervalMicroSeconds(timestamp);
            buffer->iPresentationTimestamp = TTimeIntervalMicroSeconds(timestamp);
            buffer->iPreRoll = EFalse;
            buffer->iError = EFalse;
            TRAPD(writeError, m_devVideo->WriteCodedDataL(buffer));
            if (writeError != KErrNone) {
                m_result.writeError = writeError;
                stage(QString::fromLatin1("WRITE"), writeError,
                      QString::fromLatin1("au=%1 bytes=%2")
                      .arg(m_nextUnit).arg(unit.size()));
                scheduleFinish(QString::fromLatin1("write"));
                return;
            }
            m_result.bufferError = KErrNone;
            m_result.writeError = KErrNone;
            ++m_nextUnit;
            m_result.accessUnitsWritten = m_nextUnit;
        }
        setStatus(QString::fromLatin1(
            "Case %1/%2: %3\nDecoding: AU %4/%5, pictures %6")
            .arg(m_caseIndex + 1).arg(m_cases.size())
            .arg(m_cases.at(m_caseIndex).id)
            .arg(m_result.accessUnitsWritten)
            .arg(m_result.accessUnitsTarget)
            .arg(m_result.picturesOutput));
        if (m_nextUnit >= m_result.accessUnitsTarget && !m_inputEnded) {
            m_inputEnded = true;
            m_devVideo->InputEnd();
            stage(QString::fromLatin1("INPUT_END"), KErrNone,
                  QString::fromLatin1("au=%1").arg(m_nextUnit));
        }
    }

    void saveFrame(const QByteArray &bytes, int index)
    {
        if (index >= 3 || bytes.isEmpty())
            return;
        const QString path = QDir(m_runDirectory).filePath(
            QString::fromLatin1("%1-frame%2.raw")
            .arg(m_cases.at(m_caseIndex).id)
            .arg(index, 3, 10, QLatin1Char('0')));
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(bytes);
            file.close();
        }
    }

    void collectCounters()
    {
        if (!m_devVideo || !m_initialized ||
            m_result.fatalError != KNotRun) {
            return;
        }
        CMMFDevVideoPlay::TPictureCounters pictures;
        m_devVideo->GetPictureCounters(pictures);
        m_result.picturesDecoded += pictures.iPicturesDecoded;
        m_result.picturesDisplayed += pictures.iPicturesDisplayed;
        m_result.picturesSkipped += pictures.iPicturesSkipped;
        m_result.picturesTotal += pictures.iTotalPictures;
        CMMFDevVideoPlay::TBitstreamCounters bitstream;
        m_devVideo->GetBitstreamCounters(bitstream);
        m_result.packetsLost += bitstream.iLostPackets;
        m_result.packetsTotal += bitstream.iTotalPackets;
    }

    void cleanupDevVideo()
    {
        m_watchdog.stop();
        if (m_devVideo) {
            if (m_initialized && m_result.fatalError == KNotRun)
                m_devVideo->Stop();
            delete m_devVideo;
            m_devVideo = 0;
        }
        m_decoderId = 0;
        m_initialized = false;
        m_inputEnded = false;
        m_streamData.clear();
        m_headerStreamData.clear();
        m_headerUnits.clear();
    }

    void scheduleFinish(const QString &reason)
    {
        if (m_finishScheduled || m_stage == Complete || m_stage == Idle)
            return;
        m_finishScheduled = true;
        m_finishReason = reason;
        writeEvent(QString::fromLatin1("CASE_FINISH_SCHEDULED"),
                   QString::fromLatin1("reason=%1").arg(reason));
        QTimer::singleShot(0, this, SLOT(finishCurrentCase()));
    }

    void stage(const QString &name, TInt error, const QString &fields = QString())
    {
        const QString details = QString::fromLatin1("status=%1\terror=%2%3")
            .arg(stageText(error)).arg(error)
            .arg(fields.isEmpty() ? QString() : QString::fromLatin1("\t") + fields);
        writeEvent(name, details);
        qDebug() << "WW:HWCAP_STAGE"
                 << (m_caseIndex >= 0 && m_caseIndex < m_cases.size()
                        ? m_cases.at(m_caseIndex).id : QString::fromLatin1("NONE"))
                 << name << error << fields;
    }

    void writeEvent(const QString &event, const QString &fields)
    {
        const QString caseId = m_caseIndex >= 0 && m_caseIndex < m_cases.size()
            ? m_cases.at(m_caseIndex).id : QString::fromLatin1("-");
        const QString line = QString::fromLatin1("%1\t%2\t%3\t%4\n")
            .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate))
            .arg(caseId).arg(event).arg(fields);
        if (m_log.isOpen()) {
            m_log.write(line.toUtf8());
            m_log.flush();
        }
    }

    void writeSummary()
    {
        const CaseSpec &spec = m_cases.at(m_caseIndex);
        QString verdict = QString::fromLatin1("INCOMPLETE");
        if (stageFailed(m_result.fileError) || stageFailed(m_result.hashError) ||
            stageFailed(m_result.headerFileError) ||
            stageFailed(m_result.headerHashError) ||
            stageFailed(m_result.createError) || stageFailed(m_result.infoError) ||
            stageFailed(m_result.selectError) || stageFailed(m_result.inputError)) {
            verdict = QString::fromLatin1("PREFLIGHT_ERROR");
        } else if (stageFailed(m_result.headerError)) {
            verdict = QString::fromLatin1("HEADER_REJECT");
        } else if (stageFailed(m_result.configureError)) {
            verdict = QString::fromLatin1("CONFIG_REJECT");
        } else if (stageFailed(m_result.outputListError) ||
                   stageFailed(m_result.outputSetError) ||
                   stageFailed(m_result.destinationError)) {
            verdict = QString::fromLatin1("OUTPUT_SETUP_REJECT");
        } else if (stageFailed(m_result.initializeError)) {
            verdict = QString::fromLatin1("INIT_REJECT");
        } else if (stageFailed(m_result.bufferError) ||
                   stageFailed(m_result.writeError)) {
            verdict = QString::fromLatin1("INPUT_SUBMIT_REJECT");
        } else if (stageFailed(m_result.fatalError)) {
            verdict = QString::fromLatin1("RUNTIME_FATAL");
        } else if (m_result.picturesOutput > 0) {
            verdict = QString::fromLatin1("PICTURES_OUTPUT_CRC_PENDING");
        } else if (m_result.streamEnd) {
            verdict = QString::fromLatin1("STREAM_END_NO_PICTURE");
        }

        QString lowerEvidence = QString::fromLatin1(
            "HWDEVICE_UID_ACCELERATED_ONLY");
        if (m_schema == 4 &&
            spec.group == QString::fromLatin1("SPLIT_R7") &&
            m_result.accessUnitsWritten > 0) {
            lowerEvidence = QString::fromLatin1(
                "ORIGINAL_R7_VENDOR_WRITE_ACCEPTED_RCAM_CALLCHAIN_STATIC_CHIP_UNPROVEN");
        }

        QString fields = QString::fromLatin1(
            "sha=%1\tprofile=%2\tlevel=%3\trefs=%4\tdpb=%5\treorder=%6\tweightp=%7\tweightb=%8"
            "\tcreate=%9\tinfo=%10\taccelerated=%11\tselect=%12\tinput=%13\theader=%14"
            "\theader_size=%15x%16\tconfigure=%17\toutput_list=%18\toutput_set=%19"
            "\tdestination=%20\tinitialize=%21\tau_parsed=%22\tau_written=%23"
            "\tfirst_picture=%24\tpictures_output=%25\tpicture_loss=%26\tslice_loss=%27"
            "\tfatal=%28\tstream_end=%29\tpictures_decoded=%30\tpictures_displayed=%31"
            "\tpictures_skipped=%32\tpictures_total=%33\tpackets_lost=%34\tpackets_total=%35"
            "\tfirst_crc=%36\trolling_raw_crc=%37\tcrc_match=UNKNOWN"
            "\tlower_evidence=%38\tverdict=%39\treason=%40")
            .arg(QString::fromLatin1(spec.h264Sha256))
            .arg(spec.profile).arg(spec.level).arg(spec.refs).arg(spec.dpb)
            .arg(spec.reorder).arg(spec.weightP).arg(spec.weightB)
            .arg(stageText(m_result.createError))
            .arg(stageText(m_result.infoError))
            .arg(m_result.accelerated ? 1 : 0)
            .arg(stageText(m_result.selectError))
            .arg(stageText(m_result.inputError))
            .arg(stageText(m_result.headerError))
            .arg(m_result.headerWidth).arg(m_result.headerHeight)
            .arg(stageText(m_result.configureError))
            .arg(stageText(m_result.outputListError))
            .arg(stageText(m_result.outputSetError))
            .arg(stageText(m_result.destinationError))
            .arg(stageText(m_result.initializeError))
            .arg(m_result.accessUnitsParsed)
            .arg(m_result.accessUnitsWritten)
            .arg(stageText(m_result.firstPictureError))
            .arg(m_result.picturesOutput)
            .arg(m_result.pictureLoss).arg(m_result.sliceLoss)
            .arg(stageText(m_result.fatalError))
            .arg(m_result.streamEnd ? 1 : 0)
            .arg(m_result.picturesDecoded)
            .arg(m_result.picturesDisplayed)
            .arg(m_result.picturesSkipped)
            .arg(m_result.picturesTotal)
            .arg(m_result.packetsLost).arg(m_result.packetsTotal)
            .arg(crcText(m_result.firstPictureCrc))
            .arg(crcText(m_result.rollingCrc))
            .arg(lowerEvidence).arg(verdict).arg(m_result.finishReason);
        fields += QString::fromLatin1(
            "\toutput_data=%1\toutput_pattern=%2\toutput_layout=%3"
            "\tfirst_bytes=%4\tfirst_size=%5x%6\tfirst_format=%7\tgolden_sha=%8"
            "\tgroup=%9\teos_nal=%10\theader_source=%11\theader_sha=%12"
            "\theader_refs=%13\tsubmit_source=%14\theader_file_status=%15"
            "\theader_hash_status=%16")
            .arg(m_result.outputDataFormat)
            .arg(m_result.outputPattern)
            .arg(m_result.outputLayout)
            .arg(m_result.firstPictureBytes)
            .arg(m_result.firstPictureWidth)
            .arg(m_result.firstPictureHeight)
            .arg(m_result.firstPictureFormat)
            .arg(QString::fromLatin1(spec.goldenSha256))
            .arg(spec.group).arg(m_result.eosNalCount)
            .arg(spec.headerH264File)
            .arg(QString::fromLatin1(spec.headerH264Sha256))
            .arg(spec.headerRefs).arg(spec.h264File)
            .arg(stageText(m_result.headerFileError))
            .arg(stageText(m_result.headerHashError));
        writeEvent(QString::fromLatin1("SUMMARY"), fields);
        qDebug() << "WW:HWCAP_SUMMARY" << spec.id << verdict
                 << m_result.finishReason << m_result.accessUnitsWritten
                 << m_result.picturesOutput;
    }

    void setStatus(const QString &text)
    {
        m_status->setText(text);
        m_status->repaint();
    }

    // MMMFDevVideoPlayObserver
    virtual void MdvpoNewBuffers()
    {
        feedInput();
    }

    virtual void MdvpoReturnPicture(TVideoPicture *)
    {
    }

    virtual void MdvpoSupplementalInformation(
        const TDesC8 &,
        const TTimeIntervalMicroSeconds &,
        const TPictureId &)
    {
    }

    virtual void MdvpoPictureLoss()
    {
        ++m_result.pictureLoss;
        stage(QString::fromLatin1("PICTURE_LOSS"), KErrNone);
    }

    virtual void MdvpoPictureLoss(const TArray<TPictureId> &pictures)
    {
        m_result.pictureLoss += pictures.Count();
        stage(QString::fromLatin1("PICTURE_LOSS_LIST"), KErrNone,
              QString::fromLatin1("count=%1").arg(pictures.Count()));
    }

    virtual void MdvpoSliceLoss(
        TUint firstMacroblock,
        TUint macroblocks,
        const TPictureId &)
    {
        ++m_result.sliceLoss;
        stage(QString::fromLatin1("SLICE_LOSS"), KErrNone,
              QString::fromLatin1("first=%1 count=%2")
              .arg(firstMacroblock).arg(macroblocks));
    }

    virtual void MdvpoReferencePictureSelection(const TDesC8 &)
    {
    }

    virtual void MdvpoTimedSnapshotComplete(
        TInt,
        TPictureData *,
        const TTimeIntervalMicroSeconds &,
        const TPictureId &)
    {
    }

    virtual void MdvpoNewPictures()
    {
        if (!m_devVideo || !m_initialized || m_finishScheduled)
            return;
        TUint available = 0;
        TTimeIntervalMicroSeconds earliest;
        TTimeIntervalMicroSeconds latest;
        m_devVideo->GetNewPictureInfo(available, earliest, latest);
        TUint index;
        for (index = 0; index < available; ++index) {
            TVideoPicture *picture = 0;
            TRAPD(pictureError, picture = m_devVideo->NextPictureL());
            if (pictureError != KErrNone || !picture) {
                stage(QString::fromLatin1("NEXT_PICTURE"),
                      pictureError != KErrNone ? pictureError : KErrUnderflow);
                scheduleFinish(QString::fromLatin1("next-picture"));
                return;
            }
            QByteArray snapshot;
            TInt bytes = 0;
            quint32 frameCrc = 0;
            if (picture->iData.iRawData) {
                const TDesC8 &raw = *picture->iData.iRawData;
                bytes = raw.Length();
                snapshot = QByteArray(
                    reinterpret_cast<const char *>(raw.Ptr()), raw.Length());
                quint32 frameState = crc32Update(
                    0xFFFFFFFFU,
                    reinterpret_cast<const unsigned char *>(snapshot.constData()),
                    snapshot.size());
                frameCrc = frameState ^ 0xFFFFFFFFU;
                m_crcState = crc32Update(
                    m_crcState,
                    reinterpret_cast<const unsigned char *>(snapshot.constData()),
                    snapshot.size());
            }
            const TInt pictureIndex = m_result.picturesOutput;
            if (pictureIndex == 0) {
                m_result.firstPictureError = KErrNone;
                m_result.firstPictureBytes = bytes;
                m_result.firstPictureWidth = picture->iData.iDataSize.iWidth;
                m_result.firstPictureHeight = picture->iData.iDataSize.iHeight;
                m_result.firstPictureFormat = static_cast<TUint32>(
                    picture->iData.iDataFormat);
                m_result.firstPictureCrc = frameCrc;
            }
            ++m_result.picturesOutput;
            const TInt64 timestamp =
                (picture->iOptions & TVideoPicture::ETimestamp) != 0
                ? picture->iTimestamp.Int64() : -1;
            stage(QString::fromLatin1("PICTURE"), KErrNone,
                  QString::fromLatin1(
                      "index=%1 bytes=%2 size=%3x%4 crop=%5x%6 format=%7 timestamp=%8 crc=%9")
                  .arg(pictureIndex).arg(bytes)
                  .arg(picture->iData.iDataSize.iWidth)
                  .arg(picture->iData.iDataSize.iHeight)
                  .arg(picture->iCropRect.Width())
                  .arg(picture->iCropRect.Height())
                  .arg(static_cast<TUint32>(picture->iData.iDataFormat))
                  .arg(static_cast<qint64>(timestamp)).arg(crcText(frameCrc)));
            m_devVideo->ReturnPicture(picture);
            saveFrame(snapshot, pictureIndex);
        }
        feedInput();
    }

    virtual void MdvpoFatalError(TInt error)
    {
        m_result.fatalError = error;
        stage(QString::fromLatin1("FATAL"), error);
        scheduleFinish(QString::fromLatin1("fatal-callback"));
    }

    virtual void MdvpoInitComplete(TInt error)
    {
        m_watchdog.stop();
        m_result.initializeError = error;
        stage(QString::fromLatin1("INITIALIZE"), error);
        if (error != KErrNone || !m_devVideo) {
            scheduleFinish(QString::fromLatin1("initialize"));
            return;
        }
        m_initialized = true;
        m_stage = Decoding;
        m_devVideo->Start();
        stage(QString::fromLatin1("START"), KErrNone);
        m_watchdog.start(KDecodeTimeoutMs);
        feedInput();
    }

    virtual void MdvpoStreamEnd()
    {
        m_result.streamEnd = true;
        stage(QString::fromLatin1("STREAM_END"), KErrNone,
              QString::fromLatin1("au=%1 pictures=%2")
              .arg(m_result.accessUnitsWritten).arg(m_result.picturesOutput));
        scheduleFinish(QString::fromLatin1("stream-end"));
    }

    QLabel *m_status;
    QPushButton *m_startButton;
    QTimer m_watchdog;
    QFile m_log;
    QString m_baseDirectory;
    QString m_runDirectory;
    QVector<CaseSpec> m_cases;
    QByteArray m_streamData;
    QVector<QByteArray> m_units;
    QByteArray m_headerStreamData;
    QVector<QByteArray> m_headerUnits;
    CMMFDevVideoPlay *m_devVideo;
    THwDeviceId m_decoderId;
    TUncompressedVideoFormat m_outputFormat;
    CaseResult m_result;
    int m_caseIndex;
    int m_schema;
    QString m_matrixName;
    Stage m_stage;
    int m_nextUnit;
    bool m_finishScheduled;
    bool m_initialized;
    bool m_inputEnded;
    quint32 m_crcState;
    QString m_finishReason;
};

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(
        QString::fromLatin1("NIKINIKI H264 HwCap Probe"));
    application.setQuitOnLastWindowClosed(true);

    DevVideoCapabilityProbe probe;
    probe.showMaximized();
    return application.exec();
}

#include "main.moc"
