#ifndef WILIWILI_SYMBIAN_MP4_AVC_PROBE_READER_H
#define WILIWILI_SYMBIAN_MP4_AVC_PROBE_READER_H

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>

namespace wiliwili {

// Minimal ISO-BMFF reader used by the local DevVideo compatibility backend.
// It deliberately understands only the AVC tables needed for bounded sample
// batches; it is not a general media demuxer.
class Mp4AvcProbeReader
{
public:
    struct Sample
    {
        Sample();

        quint64 offset;
        quint64 dts;
        qint64 pts;
        quint32 size;
        quint32 duration;
        bool sync;
    };

    struct AccessUnit
    {
        AccessUnit();

        QByteArray annexB;
        qint64 decodingTimeMilliseconds;
        qint64 presentationTimeMilliseconds;
        bool sync;
    };

    Mp4AvcProbeReader();

    bool parseHeader(const QByteArray &bytes, QString *errorText);
    bool isValid() const;
    bool isFirmwareRiskProfile() const;

    int profile() const;
    int compatibility() const;
    int level() const;
    int nalLengthSize() const;
    int width() const;
    int height() const;
    int maxReferenceFrames() const;
    int maxReorderFrames() const;
    int maxDecodedFrameBuffering() const;
    int weightedPrediction() const;
    int weightedBiPrediction() const;
    quint32 timescale() const;
    int sampleCount() const;

    // Returns the nearest sync sample at or before the requested time. This
    // is also the entry point used after a seek, because an AVC decoder must
    // rebuild its reference-picture state from an IDR picture.
    int syncSampleForTime(qint64 milliseconds) const;

    // Chooses a bounded, contiguous HTTP byte range beginning at firstSample.
    // lastSampleExclusive is the exact continuation point for the next batch.
    bool sampleByteRange(
        int firstSample,
        qint64 targetDurationMilliseconds,
        quint64 maximumBytes,
        quint64 *firstByte,
        quint64 *lastByte,
        int *lastSampleExclusive,
        QString *errorText) const;

    // Converts each MP4 length-prefixed AVC sample to one Annex-B coded
    // picture. Parameter sets are prepended to the first unit after start or
    // seek, matching DevVideo's EDuCodedPicture contract.
    bool makeAnnexBAccessUnits(
        const QByteArray &mediaBytes,
        quint64 mediaBase,
        int firstSample,
        int lastSampleExclusive,
        bool prependParameterSets,
        QVector<AccessUnit> *units,
        QString *errorText) const;

    // Returns one contiguous HTTP range that covers approximately the first
    // five seconds beginning at the first sync sample.
    bool probeByteRange(
        quint64 *firstByte,
        quint64 *lastByte,
        int *sampleCount,
        QString *errorText) const;

    // mediaBytes must correspond to the absolute byte range beginning at
    // mediaBase. The result contains SPS/PPS followed by the selected samples
    // as an H.264 Annex-B elementary stream.
    QByteArray makeAnnexBProbe(
        const QByteArray &mediaBytes,
        quint64 mediaBase,
        int sampleCount,
        QString *errorText) const;

private:
    void reset();

    bool m_valid;
    int m_profile;
    int m_compatibility;
    int m_level;
    int m_nalLengthSize;
    int m_width;
    int m_height;
    int m_maxReferenceFrames;
    int m_maxReorderFrames;
    int m_maxDecodedFrameBuffering;
    int m_weightedPrediction;
    int m_weightedBiPrediction;
    quint32 m_timescale;
    QByteArray m_parameterSetsAnnexB;
    QVector<Sample> m_samples;
};

} // namespace wiliwili

#endif
