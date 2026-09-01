#include "platform/flv_live_demuxer.h"

namespace wiliwili {
namespace {

static quint32 readU24(const QByteArray &bytes, int offset)
{
    const unsigned char *p = reinterpret_cast<const unsigned char *>(
        bytes.constData() + offset);
    return (static_cast<quint32>(p[0]) << 16) |
           (static_cast<quint32>(p[1]) << 8) |
           static_cast<quint32>(p[2]);
}

static quint32 readUnsigned(const QByteArray &bytes, int offset, int count)
{
    quint32 result = 0;
    int index;
    for (index = 0; index < count; ++index) {
        result = (result << 8) |
            static_cast<unsigned char>(bytes.at(offset + index));
    }
    return result;
}

static void appendStartCodeAndNal(QByteArray *target, const QByteArray &nal)
{
    target->append('\0');
    target->append('\0');
    target->append('\0');
    target->append('\1');
    target->append(nal);
}

class BitReader
{
public:
    explicit BitReader(const QByteArray &bytes)
        : m_bytes(bytes), m_offset(0), m_valid(true) {}

    bool valid() const { return m_valid; }

    quint32 readBits(int count)
    {
        if (!m_valid || count < 0 || count > 32 ||
            m_offset + count > m_bytes.size() * 8) {
            m_valid = false;
            return 0;
        }
        quint32 value = 0;
        int index;
        for (index = 0; index < count; ++index) {
            const int byteOffset = m_offset >> 3;
            const int shift = 7 - (m_offset & 7);
            value = (value << 1) |
                ((static_cast<unsigned char>(m_bytes.at(byteOffset)) >> shift) & 1);
            ++m_offset;
        }
        return value;
    }

    quint32 readBit() { return readBits(1); }

    quint32 readUe()
    {
        int zeroes = 0;
        while (m_valid && readBit() == 0) {
            if (++zeroes > 31) {
                m_valid = false;
                return 0;
            }
        }
        if (!m_valid || zeroes == 0)
            return 0;
        return ((static_cast<quint32>(1) << zeroes) - 1) +
            readBits(zeroes);
    }

    qint32 readSe()
    {
        const quint32 value = readUe();
        return (value & 1) != 0
            ? static_cast<qint32>((value + 1) / 2)
            : -static_cast<qint32>(value / 2);
    }

private:
    QByteArray m_bytes;
    int m_offset;
    bool m_valid;
};

static QByteArray removeEmulationPrevention(const QByteArray &nal)
{
    QByteArray rbsp;
    if (nal.size() <= 1)
        return rbsp;
    rbsp.reserve(nal.size() - 1);
    int zeroes = 0;
    int index;
    for (index = 1; index < nal.size(); ++index) {
        const unsigned char value =
            static_cast<unsigned char>(nal.at(index));
        if (zeroes >= 2 && value == 3) {
            zeroes = 0;
            continue;
        }
        rbsp.append(static_cast<char>(value));
        zeroes = value == 0 ? zeroes + 1 : 0;
    }
    return rbsp;
}

static void skipScalingList(BitReader *bits, int size)
{
    int lastScale = 8;
    int nextScale = 8;
    int index;
    for (index = 0; index < size && bits->valid(); ++index) {
        if (nextScale != 0)
            nextScale = (lastScale + bits->readSe() + 256) & 255;
        if (nextScale != 0)
            lastScale = nextScale;
    }
}

static bool parseSpsDimensions(
    const QByteArray &nal, int *width, int *height)
{
    BitReader bits(removeEmulationPrevention(nal));
    const quint32 profile = bits.readBits(8);
    bits.readBits(8);
    bits.readBits(8);
    bits.readUe();
    quint32 chromaFormat = 1;
    if (profile == 100 || profile == 110 || profile == 122 ||
        profile == 244 || profile == 44 || profile == 83 ||
        profile == 86 || profile == 118 || profile == 128 ||
        profile == 138 || profile == 139 || profile == 134) {
        chromaFormat = bits.readUe();
        if (chromaFormat == 3)
            bits.readBit();
        bits.readUe();
        bits.readUe();
        bits.readBit();
        if (bits.readBit() != 0) {
            const int count = chromaFormat == 3 ? 12 : 8;
            int index;
            for (index = 0; index < count && bits.valid(); ++index) {
                if (bits.readBit() != 0)
                    skipScalingList(&bits, index < 6 ? 16 : 64);
            }
        }
    }
    bits.readUe();
    const quint32 picOrderCountType = bits.readUe();
    if (picOrderCountType == 0) {
        bits.readUe();
    } else if (picOrderCountType == 1) {
        bits.readBit();
        bits.readSe();
        bits.readSe();
        const quint32 cycle = bits.readUe();
        quint32 index;
        for (index = 0; index < cycle && bits.valid(); ++index)
            bits.readSe();
    }
    bits.readUe();
    bits.readBit();
    const quint32 widthMbsMinus1 = bits.readUe();
    const quint32 heightMapUnitsMinus1 = bits.readUe();
    const quint32 frameMbsOnly = bits.readBit();
    if (frameMbsOnly == 0)
        bits.readBit();
    bits.readBit();
    quint32 cropLeft = 0;
    quint32 cropRight = 0;
    quint32 cropTop = 0;
    quint32 cropBottom = 0;
    if (bits.readBit() != 0) {
        cropLeft = bits.readUe();
        cropRight = bits.readUe();
        cropTop = bits.readUe();
        cropBottom = bits.readUe();
    }
    if (!bits.valid() || chromaFormat > 3)
        return false;
    const int subWidth = chromaFormat == 1 || chromaFormat == 2 ? 2 : 1;
    const int subHeight = chromaFormat == 1 ? 2 : 1;
    const int cropUnitX = chromaFormat == 0 ? 1 : subWidth;
    const int cropUnitY = (chromaFormat == 0 ? 1 : subHeight) *
        (2 - static_cast<int>(frameMbsOnly));
    const int parsedWidth = static_cast<int>((widthMbsMinus1 + 1) * 16) -
        static_cast<int>((cropLeft + cropRight) * cropUnitX);
    const int parsedHeight = static_cast<int>(
        (2 - frameMbsOnly) * (heightMapUnitsMinus1 + 1) * 16) -
        static_cast<int>((cropTop + cropBottom) * cropUnitY);
    if (parsedWidth <= 0 || parsedHeight <= 0 ||
        parsedWidth > 4096 || parsedHeight > 2304)
        return false;
    *width = parsedWidth;
    *height = parsedHeight;
    return true;
}

static qint32 signedU24(quint32 value)
{
    return (value & 0x800000U) != 0
        ? static_cast<qint32>(value | 0xff000000U)
        : static_cast<qint32>(value);
}

} // namespace

FlvLiveDemuxer::FlvLiveDemuxer()
{
    reset();
}

void FlvLiveDemuxer::reset()
{
    m_buffer.clear();
    m_parameterSets.clear();
    m_nalLengthSize = 0;
    m_width = 0;
    m_height = 0;
    m_aacObjectType = 0;
    m_aacSampleRateIndex = 0;
    m_aacSampleRate = 0;
    m_aacChannels = 0;
    m_firstTimestamp = -1;
    m_headerParsed = false;
    m_videoConfigured = false;
    m_audioConfigured = false;
    m_videoStarted = false;
}

bool FlvLiveDemuxer::append(
    const QByteArray &bytes,
    QVector<QByteArray> *videoAccessUnits,
    QVector<qint64> *videoDecodingTimesMilliseconds,
    QVector<qint64> *videoPresentationTimesMilliseconds,
    QByteArray *adtsAudio,
    QString *errorText)
{
    if (!videoAccessUnits || !videoDecodingTimesMilliseconds ||
        !videoPresentationTimesMilliseconds || !adtsAudio) {
        if (errorText)
            *errorText = QString::fromLatin1("null output");
        return false;
    }
    if (!bytes.isEmpty())
        m_buffer.append(bytes);
    if (m_buffer.size() > 8 * 1024 * 1024) {
        if (errorText)
            *errorText = QString::fromLatin1("FLV parser buffer exceeded limit");
        return false;
    }

    if (!m_headerParsed) {
        if (m_buffer.size() < 13)
            return true;
        if (m_buffer.left(3) != QByteArray("FLV", 3) ||
            static_cast<unsigned char>(m_buffer.at(3)) != 1) {
            if (errorText)
                *errorText = QString::fromLatin1("invalid FLV header");
            return false;
        }
        const quint32 dataOffset = readUnsigned(m_buffer, 5, 4);
        if (dataOffset < 9 || dataOffset > 4096) {
            if (errorText)
                *errorText = QString::fromLatin1("invalid FLV data offset");
            return false;
        }
        if (m_buffer.size() < static_cast<int>(dataOffset + 4))
            return true;
        m_buffer.remove(0, static_cast<int>(dataOffset + 4));
        m_headerParsed = true;
    }

    while (m_buffer.size() >= 15) {
        const int tagType = static_cast<unsigned char>(m_buffer.at(0));
        const quint32 dataSize = readU24(m_buffer, 1);
        if (dataSize > 4 * 1024 * 1024) {
            if (errorText)
                *errorText = QString::fromLatin1("FLV tag exceeded limit");
            return false;
        }
        const int completeSize = 11 + static_cast<int>(dataSize) + 4;
        if (m_buffer.size() < completeSize)
            break;
        const quint32 timestamp = readU24(m_buffer, 4) |
            (static_cast<quint32>(
                static_cast<unsigned char>(m_buffer.at(7))) << 24);
        const QByteArray payload = m_buffer.mid(11, dataSize);
        const quint32 previousSize = readUnsigned(
            m_buffer, 11 + static_cast<int>(dataSize), 4);
        if (previousSize != 11 + dataSize) {
            if (errorText)
                *errorText = QString::fromLatin1("invalid FLV previous tag size");
            return false;
        }

        if (tagType == 9 && payload.size() >= 5 &&
            (static_cast<unsigned char>(payload.at(0)) & 15) == 7) {
            const int packetType = static_cast<unsigned char>(payload.at(1));
            if (packetType == 0) {
                if (!parseAvcConfiguration(payload.mid(5), errorText))
                    return false;
            } else if (packetType == 1 && m_videoConfigured) {
                const bool keyFrame =
                    (static_cast<unsigned char>(payload.at(0)) >> 4) == 1;
                if (m_videoStarted || keyFrame) {
                    QByteArray accessUnit;
                    if (!makeAnnexBAccessUnit(
                            payload.mid(5), keyFrame,
                            &accessUnit, errorText)) {
                        return false;
                    }
                    if (!accessUnit.isEmpty()) {
                        const qint64 dts = normalizedTimestamp(timestamp);
                        const qint64 pts = qMax<qint64>(0,
                            dts + signedU24(readU24(payload, 2)));
                        videoAccessUnits->append(accessUnit);
                        videoDecodingTimesMilliseconds->append(dts);
                        videoPresentationTimesMilliseconds->append(pts);
                        if (keyFrame)
                            m_videoStarted = true;
                    }
                }
            }
        } else if (tagType == 8 && payload.size() >= 2 &&
                   (static_cast<unsigned char>(payload.at(0)) >> 4) == 10) {
            const int packetType = static_cast<unsigned char>(payload.at(1));
            if (packetType == 0) {
                if (!parseAacConfiguration(payload.mid(2), errorText))
                    return false;
            } else if (packetType == 1 && m_audioConfigured) {
                if (!appendAdtsFrame(payload.mid(2), adtsAudio, errorText))
                    return false;
                normalizedTimestamp(timestamp);
            }
        }
        m_buffer.remove(0, completeSize);
    }
    return true;
}

bool FlvLiveDemuxer::parseAvcConfiguration(
    const QByteArray &payload, QString *errorText)
{
    if (payload.size() < 7 ||
        static_cast<unsigned char>(payload.at(0)) != 1) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid AVC configuration");
        return false;
    }
    const int nalLengthSize =
        (static_cast<unsigned char>(payload.at(4)) & 3) + 1;
    if (nalLengthSize < 1 || nalLengthSize > 4) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid AVC NAL length size");
        return false;
    }
    QByteArray parameterSets;
    QByteArray firstSps;
    int offset = 6;
    const int spsCount = static_cast<unsigned char>(payload.at(5)) & 31;
    int index;
    for (index = 0; index < spsCount; ++index) {
        if (offset + 2 > payload.size())
            break;
        const int size = static_cast<int>(readUnsigned(payload, offset, 2));
        offset += 2;
        if (size <= 0 || offset + size > payload.size())
            break;
        const QByteArray nal = payload.mid(offset, size);
        appendStartCodeAndNal(&parameterSets, nal);
        if (firstSps.isEmpty())
            firstSps = nal;
        offset += size;
    }
    if (index != spsCount || offset >= payload.size()) {
        if (errorText)
            *errorText = QString::fromLatin1("truncated AVC SPS table");
        return false;
    }
    const int ppsCount = static_cast<unsigned char>(payload.at(offset++));
    for (index = 0; index < ppsCount; ++index) {
        if (offset + 2 > payload.size())
            break;
        const int size = static_cast<int>(readUnsigned(payload, offset, 2));
        offset += 2;
        if (size <= 0 || offset + size > payload.size())
            break;
        appendStartCodeAndNal(
            &parameterSets, payload.mid(offset, size));
        offset += size;
    }
    if (index != ppsCount || parameterSets.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("truncated AVC PPS table");
        return false;
    }
    int parsedWidth = 0;
    int parsedHeight = 0;
    if (!firstSps.isEmpty())
        parseSpsDimensions(firstSps, &parsedWidth, &parsedHeight);
    m_nalLengthSize = nalLengthSize;
    m_parameterSets = parameterSets;
    m_width = parsedWidth > 0 ? parsedWidth : 640;
    m_height = parsedHeight > 0 ? parsedHeight : 360;
    m_videoConfigured = true;
    return true;
}

bool FlvLiveDemuxer::parseAacConfiguration(
    const QByteArray &payload, QString *errorText)
{
    static const int sampleRates[] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000,
        22050, 16000, 12000, 11025, 8000, 7350
    };
    if (payload.size() < 2) {
        if (errorText)
            *errorText = QString::fromLatin1("truncated AAC configuration");
        return false;
    }
    const int objectType =
        (static_cast<unsigned char>(payload.at(0)) >> 3) & 31;
    const int sampleRateIndex =
        ((static_cast<unsigned char>(payload.at(0)) & 7) << 1) |
        (static_cast<unsigned char>(payload.at(1)) >> 7);
    const int channels =
        (static_cast<unsigned char>(payload.at(1)) >> 3) & 15;
    if (objectType < 1 || objectType > 4 || sampleRateIndex < 0 ||
        sampleRateIndex >= 13 || channels < 1 || channels > 7) {
        if (errorText)
            *errorText = QString::fromLatin1("unsupported AAC configuration");
        return false;
    }
    m_aacObjectType = objectType;
    m_aacSampleRateIndex = sampleRateIndex;
    m_aacSampleRate = sampleRates[sampleRateIndex];
    m_aacChannels = channels;
    m_audioConfigured = true;
    return true;
}

bool FlvLiveDemuxer::makeAnnexBAccessUnit(
    const QByteArray &payload,
    bool keyFrame,
    QByteArray *accessUnit,
    QString *errorText) const
{
    if (!accessUnit || m_nalLengthSize < 1 || m_nalLengthSize > 4)
        return false;
    if (keyFrame)
        accessUnit->append(m_parameterSets);
    int offset = 0;
    while (offset + m_nalLengthSize <= payload.size()) {
        const quint32 size = readUnsigned(
            payload, offset, m_nalLengthSize);
        offset += m_nalLengthSize;
        if (size == 0)
            continue;
        if (size > static_cast<quint32>(payload.size() - offset)) {
            if (errorText)
                *errorText = QString::fromLatin1("truncated AVC NAL unit");
            return false;
        }
        appendStartCodeAndNal(
            accessUnit, payload.mid(offset, static_cast<int>(size)));
        offset += static_cast<int>(size);
    }
    if (offset != payload.size()) {
        if (errorText)
            *errorText = QString::fromLatin1("trailing AVC NAL bytes");
        return false;
    }
    return true;
}

bool FlvLiveDemuxer::appendAdtsFrame(
    const QByteArray &payload,
    QByteArray *adtsAudio,
    QString *errorText) const
{
    const int frameLength = payload.size() + 7;
    if (!adtsAudio || payload.isEmpty() || frameLength > 0x1fff) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid AAC frame length");
        return false;
    }
    const int profile = m_aacObjectType - 1;
    char header[7];
    header[0] = static_cast<char>(0xff);
    header[1] = static_cast<char>(0xf1);
    header[2] = static_cast<char>(
        (profile << 6) | (m_aacSampleRateIndex << 2) |
        ((m_aacChannels >> 2) & 1));
    header[3] = static_cast<char>(
        ((m_aacChannels & 3) << 6) | ((frameLength >> 11) & 3));
    header[4] = static_cast<char>((frameLength >> 3) & 0xff);
    header[5] = static_cast<char>(((frameLength & 7) << 5) | 0x1f);
    header[6] = static_cast<char>(0xfc);
    adtsAudio->append(header, 7);
    adtsAudio->append(payload);
    return true;
}

qint64 FlvLiveDemuxer::normalizedTimestamp(quint32 timestamp)
{
    if (m_firstTimestamp < 0)
        m_firstTimestamp = timestamp;
    if (timestamp < static_cast<quint32>(m_firstTimestamp))
        return 0;
    return static_cast<qint64>(timestamp) - m_firstTimestamp;
}

bool FlvLiveDemuxer::hasVideoConfiguration() const
{
    return m_videoConfigured;
}

bool FlvLiveDemuxer::hasAudioConfiguration() const
{
    return m_audioConfigured;
}

bool FlvLiveDemuxer::hasStartedVideo() const
{
    return m_videoStarted;
}

int FlvLiveDemuxer::width() const
{
    return m_width;
}

int FlvLiveDemuxer::height() const
{
    return m_height;
}

int FlvLiveDemuxer::audioSampleRate() const
{
    return m_aacSampleRate;
}

int FlvLiveDemuxer::audioChannels() const
{
    return m_aacChannels;
}

} // namespace wiliwili
