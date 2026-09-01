#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileInfoList>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>
#include <QtGui/QApplication>
#include <QtGui/QLabel>
#include <QtGui/QPushButton>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWidget>

#ifdef Q_OS_SYMBIAN
#include <baspi.h>
#include <barsc2.h>
#include <barsread2.h>
#include <e32base.h>
#include <ecom/ecom.h>
#include <ecom/implementationinformation.h>
#include <hash.h>
#else
#error This read-only research probe must only be built for Symbian.
#endif

namespace {

const TUid KDevVideoDecoderInterfaceUid = { 0x101FB4BE };
const TUid KBroadcomDecoderImplementationUid = { 0x10204C21 };
const TUid KMdfProcessingUnitInterfaceUid = { 0x10273789 };
const TInt KEComResourceFormatV2 = 0x101FB0B9;
const TInt KEComResourceFormatV3 = 0x10009E47;
const TInt KEComSpiFileTypeUid = 0x10205C2C;
_LIT(KEComSpiDirectory, "Z:\\private\\10009D8F\\");
_LIT(KEComSpiBaseName, "ecom");

struct RegistrationImplementation
{
    quint32 interfaceUid;
    quint32 implementationUid;
    int version;
    QString displayName;
    QByteArray defaultData;
    QByteArray opaqueData;
    QStringList extendedInterfaces;
    int romOnly;
};

struct RegistrationData
{
    RegistrationData()
        : format(0), dllUid(0), interfaceCount(0), targetFound(false)
    {
    }

    int format;
    quint32 dllUid;
    int interfaceCount;
    QList<RegistrationImplementation> implementations;
    bool targetFound;
};

QString uidText(TInt value)
{
    return QString::fromLatin1("0x") +
        QString::number(static_cast<quint32>(value), 16)
            .rightJustified(8, QLatin1Char('0')).toUpper();
}

QString driveText(const TDriveUnit &drive)
{
    const TInt number = drive;
    if (number >= 0 && number < 26)
        return QString(QChar(QLatin1Char('A').unicode() + number)) + QLatin1Char(':');
    return QString::fromLatin1("drive-%1").arg(number);
}

QString descriptorText(const TDesC &value)
{
    return QString::fromUtf16(
        reinterpret_cast<const ushort *>(value.Ptr()), value.Length());
}

QByteArray descriptorBytes(const TDesC8 &value)
{
    return QByteArray(reinterpret_cast<const char *>(value.Ptr()), value.Length());
}

QString readTextL(RResourceReader &reader)
{
    HBufC *value = reader.ReadHBufCL();
    if (!value)
        return QString();
    CleanupStack::PushL(value);
    const QString result = descriptorText(*value);
    CleanupStack::PopAndDestroy(value);
    return result;
}

QByteArray readBytesL(RResourceReader &reader)
{
    HBufC8 *value = reader.ReadHBufC8L();
    if (!value)
        return QByteArray();
    CleanupStack::PushL(value);
    const QByteArray result = descriptorBytes(*value);
    CleanupStack::PopAndDestroy(value);
    return result;
}

QByteArray readV3Type1DataL(RResourceReader &reader)
{
    const TUint count = reader.ReadUint8L();
    if (count > 2)
        User::Leave(KErrCorrupt);
    QByteArray result;
    TUint index;
    for (index = 0; index < count; ++index)
        result.append(readBytesL(reader));
    return result;
}

QByteArray readV3Type2DataL(RResourceReader &reader)
{
    const TUint length = reader.ReadUint16L();
    if (length > 512)
        User::Leave(KErrCorrupt);
    HBufC8 *value = HBufC8::NewLC(length);
    TPtr8 pointer(value->Des());
    if (length > 0) {
        reader.ReadL(const_cast<TUint8 *>(pointer.Ptr()), length);
        pointer.SetLength(length);
    }
    const QByteArray result = descriptorBytes(pointer);
    CleanupStack::PopAndDestroy(value);
    return result;
}

void parseRegistrationReaderL(RResourceReader &reader, RegistrationData *result)
{
    if (!result)
        User::Leave(KErrArgument);
    const TInt first = reader.ReadInt32L();
    if (first == KEComResourceFormatV2) {
        result->format = 2;
        result->dllUid = static_cast<quint32>(reader.ReadInt32L());
    } else if (first == KEComResourceFormatV3) {
        result->format = 3;
        result->dllUid = static_cast<quint32>(reader.ReadInt32L());
    } else {
        result->format = 1;
        result->dllUid = static_cast<quint32>(first);
    }

    const TInt interfaceCount = reader.ReadInt16L();
    if (interfaceCount < 0 || interfaceCount > 64)
        User::Leave(KErrCorrupt);
    result->interfaceCount = interfaceCount;
    TInt interfaceIndex;
    for (interfaceIndex = 0; interfaceIndex < interfaceCount; ++interfaceIndex) {
        const quint32 interfaceUid = static_cast<quint32>(reader.ReadInt32L());
        const TInt implementationCount = reader.ReadInt16L();
        if (implementationCount < 0 || implementationCount > 256)
            User::Leave(KErrCorrupt);
        TInt implementationIndex;
        for (implementationIndex = 0;
             implementationIndex < implementationCount;
             ++implementationIndex) {
            RegistrationImplementation implementation;
            implementation.interfaceUid = interfaceUid;
            implementation.romOnly = 0;
            TInt infoFormat = 0;
            if (result->format == 3)
                infoFormat = reader.ReadInt8L();
            implementation.implementationUid =
                static_cast<quint32>(reader.ReadInt32L());
            implementation.version = reader.ReadInt8L();
            implementation.displayName = readTextL(reader);
            if (result->format == 3) {
                if (infoFormat == 1) {
                    implementation.defaultData = readV3Type1DataL(reader);
                    implementation.opaqueData = readV3Type1DataL(reader);
                } else if (infoFormat == 2) {
                    implementation.defaultData = readV3Type2DataL(reader);
                    implementation.opaqueData = readV3Type2DataL(reader);
                } else {
                    User::Leave(KErrNotSupported);
                }
                const TUint extendedCount = reader.ReadUint16L();
                if (extendedCount > 64)
                    User::Leave(KErrCorrupt);
                TUint extendedIndex;
                for (extendedIndex = 0; extendedIndex < extendedCount; ++extendedIndex)
                    implementation.extendedInterfaces.append(
                        uidText(reader.ReadInt32L()));
                implementation.romOnly = (reader.ReadInt8L() & 1) ? 1 : 0;
            } else {
                implementation.defaultData = readBytesL(reader);
                implementation.opaqueData = readBytesL(reader);
                if (result->format == 2)
                    implementation.romOnly = reader.ReadInt8L() ? 1 : 0;
            }
            if (implementation.implementationUid ==
                static_cast<quint32>(KBroadcomDecoderImplementationUid.iUid)) {
                result->targetFound = true;
            }
            result->implementations.append(implementation);
        }
    }
}

void parseRegistrationResourceFileL(
    CResourceFile *resourceFile, RegistrationData *result)
{
    if (!resourceFile || !result)
        User::Leave(KErrArgument);
    RResourceReader reader;
    reader.OpenLC(resourceFile, 1);
    parseRegistrationReaderL(reader, result);
    CleanupStack::PopAndDestroy(&reader);
}

void parseRegistrationResourceL(const QByteArray &raw, RegistrationData *result)
{
    if (!result)
        User::Leave(KErrArgument);
    const TPtrC8 rawView(
        reinterpret_cast<const TUint8 *>(raw.constData()), raw.size());
    CResourceFile *resourceFile = CResourceFile::NewL(rawView);
    CleanupStack::PushL(resourceFile);
    parseRegistrationResourceFileL(resourceFile, result);

    CleanupStack::PopAndDestroy(resourceFile);
}

QString cleanField(QString value)
{
    value.replace(QLatin1Char('\t'), QLatin1Char(' '));
    value.replace(QLatin1Char('\r'), QLatin1Char(' '));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return value.trimmed();
}

QString byteHex(const QByteArray &value)
{
    return QString::fromLatin1(value.toHex().toUpper());
}

QByteArray uidLittleEndian(quint32 value)
{
    QByteArray result;
    result.append(static_cast<char>(value & 0xff));
    result.append(static_cast<char>((value >> 8) & 0xff));
    result.append(static_cast<char>((value >> 16) & 0xff));
    result.append(static_cast<char>((value >> 24) & 0xff));
    return result;
}

QByteArray uidBigEndian(quint32 value)
{
    QByteArray result;
    result.append(static_cast<char>((value >> 24) & 0xff));
    result.append(static_cast<char>((value >> 16) & 0xff));
    result.append(static_cast<char>((value >> 8) & 0xff));
    result.append(static_cast<char>(value & 0xff));
    return result;
}

QString offsetsText(const QByteArray &data, const QByteArray &pattern)
{
    QStringList offsets;
    int offset = data.indexOf(pattern);
    while (offset >= 0) {
        offsets.append(QString::number(offset));
        offset = data.indexOf(pattern, offset + 1);
    }
    return offsets.isEmpty() ? QString::fromLatin1("-") : offsets.join(QLatin1String(","));
}

QStringList asciiStrings(const QByteArray &data, int minimum, int maximumCount)
{
    QStringList result;
    QByteArray current;
    int index;
    for (index = 0; index <= data.size(); ++index) {
        const int value = index < data.size() ? static_cast<unsigned char>(data.at(index)) : 0;
        if (value >= 32 && value <= 126) {
            if (current.size() < 160)
                current.append(static_cast<char>(value));
        } else {
            if (current.size() >= minimum)
                result.append(cleanField(QString::fromLatin1(current)));
            current.clear();
            if (result.size() >= maximumCount)
                break;
        }
    }
    return result;
}

QStringList utf16LeStrings(const QByteArray &data, int minimum, int maximumCount)
{
    QStringList result;
    QString current;
    int index;
    for (index = 0; index + 1 < data.size(); index += 2) {
        const int low = static_cast<unsigned char>(data.at(index));
        const int high = static_cast<unsigned char>(data.at(index + 1));
        const ushort value = static_cast<ushort>(low | (high << 8));
        if (value >= 32 && value < 127) {
            if (current.size() < 160)
                current.append(QChar(value));
        } else {
            if (current.size() >= minimum)
                result.append(cleanField(current));
            current.clear();
            if (result.size() >= maximumCount)
                break;
        }
    }
    if (current.size() >= minimum && result.size() < maximumCount)
        result.append(cleanField(current));
    return result;
}

bool hashFile(const QString &path, QByteArray *digest, qint64 *size, QString *error)
{
    CSHA2 *hash = 0;
    TRAPD(createError, hash = CSHA2::NewL(E256Bit));
    if (createError != KErrNone || !hash) {
        if (error)
            *error = QString::fromLatin1("sha-create-%1").arg(
                createError != KErrNone ? createError : KErrNoMemory);
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        delete hash;
        return false;
    }
    qint64 total = 0;
    while (!file.atEnd()) {
        const QByteArray block = file.read(65536);
        if (block.isEmpty() && file.error() != QFile::NoError) {
            if (error)
                *error = file.errorString();
            delete hash;
            return false;
        }
        total += block.size();
        if (!block.isEmpty()) {
            const TPtrC8 view(
                reinterpret_cast<const TUint8 *>(block.constData()),
                block.size());
            hash->Update(view);
        }
    }
    const TPtrC8 result = hash->Final();
    if (digest) {
        const QByteArray bytes(
            reinterpret_cast<const char *>(result.Ptr()), result.Length());
        *digest = bytes.toHex().toUpper();
    }
    delete hash;
    if (size)
        *size = total;
    return true;
}

bool hashBytes(const QByteArray &data, QByteArray *digest, QString *error)
{
    CSHA2 *hash = 0;
    TRAPD(createError, hash = CSHA2::NewL(E256Bit));
    if (createError != KErrNone || !hash) {
        if (error)
            *error = QString::fromLatin1("sha-create-%1").arg(
                createError != KErrNone ? createError : KErrNoMemory);
        return false;
    }
    if (!data.isEmpty()) {
        const TPtrC8 view(
            reinterpret_cast<const TUint8 *>(data.constData()), data.size());
        hash->Update(view);
    }
    const TPtrC8 result = hash->Final();
    if (digest) {
        const QByteArray bytes(
            reinterpret_cast<const char *>(result.Ptr()), result.Length());
        *digest = bytes.toHex().toUpper();
    }
    delete hash;
    return true;
}

void writeEvent(QTextStream &stream, const QString &event, const QString &fields)
{
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
           << QLatin1Char('\t') << event << QLatin1Char('\t') << fields << QLatin1Char('\n');
    stream.flush();
}

} // namespace

class AuditWidget : public QWidget
{
    Q_OBJECT

public:
    AuditWidget()
        : m_status(new QLabel(this)), m_button(new QPushButton(
              QString::fromLatin1("Export read-only ECom audit"), this))
    {
        setWindowTitle(QString::fromLatin1("NIKINIKI DevVideo ECom Audit"));
        QVBoxLayout *layout = new QVBoxLayout(this);
        QLabel *description = new QLabel(QString::fromLatin1(
            "Enumerates the published DevVideo decoder ECom interface and scans "
            "ROM registration resources for implementation UID 0x10204C21.\n\n"
            "No decoder is created. No ROM, DLL, RSC or setting is modified."), this);
        description->setWordWrap(true);
        m_status->setWordWrap(true);
        m_status->setText(QString::fromLatin1("Ready. Results will be written under F:/Data first."));
        layout->addWidget(description);
        layout->addWidget(m_button);
        layout->addWidget(m_status);
        layout->addStretch();
        connect(m_button, SIGNAL(clicked()), this, SLOT(runAudit()));
        resize(360, 520);
    }

private slots:
    void runAudit()
    {
        m_button->setEnabled(false);
        m_status->setText(QString::fromLatin1("Opening result file..."));
        QApplication::processEvents();

        const QString runId = QDateTime::currentDateTime().toString(
            QString::fromLatin1("yyyyMMdd-HHmmss"));
        const QStringList roots = QStringList()
            << QString::fromLatin1("F:")
            << QString::fromLatin1("E:")
            << QString::fromLatin1("C:");
        QFile log;
        QString runDirectory;
        int rootIndex;
        for (rootIndex = 0; rootIndex < roots.size(); ++rootIndex) {
            const QString candidate = roots.at(rootIndex) +
                QString::fromLatin1("/Data/NIKINIKI/hwcap/ecom-audit/") + runId;
            if (!QDir().mkpath(candidate))
                continue;
            log.setFileName(QDir(candidate).filePath(QString::fromLatin1("inventory.tsv")));
            if (log.open(QIODevice::WriteOnly | QIODevice::Text)) {
                runDirectory = candidate;
                break;
            }
        }
        if (!log.isOpen()) {
            m_status->setText(QString::fromLatin1("Cannot create inventory.tsv on F:, E: or C:."));
            m_button->setEnabled(true);
            return;
        }

        QTextStream stream(&log);
        stream.setCodec("UTF-8");
        stream << "utc\tevent\tfields\n";
        writeEvent(stream, QString::fromLatin1("AUDIT_BEGIN"),
            QString::fromLatin1("interface_uid=%1 implementation_uid=%2 mode=read-only")
                .arg(uidText(KDevVideoDecoderInterfaceUid.iUid))
                .arg(uidText(KBroadcomDecoderImplementationUid.iUid)));

        m_status->setText(QString::fromLatin1("Enumerating ECom decoder implementations..."));
        QApplication::processEvents();
        RImplInfoPtrArray implementations;
        TRAPD(listError, REComSession::ListImplementationsL(
            KDevVideoDecoderInterfaceUid, implementations));
        writeEvent(stream, QString::fromLatin1("ECOM_LIST"),
            QString::fromLatin1("status=%1 count=%2")
                .arg(listError).arg(implementations.Count()));
        bool targetListed = false;
        if (listError == KErrNone) {
            int index;
            for (index = 0; index < implementations.Count(); ++index) {
                CImplementationInformation *info = implementations[index];
                const bool target = info->ImplementationUid() ==
                    KBroadcomDecoderImplementationUid;
                if (target)
                    targetListed = true;
                const QByteArray dataType = descriptorBytes(info->DataType());
                const QByteArray opaque = descriptorBytes(info->OpaqueData());
                RExtendedInterfacesArray extended;
                TRAPD(extendedError, info->GetExtendedInterfaceListL(extended));
                QStringList extendedUids;
                if (extendedError == KErrNone) {
                    int extendedIndex;
                    for (extendedIndex = 0; extendedIndex < extended.Count(); ++extendedIndex)
                        extendedUids.append(uidText(extended[extendedIndex].iUid));
                }
                writeEvent(stream, QString::fromLatin1("IMPLEMENTATION"),
                    QString::fromLatin1(
                        "uid=%1 target=%2 version=%3 display=%4 drive=%5 rom_only=%6 "
                        "rom_based=%7 disabled=%8 vendor_id=%9 datatype_hex=%10 "
                        "opaque_hex=%11 extended_status=%12 extended=%13 "
                        "datatype_text=%14 opaque_text=%15")
                        .arg(uidText(info->ImplementationUid().iUid))
                        .arg(target ? 1 : 0)
                        .arg(info->Version())
                        .arg(cleanField(descriptorText(info->DisplayName())))
                        .arg(driveText(info->Drive()))
                        .arg(info->RomOnly() ? 1 : 0)
                        .arg(info->RomBased() ? 1 : 0)
                        .arg(info->Disabled() ? 1 : 0)
                        .arg(uidText(info->VendorId().iId))
                        .arg(byteHex(dataType))
                        .arg(byteHex(opaque))
                        .arg(extendedError)
                        .arg(extendedUids.isEmpty() ? QString::fromLatin1("-") :
                            extendedUids.join(QLatin1String(",")))
                        .arg(cleanField(QString::fromLatin1(dataType)))
                        .arg(cleanField(QString::fromLatin1(opaque))));
                extended.Close();
            }
        }
        implementations.ResetAndDestroy();
        implementations.Close();
        REComSession::FinalClose();

        m_status->setText(QString::fromLatin1("Checking MDF Processing Unit registry..."));
        QApplication::processEvents();
        RImplInfoPtrArray processingUnits;
        TRAPD(puListError, REComSession::ListImplementationsL(
            KMdfProcessingUnitInterfaceUid, processingUnits));
        bool targetPuListed = false;
        writeEvent(stream, QString::fromLatin1("MDF_PU_LIST"),
            QString::fromLatin1("interface_uid=%1 status=%2 count=%3")
                .arg(uidText(KMdfProcessingUnitInterfaceUid.iUid))
                .arg(puListError).arg(processingUnits.Count()));
        if (puListError == KErrNone) {
            int puIndex;
            for (puIndex = 0; puIndex < processingUnits.Count(); ++puIndex) {
                CImplementationInformation *info = processingUnits[puIndex];
                const bool target = info->ImplementationUid() ==
                    KBroadcomDecoderImplementationUid;
                if (target)
                    targetPuListed = true;
                if (target) {
                    const QByteArray dataType = descriptorBytes(info->DataType());
                    const QByteArray opaque = descriptorBytes(info->OpaqueData());
                    writeEvent(stream, QString::fromLatin1("MDF_TARGET"),
                        QString::fromLatin1(
                            "uid=%1 version=%2 display=%3 drive=%4 rom_only=%5 "
                            "rom_based=%6 vendor_id=%7 datatype_hex=%8 opaque_hex=%9 "
                            "datatype_text=%10 opaque_text=%11")
                            .arg(uidText(info->ImplementationUid().iUid))
                            .arg(info->Version())
                            .arg(cleanField(descriptorText(info->DisplayName())))
                            .arg(driveText(info->Drive()))
                            .arg(info->RomOnly() ? 1 : 0)
                            .arg(info->RomBased() ? 1 : 0)
                            .arg(uidText(info->VendorId().iId))
                            .arg(byteHex(dataType)).arg(byteHex(opaque))
                            .arg(cleanField(QString::fromLatin1(dataType)))
                            .arg(cleanField(QString::fromLatin1(opaque))));
                }
            }
        }
        writeEvent(stream, QString::fromLatin1("MDF_PU_RESULT"),
            QString::fromLatin1("target_uid=%1 target_listed=%2")
                .arg(uidText(KBroadcomDecoderImplementationUid.iUid))
                .arg(targetPuListed ? 1 : 0));
        processingUnits.ResetAndDestroy();
        processingUnits.Close();
        REComSession::FinalClose();

        m_status->setText(QString::fromLatin1("Scanning Z:/resource/plugins/*.rsc..."));
        QApplication::processEvents();
        const QDir pluginDirectory(QString::fromLatin1("Z:/resource/plugins"));
        const QFileInfoList files = pluginDirectory.entryInfoList(
            QStringList() << QString::fromLatin1("*.rsc"), QDir::Files, QDir::Name);
        writeEvent(stream, QString::fromLatin1("RSC_SCAN_BEGIN"),
            QString::fromLatin1("path=Z:/resource/plugins files=%1 readable=%2")
                .arg(files.size()).arg(pluginDirectory.isReadable() ? 1 : 0));
        const QByteArray implementationLe = uidLittleEndian(0x10204C21u);
        const QByteArray implementationBe = uidBigEndian(0x10204C21u);
        const QByteArray interfaceLe = uidLittleEndian(0x101FB4BEu);
        const QByteArray interfaceBe = uidBigEndian(0x101FB4BEu);
        int matchedResources = 0;
        int rawMatchedResources = 0;
        int parseErrors = 0;
        int readErrors = 0;
        int fileIndex;
        for (fileIndex = 0; fileIndex < files.size(); ++fileIndex) {
            if ((fileIndex & 63) == 0)
                QApplication::processEvents();
            QFile resource(files.at(fileIndex).absoluteFilePath());
            if (!resource.open(QIODevice::ReadOnly)) {
                ++readErrors;
                continue;
            }
            const QByteArray data = resource.readAll();
            const bool implementationMatch = data.contains(implementationLe) ||
                data.contains(implementationBe);
            if (implementationMatch)
                ++rawMatchedResources;
            RegistrationData registration;
            TRAPD(registrationError,
                parseRegistrationResourceL(data, &registration));
            if (registrationError != KErrNone) {
                ++parseErrors;
                writeEvent(stream, QString::fromLatin1("RSC_PARSE_ERROR"),
                    QString::fromLatin1("path=%1 status=%2 raw_uid_match=%3")
                        .arg(cleanField(files.at(fileIndex).absoluteFilePath()))
                        .arg(registrationError).arg(implementationMatch ? 1 : 0));
                continue;
            }
            writeEvent(stream, QString::fromLatin1("RSC_PARSED"),
                QString::fromLatin1(
                    "path=%1 format=%2 dll_uid=%3 interfaces=%4 implementations=%5 "
                    "target=%6 raw_uid_match=%7")
                    .arg(cleanField(files.at(fileIndex).absoluteFilePath()))
                    .arg(registration.format).arg(uidText(registration.dllUid))
                    .arg(registration.interfaceCount)
                    .arg(registration.implementations.size())
                    .arg(registration.targetFound ? 1 : 0)
                    .arg(implementationMatch ? 1 : 0));
            if (!registration.targetFound)
                continue;
            ++matchedResources;
            QByteArray digest;
            QString digestError;
            const bool digestOk = hashBytes(data, &digest, &digestError);
            const QString ascii = asciiStrings(data, 4, 40).join(QLatin1String("|"));
            const QString utf16 = utf16LeStrings(data, 4, 40).join(QLatin1String("|"));
            writeEvent(stream, QString::fromLatin1("RSC_TARGET"),
                QString::fromLatin1(
                    "path=%1 format=%2 dll_uid=%3 size=%4 sha256=%5 hash_error=%6 "
                    "impl_le=%7 impl_be=%8 interface_le=%9 interface_be=%10 "
                    "ascii=%11 utf16le=%12")
                    .arg(cleanField(files.at(fileIndex).absoluteFilePath()))
                    .arg(registration.format).arg(uidText(registration.dllUid))
                    .arg(data.size())
                    .arg(digestOk ? QString::fromLatin1(digest) : QString::fromLatin1("-"))
                    .arg(digestOk ? QString::fromLatin1("-") : cleanField(digestError))
                    .arg(offsetsText(data, implementationLe))
                    .arg(offsetsText(data, implementationBe))
                    .arg(offsetsText(data, interfaceLe))
                    .arg(offsetsText(data, interfaceBe))
                    .arg(cleanField(ascii)).arg(cleanField(utf16)));

            int registrationIndex;
            for (registrationIndex = 0;
                 registrationIndex < registration.implementations.size();
                 ++registrationIndex) {
                const RegistrationImplementation &implementation =
                    registration.implementations.at(registrationIndex);
                writeEvent(stream, QString::fromLatin1("RSC_IMPLEMENTATION"),
                    QString::fromLatin1(
                        "path=%1 dll_uid=%2 interface_uid=%3 implementation_uid=%4 "
                        "target=%5 version=%6 display=%7 default_hex=%8 opaque_hex=%9 "
                        "extended=%10 rom_only=%11")
                        .arg(cleanField(files.at(fileIndex).absoluteFilePath()))
                        .arg(uidText(registration.dllUid))
                        .arg(uidText(implementation.interfaceUid))
                        .arg(uidText(implementation.implementationUid))
                        .arg(implementation.implementationUid ==
                            static_cast<quint32>(KBroadcomDecoderImplementationUid.iUid) ? 1 : 0)
                        .arg(implementation.version)
                        .arg(cleanField(implementation.displayName))
                        .arg(byteHex(implementation.defaultData))
                        .arg(byteHex(implementation.opaqueData))
                        .arg(implementation.extendedInterfaces.isEmpty() ?
                            QString::fromLatin1("-") :
                            implementation.extendedInterfaces.join(QLatin1String(",")))
                        .arg(implementation.romOnly));
            }

            const QString dllPath = QString::fromLatin1("Z:/sys/bin/") +
                files.at(fileIndex).completeBaseName() + QString::fromLatin1(".dll");
            QByteArray dllDigest;
            qint64 dllSize = 0;
            QString dllError;
            const bool dllReadable = hashFile(dllPath, &dllDigest, &dllSize, &dllError);
            writeEvent(stream, QString::fromLatin1("DLL_CANDIDATE"),
                QString::fromLatin1(
                    "path=%1 basis=same-basename exists=%2 readable=%3 size=%4 sha256=%5 error=%6")
                    .arg(dllPath).arg(QFileInfo(dllPath).exists() ? 1 : 0)
                    .arg(dllReadable ? 1 : 0).arg(dllSize)
                    .arg(dllReadable ? QString::fromLatin1(dllDigest) : QString::fromLatin1("-"))
                    .arg(dllReadable ? QString::fromLatin1("-") : cleanField(dllError)));
        }
        writeEvent(stream, QString::fromLatin1("RSC_SCAN_END"),
            QString::fromLatin1(
                "files=%1 structural_matches=%2 raw_matches=%3 parse_errors=%4 read_errors=%5")
                .arg(files.size()).arg(matchedResources).arg(rawMatchedResources)
                .arg(parseErrors).arg(readErrors));

        m_status->setText(QString::fromLatin1("Checking ROM ECom SPI archive..."));
        QApplication::processEvents();
        TInt spiConnectError = KErrNone;
        TInt spiEntryError = KErrNotReady;
        TInt spiOpenError = KErrNotReady;
        TInt spiType = 0;
        int spiEntries = 0;
        int spiParseErrors = 0;
        int spiTargetMatches = 0;
        RFs fileServer;
        spiConnectError = fileServer.Connect();
        if (spiConnectError == KErrNone) {
            TEntry spiEntry;
            spiEntryError = fileServer.Entry(KEComSpiDirectory, spiEntry);
            writeEvent(stream, QString::fromLatin1("SPI_FILE"),
                QString::fromLatin1(
                    "path=Z:/private/10009D8F/ base=ecom entry_status=%1 size=%2")
                    .arg(spiEntryError)
                    .arg(spiEntryError == KErrNone ? spiEntry.iSize : 0));

            RResourceArchive archive;
            TRAP(spiOpenError,
                archive.OpenL(fileServer, KEComSpiDirectory, KEComSpiBaseName));
            if (spiOpenError == KErrNone) {
                spiType = archive.Type().iUid;
                writeEvent(stream, QString::fromLatin1("SPI_OPEN"),
                    QString::fromLatin1("status=0 type=%1 expected_type=%2")
                        .arg(uidText(spiType)).arg(uidText(KEComSpiFileTypeUid)));
                while (!archive.End() && spiEntries < 4096) {
                    CResourceFile *archiveResource = 0;
                    HBufC *archiveName = 0;
                    TInt nextError = KErrNone;
                    TRAP(nextError,
                        archiveResource = archive.NextL(archiveName));
                    if (nextError != KErrNone || !archiveResource || !archiveName) {
                        delete archiveResource;
                        delete archiveName;
                        writeEvent(stream, QString::fromLatin1("SPI_NEXT_ERROR"),
                            QString::fromLatin1("index=%1 status=%2")
                                .arg(spiEntries).arg(nextError));
                        break;
                    }
                    ++spiEntries;
                    const QString resourceName = cleanField(descriptorText(*archiveName));
                    RegistrationData archiveRegistration;
                    TRAPD(archiveParseError,
                        parseRegistrationResourceFileL(
                            archiveResource, &archiveRegistration));
                    if (archiveParseError != KErrNone) {
                        ++spiParseErrors;
                        writeEvent(stream, QString::fromLatin1("SPI_RSC_PARSE_ERROR"),
                            QString::fromLatin1("name=%1 status=%2")
                                .arg(resourceName).arg(archiveParseError));
                    } else {
                        writeEvent(stream, QString::fromLatin1("SPI_RSC"),
                            QString::fromLatin1(
                                "name=%1 format=%2 dll_uid=%3 interfaces=%4 "
                                "implementations=%5 target=%6")
                                .arg(resourceName).arg(archiveRegistration.format)
                                .arg(uidText(archiveRegistration.dllUid))
                                .arg(archiveRegistration.interfaceCount)
                                .arg(archiveRegistration.implementations.size())
                                .arg(archiveRegistration.targetFound ? 1 : 0));
                        if (archiveRegistration.targetFound) {
                            ++spiTargetMatches;
                            int implementationIndex;
                            for (implementationIndex = 0;
                                 implementationIndex <
                                     archiveRegistration.implementations.size();
                                 ++implementationIndex) {
                                const RegistrationImplementation &implementation =
                                    archiveRegistration.implementations.at(
                                        implementationIndex);
                                writeEvent(stream,
                                    QString::fromLatin1("SPI_IMPLEMENTATION"),
                                    QString::fromLatin1(
                                        "name=%1 dll_uid=%2 interface_uid=%3 "
                                        "implementation_uid=%4 target=%5 version=%6 "
                                        "display=%7 default_hex=%8 opaque_hex=%9 "
                                        "extended=%10 rom_only=%11")
                                        .arg(resourceName)
                                        .arg(uidText(archiveRegistration.dllUid))
                                        .arg(uidText(implementation.interfaceUid))
                                        .arg(uidText(implementation.implementationUid))
                                        .arg(implementation.implementationUid ==
                                            static_cast<quint32>(
                                                KBroadcomDecoderImplementationUid.iUid) ? 1 : 0)
                                        .arg(implementation.version)
                                        .arg(cleanField(implementation.displayName))
                                        .arg(byteHex(implementation.defaultData))
                                        .arg(byteHex(implementation.opaqueData))
                                        .arg(implementation.extendedInterfaces.isEmpty() ?
                                            QString::fromLatin1("-") :
                                            implementation.extendedInterfaces.join(
                                                QLatin1String(",")))
                                        .arg(implementation.romOnly));
                            }
                            const QString dllPath = QString::fromLatin1("Z:/sys/bin/") +
                                resourceName + QString::fromLatin1(".dll");
                            QByteArray dllDigest;
                            qint64 dllSize = 0;
                            QString dllError;
                            const bool dllReadable = hashFile(
                                dllPath, &dllDigest, &dllSize, &dllError);
                            writeEvent(stream, QString::fromLatin1("SPI_DLL_CANDIDATE"),
                                QString::fromLatin1(
                                    "path=%1 basis=archive-name exists=%2 readable=%3 "
                                    "size=%4 sha256=%5 error=%6")
                                    .arg(dllPath)
                                    .arg(QFileInfo(dllPath).exists() ? 1 : 0)
                                    .arg(dllReadable ? 1 : 0).arg(dllSize)
                                    .arg(dllReadable ? QString::fromLatin1(dllDigest) :
                                        QString::fromLatin1("-"))
                                    .arg(dllReadable ? QString::fromLatin1("-") :
                                        cleanField(dllError)));
                        }
                    }
                    delete archiveResource;
                    delete archiveName;
                }
                archive.Close();
            } else {
                writeEvent(stream, QString::fromLatin1("SPI_OPEN"),
                    QString::fromLatin1("status=%1 type=- expected_type=%2")
                        .arg(spiOpenError).arg(uidText(KEComSpiFileTypeUid)));
            }
            fileServer.Close();
        } else {
            writeEvent(stream, QString::fromLatin1("SPI_FILE"),
                QString::fromLatin1(
                    "path=Z:/private/10009D8F/ base=ecom entry_status=%1 size=0")
                    .arg(spiConnectError));
        }
        writeEvent(stream, QString::fromLatin1("SPI_RESULT"),
            QString::fromLatin1(
                "connect_status=%1 entry_status=%2 open_status=%3 type=%4 entries=%5 "
                "parse_errors=%6 target_matches=%7")
                .arg(spiConnectError).arg(spiEntryError).arg(spiOpenError)
                .arg(spiOpenError == KErrNone ? uidText(spiType) : QString::fromLatin1("-"))
                .arg(spiEntries).arg(spiParseErrors).arg(spiTargetMatches));
        writeEvent(stream, QString::fromLatin1("AUDIT_END"),
            QString::fromLatin1(
                "target_listed=%1 target_pu_listed=%2 rsc_matches=%3 "
                "spi_matches=%4 status=COMPLETE")
                .arg(targetListed ? 1 : 0).arg(targetPuListed ? 1 : 0)
                .arg(matchedResources).arg(spiTargetMatches));
        log.close();

        m_status->setText(QString::fromLatin1(
            "Complete. Send this directory:\n%1\n\nTarget listed: %2\n"
            "Target is MDF PU: %3\nLoose RSC matches: %4\nSPI matches: %5")
            .arg(QDir::toNativeSeparators(runDirectory))
            .arg(targetListed ? QString::fromLatin1("YES") : QString::fromLatin1("NO"))
            .arg(targetPuListed ? QString::fromLatin1("YES") : QString::fromLatin1("NO"))
            .arg(matchedResources).arg(spiTargetMatches));
        m_button->setEnabled(true);
    }

private:
    QLabel *m_status;
    QPushButton *m_button;
};

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    AuditWidget widget;
    widget.showMaximized();
    return application.exec();
}

#include "main.moc"
