# NIKINIKI 播放器架构

> 状态：Active
> 适用版本：1.0.0 与当前主线
> 验证基线：Nokia 603 / Nokia Belle
> 本页职责：播放器当前实现的唯一结构说明

## 产品策略

播放器优先使用固件 MMF 硬件路径；Broadcom 插件明确拒绝真实 H.264 header 时，才使用
手机本机 H.264 软件解码。应用不使用桥接、远端重封装或远端转码。

```text
Bilibili progressive MP4
        │
        ├─ bounded header/AU prefetch
        │        │
        │        └─ Broadcom GetHeaderInformationL()
        │                 ├─ accept → MMF video + audio
        │                 └─ reject → FFmpeg video + MMF AAC/audio clock
        │
        └─ prefetch/preflight failure → conservative MMF attempt
```

一个会话只允许一次有方向的后端选择，不在清晰度、CDN 和后端之间循环。

## Header preflight

`Mp4AvcProbeReader` 通过有界 HTTP Range 获取 MP4 `moov/avcC`、SPS/PPS 和首个同步 AU，
构造与真机控制实验一致的 Annex-B 输入。只读 preflight 使用 Broadcom decoder UID
`0x10204C21` 和 `video/h264` 输入格式调用 `GetHeaderInformationL()`。

preflight 不调用 Configure、Initialize、Start、DSA、post-processor 或 coded-data write；
临时 DevVideo 对象在返回前销毁。它是固件能力查询，不是第二套播放器。

## MMF 路径

Broadcom 接受 header 或 preflight 无法安全完成时，媒体交给
`CVideoPlayerUtility2`：

- MMF 负责容器、H.264/AAC、播放状态和 position；
- `KErrMMPartialPlayback (-12017)` 触发轨道检查，而不是统一视为致命错误；
- 视频可用时继续原生硬件播放；只有音频可用且 preflight 已拒绝时进入软件视频路径；
- MMF `RWindow` 不旋转，直接使用物理横屏坐标。

## FFmpeg 软件路径

软件回退只接管 H.264 视频：

- 裁剪的 PPSSPP-FFmpeg/libavcodec 由 GCCE 4.4.1 从固定源码重建；
- 解码线程消费 Annex-B access unit，并保留 MP4 `ctts` version 0/1 产生的 DTS 与 PTS；
- AAC 继续由 MMF 播放，MMF position 是主时钟；
- 队列有界，消费端先选择 stale/drop 后的目标帧，再转换为 RGB565；
- 转换使用 CPU YUV420P→RGB565 LUT2X2；
- 当前普通构建启用 ARMv6/VFP、ARM assembly、`-Os`，禁用 NEON；
- `ffmpeglatedrop1` 只用于历史诊断，不属于产品包。

ARM1176 没有 NEON 或 ARMv6T2。FFmpeg 3.0.2 的主要 H.264 qpel、chroma、prediction、
weighted prediction、deblock、IDCT 和 CABAC 快路径因此大多仍为 generic C。不能仅凭“assembly
enabled”宣称整个 H.264 hot path 已硬件优化。

## 显示分层

横屏窗口从下到上为：

```text
MMF native video RWindow 或 opaque RGB565 native surface
                           ↓
single transparent ARGB overlay: danmaku + controls + input
```

- 软件视频表面是持久、原生、`WA_NativeWindow`、不透明的 640×360 子窗口；
- ARGB overlay 不再绘制整帧视频；
- 弹幕、控制和触摸直接使用 640×360 坐标；
- `overlayIntermediateMs` 和 `overlayRotateMs` 应为 0；
- 历史 GLES 三平面上传和整帧 90° ARGB 旋转不在普通路径中。

## 横屏状态机

进入播放器：

```text
portrait main QGL fullscreen
→ main QGL showMaximized(), keep AVKON panes constructed
→ wait for stable portrait work area
→ request landscape
→ wait workAreaResized() and physical 640×360
→ show persistent player top-level fullscreen
→ show video surface and overlay, then open media
```

退出播放器：

```text
stop/park media and decoder
→ hide overlay, video surface and player top-level
→ request portrait
→ wait for physical 360×640
→ dynamically restore main QGL fullscreen
→ release foreground guard
```

每个等待阶段有保护超时。失败时结束本次播放并尝试恢复主页，不回退到虚拟旋转。

## 会话生命周期

以下对象创建一次并存活到应用退出：

- `VideoPlayerWidget` controller；
- MMF native QWidget/CCoeControl/RWindow host；
- soft RGB565 native surface；
- backend、observer 和 `CVideoPlayerUtility2`；
- 单一 ARGB overlay。

关闭播放只停止、解绑、清空会话数据并隐藏；再次进入复用原生对象身份，只关闭和重新打开媒体。
这一边界修复了原来在第二次 `PLAYER_REBUILD_BEGIN` 之前发生的确定性 data abort。

## 时钟和可观测性

soft 路径每 500 ms 校准一次 MMF `PositionL()`，中间按单调时间和播放倍速外推。
pause、seek、倍速、媒体会话变化会使缓存失效。普通 MMF 播放保持原有直接行为。

关键验证标记包括：

- 选路：`DEVVIDEO_HEADER_PREFLIGHT_ACCEPT/REJECT/ROUTE`；
- 横屏：`PLAYER_NATIVE_LANDSCAPE_640X360_READY`、`PLAYER_NATIVE_LANDSCAPE_VISIBLE`；
- soft：`FFMPEG_SOFT_READY ... RGB565_LUT2X2`、`SOFT_SURFACE_ACTIVE`、
  `SOFT_SURFACE_FIRST_PAINT`；
- 返回：`PLAYER_NATIVE_PORTRAIT_FULLSCREEN_READY`；
- 聚合性能：`SOFT_STATS`。

逐帧日志会显著干扰旧设备时序，普通验证优先使用聚合计数。

## 失败与回退

- Range 或 preflight 获取失败：记录原因并保守尝试 MMF；
- MMF 硬件视频不可用且 preflight 已拒绝：进入本机 FFmpeg；
- 软件解码初始化、内存、编码或性能不可接受：显示明确错误；外部播放器交接仍是待实现产品项；
- 任何失败都不得泄露 Cookie、完整签名 URL 或触发无限重试。

## 证据入口

当前决定见 `docs/decisions/`；完整控制实验、第二次进入分析、GLES 测量、原生横屏日志和
发布后硬件研究见[播放器研究索引](../research/README_ZH.md)。这些研究文档用于证明边界，
不再各自维护一份“当前实现”。
