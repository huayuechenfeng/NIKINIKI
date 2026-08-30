# wiliwili 直播实现与 Symbian³ 兼容方案

> 文档状态：Historical research。直播仍在路线图 Later；本文不能覆盖当前普通视频架构。

记录日期：2026-08-24

> **0.7 接手说明（2026-08-25）：**普通视频正式后端已经由 Qt Mobility `QMediaPlayer` 改为原生 `CVideoPlayerUtility2`，直播最终也应复用该后端。`surfacepersist1` 完整复用 controller/native video host/MMF utility/overlay 对象图，用户已确认可退出后再次播放。当前普通视频 P0 转为 H.264 编码覆盖；在兼容回退和 Release 50 次压力门槛通过前，直播线路和直播弹幕保持 P1/P2，不应抢先修改播放器显示架构。最新状态见 `docs/archive/wiliwiliforsymbian3/DEVELOPMENT_STAGE_REPORT_2026-08-25_ZH.md` 和 `docs/research/player/PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md`。

## 1. 上游 wiliwili 的实现

上游直播播放分为四层：

1. `HomeLive` 获取直播推荐并把 `roomid` 交给直播 Activity。
2. `LiveDataRequest::requestData()` 调用
   `get_live_room_play_info(roomid, qn)`；对应接口是
   `/xlive/web-room/v2/index/getRoomPlayInfo`。
3. 接口返回 `stream -> format -> codec -> url_info`。上游选择第一个协议、
   第一个封装和第一个编码，将 `host + base_url + extra` 拼成播放地址，
   再交给 mpv。可选清晰度来自 `g_qn_desc`/`accept_qn`；切换清晰度时，
   保存新的 `qn` 并重新请求整个直播信息。
4. 直播弹幕不是普通视频的 XML 弹幕。上游先请求
   `/xlive/web-room/v1/index/getDanmuInfo` 获取 token 与弹幕服务器，然后通过
   WebSocket 发送认证/心跳包，解包直播消息。播放器加载失败或 FLV 提前 EOF 时，
   上游会重新读取房间状态并重试。

相关上游代码：

- `wiliwili/source/api/video_detail_api.cpp`
- `wiliwili/source/presenter/live_data.cpp`
- `wiliwili/source/activity/live_player_activity.cpp`
- `wiliwili/include/api/bilibili/api.h`

## 2. 为什么 Symbian 版不能照搬 mpv 路线

Switch/PSV 版以 FFmpeg/mpv 处理 FLV、HLS、DASH 和多种编码；Belle 版 0.7 采用
原生 `CVideoPlayerUtility2`，直接进入 Symbian MMF/固件媒体插件。这样可以复用
Nokia 603 的 H.264 硬件解码路径，并避免在 ARMv5 上重新移植软件解码器，但能否
打开 HLS、FLV、特定 H.264 profile/level，仍由手机固件决定。

Belle SDK 中存在 MMF 媒体引擎和 M3U 播放列表插件，所以 HLS 是最有机会工作的
直播路径；这只是 SDK 能力证据，最终仍需真机验证。

## 3. 0.6.0 的兼容实现

### 播放源选择

- 请求 V2 直播接口时沿用上游参数，但把 `codec` 限制为 `0`，只接受 AVC/H.264。
- 遍历全部 `stream/format/codec/url_info`，不只取第一个返回项。
- 优先级为 HLS/TS、HLS/fMP4、HTTP-stream/FLV。
- 保留全部 CDN；每个 HTTPS 地址后紧跟同地址的 HTTP 兼容线路。
- V2 没有可用 AVC 源或请求失败时，回退旧
  `/room/v1/Room/playUrl` FLV 接口。
- 未开播、轮播或被封禁是房间终态，不使用可能返回陈旧地址的旧接口。

### 清晰度

- 普通视频解析 `accept_quality + accept_description`。
- 直播解析 `g_qn_desc`，旧接口解析 `quality_description`。
- 播放器中的清晰度按钮列出接口实际提供的档位；点击后重新请求播放地址。
- 视频与直播分别记忆上次选择。
- 界面显示服务端最终返回的 `current_qn/quality`。如果 B 站把请求画质降级，
  不会错误显示成用户请求的档位。
- Nokia 603 的默认值保守设置为视频 720P (`qn=64`) 和直播高清 (`qn=150`)。

### 直播交互

- 直播卡片可以直接进入横屏 MMF 播放器。
- 直播模式显示“直播中”，禁用进度拖动、快进退和倍速。
- 保留播放/暂停、音量、线路自动回退、清晰度切换和返回。

## 4. 当前边界

- 0.7 的普通视频 native MMF 主链是直播播放器的正式基础；不要恢复 0.6 的 `QMediaPlayer/QVideoWidget` 显示路径。
- 当前先完成普通视频 `surfacepersist1` 的 Release 10 次、Debug 10 次和 Release 50 次稳定门槛，之后才恢复直播真机验证。
- 0.6.0 完成的是直播视频链路；直播 WebSocket 弹幕尚未移植。普通视频 XML
  弹幕继续可用。
- 大航海付费直播、DRM、HEVC、AV1、杜比和 4K 不是 Nokia 603 的目标能力。
- HLS 与 FLV 是否被 Nokia 603 的具体 Belle 固件接受必须真机验证。若 HLS
  不可用，播放器会继续尝试 fMP4、FLV、备用 CDN 和 HTTP 线路。
- 当前不会在网络断流后无限重连，避免老设备后台产生不可控请求；真机确认媒体
  格式后再加入有上限、可取消的直播重连状态机。

## 5. 真机验证记录要求

1. 首页切到“直播”，打开正在直播的房间。
2. 记录右上角 `MMF <清晰度> / <状态>`，确认是否出现画面和声音。
3. 打开清晰度菜单，从默认高清切换到流畅，再切回较高档位。
4. 若失败，等待备用线路全部尝试；记录 Qt Creator 输出中的
   `WW:LIVE_*`、`WW:PLAYER_SOURCE`、`WW:PLAYER_ERROR`，不要记录或发送 Cookie。
5. 直播播放五分钟后返回主页，再打开普通视频，确认方向和播放器状态正确恢复。
