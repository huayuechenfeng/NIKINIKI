# wiliwili for Symbian³ 0.6.5 测试版

## 安装包

Debug 完整包（本轮优先使用）：

`symbian/out/releases/v0.6.5/wiliwili_symbian_0.6.5_debug_full_currentcert.sis`

- 大小：9,140,772 字节
- SHA-256：`7B21E5A26342BCEDE11CAC9EAEE75B09A73814DF06BD66B240A701E97334CD55`

Release 完整包：

`symbian/out/releases/v0.6.5/wiliwili_symbian_0.6.5_release_full_currentcert.sis`

- 大小：9,145,136 字节
- SHA-256：`1FBEA8901A4267DB5319EE904A0C8394D390F24E57D6EA5D1771F1177B5A318D`

两个配置均由 Belle Qt 4.7.4/GCCE 构建，`sbs errors: 0`。SIS 包头
为 0.6.5，签名证书有效期为 2026-08-24 至 2036-08-21。

## 本版修改

### 播放线路

- 对 `BV1xz8M6CEdi` 与 `BV1f98M6AEEz` 的实际渐进式 MP4 做了接口、
  CDN、Range 和容器级比较。两者都是 640×360、H.264 High@L5.1、AAC、
  fast-start MP4；不是 720P 或视频编码家族差异。
- 真机日志中的 `Symbian:-34` 是 MMF 建立媒体连接失败。Belle MMF 不共享
  应用 RHTTP 已可用的 TLS 1.2 链路，因此原来的 HTTPS 首选线路必然先失败。
- 普通视频和直播现在都优先尝试 HTTP；HTTPS 只保留为最后诊断线路。
- HTML5 播放接口只返回一个区域 CDN。本版根据同一签名的 path/query 增加
  `upos-sz-estghw.bilivideo.com` 通用镜像；两条样本均已通过 HTTP Range
  `206 video/mp4` 验证。
- 明确跳过 `edge.mountaintoys.cn:4483` 的 HTTP 降级：该端口只接受 TLS，
  将 scheme 改成 HTTP 会稳定返回 400。

### 扫码登录

- 0.6.4 已正确捕获 `SESSDATA`、`bili_jct`、`DedeUserID` 等五项 Cookie，
  因而 `-101` 已不是凭据漏解析。
- 修复 Symbian RHTTP 的 Cookie 发出方式：不再把完整 Cookie 串交给旧的
  结构化 Header codec，而使用 `SetRawFieldL` 原样写到线缆；这与 wiliwili
  上游绕开 cpr RFC6265 行为、手工设置 `cookie` Header 的语义一致。
- 补齐上游全局 `Origin: https://www.bilibili.com`。
- 新增安全日志 `WW:COOKIE_WIRE 原长度 线缆长度 分隔符数`，只输出长度，
  不输出凭据内容。正常时前两个长度应相同。

## PSV 播放实现结论

PSV 版不使用系统播放器。它请求现代 DASH，按画质选择 AVC/H.264 视频轨，
再独立选择 AAC 音频轨及各自备用 URL，传给自带的 mpv/FFmpeg 合流。OpenGL
构建固定启用 `vita-copy` 硬解，GXM 构建使用 `auto`；PSV 画质上限被限制为
64（720P），并给 mpv 设置 Bilibili referrer、10 秒网络超时和备用轨。

Symbian 版当前使用 Qt Mobility MMF。MMF 能调用固件 H.264 硬解，但不能像
mpv 一样把两个 DASH URL 在应用层合流，因此不能只复制 PSV 的 URL 选择代码。
0.6.5 继续使用音视频合一的渐进式 MP4，并移植 PSV 可复用的 referrer、
线路回退和低画质策略。完整 DASH 方案需要移植 FFmpeg/mpv 或实现可靠的
双播放器同步/本地重封装，不能假装已经由 MMF 支持。

## 建议真机测试

1. 完整安装 Debug SIS，在应用管理器确认版本为 0.6.5。
2. 重新扫码。关注 `WW:COOKIE_WIRE`：两个长度应相同；随后应出现
   `WW:PROFILE_READY`，不应再出现 `PROFILE_FAILED ... -101`。
3. 分别播放 `BV1xz8M6CEdi` 和 `BV1f98M6AEEz`。第一条
   `WW:PLAYER_SOURCE` 的最后一个布尔值应为 `false`，表示直接从 HTTP 开始；
   总线路数应大于 2。
4. 若区域 CDN 失败，应自动进入通用镜像，不应先出现一次 HTTPS `-34`。
5. 第二条首次缓冲可能比第一条久：其文件约 12.7 MB、moov 约 331 KB，
   而第一条约 3.2 MB、moov 约 53 KB。

日志不要包含 Cookie 值或完整带签名的媒体 URL。
