const allEntries = [
  { bvid: "BV1xz8M6CEdi", cid: "41118993864" },
  { bvid: "BV1f98M6AEEz", cid: "41116699165" },
  { bvid: "BV1fhhP6xEMp", cid: "41223195265" },
];
const requestedBvid = process.argv[2];
const entries = requestedBvid
  ? allEntries.filter((entry) => entry.bvid === requestedBvid)
  : allEntries;

const apiHeaders = {
  "user-agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/132 Safari/537.36",
  referer: "https://www.bilibili.com/",
  origin: "https://www.bilibili.com",
  accept: "application/json",
};

function cleanMedia(media) {
  if (!media) return null;
  const urls = [media.url, ...(media.backup_url || [])].filter(Boolean);
  return {
    order: media.order,
    length: media.length,
    size: media.size,
    urls: urls.map((value) => {
      const url = new URL(value);
      return { protocol: url.protocol, host: url.host, path: url.pathname };
    }),
  };
}

async function probeMedia(value) {
  if (!value) return null;
  const url = new URL(value);
  try {
    const response = await fetch(url, {
      redirect: "manual",
      headers: {
        ...apiHeaders,
        accept: "*/*",
        range: "bytes=0-4095",
      },
    });
    const data = Buffer.from(await response.arrayBuffer());
    return {
      host: url.host,
      status: response.status,
      locationHost: response.headers.get("location")
        ? new URL(response.headers.get("location"), url).host
        : null,
      type: response.headers.get("content-type"),
      range: response.headers.get("content-range"),
      length: response.headers.get("content-length"),
      bytes: data.length,
      signature: data.subarray(0, 32).toString("hex"),
    };
  } catch (error) {
    return { host: url.host, error: error.message };
  }
}

async function probePlainMedia(value) {
  if (!value) return null;
  const url = new URL(value);
  url.protocol = "http:";
  return probeMedia(url.toString());
}

async function probeMirror(value, host) {
  if (!value) return null;
  const url = new URL(value);
  url.protocol = "http:";
  url.host = host;
  return probeMedia(url.toString());
}

async function inspectMp4(value) {
  if (!value) return null;
  const response = await fetch(value, {
    headers: { ...apiHeaders, accept: "*/*" },
  });
  if (!response.ok) return { status: response.status };
  const data = Buffer.from(await response.arrayBuffer());
  const moovType = data.indexOf(Buffer.from("moov"));
  const mdatType = data.indexOf(Buffer.from("mdat"));
  const avcCType = data.indexOf(Buffer.from("avcC"));
  const mp4aType = data.indexOf(Buffer.from("mp4a"), 32);
  let avc1Type = -1;
  if (avcCType >= 0) {
    avc1Type = data.lastIndexOf(
      Buffer.from("avc1"), avcCType - 1);
  }
  return {
    bytes: data.length,
    moov: moovType >= 4 ? moovType - 4 : -1,
    mdat: mdatType >= 4 ? mdatType - 4 : -1,
    fastStart: moovType >= 0 && mdatType >= 0 && moovType < mdatType,
    avc: avcCType >= 0 ? {
      profile: data[avcCType + 5],
      compatibility: data[avcCType + 6],
      level: data[avcCType + 7],
      width: avc1Type >= 0 ? data.readUInt16BE(avc1Type + 28) : null,
      height: avc1Type >= 0 ? data.readUInt16BE(avc1Type + 30) : null,
    } : null,
    aac: mp4aType >= 0 ? {
      channels: data.readUInt16BE(mp4aType + 20),
      sampleRate: data.readUInt32BE(mp4aType + 28) >>> 16,
    } : null,
  };
}

async function probe(entry, variant) {
  const params = new URLSearchParams({
    bvid: entry.bvid,
    cid: entry.cid,
    qn: "32",
    fnver: "0",
    fnval: variant.fnval,
    fourk: "1",
    high_quality: "1",
  });
  if (variant.platform) params.set("platform", variant.platform);
  const endpoint = `https://api.bilibili.com/x/player/playurl?${params}`;
  const response = await fetch(endpoint, { headers: apiHeaders });
  const payload = await response.json();
  const data = payload.data || {};
  const first = data.durl?.[0];
  const result = {
    bvid: entry.bvid,
    variant: variant.name,
    http: response.status,
    code: payload.code,
    message: payload.message,
    quality: data.quality,
    format: data.format,
    acceptQuality: data.accept_quality,
    durl: (data.durl || []).map(cleanMedia),
    dashVideo: data.dash?.video?.map((item) => ({
      id: item.id,
      codecid: item.codecid,
      codecs: item.codecs,
      width: item.width,
      height: item.height,
      bandwidth: item.bandwidth,
      host: item.baseUrl || item.base_url
        ? new URL(item.baseUrl || item.base_url).host
        : null,
    })),
    dashAudio: data.dash?.audio?.map((item) => ({
      id: item.id,
      codecid: item.codecid,
      codecs: item.codecs,
      bandwidth: item.bandwidth,
      host: item.baseUrl || item.base_url
        ? new URL(item.baseUrl || item.base_url).host
        : null,
    })),
    mediaProbe: await probeMedia(first?.url),
    plainMediaProbe: await probePlainMedia(first?.url),
    genericMirrorProbes: await Promise.all([
      "upos-sz-estghw.bilivideo.com",
      "upos-sz-mirrorhw.bilivideo.com",
      "upos-sz-mirror08c.bilivideo.com",
      "upos-sz-mirrorcos.bilivideo.com",
    ].map(async (host) => ({
      host,
      result: await probeMirror(first?.url, host),
    }))),
    mp4: variant.name === "symbian-html5"
      ? await inspectMp4(first?.url) : undefined,
    backupProbe: await probeMedia(first?.backup_url?.[0]),
    plainBackupProbe: await probePlainMedia(first?.backup_url?.[0]),
  };
  console.log(JSON.stringify(result));
}

for (const entry of entries) {
  for (const variant of [
    { name: "symbian-html5", platform: "html5", fnval: "0" },
    { name: "legacy-web", fnval: "0" },
    { name: "dash-web", fnval: "4048" },
  ]) {
    try {
      await probe(entry, variant);
    } catch (error) {
      console.log(JSON.stringify({ bvid: entry.bvid, variant: variant.name, error: error.message }));
    }
  }
}
