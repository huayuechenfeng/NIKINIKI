# NIKINIKI 播放器架构

> 状态：Active
> 适用版本：1.1 当前主线
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

## 用户播放策略

设置页保存两个彼此独立的点播 MP4 策略；点击设置卡片会进入三项单选列表，选择后立即保存、
从下一次播放生效，不再通过反复点击卡片循环切换。默认值仍是原有的“流式播放 + 自动选择”，
升级安装不会改变既有播放行为。

| 设置 | 选项 | 实际路径 |
|---|---|---|
| 播放方式 | 流式播放 | MMF 直接 `OpenUrlL()`；原有 URL 全部失败后仍可做一次完整下载回退 |
| 播放方式 | OpenFile 流式播放 | 网络通过 `EFileShareReadersOrWriters` 原生 `RFile` 写入临时 MP4；写入并 flush 至 8 MiB 后，MMF 以同一共享模式取得只读 `RFile` 并调用 `OpenFileL(RFile)`，下载继续追加和定期 flush；不足 8 MiB 的短文件在完成后打开 |
| 播放方式 | 下载后播放 | 完整下载临时 MP4 后再调用 `OpenFileL()`；准备和下载期间由现有 ARGB overlay 显示文件大小、已下载量、百分比和进度条 |
| 解码方式 | 自动选择 | 保留 Broadcom header preflight：接受走 MMF，明确拒绝走 FFmpeg |
| 解码方式 | 全程硬解 | 跳过 header preflight，直接走所选 MMF 传输路径，绝不进入 FFmpeg；固件不支持时允许失败或黑屏 |
| 解码方式 | 全程软解 | 跳过 Broadcom 能力决定，MP4 Range 解析后直接启动 FFmpeg 视频；MMF 在 `Prepare` 后、`Play` 前尽力执行 `SetVideoEnabledL(false)`，AAC/时钟仍由 MMF 提供；旧 controller 拒绝关闭原生视频轨时继续使用上层不透明软件表面，只有解析或软件初始化失败才显示 `SWERR`，不暗中切回 MMF 视频 |

播放会话开始时锁定当次策略，设置变更从下一次播放生效。两种 `OpenFileL()` 路径共用单一、
有 96 MiB 上限的临时缓存，关闭播放器时删除。当前 Range/FFmpeg 管线只支持 progressive MP4；
直播仍固定使用原有 `OpenUrlL()` + MMF，若选择了不适用于直播的策略会记录
`PLAYER_POLICY_LIVE_USES_OPENURL_MMF`，不会伪装成已经执行。

`OpenFileL()` 边下边播是为区分旧机型 MMF streaming controller 与本地 controller 而加入的
产品可选路径。最初的 `QFile` 写入 + 按路径 `OpenFileL()` 已在 Nokia 603 真机复现
`KErrInUse (-14)`；当前实现改为读写双方显式共享的 `RFile` 句柄。Nokia 603 已确认该版本可以
打开持续增长的文件并播放，全程软解的 best-effort 视频轨关闭也不再提前进入 `SWERR`。603 在
2 MiB 起播时仍有开头卡顿，因此当前阈值提高到 8 MiB；该体验调整仍需真机确认。N8/X7/C7
是否因此解决“有声无画”仍必须分别真机验证，不能从系统播放器能播放本地完整文件直接推断。

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
- 软件视频路由在 MMF `Prepare` 完成后尽力禁用 MMF video track；旧 controller 拒绝时继续
  FFmpeg 与不透明软件表面，不能把这一优化当作软件解码准入条件；
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
- 完整下载模式在开始播放前复用同一个 ARGB overlay 绘制准备状态、文件大小、已下载量、
  百分比和进度条，不创建额外顶层窗口；
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

- 设置与传输：`PLAYER_POLICY`、`PLAYER_DECODER_ROUTE`、`PLAYER_LOCAL_ROUTE`、
  `PLAYER_LOCAL_OPEN`、`NATIVE_MMF_FILE_HANDLE`、`NATIVE_MMF_OPEN`；
- 选路：`DEVVIDEO_HEADER_PREFLIGHT_ACCEPT/REJECT/ROUTE`；
- 横屏：`PLAYER_NATIVE_LANDSCAPE_640X360_READY`、`PLAYER_NATIVE_LANDSCAPE_VISIBLE`；
- soft：`FFMPEG_SOFT_READY ... RGB565_LUT2X2`、`SOFT_SURFACE_ACTIVE`、
  `SOFT_SURFACE_FIRST_PAINT`；
- 返回：`PLAYER_NATIVE_PORTRAIT_FULLSCREEN_READY`；
- 聚合性能：`SOFT_STATS`。

逐帧日志会显著干扰旧设备时序，普通验证优先使用聚合计数。

## 失败与回退

- Range 或 preflight 获取失败：记录原因并保守尝试 MMF；
- 全程软解时 Range/MP4 解析失败：显示 `SWERR`，不回退 MMF 视频；
- OpenFile 下载失败：按备用 URL 单向尝试，全部失败后显示 `DLERR`；
- MMF 硬件视频不可用且 preflight 已拒绝：进入本机 FFmpeg；
- 软件解码初始化、内存、编码或性能不可接受：显示明确错误；外部播放器交接仍是待实现产品项；
- 任何失败都不得泄露 Cookie、完整签名 URL 或触发无限重试。

## 证据入口

当前决定见 `docs/decisions/`；完整控制实验、第二次进入分析、GLES 测量、原生横屏日志和
发布后硬件研究见[播放器研究索引](../research/README_ZH.md)。这些研究文档用于证明边界，
不再各自维护一份“当前实现”。
