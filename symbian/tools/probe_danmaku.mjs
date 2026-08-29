import https from "node:https";
import zlib from "node:zlib";

const cids = ["41118993864", "41116699165"];
const variants = [
  {
    name: "legacy-api",
    url: (cid) => `https://api.bilibili.com/x/v1/dm/list.so?oid=${cid}`,
  },
  {
    name: "comment-xml",
    url: (cid) => `https://comment.bilibili.com/${cid}.xml`,
  },
  {
    name: "web-segment-protobuf",
    url: (cid) =>
      `https://api.bilibili.com/x/v2/dm/web/seg.so?type=1&oid=${cid}&segment_index=1`,
  },
];

function requestRaw(url) {
  return new Promise((resolve, reject) => {
    const request = https.get(url, {
      headers: {
        "user-agent": "wiliwili",
        accept: "application/json,text/xml,application/xml,text/plain,*/*",
        "accept-encoding": "identity",
        referer: "https://www.bilibili.com/client",
        origin: "https://www.bilibili.com",
      },
      timeout: 20000,
    }, (response) => {
      const chunks = [];
      response.on("data", (chunk) => chunks.push(chunk));
      response.on("end", () => {
        const body = Buffer.concat(chunks);
        let inflated;
        try {
          inflated = zlib.inflateRawSync(body);
        } catch {
          inflated = null;
        }
        resolve({
          status: response.statusCode,
          type: response.headers["content-type"],
          encoding: response.headers["content-encoding"],
          lengthHeader: response.headers["content-length"],
          bytes: body.length,
          prefix: body.subarray(0, 24).toString("hex"),
          textPrefix: body.subarray(0, 80).toString("utf8")
            .replace(/[\r\n]/g, " "),
          inflatedBytes: inflated?.length,
          inflatedPrefix: inflated?.subarray(0, 80).toString("utf8")
            .replace(/[\r\n]/g, " "),
          danmakuItems: inflated
            ? (inflated.toString("utf8").match(/<d p=/g) || []).length
            : undefined,
        });
      });
    });
    request.on("timeout", () => request.destroy(new Error("timeout")));
    request.on("error", reject);
  });
}

for (const cid of cids) {
  for (const variant of variants) {
    try {
      const result = await requestRaw(variant.url(cid));
      console.log(JSON.stringify({ cid, variant: variant.name, ...result }));
    } catch (error) {
      console.log(JSON.stringify({
        cid, variant: variant.name, error: error.message,
      }));
    }
  }
}
