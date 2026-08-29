# 1.0 前播放器解码与外部回退政策

> 决定日期：2026-08-26；路由修订：2026-08-29  
> 状态：1.0 公开发布前的强制执行路线  
> 目标设备：Nokia 603 / Nokia 808 等 Symbian³ / Belle 设备

## 1. 已冻结的产品决定

1. 1.0 公开发布前，不再探索 BCM2727/BCM2763 硬件解码边界，不再尝试修改 profile/level/SPS、伪造能力表、替换 HwDevice UID、DSA/post-processor 直显或继续猜测私有固件接口。唯一例外是本政策定义的只读 header preflight：它复用已证实的 Broadcom UID 和输入契约，不启动解码器。
2. 系统 MMF 能正常硬解的视频继续使用 0.7 已验证的 `CVideoPlayerUtility2` 路径。
3. 每个首选 progressive MP4 在 MMF 打开 URL 前，先对其 SPS/PPS 加首个同步 access unit 进行一次 `CMMFDevVideoPlay::GetHeaderInformationL()` preflight。`0x10204C21` 接受则使用 MMF；明确拒绝则直接转手机本机软件解码。不再用 refs/DPB/weighted 等风险模板猜测，也不插入已实测只返回同一 Q16 对象的 Q6 降级。
4. 若本软件内置软解不支持该编码、性能不足、内存不足或初始化失败，允许用户明确选择外部播放器。
5. 不引入桥接、远端重封装或远端转码；视频兼容处理优先在手机本机完成。

这项决定覆盖旧文档中“继续拓展 BCM2727 Direct DevVideo 并作为 1.0 正式候选”的描述。

## 2. 决策依据

Nokia 603 真机已经完成严格控制实验：

| 输入 | MMF | BCM H.264 `0x10204C21` | ARM H.264 `0x102073EF` |
|---|---:|---:|---:|
| 正常 640×360 High@5.1、4 refs/DPB4/weighted-off | `0` | header 返回 `0`，尺寸 640×360 | 未测 |
| 故障 640×360 High@3.0、7 refs/DPB7/weighted 1,2 | `-12017` | header 稳定返回 `-5` | header 返回 `0`，尺寸 0×0 |

两组使用相同的 `video/h264`、`EDuCodedPicture`、`EDuElementaryStream` 和 Annex-B 输入；MMF 在 Direct DevVideo 前已经完全关闭。因此通用输入协商、SPS/PPS 封装和 MMF 资源占用都不是 BCM 失败原因。

ARM decoder 的返回值只证明其 header parser 接受故障流，不能证明已经可持续解码。由于它报告为非硬件加速设备，后续可以把它作为“本机软件解码候选”评估，但不再借此继续扩大 BCM 硬解研究范围。

## 3. 1.0 播放选择顺序

```text
打开 Bilibili progressive MP4
        │
        ├── MP4 header + 首个同步 AU → Broadcom header preflight
        │       │
        │       ├── `GetHeaderInformationL()==0`
        │       │       └── 原生 MMF 硬解 + MMF RWindow + ARGB 弹幕/控制
        │       │
        │       └── plugin 明确拒绝
        │               ├── 应用内本机软解可用
        │               │       └── 软解视频 + MMF AAC/主时钟 + ARGB 弹幕/控制
        │               │
        │               └── 应用内软解失败或不支持
        │                       └── 提示用户交给外部播放器
        │
        └── Range/容器 preflight 失败
                └── 保守回到 MMF；日志保留失败原因
```

不得在几条 CDN、几个清晰度、MMF、软解和外部程序之间循环。每次媒体会话只允许一次有方向的降级。

## 4. 应用内软解路线

### 4.1 可复用基础

- `Mp4AvcProbeReader` 已能解析 `moov/avcC`、SPS/PPS、sample table 和同步样本；
- 已能按 MP4 sample 生成 Annex-B access unit 和 DTS；
- MMF 对故障流仍可提供 AAC 与 position，可作为音频和主时钟；
- 单 ARGB 覆盖窗已经能按“视频帧 → 弹幕 → 控制”合成，横屏继续采用 640×360 离屏图像的 1:1 旋转；
- 播放器完整对象图已经支持重复进入，不能为软解回退破坏这一生命周期。

### 4.2 候选优先级

1. 将固件提供的 ARM H.264 decoder `0x102073EF` 作为本机软件候选，独立验证 `ConfigureDecoderL`、输出格式、`Initialize`、首帧、连续 30 秒性能和内存；
2. 若系统 ARM decoder 不可用，使用 QtSDK/GCCE 4.4.1 可从源码重编的裁剪 libavcodec 或 OpenH264 类实现；
3. `hrydgard/ppsspp-ffmpeg` 的 Symbian/ARMv6 资料可用于参考补丁和构建参数，但现有预编译 `avcodec.lib`/`avutil.lib` 由更新 GCC 生成，带 GCCE 4.4.1 无法识别的强制 EABI object attribute 44，不能直接链接进当前应用；必须用兼容工具链从源码重编。

两条路线的执行阶段、止损条件和相对工作量见 `docs/PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md`。当前选择是先做系统 ARM decoder 的 Configure/Initialize/首帧与 30 秒性能验证；它失败后才启动 PPSSPP-FFmpeg 的 GCCE H.264-only 源码重编。

### 4.3 最低实现要求

- 首个目标只覆盖 640×360 H.264/yuv420p，不承诺 720P/1080P 软件解码；
- 保持 MMF AAC，不重复软件解音频；
- 以 MMF position 为主时钟，视频帧根据 PTS/DTS 丢帧或等待；
- `Mp4AvcProbeReader` 必须先解析 `ctts`，为每个 access unit 同时提供 DTS 和 PTS；故障流有 B 帧重排，不能继续把 DTS 当显示时间；
- 输出优先采用能低成本上传或转换的 YUV420；必要时生成覆盖窗尺寸的 RGB565/RGB888；
- 允许主动丢帧，优先保证音频连续、UI 响应和弹幕时钟正确；
- pause、seek、返回、前后台和第二次进入必须清理 decoder、Range reply 与帧队列；
- 内存和性能失败必须可恢复地进入外部播放器提示，不能闪退。

## 5. 外部播放器回退

外部程序是明确允许的产品降级，而不是隐藏的调试行为。

- 只能在 MMF 和应用内软解都不可用后触发；
- 必须显示确认提示，说明内置播放器无法解码，不能静默切走；
- 优先通过系统文档/内容处理机制打开本地临时媒体；若外部播放器支持网络 URL，可在不泄露 Cookie 的前提下传递短期直链；
- 不把完整 Cookie、登录令牌或日志中的签名请求头写入命令行、公共文件或外部程序参数；
- 临时文件要有空间检查、取消按钮和后续清理策略；不能在外部程序仍使用文件时立即删除；
- 外部播放器退出后，wiliwili 应恢复页面、方向、覆盖层和前台状态。

具体 Symbian AppArc/文档处理 API 在实现前另做最小探针，不在本政策文档中假定某个外部播放器一定存在。

## 6. 当前源码保护

2026-08-29 当前源码的保护边界：

- `headercontrol1` 的完整 decoder configure/init/output 控制函数、ARM UID 参数和 `DEVVIDEO_CONTROL_*` 路径仍然删除；
- 新的 `probeAvcHardwareHeader()` 只执行 `NewL → SelectDecoderL(0x10204C21) → SetInputFormatL(video/h264) → GetHeaderInformationL`，并在返回前销毁临时对象；不调用 `ConfigureDecoderL`、`Initialize`、`Start`、DSA、post-processor 或 `WriteCodedDataL`；
- 正常构建不再输出 `PROFILE_SKIP` 或由 `7/4/7/weighted` 规则选路。必须记录 `WW:DEVVIDEO_HEADER_PREFLIGHT_ACCEPT/REJECT/ROUTE`；
- FFmpeg、MMF AAC、PTS/DTS、Range 批次、队列、catch-up、RGB565 surface 与 ARGB overlay 都保持原实现；
- 2026-08-29 GCCE ARMv5 Debug 完整构建为 `sbs errors: 0`。

`headercontrol1`、`codeccompat1`、`devvideosample1` 均为历史证据，不是 1.0 播放或发布基线。已验证可重复进入的 `surfacepersist1` 仍是 MMF 回归对照。

## 7. 1.0 验收门槛

- 一个已知正常样本必须得到 `PREFLIGHT_ACCEPT → ROUTE MMF`，且由 MMF 连续播放；
- 一个此前 AUDIO-only 样本必须得到 `PREFLIGHT_REJECT → ROUTE FFMPEG` 和应用内软解画面，或清晰、可执行的外部播放器回退；
- 软解/外部回退不得破坏声音、弹幕、清晰横屏 UI 和重复进入；
- 失败不能触发无限清晰度/CDN/后端循环；
- Release 完成 50 次播放器进入/退出，并通过至少 30 分钟的内存与温度观察；
- 只有达到上述门槛，才能把 1.0 标记为公开稳定版本。
