#include "platform/mp4_avc_probe_reader.h"

#include <QtCore/QDebug>
#include <QtCore/QSet>

namespace wiliwili {
namespace {

static const int KMaxSampleBatchSamples = 480;

struct Box
{
    Box() : start(0), size(0), headerSize(0), type() {}

    quint64 start;
    quint64 size;
    int headerSize;
    QByteArray type;

    quint64 payload() const
    {
        return start + static_cast<quint64>(headerSize);
    }

    quint64 end() const
    {
        return start + size;
    }
};

struct StscEntry
{
    quint32 firstChunk;
    quint32 samplesPerChunk;
};

struct SttsEntry
{
    quint32 count;
    quint32 delta;
};

struct CttsEntry
{
    quint32 count;
    qint64 offset;
};

class BitReader
{
public:
    explicit BitReader(const QByteArray &bytes)
        : m_bytes(bytes), m_bitOffset(0), m_valid(true)
    {
    }

    bool valid() const { return m_valid; }

    quint32 readBits(int count)
    {
        if (!m_valid || count < 0 || count > 32 ||
            static_cast<quint64>(m_bitOffset) + count >
                static_cast<quint64>(m_bytes.size()) * 8) {
            m_valid = false;
            return 0;
        }
        quint32 value = 0;
        int index;
        for (index = 0; index < count; ++index) {
            const int byteOffset = m_bitOffset >> 3;
            const int shift = 7 - (m_bitOffset & 7);
            value = (value << 1) |
                ((static_cast<unsigned char>(m_bytes.at(byteOffset)) >> shift) & 1);
            ++m_bitOffset;
        }
        return value;
    }

    quint32 readBit() { return readBits(1); }

    quint32 readUe()
    {
        int zeroes = 0;
        while (m_valid && readBit() == 0) {
            ++zeroes;
            if (zeroes > 31) {
                m_valid = false;
                return 0;
            }
        }
        if (!m_valid || zeroes == 0)
            return 0;
        const quint32 suffix = readBits(zeroes);
        if (!m_valid)
            return 0;
        return ((static_cast<quint32>(1) << zeroes) - 1) + suffix;
    }

    qint32 readSe()
    {
        const quint32 code = readUe();
        if (!m_valid)
            return 0;
        return (code & 1) != 0
            ? static_cast<qint32>((code + 1) / 2)
            : -static_cast<qint32>(code / 2);
    }

private:
    QByteArray m_bytes;
    int m_bitOffset;
    bool m_valid;
};

static bool hasBytes(const QByteArray &bytes, quint64 offset, quint64 count)
{
    const quint64 length = static_cast<quint64>(bytes.size());
    return offset <= length && count <= length - offset;
}

static quint16 readU16(const QByteArray &bytes, quint64 offset)
{
    const unsigned char *p = reinterpret_cast<const unsigned char *>(
        bytes.constData() + static_cast<int>(offset));
    return static_cast<quint16>((p[0] << 8) | p[1]);
}

static quint32 readU32(const QByteArray &bytes, quint64 offset)
{
    const unsigned char *p = reinterpret_cast<const unsigned char *>(
        bytes.constData() + static_cast<int>(offset));
    return (static_cast<quint32>(p[0]) << 24) |
           (static_cast<quint32>(p[1]) << 16) |
           (static_cast<quint32>(p[2]) << 8) |
           static_cast<quint32>(p[3]);
}

static quint64 readU64(const QByteArray &bytes, quint64 offset)
{
    return (static_cast<quint64>(readU32(bytes, offset)) << 32) |
           static_cast<quint64>(readU32(bytes, offset + 4));
}

static bool readBox(
    const QByteArray &bytes,
    quint64 offset,
    quint64 parentEnd,
    Box *box)
{
    if (!box || !hasBytes(bytes, offset, 8) || offset + 8 > parentEnd)
        return false;

    const quint32 size32 = readU32(bytes, offset);
    box->start = offset;
    box->type = bytes.mid(static_cast<int>(offset + 4), 4);
    box->headerSize = 8;
    if (size32 == 1) {
        if (!hasBytes(bytes, offset, 16) || offset + 16 > parentEnd)
            return false;
        box->size = readU64(bytes, offset + 8);
        box->headerSize = 16;
    } else if (size32 == 0) {
        box->size = parentEnd - offset;
    } else {
        box->size = size32;
    }

    if (box->size < static_cast<quint64>(box->headerSize) ||
        box->size > parentEnd - offset || !hasBytes(bytes, offset, box->size)) {
        return false;
    }
    return true;
}

static bool findChild(
    const QByteArray &bytes,
    quint64 begin,
    quint64 end,
    const char *type,
    Box *result)
{
    quint64 offset = begin;
    while (offset + 8 <= end) {
        Box box;
        if (!readBox(bytes, offset, end, &box))
            return false;
        if (box.type == QByteArray(type, 4)) {
            if (result)
                *result = box;
            return true;
        }
        offset = box.end();
    }
    return false;
}

static void appendStartCodeAndNal(QByteArray *target, const QByteArray &nal)
{
    if (!target || nal.isEmpty())
        return;
    target->append('\0');
    target->append('\0');
    target->append('\0');
    target->append('\1');
    target->append(nal);
}

static QByteArray removeEmulationPrevention(const QByteArray &nal)
{
    QByteArray rbsp;
    if (nal.size() <= 1)
        return rbsp;
    rbsp.reserve(nal.size() - 1);
    int zeroes = 0;
    int index;
    // Byte zero is the NAL header and not part of the RBSP syntax below.
    for (index = 1; index < nal.size(); ++index) {
        const unsigned char value =
            static_cast<unsigned char>(nal.at(index));
        if (zeroes >= 2 && value == 3) {
            zeroes = 0;
            continue;
        }
        rbsp.append(static_cast<char>(value));
        if (value == 0)
            ++zeroes;
        else
            zeroes = 0;
    }
    return rbsp;
}

static void skipScalingList(BitReader *bits, int size)
{
    int lastScale = 8;
    int nextScale = 8;
    int index;
    for (index = 0; index < size && bits->valid(); ++index) {
        if (nextScale != 0) {
            const int deltaScale = bits->readSe();
            nextScale = (lastScale + deltaScale + 256) & 255;
        }
        if (nextScale != 0)
            lastScale = nextScale;
    }
}

static void skipHrd(BitReader *bits)
{
    const quint32 cpbCountMinus1 = bits->readUe();
    bits->readBits(4);
    bits->readBits(4);
    quint32 index;
    for (index = 0; index <= cpbCountMinus1 && bits->valid(); ++index) {
        bits->readUe();
        bits->readUe();
        bits->readBit();
    }
    bits->readBits(5);
    bits->readBits(5);
    bits->readBits(5);
    bits->readBits(5);
}

static bool parseSpsCompatibility(
    const QByteArray &nal,
    int *maxReferenceFrames,
    int *maxReorderFrames,
    int *maxDecodedFrameBuffering)
{
    BitReader bits(removeEmulationPrevention(nal));
    const quint32 profileIdc = bits.readBits(8);
    bits.readBits(8); // constraint flags and reserved bits
    bits.readBits(8); // level_idc
    bits.readUe();    // seq_parameter_set_id

    if (profileIdc == 100 || profileIdc == 110 || profileIdc == 122 ||
        profileIdc == 244 || profileIdc == 44 || profileIdc == 83 ||
        profileIdc == 86 || profileIdc == 118 || profileIdc == 128 ||
        profileIdc == 138 || profileIdc == 139 || profileIdc == 134) {
        const quint32 chromaFormatIdc = bits.readUe();
        if (chromaFormatIdc == 3)
            bits.readBit();
        bits.readUe(); // bit_depth_luma_minus8
        bits.readUe(); // bit_depth_chroma_minus8
        bits.readBit();
        if (bits.readBit() != 0) {
            const int scalingCount = chromaFormatIdc == 3 ? 12 : 8;
            int index;
            for (index = 0; index < scalingCount && bits.valid(); ++index) {
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
    const quint32 references = bits.readUe();
    bits.readBit();
    bits.readUe();
    bits.readUe();
    const quint32 frameMbsOnly = bits.readBit();
    if (frameMbsOnly == 0)
        bits.readBit();
    bits.readBit();
    if (bits.readBit() != 0) {
        bits.readUe();
        bits.readUe();
        bits.readUe();
        bits.readUe();
    }

    int reorder = -1;
    int buffering = -1;
    if (bits.readBit() != 0) {
        if (bits.readBit() != 0) {
            const quint32 aspectRatioIdc = bits.readBits(8);
            if (aspectRatioIdc == 255) {
                bits.readBits(16);
                bits.readBits(16);
            }
        }
        if (bits.readBit() != 0)
            bits.readBit();
        if (bits.readBit() != 0) {
            bits.readBits(3);
            bits.readBit();
            if (bits.readBit() != 0) {
                bits.readBits(8);
                bits.readBits(8);
                bits.readBits(8);
            }
        }
        if (bits.readBit() != 0) {
            bits.readUe();
            bits.readUe();
        }
        if (bits.readBit() != 0) {
            bits.readBits(32);
            bits.readBits(32);
            bits.readBit();
        }
        const bool nalHrd = bits.readBit() != 0;
        if (nalHrd)
            skipHrd(&bits);
        const bool vclHrd = bits.readBit() != 0;
        if (vclHrd)
            skipHrd(&bits);
        if (nalHrd || vclHrd)
            bits.readBit();
        bits.readBit();
        if (bits.readBit() != 0) {
            bits.readBit();
            bits.readUe();
            bits.readUe();
            bits.readUe();
            bits.readUe();
            reorder = static_cast<int>(bits.readUe());
            buffering = static_cast<int>(bits.readUe());
        }
    }

    if (!bits.valid())
        return false;
    *maxReferenceFrames = static_cast<int>(references);
    *maxReorderFrames = reorder;
    *maxDecodedFrameBuffering = buffering;
    return true;
}

static bool parsePpsCompatibility(
    const QByteArray &nal,
    int *weightedPrediction,
    int *weightedBiPrediction)
{
    BitReader bits(removeEmulationPrevention(nal));
    bits.readUe();
    bits.readUe();
    bits.readBit();
    bits.readBit();
    const quint32 sliceGroupsMinus1 = bits.readUe();
    if (sliceGroupsMinus1 > 0) {
        const quint32 mapType = bits.readUe();
        quint32 index;
        if (mapType == 0) {
            for (index = 0; index <= sliceGroupsMinus1 && bits.valid(); ++index)
                bits.readUe();
        } else if (mapType == 2) {
            for (index = 0; index < sliceGroupsMinus1 && bits.valid(); ++index) {
                bits.readUe();
                bits.readUe();
            }
        } else if (mapType == 3 || mapType == 4 || mapType == 5) {
            bits.readBit();
            bits.readUe();
        } else if (mapType == 6) {
            const quint32 mapUnitsMinus1 = bits.readUe();
            int width = 0;
            quint32 values = sliceGroupsMinus1 + 1;
            while ((static_cast<quint32>(1) << width) < values)
                ++width;
            for (index = 0; index <= mapUnitsMinus1 && bits.valid(); ++index)
                bits.readBits(width);
        }
    }
    bits.readUe();
    bits.readUe();
    const int weighted = static_cast<int>(bits.readBit());
    const int weightedBi = static_cast<int>(bits.readBits(2));
    if (!bits.valid())
        return false;
    *weightedPrediction = weighted;
    *weightedBiPrediction = weightedBi;
    return true;
}

static bool parseAvcConfiguration(
    const QByteArray &bytes,
    const Box &avcC,
    int *profile,
    int *compatibility,
    int *level,
    int *nalLengthSize,
    QByteArray *parameterSets,
    QByteArray *firstSps,
    QByteArray *firstPps,
    QString *errorText)
{
    const quint64 begin = avcC.payload();
    const quint64 end = avcC.end();
    if (end - begin < 7) {
        if (errorText)
            *errorText = QString::fromLatin1("avcC is truncated");
        return false;
    }

    *profile = static_cast<unsigned char>(bytes.at(static_cast<int>(begin + 1)));
    *compatibility = static_cast<unsigned char>(
        bytes.at(static_cast<int>(begin + 2)));
    *level = static_cast<unsigned char>(bytes.at(static_cast<int>(begin + 3)));
    *nalLengthSize =
        (static_cast<unsigned char>(bytes.at(static_cast<int>(begin + 4))) & 3) + 1;
    if (*nalLengthSize < 1 || *nalLengthSize > 4) {
        if (errorText)
            *errorText = QString::fromLatin1("unsupported AVC NAL length size");
        return false;
    }

    quint64 offset = begin + 6;
    const int spsCount = static_cast<unsigned char>(
        bytes.at(static_cast<int>(begin + 5))) & 31;
    int index;
    for (index = 0; index < spsCount; ++index) {
        if (offset + 2 > end) {
            if (errorText)
                *errorText = QString::fromLatin1("SPS table is truncated");
            return false;
        }
        const quint16 size = readU16(bytes, offset);
        offset += 2;
        if (offset + size > end) {
            if (errorText)
                *errorText = QString::fromLatin1("SPS payload is truncated");
            return false;
        }
        const QByteArray nal =
            bytes.mid(static_cast<int>(offset), static_cast<int>(size));
        if (firstSps->isEmpty())
            *firstSps = nal;
        appendStartCodeAndNal(parameterSets, nal);
        offset += size;
    }

    if (offset + 1 > end) {
        if (errorText)
            *errorText = QString::fromLatin1("PPS count is missing");
        return false;
    }
    const int ppsCount = static_cast<unsigned char>(
        bytes.at(static_cast<int>(offset)));
    ++offset;
    for (index = 0; index < ppsCount; ++index) {
        if (offset + 2 > end) {
            if (errorText)
                *errorText = QString::fromLatin1("PPS table is truncated");
            return false;
        }
        const quint16 size = readU16(bytes, offset);
        offset += 2;
        if (offset + size > end) {
            if (errorText)
                *errorText = QString::fromLatin1("PPS payload is truncated");
            return false;
        }
        const QByteArray nal =
            bytes.mid(static_cast<int>(offset), static_cast<int>(size));
        if (firstPps->isEmpty())
            *firstPps = nal;
        appendStartCodeAndNal(parameterSets, nal);
        offset += size;
    }

    if (parameterSets->isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("avcC contains no parameter sets");
        return false;
    }
    return true;
}

static bool parseVideoSampleDescription(
    const QByteArray &bytes,
    const Box &stsd,
    int *width,
    int *height,
    int *profile,
    int *compatibility,
    int *level,
    int *nalLengthSize,
    QByteArray *parameterSets,
    QByteArray *firstSps,
    QByteArray *firstPps,
    QString *errorText)
{
    quint64 offset = stsd.payload();
    if (stsd.end() - offset < 8) {
        if (errorText)
            *errorText = QString::fromLatin1("stsd is truncated");
        return false;
    }
    const quint32 entryCount = readU32(bytes, offset + 4);
    offset += 8;
    quint32 entryIndex;
    for (entryIndex = 0; entryIndex < entryCount && offset + 8 <= stsd.end();
         ++entryIndex) {
        Box entry;
        if (!readBox(bytes, offset, stsd.end(), &entry))
            break;
        if (entry.type == QByteArray("avc1", 4) ||
            entry.type == QByteArray("avc3", 4)) {
            // VisualSampleEntry is 86 bytes including its box header.
            if (entry.size < 86 || !hasBytes(bytes, entry.start + 32, 4)) {
                if (errorText)
                    *errorText = QString::fromLatin1("AVC sample entry is truncated");
                return false;
            }
            *width = readU16(bytes, entry.start + 32);
            *height = readU16(bytes, entry.start + 34);
            Box avcC;
            if (!findChild(
                    bytes, entry.start + 86, entry.end(), "avcC", &avcC)) {
                if (errorText)
                    *errorText = QString::fromLatin1("AVC sample entry has no avcC");
                return false;
            }
            return parseAvcConfiguration(
                bytes, avcC, profile, compatibility, level, nalLengthSize,
                parameterSets, firstSps, firstPps, errorText);
        }
        offset = entry.end();
    }

    if (errorText)
        *errorText = QString::fromLatin1("video track is not AVC");
    return false;
}

static bool parseSampleSizes(
    const QByteArray &bytes,
    const Box &stsz,
    QVector<quint32> *sizes,
    QString *errorText)
{
    const quint64 begin = stsz.payload();
    if (stsz.end() - begin < 12) {
        if (errorText)
            *errorText = QString::fromLatin1("stsz is truncated");
        return false;
    }
    const quint32 fixedSize = readU32(bytes, begin + 4);
    const quint32 count = readU32(bytes, begin + 8);
    if (count == 0 || count > 1000000) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid AVC sample count");
        return false;
    }
    if (fixedSize == 0 &&
        static_cast<quint64>(count) * 4 > stsz.end() - (begin + 12)) {
        if (errorText)
            *errorText = QString::fromLatin1("stsz entries are truncated");
        return false;
    }
    sizes->reserve(static_cast<int>(count));
    quint32 index;
    for (index = 0; index < count; ++index) {
        sizes->append(fixedSize != 0
            ? fixedSize : readU32(bytes, begin + 12 + index * 4));
    }
    return true;
}

static bool parseChunkOffsets(
    const QByteArray &bytes,
    const Box &box,
    QVector<quint64> *offsets,
    QString *errorText)
{
    const quint64 begin = box.payload();
    if (box.end() - begin < 8) {
        if (errorText)
            *errorText = QString::fromLatin1("chunk offset table is truncated");
        return false;
    }
    const quint32 count = readU32(bytes, begin + 4);
    const quint64 width = box.type == QByteArray("co64", 4) ? 8 : 4;
    if (count == 0 || count > 1000000 ||
        static_cast<quint64>(count) * width > box.end() - (begin + 8)) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid chunk offset table");
        return false;
    }
    offsets->reserve(static_cast<int>(count));
    quint32 index;
    for (index = 0; index < count; ++index) {
        const quint64 value = width == 8
            ? readU64(bytes, begin + 8 + index * width)
            : readU32(bytes, begin + 8 + index * width);
        offsets->append(value);
    }
    return true;
}

static bool parseStsc(
    const QByteArray &bytes,
    const Box &stsc,
    QVector<StscEntry> *entries,
    QString *errorText)
{
    const quint64 begin = stsc.payload();
    if (stsc.end() - begin < 8) {
        if (errorText)
            *errorText = QString::fromLatin1("stsc is truncated");
        return false;
    }
    const quint32 count = readU32(bytes, begin + 4);
    if (count == 0 || count > 100000 ||
        static_cast<quint64>(count) * 12 > stsc.end() - (begin + 8)) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid stsc table");
        return false;
    }
    entries->reserve(static_cast<int>(count));
    quint32 index;
    for (index = 0; index < count; ++index) {
        StscEntry entry;
        const quint64 row = begin + 8 + index * 12;
        entry.firstChunk = readU32(bytes, row);
        entry.samplesPerChunk = readU32(bytes, row + 4);
        if (entry.firstChunk == 0 || entry.samplesPerChunk == 0 ||
            (!entries->isEmpty() &&
             entry.firstChunk <= entries->last().firstChunk)) {
            if (errorText)
                *errorText = QString::fromLatin1("invalid stsc entry");
            return false;
        }
        entries->append(entry);
    }
    return true;
}

static bool parseStts(
    const QByteArray &bytes,
    const Box &stts,
    QVector<SttsEntry> *entries,
    QString *errorText)
{
    const quint64 begin = stts.payload();
    if (stts.end() - begin < 8) {
        if (errorText)
            *errorText = QString::fromLatin1("stts is truncated");
        return false;
    }
    const quint32 count = readU32(bytes, begin + 4);
    if (count == 0 || count > 100000 ||
        static_cast<quint64>(count) * 8 > stts.end() - (begin + 8)) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid stts table");
        return false;
    }
    entries->reserve(static_cast<int>(count));
    quint32 index;
    for (index = 0; index < count; ++index) {
        SttsEntry entry;
        const quint64 row = begin + 8 + index * 8;
        entry.count = readU32(bytes, row);
        entry.delta = readU32(bytes, row + 4);
        if (entry.count == 0 || entry.delta == 0) {
            if (errorText)
                *errorText = QString::fromLatin1("invalid stts entry");
            return false;
        }
        entries->append(entry);
    }
    return true;
}

static bool parseCtts(
    const QByteArray &bytes,
    const Box &ctts,
    QVector<CttsEntry> *entries,
    int *version,
    QString *errorText)
{
    const quint64 begin = ctts.payload();
    if (ctts.end() - begin < 8) {
        if (errorText)
            *errorText = QString::fromLatin1("ctts is truncated");
        return false;
    }
    const int tableVersion = static_cast<unsigned char>(
        bytes.at(static_cast<int>(begin)));
    if (tableVersion != 0 && tableVersion != 1) {
        if (errorText)
            *errorText = QString::fromLatin1("unsupported ctts version");
        return false;
    }
    const quint32 count = readU32(bytes, begin + 4);
    if (count == 0 || count > 100000 ||
        static_cast<quint64>(count) * 8 > ctts.end() - (begin + 8)) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid ctts table");
        return false;
    }
    entries->reserve(static_cast<int>(count));
    quint32 index;
    for (index = 0; index < count; ++index) {
        CttsEntry entry;
        const quint64 row = begin + 8 + index * 8;
        entry.count = readU32(bytes, row);
        const quint32 rawOffset = readU32(bytes, row + 4);
        if (tableVersion == 0) {
            entry.offset = static_cast<qint64>(rawOffset);
        } else if ((rawOffset & 0x80000000U) != 0) {
            const quint32 magnitude = (~rawOffset) + 1U;
            entry.offset = -static_cast<qint64>(magnitude);
        } else {
            entry.offset = static_cast<qint64>(rawOffset);
        }
        if (entry.count == 0) {
            if (errorText)
                *errorText = QString::fromLatin1("invalid ctts entry");
            return false;
        }
        entries->append(entry);
    }
    if (version)
        *version = tableVersion;
    return true;
}

static void parseSyncSamples(
    const QByteArray &bytes,
    const Box *stss,
    QSet<quint32> *syncSamples)
{
    if (!stss)
        return;
    const quint64 begin = stss->payload();
    if (stss->end() - begin < 8)
        return;
    const quint32 count = readU32(bytes, begin + 4);
    if (static_cast<quint64>(count) * 4 > stss->end() - (begin + 8))
        return;
    quint32 index;
    for (index = 0; index < count; ++index)
        syncSamples->insert(readU32(bytes, begin + 8 + index * 4));
}

} // namespace

Mp4AvcProbeReader::Sample::Sample()
    : offset(0), dts(0), pts(0), size(0), duration(0), sync(false)
{
}

Mp4AvcProbeReader::AccessUnit::AccessUnit()
    : decodingTimeMilliseconds(0), presentationTimeMilliseconds(0),
      sync(false)
{
}

Mp4AvcProbeReader::Mp4AvcProbeReader()
{
    reset();
}

void Mp4AvcProbeReader::reset()
{
    m_valid = false;
    m_profile = 0;
    m_compatibility = 0;
    m_level = 0;
    m_nalLengthSize = 0;
    m_width = 0;
    m_height = 0;
    m_maxReferenceFrames = -1;
    m_maxReorderFrames = -1;
    m_maxDecodedFrameBuffering = -1;
    m_weightedPrediction = -1;
    m_weightedBiPrediction = -1;
    m_timescale = 0;
    m_parameterSetsAnnexB.clear();
    m_samples.clear();
}

bool Mp4AvcProbeReader::parseHeader(
    const QByteArray &bytes, QString *errorText)
{
    reset();
    if (errorText)
        errorText->clear();

    Box moov;
    if (!findChild(bytes, 0, static_cast<quint64>(bytes.size()), "moov", &moov)) {
        if (errorText)
            *errorText = QString::fromLatin1("complete moov box is not in range");
        return false;
    }

    Box videoMdia;
    bool foundVideo = false;
    quint64 trakOffset = moov.payload();
    while (trakOffset + 8 <= moov.end()) {
        Box trak;
        if (!readBox(bytes, trakOffset, moov.end(), &trak))
            break;
        trakOffset = trak.end();
        if (trak.type != QByteArray("trak", 4))
            continue;
        Box mdia;
        Box hdlr;
        if (!findChild(bytes, trak.payload(), trak.end(), "mdia", &mdia) ||
            !findChild(bytes, mdia.payload(), mdia.end(), "hdlr", &hdlr) ||
            hdlr.end() - hdlr.payload() < 12) {
            continue;
        }
        if (bytes.mid(static_cast<int>(hdlr.payload() + 8), 4) ==
            QByteArray("vide", 4)) {
            videoMdia = mdia;
            foundVideo = true;
            break;
        }
    }
    if (!foundVideo) {
        if (errorText)
            *errorText = QString::fromLatin1("MP4 has no video track");
        return false;
    }

    Box mdhd;
    Box minf;
    Box stbl;
    Box stsd;
    Box stsz;
    Box stsc;
    Box stts;
    Box ctts;
    Box chunkOffsets;
    if (!findChild(bytes, videoMdia.payload(), videoMdia.end(), "mdhd", &mdhd) ||
        !findChild(bytes, videoMdia.payload(), videoMdia.end(), "minf", &minf) ||
        !findChild(bytes, minf.payload(), minf.end(), "stbl", &stbl) ||
        !findChild(bytes, stbl.payload(), stbl.end(), "stsd", &stsd) ||
        !findChild(bytes, stbl.payload(), stbl.end(), "stsz", &stsz) ||
        !findChild(bytes, stbl.payload(), stbl.end(), "stsc", &stsc) ||
        !findChild(bytes, stbl.payload(), stbl.end(), "stts", &stts) ||
        (!findChild(bytes, stbl.payload(), stbl.end(), "stco", &chunkOffsets) &&
         !findChild(bytes, stbl.payload(), stbl.end(), "co64", &chunkOffsets))) {
        if (errorText)
            *errorText = QString::fromLatin1("video sample tables are incomplete");
        return false;
    }

    const quint64 mdhdBegin = mdhd.payload();
    if (mdhd.end() - mdhdBegin < 16) {
        if (errorText)
            *errorText = QString::fromLatin1("mdhd is truncated");
        return false;
    }
    const int mdhdVersion = static_cast<unsigned char>(
        bytes.at(static_cast<int>(mdhdBegin)));
    const quint64 timescaleOffset = mdhdBegin + (mdhdVersion == 1 ? 20 : 12);
    if (timescaleOffset + 4 > mdhd.end()) {
        if (errorText)
            *errorText = QString::fromLatin1("mdhd timescale is missing");
        return false;
    }
    m_timescale = readU32(bytes, timescaleOffset);
    if (m_timescale == 0) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid video timescale");
        return false;
    }

    QByteArray firstSps;
    QByteArray firstPps;
    if (!parseVideoSampleDescription(
            bytes, stsd, &m_width, &m_height, &m_profile,
            &m_compatibility, &m_level, &m_nalLengthSize,
            &m_parameterSetsAnnexB, &firstSps, &firstPps, errorText)) {
        return false;
    }
    if (!parseSpsCompatibility(
            firstSps, &m_maxReferenceFrames, &m_maxReorderFrames,
            &m_maxDecodedFrameBuffering) ||
        !parsePpsCompatibility(
            firstPps, &m_weightedPrediction, &m_weightedBiPrediction)) {
        if (errorText)
            *errorText = QString::fromLatin1("AVC SPS/PPS compatibility parse failed");
        reset();
        return false;
    }

    QVector<quint32> sizes;
    QVector<quint64> chunks;
    QVector<StscEntry> chunkMap;
    QVector<SttsEntry> timings;
    QVector<CttsEntry> compositions;
    if (!parseSampleSizes(bytes, stsz, &sizes, errorText) ||
        !parseChunkOffsets(bytes, chunkOffsets, &chunks, errorText) ||
        !parseStsc(bytes, stsc, &chunkMap, errorText) ||
        !parseStts(bytes, stts, &timings, errorText)) {
        return false;
    }
    const bool hasCtts = findChild(
        bytes, stbl.payload(), stbl.end(), "ctts", &ctts);
    int cttsVersion = -1;
    if (hasCtts &&
        !parseCtts(bytes, ctts, &compositions, &cttsVersion, errorText)) {
        return false;
    }

    QSet<quint32> syncSamples;
    Box stss;
    const bool hasStss = findChild(
        bytes, stbl.payload(), stbl.end(), "stss", &stss);
    if (hasStss)
        parseSyncSamples(bytes, &stss, &syncSamples);

    m_samples.reserve(sizes.size());
    int sampleIndex = 0;
    int stscIndex = 0;
    int chunkIndex;
    for (chunkIndex = 0;
         chunkIndex < chunks.size() && sampleIndex < sizes.size();
         ++chunkIndex) {
        const quint32 chunkNumber = static_cast<quint32>(chunkIndex + 1);
        while (stscIndex + 1 < chunkMap.size() &&
               chunkMap.at(stscIndex + 1).firstChunk <= chunkNumber) {
            ++stscIndex;
        }
        quint64 sampleOffset = chunks.at(chunkIndex);
        quint32 inChunk;
        for (inChunk = 0;
             inChunk < chunkMap.at(stscIndex).samplesPerChunk &&
             sampleIndex < sizes.size();
             ++inChunk, ++sampleIndex) {
            Sample sample;
            sample.offset = sampleOffset;
            sample.size = sizes.at(sampleIndex);
            sample.sync = !hasStss ||
                syncSamples.contains(static_cast<quint32>(sampleIndex + 1));
            m_samples.append(sample);
            sampleOffset += sample.size;
        }
    }
    if (m_samples.size() != sizes.size()) {
        if (errorText)
            *errorText = QString::fromLatin1("chunk map does not cover samples");
        reset();
        return false;
    }

    quint64 dts = 0;
    sampleIndex = 0;
    int timingIndex;
    for (timingIndex = 0;
         timingIndex < timings.size() && sampleIndex < m_samples.size();
         ++timingIndex) {
        quint32 repeat;
        for (repeat = 0;
             repeat < timings.at(timingIndex).count &&
             sampleIndex < m_samples.size();
             ++repeat, ++sampleIndex) {
            m_samples[sampleIndex].dts = dts;
            m_samples[sampleIndex].pts = static_cast<qint64>(dts);
            m_samples[sampleIndex].duration = timings.at(timingIndex).delta;
            dts += timings.at(timingIndex).delta;
        }
    }
    if (sampleIndex != m_samples.size()) {
        if (errorText)
            *errorText = QString::fromLatin1("timing table does not cover samples");
        reset();
        return false;
    }

    qint64 minimumCompositionOffset = 0;
    qint64 maximumCompositionOffset = 0;
    if (hasCtts) {
        quint64 compositionSampleCount = 0;
        int countIndex;
        for (countIndex = 0; countIndex < compositions.size(); ++countIndex)
            compositionSampleCount += compositions.at(countIndex).count;
        if (compositionSampleCount !=
            static_cast<quint64>(m_samples.size())) {
            if (errorText)
                *errorText = QString::fromLatin1(
                    "composition timing table does not cover samples");
            reset();
            return false;
        }
        sampleIndex = 0;
        int compositionIndex;
        for (compositionIndex = 0;
             compositionIndex < compositions.size() &&
             sampleIndex < m_samples.size();
             ++compositionIndex) {
            const CttsEntry &entry = compositions.at(compositionIndex);
            if (compositionIndex == 0 ||
                entry.offset < minimumCompositionOffset) {
                minimumCompositionOffset = entry.offset;
            }
            if (compositionIndex == 0 ||
                entry.offset > maximumCompositionOffset) {
                maximumCompositionOffset = entry.offset;
            }
            quint32 repeat;
            for (repeat = 0;
                 repeat < entry.count && sampleIndex < m_samples.size();
                 ++repeat, ++sampleIndex) {
                m_samples[sampleIndex].pts = static_cast<qint64>(
                    m_samples.at(sampleIndex).dts) + entry.offset;
            }
        }
    }
    qDebug() << "WW:DEVVIDEO_MP4_CTTS"
             << hasCtts << cttsVersion << compositions.size()
             << minimumCompositionOffset << maximumCompositionOffset;

    m_valid = true;
    return true;
}

bool Mp4AvcProbeReader::isValid() const
{
    return m_valid;
}

bool Mp4AvcProbeReader::isFirmwareRiskProfile() const
{
    // This is the exact template shared by all confirmed audio-only samples.
    // Level and resolution are intentionally not used as compatibility gates.
    return m_valid && m_maxReferenceFrames >= 7 &&
           m_maxReorderFrames >= 4 &&
           m_maxDecodedFrameBuffering >= 7 &&
           m_weightedPrediction == 1 && m_weightedBiPrediction == 2;
}

int Mp4AvcProbeReader::profile() const { return m_profile; }
int Mp4AvcProbeReader::compatibility() const { return m_compatibility; }
int Mp4AvcProbeReader::level() const { return m_level; }
int Mp4AvcProbeReader::nalLengthSize() const { return m_nalLengthSize; }
int Mp4AvcProbeReader::width() const { return m_width; }
int Mp4AvcProbeReader::height() const { return m_height; }
int Mp4AvcProbeReader::maxReferenceFrames() const
{
    return m_maxReferenceFrames;
}
int Mp4AvcProbeReader::maxReorderFrames() const
{
    return m_maxReorderFrames;
}
int Mp4AvcProbeReader::maxDecodedFrameBuffering() const
{
    return m_maxDecodedFrameBuffering;
}
int Mp4AvcProbeReader::weightedPrediction() const
{
    return m_weightedPrediction;
}
int Mp4AvcProbeReader::weightedBiPrediction() const
{
    return m_weightedBiPrediction;
}
quint32 Mp4AvcProbeReader::timescale() const { return m_timescale; }

int Mp4AvcProbeReader::sampleCount() const
{
    return m_valid ? m_samples.size() : 0;
}

int Mp4AvcProbeReader::syncSampleForTime(qint64 milliseconds) const
{
    if (!m_valid || m_samples.isEmpty() || m_timescale == 0)
        return -1;
    const qint64 wanted = qMax<qint64>(0, milliseconds) *
        static_cast<qint64>(m_timescale) / 1000;
    int selected = -1;
    qint64 selectedPresentationTime = 0;
    int index;
    for (index = 0; index < m_samples.size(); ++index) {
        const Sample &sample = m_samples.at(index);
        if (sample.sync && sample.pts <= wanted &&
            (selected < 0 || sample.pts >= selectedPresentationTime)) {
            selected = index;
            selectedPresentationTime = sample.pts;
        }
    }
    if (selected >= 0)
        return selected;
    for (index = 0; index < m_samples.size(); ++index) {
        if (m_samples.at(index).sync)
            return index;
    }
    return -1;
}

bool Mp4AvcProbeReader::sampleByteRange(
    int firstSample,
    qint64 targetDurationMilliseconds,
    quint64 maximumBytes,
    quint64 *firstByte,
    quint64 *lastByte,
    int *lastSampleExclusive,
    QString *errorText) const
{
    if (!m_valid || firstSample < 0 || firstSample >= m_samples.size() ||
        !firstByte || !lastByte || !lastSampleExclusive ||
        targetDurationMilliseconds <= 0 || maximumBytes == 0) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid AVC batch request");
        return false;
    }

    const quint64 startDts = m_samples.at(firstSample).dts;
    const quint64 durationTicks = static_cast<quint64>(
        targetDurationMilliseconds) * m_timescale / 1000;
    const quint64 targetDts = startDts + qMax<quint64>(1, durationTicks);
    quint64 rangeStart = m_samples.at(firstSample).offset;
    quint64 rangeEnd = rangeStart;
    int endSample = firstSample;
    for (; endSample < m_samples.size() &&
         endSample - firstSample < KMaxSampleBatchSamples;
         ++endSample) {
        const Sample &sample = m_samples.at(endSample);
        if (endSample > firstSample && sample.dts >= targetDts)
            break;
        const quint64 candidateStart = qMin(rangeStart, sample.offset);
        const quint64 candidateEnd = qMax(
            rangeEnd, sample.offset + static_cast<quint64>(sample.size));
        if (endSample > firstSample &&
            candidateEnd - candidateStart > maximumBytes) {
            break;
        }
        rangeStart = candidateStart;
        rangeEnd = candidateEnd;
    }
    if (endSample <= firstSample || rangeEnd <= rangeStart ||
        rangeEnd - rangeStart > maximumBytes) {
        if (errorText)
            *errorText = QString::fromLatin1("AVC batch range is empty or too large");
        return false;
    }
    *firstByte = rangeStart;
    *lastByte = rangeEnd - 1;
    *lastSampleExclusive = endSample;
    return true;
}

bool Mp4AvcProbeReader::makeAnnexBAccessUnits(
    const QByteArray &mediaBytes,
    quint64 mediaBase,
    int firstSample,
    int lastSampleExclusive,
    bool prependParameterSets,
    QVector<AccessUnit> *units,
    QString *errorText) const
{
    if (units)
        units->clear();
    if (!m_valid || !units || firstSample < 0 ||
        firstSample >= lastSampleExclusive ||
        lastSampleExclusive > m_samples.size()) {
        if (errorText)
            *errorText = QString::fromLatin1("invalid AVC access-unit request");
        return false;
    }

    units->reserve(lastSampleExclusive - firstSample);
    int index;
    for (index = firstSample; index < lastSampleExclusive; ++index) {
        const Sample &sample = m_samples.at(index);
        if (sample.offset < mediaBase ||
            sample.offset - mediaBase > static_cast<quint64>(mediaBytes.size()) ||
            sample.size > static_cast<quint64>(mediaBytes.size()) -
                (sample.offset - mediaBase)) {
            if (errorText)
                *errorText = QString::fromLatin1("media range misses an AVC sample");
            units->clear();
            return false;
        }

        AccessUnit unit;
        if (prependParameterSets && index == firstSample)
            unit.annexB.append(m_parameterSetsAnnexB);
        unit.decodingTimeMilliseconds = static_cast<qint64>(
            sample.dts * 1000 / m_timescale);
        unit.presentationTimeMilliseconds =
            sample.pts * 1000 / static_cast<qint64>(m_timescale);
        unit.sync = sample.sync;

        const quint64 local = sample.offset - mediaBase;
        quint64 cursor = local;
        const quint64 end = local + sample.size;
        int skippedParameterSets = 0;
        while (cursor < end) {
            if (static_cast<quint64>(m_nalLengthSize) > end - cursor) {
                if (errorText)
                    *errorText = QString::fromLatin1(
                        "AVC sample has a truncated NAL length");
                units->clear();
                return false;
            }
            quint32 nalSize = 0;
            int byteIndex;
            for (byteIndex = 0; byteIndex < m_nalLengthSize; ++byteIndex) {
                nalSize = (nalSize << 8) |
                    static_cast<unsigned char>(mediaBytes.at(
                        static_cast<int>(cursor + byteIndex)));
            }
            cursor += m_nalLengthSize;
            if (nalSize == 0 || nalSize > end - cursor) {
                if (errorText)
                    *errorText = QString::fromLatin1(
                        "AVC sample has an invalid NAL payload");
                units->clear();
                return false;
            }
            const QByteArray nal = mediaBytes.mid(
                static_cast<int>(cursor), static_cast<int>(nalSize));
            const int nalType = nal.isEmpty() ? -1 :
                (static_cast<unsigned char>(nal.at(0)) & 0x1f);
            // avcC already supplied the canonical SPS/PPS at the start of the
            // first access unit. Some Bilibili keyframes repeat them in-band;
            // keep one set only so the old BCM header parser sees the simplest
            // legal Annex-B coded picture.
            if (prependParameterSets && index == firstSample &&
                (nalType == 7 || nalType == 8)) {
                ++skippedParameterSets;
            } else {
                appendStartCodeAndNal(&unit.annexB, nal);
            }
            cursor += nalSize;
        }
        if (prependParameterSets && index == firstSample) {
            qDebug() << "WW:DEVVIDEO_MP4_PARAMETER_SETS_DEDUP"
                     << skippedParameterSets << unit.annexB.size();
        }
        if (unit.annexB.isEmpty()) {
            if (errorText)
                *errorText = QString::fromLatin1("AVC access unit is empty");
            units->clear();
            return false;
        }
        units->append(unit);
    }
    return true;
}

bool Mp4AvcProbeReader::probeByteRange(
    quint64 *firstByte,
    quint64 *lastByte,
    int *sampleCount,
    QString *errorText) const
{
    if (!m_valid || !firstByte || !lastByte || !sampleCount) {
        if (errorText)
            *errorText = QString::fromLatin1("AVC sample table is not ready");
        return false;
    }

    int firstSample = 0;
    while (firstSample < m_samples.size() && !m_samples.at(firstSample).sync)
        ++firstSample;
    if (firstSample >= m_samples.size()) {
        if (errorText)
            *errorText = QString::fromLatin1("video track has no sync sample");
        return false;
    }

    const quint64 startDts = m_samples.at(firstSample).dts;
    const quint64 targetDts = startDts + static_cast<quint64>(m_timescale) * 5;
    quint64 rangeStart = m_samples.at(firstSample).offset;
    quint64 rangeEnd = rangeStart;
    int count = 0;
    int index;
    for (index = firstSample; index < m_samples.size() && count < 180; ++index) {
        const Sample &sample = m_samples.at(index);
        if (count > 0 && sample.dts >= targetDts)
            break;
        if (sample.offset < rangeStart)
            rangeStart = sample.offset;
        const quint64 sampleEnd = sample.offset + sample.size;
        if (sampleEnd > rangeEnd)
            rangeEnd = sampleEnd;
        ++count;
    }
    if (count == 0 || rangeEnd <= rangeStart || rangeEnd - rangeStart > 8 * 1024 * 1024) {
        if (errorText)
            *errorText = QString::fromLatin1("probe media range is invalid or too large");
        return false;
    }
    *firstByte = rangeStart;
    *lastByte = rangeEnd - 1;
    *sampleCount = count;
    return true;
}

QByteArray Mp4AvcProbeReader::makeAnnexBProbe(
    const QByteArray &mediaBytes,
    quint64 mediaBase,
    int sampleCount,
    QString *errorText) const
{
    QByteArray result;
    if (!m_valid || sampleCount <= 0) {
        if (errorText)
            *errorText = QString::fromLatin1("AVC sample table is not ready");
        return result;
    }

    int firstSample = 0;
    while (firstSample < m_samples.size() && !m_samples.at(firstSample).sync)
        ++firstSample;
    if (firstSample >= m_samples.size()) {
        if (errorText)
            *errorText = QString::fromLatin1("video track has no sync sample");
        return result;
    }

    result.reserve(qMin(8 * 1024 * 1024,
        mediaBytes.size() + m_parameterSetsAnnexB.size() + 4096));
    result.append(m_parameterSetsAnnexB);
    int index;
    for (index = firstSample;
         index < m_samples.size() && index < firstSample + sampleCount;
         ++index) {
        const Sample &sample = m_samples.at(index);
        if (sample.offset < mediaBase ||
            sample.offset - mediaBase > static_cast<quint64>(mediaBytes.size()) ||
            sample.size > static_cast<quint64>(mediaBytes.size()) -
                (sample.offset - mediaBase)) {
            if (errorText)
                *errorText = QString::fromLatin1("probe range misses an AVC sample");
            return QByteArray();
        }
        const quint64 local = sample.offset - mediaBase;
        quint64 cursor = local;
        const quint64 end = local + sample.size;
        while (cursor < end) {
            if (static_cast<quint64>(m_nalLengthSize) > end - cursor) {
                if (errorText)
                    *errorText = QString::fromLatin1("AVC sample has a truncated NAL length");
                return QByteArray();
            }
            quint32 nalSize = 0;
            int byteIndex;
            for (byteIndex = 0; byteIndex < m_nalLengthSize; ++byteIndex) {
                nalSize = (nalSize << 8) |
                    static_cast<unsigned char>(mediaBytes.at(
                        static_cast<int>(cursor + byteIndex)));
            }
            cursor += m_nalLengthSize;
            if (nalSize == 0 || nalSize > end - cursor) {
                if (errorText)
                    *errorText = QString::fromLatin1("AVC sample has an invalid NAL payload");
                return QByteArray();
            }
            appendStartCodeAndNal(
                &result,
                mediaBytes.mid(static_cast<int>(cursor), static_cast<int>(nalSize)));
            cursor += nalSize;
        }
    }
    return result;
}

} // namespace wiliwili
