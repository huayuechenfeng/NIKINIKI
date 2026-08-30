# 0.7 播放器 H.264 兼容性证据与实现状态

> 文档状态：Historical evidence。当前选路见 `docs/developer/PLAYBACK_ARCHITECTURE_ZH.md`。

> 记录日期：2026-08-25，最新更新：2026-08-28  
> 状态：BCM2727 已被真机证明拒绝故障码流；系统 ARM decoder 在 `ConfigureDecoderL()` 返回 `KErrNotSupported (-5)` 后已废止；`ffmpegsoft1` 已解出原有声无画视频。`ffmpegsoft2` 的 CPU RGB565 优化达到约 11.4–12.0 fps，late-drop 已关闭；GLES-YUV 候选已在真机测得约 216 ms/帧上传并退役，当前恢复 CPU RGB565 默认输出。  
> 产品约束：全部解码、同步和显示均在手机本机完成，不使用桥接、远端重封装或远端转码

## 0. 2026-08-26 控制实验结论（覆盖下文旧候选描述）

在 MMF 完全关闭、输入方式完全相同的条件下进行了重复对照：

| 码流 | MMF | BCM2727 `0x10204C21` | ARM H.264 `0x102073EF` |
|---|---:|---:|---:|
| 正常 High@5.1、4 refs/3 reorder/DPB4/weighted-off | `0` | `GetHeaderInformationL() = 0`，正确返回 640×360 | 未测 |
| 故障 High@3.0、7 refs/4 reorder/DPB7/weighted 1,2 | `-12017` | 稳定 `KErrNotSupported (-5)` | `0`，但尺寸字段为 0×0 |

因此已排除通用的 `SetInputFormatL()` 参数错误、Annex-B/SPS/PPS 组装错误、MMF 占用 BCM 和 CODA/偶发时序。BCM 固件确实拒绝故障模板；ARM 的结果只证明 header parser 接受，不能冒充“已经可以软解”。

后续 `armsoftprobe1` 真机实验已完成止损：`GetHeaderInformationL()==0` 后仍返回 0×0，`ConfigureDecoderL()` 稳定返回 `-5`，无法进入 Initialize/首帧。所以 ARM 结果不再是“待验证”，而是本轮明确失败的历史证据。

`headercontrol1` 为一次性诊断包，会在数秒后主动关闭 MMF，故无论视频原本是否可播都会停止。实验已完成，专用源码入口已经删除，绝不能作为日常或 Release 构建基线。默认源码行为如下：

- 正常 4/3/4/0,0 流输出 `WW:DEVVIDEO_MP4_PROFILE_SKIP`，继续由 MMF 播放；
- 故障 7/4/7/1,2 流输出 `WW:DEVVIDEO_MP4_BCM_REJECTED_KEEP_MMF`，保留 MMF 可用的 AAC，不再自动进入已知必败的 BCM 路径；
- 若确需继续旧 BCM 实验，必须显式加入 `WILIWILI_ENABLE_UNSUPPORTED_BCM_DEVVIDEO`，避免诊断行为被误编进普通包。

## 1. 问题边界

Nokia 603 上，部分 Bilibili progressive MP4 会由 MMF 返回
`KErrMMPartialPlayback (-12017)`：AAC 有声音，但 H.264 没有画面。样本矩阵稳定分为两组：

| 真机结果 | 样本 | refs | reorder / DPB | weighted P / B |
|---|---|---:|---|---|
| 失败 | `BV1Uy8x6AETG`、`BV1AxhK6BE2j`、`BV1oX876REZK`、`BV1iL8K6HEMy` | 7 | 4 / 7 | 1 / 2 |
| 成功 | `BV1oyhM6AETw`、`BV1eohG6rESA`、`BV16uhN68EUC`、`BV1wqhg62ERJ` | 4 | 3 / 4 | 0 / 0 |

两组都是 H.264/AVC、YUV420。失败组即使请求 360P，SPS/PPS 的
7/4/7/weighted 模板仍不变，因此“统一降清晰度”不是根治方法。

2026-08-25 再次核对官方 playurl：`BV1Uy8x6AETG` 的 Q64 返回
`format=mp4720`、单文件 1280×720 progressive MP4；Q16 返回
`format=mp4`。旧代码只接受格式字符串精确等于 `mp4`，导致 Q64 的兼容检测从未启动。这一入口错误已修正为接受 `mp4*`。

Q6 仍是文档定义的历史 HTML5 progressive-MP4 能力，但“参数可取”不等于
每个稿件都存在 240P 对象。2026-08-28 直接重查官方
`x/player/playurl`/`x/player/wbi/playurl`，固定请求
`qn=6&platform=html5&fnval=1`。六个对照样本
`BV1Uy8x6AETG`、`BV1AxhK6BE2j`、`BV1oX876REZK`、
`BV1iL8K6HEMy`、`BV1oyhM6AETw`、`BV1eohG6rESA` 全部返回
`quality=16`、`format=mp4`，`accept_quality=[64,16]`，
`support_formats` 也只有 720P/360P，没有 DASH 响应。对故障与正常样本
分别做 Q6/F1、Q16/F1、Q16/F0 对照，服务端返回的对象路径均为
`...-1-16.mp4`，时长与字节数完全一致。另外对早期 AV 稿件、文档的旧
PGC 样例、移动/desktop/Symbian UA 以及用户指定的四 P 视频
`BV15EhG6qEAg` 进行对照，当前区域和未登录权限下也都是服务端
把 Q6 提升回 Q16。例如该视频第 1 P 的 Q6/Q16 均为
`41250393999-1-16.mp4`、`183040 ms`、`14166873 bytes`。

上述返回已证明当前样本没有可用的 240P 对象。曾尝试加入“风险 Q16 →
Q6 验真 → 软解”链路，但它只会额外请求同一条 Q16 对象，增加状态和
等待时间。现已按用户要求完整撤回，不在播放器保留 Q6 分支。当前有效路由
仍是：SPS 预检安全则留在 MMF，命中已知硬解风险模板则直接进入本机
FFmpeg 软解。

## 2. 历史候选：本机 BCM 兼容链路（已被 header 控制实验否决）

正常 4/3/4/0,0 码流仍走已经验证的 `CVideoPlayerUtility2` 原生 MMF 路径。
下列链路曾经实现并通过编译，但 BCM 在 `GetHeaderInformationL()` 即拒绝目标码流，因此不是当前正式后端，也不得默认接管：

```text
Bilibili progressive MP4
        │
        ├── MMF CVideoPlayerUtility2：网络/容器/AAC 音频/主时钟
        │
        └── HTTP Range + Mp4AvcProbeReader
                ├── moov/avcC/SPS/PPS 与 sample table
                ├── 每约 5 秒一批，最多 6 MiB
                ├── 每个 MP4 sample 独立转为一个 Annex-B access unit
                └── CMMFDevVideoPlay / BCM2727 0x10204C21
                        ├── EDuCodedPicture + EDuElementaryStream
                        ├── DTS、sequence number、系统时钟同步
                        ├── YUV/RGB memory picture → QImage
                        └── 持久 ARGB 覆盖窗：视频 → 弹幕 → 控制
```

关键实现：

- `Mp4AvcProbeReader` 解析 `avcC`、SPS/PPS、`stsz/stsc/stco|co64/stts/stss`；
- 从目标时间之前最近的同步帧开始，按 sample 边界请求连续 Range；
- SPS/PPS 只注入开始或 seek 后的第一个 access unit；
- 不再使用旧探针的 `EDuArbitraryStreamSection` 和任意 32 KiB 分块；正式路径严格使用“一缓冲区一张编码图像”的 `EDuCodedPicture`；
- DevVideo 固定选择真机已确认的 Broadcom BCM2727 UID `0x10204C21`，最大 1280×720 / 14 Mbps；
- MMF 保留 AAC 播放。兼容路径启动时调用 `SetVideoEnabledL(false)` 释放不可用的视频轨，不关闭媒体和音频；
- DevVideo 使用 `CSystemClockSource`，以 MMF position 初始化；漂移超过 150 ms 时重同步；pause/resume 同时控制音频、硬解器和时钟；
- seek 会中止旧 Range、清空旧硬解器，从目标时间之前最近的关键帧重新初始化；
- 输出格式优先选择 YUV420 planar，其次 semi-planar、RGB565/RGB888、YUV422；转换只生成覆盖窗实际需要的最大 640×360 帧，并把转换频率限制到约 22 fps，避免 Nokia 603 的 CPU 被颜色转换占满；
- 持久 ARGB 窗口先绘制不透明视频帧，再绘制弹幕和控制，弹幕不可能被视频窗口遮挡；横屏继续使用已经验证的 640×360 离屏栅格化和 1:1 旋转，UI 清晰度不回退；
- 约 5 秒一批持续补充 sample，结束时调用 `InputEnd()`；退出、新媒体和析构统一停止并销毁 DevVideo，保留 0.7 已验证的 MMF/窗口重复进入对象图。
- 已提交到 DevVideo 自有输入 buffer 的压缩 access unit 会分批从 Qt 容器释放，避免长视频把完整压缩轨持续保留在 RAM；输入或硬解中途失败会撤掉缓存视频帧并恢复安全回退判定，避免冻结帧遮住播放器。

## 3. 安全回退

默认构建不再尝试已知必败的 BCM 接管：风险模板识别完成后立即结束 Range 探测，让原 MMF 会话继续提供可用的 AAC，并显示 `AUDIO` 状态。这样不会再重现 `headercontrol1` 对所有视频数秒后关停的问题。

在历史 0.7 构建中，“保留音频”是安全行为，不冒充完整兼容。`ffmpegsoft1` 已在 Nokia 603 真机把原故障码流解出画面，证明 FFmpeg 3.0.2 H.264 decoder、MP4 sample、时间戳和 overlay 出口均能工作；但用户观察只有约 2–3 fps，因此该版本只作为兼容性/纯 C 性能基线。0.9 已将优化后的 `ffmpegsoft2` 合并主线，风险模板现在按本机 FFmpeg 路由，当前签名包和限制见 `docs/releases/RELEASE_0.9.0_ZH.md`。

## 4. 关键日志

| 日志 | 含义 |
|---|---|
| `WW:DEVVIDEO_MP4_AVC ... risk` | MP4/SPS/PPS 分类；最后一个值为 true 表示已知风险模板，普通构建不会再交给 BCM |
| `WW:DEVVIDEO_MP4_PROFILE_SKIP` | 正常模板继续使用 MMF，不进入 DevVideo |
| `WW:DEVVIDEO_MP4_BCM_REJECTED_KEEP_MMF` | 风险模板已知会被 BCM 拒绝；普通构建保留 MMF AAC |
| `WW:DEVVIDEO_PLAYER_RANGE_BEGIN` | 正式 sample 批次范围与 sample 序号 |
| `WW:DEVVIDEO_PLAYER_HANDOFF` | access unit 数量、首末 DTS、是否重置 decoder |
| `WW:DEVVIDEO_PLAYER_MMF_VIDEO_DISABLED` | 释放 MMF 不可用视频轨的结果，应为 0 |
| `WW:DEVVIDEO_PLAYER_HEADER` | BCM2727 识别出的 profile/level/尺寸 |
| `WW:DEVVIDEO_PLAYER_OUTPUT_SELECTED` | 选中的内存像素格式 |
| `WW:DEVVIDEO_PLAYER_INIT` | DevVideo 初始化结果，应为 0 |
| `WW:DEVVIDEO_PLAYER_FIRST_PICTURE` | 第一张真实内存帧；末尾应为 true 和非空尺寸 |
| `WW:DEVVIDEO_PLAYER_PROGRESS` | 已解图片与尚未写入的 access unit 数 |
| `WW:DEVVIDEO_MP4_CTTS` | MP4 是否含 `ctts`、版本、entry 数和 composition offset 范围 |
| `WW:ARM_SOFT_MMF_AUDIO_RETAINED` | ARM 路径启动时 MMF AAC 会话仍然存活 |
| `WW:ARM_SOFT_PROGRESS` | 每 25 张图片记录累计图片、耗时、音频位置、帧 PTS 和压缩队列 |
| `WW:FFMPEG_SOFT_MMF_AUDIO_RETAINED` | FFmpeg 路径启动前 MMF AAC 会话和主时钟仍在 |
| `WW:FFMPEG_SOFT_BEGIN` | 首批 Annex-B access unit、编码尺寸和首个 DTS/PTS 已交给工作线程 |
| `WW:FFMPEG_SOFT_READY` | libavcodec H.264 decoder 已打开 |
| `WW:FFMPEG_SOFT_FIRST_FRAME` | 首张 YUV420 解码帧已进入输出队列；consumer 选中后转为有效 RGB565 `QImage` |
| `WW:FFMPEG_SOFT_PROGRESS` | 每 25 帧的解码进度、PTS 和待解 access unit |
| `WW:FFMPEG_SOFT_TIMING` | 每 25 张输出的累计 `pictures wall_ms decode_ms convert_ms late_drops`，用于区分 H.264、RGB565 与迟到帧丢弃 |
| `WW:FFMPEG_SOFT_CATCHUP` | 视频相对 MMF AAC 落后超过 500 ms 时进入追帧，追回到 150 ms 内退出 |
| `WW:FFMPEG_SOFT_LATE_DROP` | 仅 `CONFIG+=ffmpeglatedrop1` 诊断包出现；普通 `ffmpegsoft2` 已禁用该失败策略 |
| `WW:FFMPEG_SOFT_ERROR` | find/open/decode/pixel-format 阶段和 libavcodec 错误码 |
| `WW:DEVVIDEO_PLAYER_CLOCK_RESYNC` | 音频/视频时钟漂移超过 150 ms 后的校正 |
| `WW:DEVVIDEO_PLAYER_SEEK` | seek 目标和采用的同步 sample |
| `WW:DEVVIDEO_PLAYER_FATAL/FAILED` | 固件拒绝或运行期硬解失败 |

日志不得包含 Cookie 或完整签名媒体 URL。

## 5. 构建与验收状态

- Debug ARMv5/GCCE：2026-08-25 六轮完整构建均 `sbs errors: 0`；
- Release ARMv5/GCCE：四轮完整构建均 `sbs errors: 0`；
- SIS：Debug/Release `codeccompat1` 的 unsigned 与 current-certificate 归档均已生成；`signsis -o` 已验证当前证书有效期至 2036-08-21，大小和 SHA-256 见 `docs/releases/RELEASE_0.7.0_ZH.md`。它们是历史诊断包，不是 0.9 安装包；
- `armsoftprobe1` Debug 与无实验宏的标准 Debug 均完成 GCCE ARMv5 全量构建，`sbs errors: 0`；当前签名实验包 SHA-256 为 `17730DC83BAD4FDC66AF35FE721AE6A2C6D56D772EC0B1CEDCF7B370F2E0CF3E`；
- ARM 真机状态：`ConfigureDecoderL()=-5`，路线已废止；`armsoftprobe1` 只保留作历史证据；
- PPSSPP-FFmpeg 源码固定在 commit `b87f7c6d522d1edba77cfc4fac96ce48a236f806`；GCCE 4.4.1 已生成 H.264-only `libavcodec`/`libavutil`；当前优化库使用 ARM1176JZF-S、ARMv6/VFPv2、`-Os` 且禁用 NEON。全库 `-O2` 会让最终 `.rodata` 越过固定 `0x400000` 数据边界，不能用于该 Symbian EXE；进一步源码审计确认主要 H.264 快路径只为 NEON/ARMv6T2 提供，ARM1176 上仍主要走 generic C；
- `ffmpegsoft1` Debug 应用全量构建 `sbs errors: 0`，已打包和签名；普通 qmake 构建对 FFmpeg 宏/源文件/静态库的 MMP 命中为 0；
- 当前签名候选：`symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_ffmpegsoft1_currentcert.sis`，9,709,300 bytes，SHA-256 `5FCA1231349D67088C93F0A2D7AEF5D6A90E2DB0E6ECBCD382EC8ED40B7E609A`；
- 真机状态：BCM 与 ARM 候选已被否决；`ffmpegsoft1` 首帧/画面通过、约 2–3 fps 性能失败；`ffmpegsoft2` 的 LUT2X2 CPU 版约 11.4–12.0 fps。late-drop 两段分别约 5.8/7.1 可见 fps、媒体推进约 0.78×，已从普通候选关闭。GLES-YUV/ping-pong 候选已实测约 216 ms/帧上传、约 321 ms/帧提交，已退出普通路径。

真机验收顺序：

1. `BV1oyhM6AETw`：应输出 `PROFILE_SKIP`，继续由 MMF 正常播放；
2. `BV1Uy8x6AETG`：0.9 主线应输出 `FFMPEG_SOFT_READY` 并保留 MMF AAC，不能自动关闭播放器；
3. 重新构建并安装 `docs/releases/RELEASE_0.9.0_ZH.md` 中的 Release 包进行 30 秒方向/覆盖/性能、pause/seek 和音画同步验收；主线 READY 应为 `ARM11_GENERIC_H264_RGB565_LUT2X2`，不加入 `ffmpeglatedrop1`；
4. Release 完成 50 次进入/退出后，才把 0.7 标为稳定版。

## 6. 代码入口

- `symbian/source/platform/mp4_avc_probe_reader.cpp`：MP4/SPS/PPS/sample 表、分批 Range 所需索引、AVCC→逐帧 Annex-B；
- `symbian/source/platform/video_playback_backend.cpp`：MMF 音频、DevVideo BCM2727、时钟、像素转换与帧交付；
- `symbian/include/platform/ffmpeg_h264_decoder.h`、`symbian/source/platform/ffmpeg_h264_decoder.cpp`：FFmpeg 工作线程、有界 YUV 帧队列、PTS 同步和 consumer 侧 RGB565 输出；
- `symbian/third_party/ppsspp_ffmpeg/`：GCCE 可复现构建脚本、库/头文件和 LGPL 许可文件；
- `symbian/source/ui/video_player_widget.cpp`：风险检测、Range 状态机、seek/回退、视频/弹幕/控制合成；
- `symbian/include/platform/*.h`、`symbian/include/ui/video_player_widget.h`：正式接口与生命周期状态。
