#include "network/bilibili_playback_parser.h"

#include <cstdlib>
#include <cstring>

#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QtAlgorithms>
#include <QtCore/QXmlStreamReader>

#include <zlib.h>

extern "C" {
#include "../../third_party/mongoose_compat/mg_json.h"
}

namespace wiliwili {

struct LiveUrlCandidateCompat
{
    int score;
    QString url;
    QString format;
    int quality;
};

static bool liveCandidateEarlier(
    const LiveUrlCandidateCompat &left,
    const LiveUrlCandidateCompat &right)
{
    return left.score < right.score;
}

static bool danmakuEarlier(
    const DanmakuItemCompat &left,
    const DanmakuItemCompat &right)
{
    return left.timeMilliseconds < right.timeMilliseconds;
}

static bool inflateDanmakuBody(
    const QByteArray &compressed,
    int windowBits,
    QByteArray *uncompressed)
{
    if (!uncompressed || compressed.isEmpty())
        return false;

    const int maximumOutputBytes = 4 * 1024 * 1024;
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    stream.next_in = reinterpret_cast<Bytef *>(
        const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, windowBits) != Z_OK)
        return false;

    uncompressed->clear();
    char outputChunk[16384];
    bool complete = false;
    for (;;) {
        stream.next_out = reinterpret_cast<Bytef *>(outputChunk);
        stream.avail_out = sizeof(outputChunk);
        const int result = inflate(&stream, Z_NO_FLUSH);
        const int produced = static_cast<int>(
            sizeof(outputChunk) - stream.avail_out);
        if (produced > 0) {
            if (uncompressed->size() + produced > maximumOutputBytes)
                break;
            uncompressed->append(outputChunk, produced);
        }
        if (result == Z_STREAM_END) {
            complete = true;
            break;
        }
        if (result != Z_OK || (produced == 0 && stream.avail_in == 0))
            break;
    }
    inflateEnd(&stream);
    if (!complete)
        uncompressed->clear();
    return complete;
}

static QString playbackJsonString(
    const struct mg_str &json,
    const char *path)
{
    char *value = mg_json_get_str(json, path);
    if (!value)
        return QString();
    const QString result = QString::fromUtf8(value);
    std::free(value);
    return result;
}

static bool playbackJsonNumber(
    const struct mg_str &json,
    const char *path,
    double *value)
{
    return mg_json_get_num(json, path, value);
}

static QString playbackQualityDescription(int quality)
{
    switch (quality) {
    case 6: return QString::fromUtf8("240P");
    case 16: return QString::fromUtf8("360P");
    case 32: return QString::fromUtf8("480P");
    case 64: return QString::fromUtf8("720P");
    case 74: return QString::fromUtf8("720P60");
    case 80: return QString::fromUtf8("1080P");
    case 112: return QString::fromUtf8("1080P+");
    case 116: return QString::fromUtf8("1080P60");
    case 120: return QString::fromUtf8("4K");
    case 125: return QString::fromUtf8("HDR");
    case 126: return QString::fromUtf8("杜比视界");
    case 127: return QString::fromUtf8("8K");
    case 30000: return QString::fromUtf8("杜比");
    case 20000: return QString::fromUtf8("4K");
    case 15000: return QString::fromUtf8("2K");
    case 10000: return QString::fromUtf8("原画");
    case 400: return QString::fromUtf8("蓝光");
    case 250: return QString::fromUtf8("超清");
    case 150: return QString::fromUtf8("高清");
    default:
        return QString::fromLatin1("Q%1").arg(quality);
    }
}

static void appendQuality(
    QVector<PlaybackQualityCompat> *qualities,
    int quality,
    const QString &description)
{
    if (!qualities || quality <= 0)
        return;
    int index;
    for (index = 0; index < qualities->size(); ++index) {
        if (qualities->at(index).quality == quality)
            return;
    }
    qualities->append(PlaybackQualityCompat(
        quality,
        description.isEmpty()
            ? playbackQualityDescription(quality)
            : description));
}

static void parseParallelQualityArrays(
    const struct mg_str &json,
    const char *qualityPath,
    const char *descriptionPath,
    QVector<PlaybackQualityCompat> *qualities)
{
    const struct mg_str ids = mg_json_get_tok(json, qualityPath);
    const struct mg_str labels = mg_json_get_tok(json, descriptionPath);
    QVector<int> values;
    QStringList descriptions;
    size_t offset = 0;
    struct mg_str item;
    double number = 0.0;
    while ((offset = mg_json_next(ids, offset, 0, &item)) != 0) {
        if (playbackJsonNumber(item, "$", &number))
            values.append(static_cast<int>(number));
    }
    offset = 0;
    while ((offset = mg_json_next(labels, offset, 0, &item)) != 0)
        descriptions.append(playbackJsonString(item, "$"));
    int index;
    for (index = 0; index < values.size(); ++index) {
        appendQuality(
            qualities,
            values.at(index),
            index < descriptions.size()
                ? descriptions.at(index) : QString());
    }
}

bool BilibiliPlaybackParser::parsePlaybackSource(
    const QByteArray &body,
    PlaybackSourceCompat *source,
    int *apiCode,
    QString *errorText)
{
    if (!source)
        return false;
    if (apiCode)
        *apiCode = -9999;
    if (errorText)
        errorText->clear();
    if (body.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("empty response");
        return false;
    }

    const struct mg_str root = mg_str_n(
        body.constData(), static_cast<size_t>(body.size()));
    double number = -9999.0;
    if (!playbackJsonNumber(root, "$.code", &number)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing API code");
        return false;
    }
    const int code = static_cast<int>(number);
    if (apiCode)
        *apiCode = code;
    if (code != 0) {
        if (errorText) {
            *errorText = playbackJsonString(root, "$.message");
            if (errorText->isEmpty())
                *errorText = QString::fromLatin1("API %1").arg(code);
        }
        return false;
    }

    const struct mg_str data = mg_json_get_tok(root, "$.data");
    if (!data.buf || data.len < 2 || data.buf[0] != '{') {
        if (errorText)
            *errorText = QString::fromLatin1("missing data");
        return false;
    }

    PlaybackSourceCompat parsed;
    parsed.format = playbackJsonString(data, "$.format");
    if (playbackJsonNumber(data, "$.quality", &number))
        parsed.quality = static_cast<int>(number);
    if (playbackJsonNumber(data, "$.timelength", &number))
        parsed.durationMilliseconds = static_cast<qint64>(number);
    parsed.referer = QString::fromLatin1("https://www.bilibili.com/");
    parseParallelQualityArrays(
        data, "$.accept_quality", "$.accept_description",
        &parsed.qualities);

    const struct mg_str durl = mg_json_get_tok(data, "$.durl");
    size_t offset = 0;
    struct mg_str item;
    if (durl.buf && durl.len >= 2 && durl.buf[0] == '[' &&
        (offset = mg_json_next(durl, offset, 0, &item)) != 0) {
        parsed.url = playbackJsonString(item, "$.url");
        const struct mg_str backups =
            mg_json_get_tok(item, "$.backup_url");
        size_t backupOffset = 0;
        struct mg_str backup;
        int backupIndex = 0;
        while (parsed.backupUrls.size() < 8 &&
               (backupOffset = mg_json_next(
                    backups, backupOffset, 0, &backup)) != 0) {
            if (!backup.buf || backup.len < 2 || backup.buf[0] != '"') {
                ++backupIndex;
                continue;
            }
            const QString value = playbackJsonString(
                item,
                QString::fromLatin1("$.backup_url[%1]")
                    .arg(backupIndex)
                    .toLatin1().constData());
            if (!value.isEmpty())
                parsed.backupUrls.append(value);
            ++backupIndex;
        }
    }

    if (parsed.url.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1(
                "progressive MP4 URL is unavailable");
        return false;
    }

#ifdef Q_OS_SYMBIAN
    // platform=html5 supplies a single regional HTTPS URL. Belle MMF uses a
    // separate TLS implementation from RHTTP and consistently reports -34 on
    // that URL. Bilibili's upos signature is path/query based, so the same
    // signed object is also available through its generic upos mirror. Keep a
    // small deterministic mirror set in the fallback list. Every host below
    // is checked with byte-range requests by tools/probe_playurl.mjs.
    QUrl mirror(parsed.url);
    if (mirror.isValid() && !mirror.path().isEmpty()) {
        mirror.setScheme(QString::fromLatin1("http"));
        const char * const mirrorHosts[] = {
            "upos-sz-estghw.bilivideo.com",
            "upos-sz-mirrorhw.bilivideo.com",
            "upos-sz-mirror08c.bilivideo.com",
            "upos-sz-mirrorcos.bilivideo.com"
        };
        int mirrorIndex;
        // Prepend in reverse so the stable preferred order is preserved even
        // when an API response happens to contain regional backup URLs.
        for (mirrorIndex = 3; mirrorIndex >= 0; --mirrorIndex) {
            mirror.setHost(QString::fromLatin1(mirrorHosts[mirrorIndex]));
            mirror.setPort(-1);
            const QString mirrorUrl = mirror.toString();
            if (!mirrorUrl.isEmpty() && mirrorUrl != parsed.url &&
                !parsed.backupUrls.contains(mirrorUrl)) {
                parsed.backupUrls.prepend(mirrorUrl);
            }
        }
    }
#endif
    appendQuality(&parsed.qualities, parsed.quality, QString());
    *source = parsed;
    return true;
}

bool BilibiliPlaybackParser::parseLivePlaybackSource(
    const QByteArray &body,
    PlaybackSourceCompat *source,
    int *apiCode,
    QString *errorText)
{
    if (!source)
        return false;
    if (apiCode)
        *apiCode = -9999;
    if (errorText)
        errorText->clear();
    if (body.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("empty live response");
        return false;
    }

    const struct mg_str root = mg_str_n(
        body.constData(), static_cast<size_t>(body.size()));
    double number = -9999.0;
    if (!playbackJsonNumber(root, "$.code", &number)) {
        if (errorText)
            *errorText = QString::fromLatin1("missing live API code");
        return false;
    }
    const int code = static_cast<int>(number);
    if (apiCode)
        *apiCode = code;
    if (code != 0) {
        if (errorText) {
            *errorText = playbackJsonString(root, "$.message");
            if (errorText->isEmpty())
                *errorText = QString::fromLatin1("live API %1").arg(code);
        }
        return false;
    }

    const struct mg_str data = mg_json_get_tok(root, "$.data");
    bool locked = false;
    mg_json_get_bool(data, "$.is_locked", &locked);
    if (locked) {
        if (errorText)
            *errorText = QString::fromUtf8("直播间已被封禁");
        return false;
    }
    int liveStatus = 0;
    if (playbackJsonNumber(data, "$.live_status", &number))
        liveStatus = static_cast<int>(number);
    if (liveStatus != 1) {
        if (errorText) {
            *errorText = liveStatus == 2
                ? QString::fromUtf8("直播间正在轮播，Belle 暂不支持")
                : QString::fromUtf8("主播尚未开播");
        }
        return false;
    }

    PlaybackSourceCompat parsed;
    parsed.live = true;
    quint64 roomId = 0;
    if (playbackJsonNumber(data, "$.room_id", &number))
        roomId = static_cast<quint64>(number);
    parsed.referer = QString::fromLatin1(
        "https://live.bilibili.com/%1").arg(roomId);

    const struct mg_str playurl =
        mg_json_get_tok(data, "$.playurl_info.playurl");
    const struct mg_str qualityList =
        mg_json_get_tok(playurl, "$.g_qn_desc");
    size_t offset = 0;
    struct mg_str item;
    while ((offset = mg_json_next(
                qualityList, offset, 0, &item)) != 0) {
        if (!item.buf || item.len < 2 || item.buf[0] != '{')
            continue;
        if (!playbackJsonNumber(item, "$.qn", &number))
            continue;
        appendQuality(
            &parsed.qualities,
            static_cast<int>(number),
            playbackJsonString(item, "$.desc"));
    }

    QVector<LiveUrlCandidateCompat> candidates;
    const struct mg_str streams = mg_json_get_tok(playurl, "$.stream");
    size_t streamOffset = 0;
    struct mg_str stream;
    while ((streamOffset = mg_json_next(
                streams, streamOffset, 0, &stream)) != 0) {
        const QString protocol = playbackJsonString(
            stream, "$.protocol_name");
        const struct mg_str formats = mg_json_get_tok(stream, "$.format");
        size_t formatOffset = 0;
        struct mg_str format;
        while ((formatOffset = mg_json_next(
                    formats, formatOffset, 0, &format)) != 0) {
            const QString formatName = playbackJsonString(
                format, "$.format_name");
            const struct mg_str codecs = mg_json_get_tok(format, "$.codec");
            size_t codecOffset = 0;
            struct mg_str codec;
            while ((codecOffset = mg_json_next(
                        codecs, codecOffset, 0, &codec)) != 0) {
                const QString codecName = playbackJsonString(
                    codec, "$.codec_name");
                if (codecName.compare(
                        QString::fromLatin1("avc"),
                        Qt::CaseInsensitive) != 0) {
                    continue;
                }
                int currentQuality = 0;
                if (playbackJsonNumber(codec, "$.current_qn", &number))
                    currentQuality = static_cast<int>(number);
                const QString baseUrl = playbackJsonString(
                    codec, "$.base_url");
                if (baseUrl.isEmpty())
                    continue;
                int score = 60;
                if (protocol == QString::fromLatin1("http_stream") &&
                    formatName == QString::fromLatin1("flv")) {
                    score = 0;
                } else if (protocol == QString::fromLatin1("http_hls") &&
                           formatName == QString::fromLatin1("ts")) {
                    score = 20;
                } else if (protocol == QString::fromLatin1("http_hls") &&
                           formatName == QString::fromLatin1("fmp4")) {
                    // Belle's HLS controller predates fragmented-MP4 HLS.
                    // Keep it as a final V2 fallback after TS and FLV.
                    score = 40;
                }
                const struct mg_str urlInfos =
                    mg_json_get_tok(codec, "$.url_info");
                size_t urlOffset = 0;
                struct mg_str urlInfo;
                int hostIndex = 0;
                while ((urlOffset = mg_json_next(
                            urlInfos, urlOffset, 0, &urlInfo)) != 0) {
                    const QString host = playbackJsonString(
                        urlInfo, "$.host");
                    const QString extra = playbackJsonString(
                        urlInfo, "$.extra");
                    if (host.isEmpty())
                        continue;
                    LiveUrlCandidateCompat candidate;
                    candidate.score = score + hostIndex;
                    candidate.url = host + baseUrl + extra;
                    candidate.format = QString::fromLatin1("LIVE %1/%2")
                        .arg(formatName).arg(codecName);
                    candidate.quality = currentQuality;
                    candidates.append(candidate);
                    ++hostIndex;
                }
            }
        }
    }

    if (candidates.isEmpty()) {
        if (errorText)
            *errorText = QString::fromUtf8("直播 API 没有返回 AVC 播放源");
        return false;
    }
    qSort(candidates.begin(), candidates.end(), liveCandidateEarlier);
    int index;
    for (index = 0; index < candidates.size(); ++index) {
        const LiveUrlCandidateCompat &candidate = candidates.at(index);
        if (parsed.url.isEmpty()) {
            parsed.url = candidate.url;
            parsed.format = candidate.format;
            parsed.quality = candidate.quality;
        } else if (candidate.url != parsed.url &&
                   !parsed.backupUrls.contains(candidate.url)) {
            parsed.backupUrls.append(candidate.url);
        }
    }
    appendQuality(&parsed.qualities, parsed.quality, QString());
    *source = parsed;
    return true;
}

bool BilibiliPlaybackParser::parseLegacyLivePlaybackSource(
    const QByteArray &body,
    quint64 roomId,
    PlaybackSourceCompat *source,
    int *apiCode,
    QString *errorText)
{
    if (!source)
        return false;
    if (apiCode)
        *apiCode = -9999;
    if (errorText)
        errorText->clear();
    if (body.isEmpty())
        return false;
    const struct mg_str root = mg_str_n(
        body.constData(), static_cast<size_t>(body.size()));
    double number = -9999.0;
    if (!playbackJsonNumber(root, "$.code", &number))
        return false;
    const int code = static_cast<int>(number);
    if (apiCode)
        *apiCode = code;
    if (code != 0) {
        if (errorText)
            *errorText = playbackJsonString(root, "$.message");
        return false;
    }
    const struct mg_str data = mg_json_get_tok(root, "$.data");
    PlaybackSourceCompat parsed;
    parsed.live = true;
    parsed.format = QString::fromLatin1("LIVE flv/avc legacy");
    parsed.referer = QString::fromLatin1(
        "https://live.bilibili.com/%1").arg(roomId);
    if (playbackJsonNumber(data, "$.current_qn", &number))
        parsed.quality = static_cast<int>(number);

    const struct mg_str qualityList =
        mg_json_get_tok(data, "$.quality_description");
    size_t offset = 0;
    struct mg_str item;
    while ((offset = mg_json_next(
                qualityList, offset, 0, &item)) != 0) {
        if (!playbackJsonNumber(item, "$.qn", &number))
            continue;
        appendQuality(
            &parsed.qualities,
            static_cast<int>(number),
            playbackJsonString(item, "$.desc"));
    }
    const struct mg_str urls = mg_json_get_tok(data, "$.durl");
    offset = 0;
    while ((offset = mg_json_next(urls, offset, 0, &item)) != 0) {
        const QString url = playbackJsonString(item, "$.url");
        if (url.isEmpty())
            continue;
        if (parsed.url.isEmpty())
            parsed.url = url;
        else if (!parsed.backupUrls.contains(url))
            parsed.backupUrls.append(url);
    }
    if (parsed.url.isEmpty()) {
        if (errorText)
            *errorText = QString::fromUtf8("旧直播 API 没有返回地址");
        return false;
    }
    appendQuality(&parsed.qualities, parsed.quality, QString());
    *source = parsed;
    return true;
}

bool BilibiliPlaybackParser::parseDanmaku(
    const QByteArray &body,
    QVector<DanmakuItemCompat> *items,
    QString *errorText)
{
    if (!items)
        return false;
    items->clear();
    if (errorText)
        errorText->clear();
    if (body.isEmpty()) {
        if (errorText)
            *errorText = QString::fromLatin1("empty danmaku response");
        return false;
    }

    QByteArray xmlBody = body;
    int firstNonSpace = 0;
    while (firstNonSpace < body.size() &&
           (body.at(firstNonSpace) == ' ' ||
            body.at(firstNonSpace) == '\t' ||
            body.at(firstNonSpace) == '\r' ||
            body.at(firstNonSpace) == '\n')) {
        ++firstNonSpace;
    }
    if (firstNonSpace >= body.size() || body.at(firstNonSpace) != '<') {
        QByteArray inflated;
        // Bilibili's legacy list.so endpoint currently sends a raw DEFLATE
        // stream even when Accept-Encoding is identity. Try that format first,
        // then tolerate zlib/gzip wrappers used by older CDN edges.
        bool inflatedOk = inflateDanmakuBody(body, -MAX_WBITS, &inflated);
        if (!inflatedOk)
            inflatedOk = inflateDanmakuBody(body, MAX_WBITS, &inflated);
        if (!inflatedOk)
            inflatedOk = inflateDanmakuBody(body, MAX_WBITS + 32, &inflated);
        if (!inflatedOk) {
            if (errorText)
                *errorText = QString::fromLatin1(
                    "danmaku deflate decode failed");
            return false;
        }
        qDebug() << "WW:DANMAKU_INFLATED"
                 << body.size() << inflated.size();
        xmlBody = inflated;
    }

    // Some legacy CDN edges label the document UTF-8 but return isolated
    // invalid byte sequences. Constructing the reader directly from the byte
    // array makes Qt 4.7 abort the whole document as "incorrectly encoded".
    // Decode once with replacement and remove XML 1.0-forbidden controls so
    // valid surrounding danmaku can still be consumed.
    QString xmlText = QString::fromUtf8(
        xmlBody.constData(), xmlBody.size());
    int characterIndex;
    for (characterIndex = xmlText.size() - 1;
         characterIndex >= 0; --characterIndex) {
        const ushort value = xmlText.at(characterIndex).unicode();
        if (value < 0x20 && value != 0x09 &&
            value != 0x0a && value != 0x0d) {
            xmlText.remove(characterIndex, 1);
        }
    }
    QXmlStreamReader xml(xmlText);
    while (!xml.atEnd() && items->size() < 1200) {
        xml.readNext();
        if (!xml.isStartElement() ||
            xml.name() != QString::fromLatin1("d")) {
            continue;
        }
        const QString attributes = xml.attributes()
            .value(QString::fromLatin1("p")).toString();
        const QStringList fields = attributes.split(QLatin1Char(','));
        const QString text = xml.readElementText().trimmed();
        if (fields.size() < 4 || text.isEmpty())
            continue;
        bool timeOk = false;
        const double seconds = fields.at(0).toDouble(&timeOk);
        if (!timeOk || seconds < 0.0)
            continue;
        DanmakuItemCompat danmaku;
        danmaku.timeMilliseconds = static_cast<qint64>(seconds * 1000.0);
        danmaku.mode = fields.at(1).toInt();
        danmaku.fontSize = fields.at(2).toInt();
        danmaku.color = fields.at(3).toInt();
        danmaku.text = text.left(120);
        // Preserve Bilibili's mode semantics. Modes 4/5 are fixed bottom/top
        // comments and modes 1/6 are scrolling comments.
        if (danmaku.mode == 1 || danmaku.mode == 4 ||
            danmaku.mode == 5 || danmaku.mode == 6) {
            items->append(danmaku);
        }
    }
    if (xml.hasError()) {
        if (errorText)
            *errorText = xml.errorString();
        items->clear();
        return false;
    }
    qSort(items->begin(), items->end(), danmakuEarlier);
    return true;
}

} // namespace wiliwili
