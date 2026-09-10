# E7 DevVideo memory-output 路线计划

> 状态：C1、C2 已关闭；C3/BD 未启动（`codex/e7-devvideo-memory-routes`）
> 目标设备：Nokia E7 / Symbian^3，USB CODA 真机连接
> 产品边界：本文件只记录诊断路线和准入条件；诊断 CONFIG、探针和未通过结果不得写成正式播放器能力。
> 本页保留本轮路线与停止条件；六次/C1/C2之后的建议以[重新评估](E7_BLACK_SCREEN_REASSESSMENT_ZH.md)为准，不自动启动C3。

## 背景与决策

E7 六次 Qt/MMF 观察中，音频、轨道和 position 可以推进，但 R 路径没有首帧、renderer 入队或
surface submit 证据。P v2 同一媒体成功显示（`Received=4520`、`Displayed=4498`、`Skipped=22`、
`overflow=0`），因此当前证据不足以把黑屏归因于 H.264 解码失败，也不足以证明只是 Qt 顶层遮挡。

现有 FFmpeg 软件回退虽然能出画面，但 Nokia 603 的 CPU YUV420P→RGB565 基线约 11.4–12 fps，
在 E7 上不能作为可用体验的唯一修复。独立 Player 不再建立；新路线必须留在现有 NIKINIKI
player controller、时钟、输入和 overlay 生命周期内。

## 固定尝试顺序

| 顺序 | 路线 | 输出链 | 目的 | 进入条件 |
|---|---|---|---|---|
| C1 | DevVideo memory picture → opaque native surface | 应用自有 `CMMFDevVideoPlay`；memory YUV/RGB → 既有不透明表面 | 先证明 E7 固件能否在应用内拿到可读 picture，并绕开 MMF display-window | **已关闭（C1-Solo + 唯一 C1-PP 结构性补测）**：Solo 的真实 Header/Configure/OutputSet 可达，但 `EFalse` 初始化回调为 `-44`、无 picture；`ETrue` 同步 `-5`。补测的 PP `0x10273417` 与 decoder 成功协商共同 YUV，`SetBufferOptionsL`、`SetVideoDestScreenL(EFalse)` 均成功；optional `GetOutputFormatListL(PP)` 虽为 0 项，默认 memory 合约仍实际进入 `Initialize()`，异步回调仍为 `-44`、无 picture。C1 不再追加 clock/flag/PP 参数，转 C2。 |
| C2 | MMF graphics surface / `AddDisplayL` | 仍用现有 controller/时钟，改用 graphics surface 绑定 | 判断黑屏是否仅由传统 `RWindow`/display-window 造成 | **已关闭（失败）**：E7 `Open=0`、`Prepare=-12017`，音视频轨均存在且 `AddDisplayL` 返回 0；但 90 秒运行没有 `MmsehSurfaceCreated`、surface attach、首帧或 renderer/display 计数。用户确认手机“有声音、无画面”，且不能正常证明视频可见。该路线未建立可用 surface 输出，也未证明黑屏只由传统 display-window 引起；不进入 C3。 |
| C3 | 正式 MMF display-window binding | 修正同会话首帧、renderer 入队、surface submit 和窗口时序 | 继续攻坚最小改动的系统路径 | C2 有正向或强区分力证据；不允许只靠“音频在走”判定成功 |
| BD | DevVideo memory picture → NIKINIKI GLES | memory frame 直接上传/绘制到现有 QGL/GLES；不新增独立 Player | 在 C1 可读 picture 后评估更高效的显示，避免 CPU RGB 转换 | 当前搁置：C1 没有任何 picture callback。仅在未来独立证据取得稳定 memory frame、格式/stride/crop 后才可重开；先做性能测量再决定是否产品化。 |

C1/C2/C3/BD 必须按此顺序保存结果；任一路线失败都保留日志、构建身份和失败边界，不能跳过
控制实验直接宣称下一路线可行。

## C1 两阶段实验

### C1-Solo（已完成配对，E7 上停止）

`e7devvideomemorysolo1` 禁用 FFmpeg 且完全不打开 MMF。应用自有 Broadcom DevVideo 会话执行：

```text
SelectDecoderL(0x10204C21)
→ SetInputFormatL(video/h264)
→ GetHeaderInformationL(real header)
→ ConfigureDecoderL
→ GetOutputFormatListL / 选择 YUV 或 RGB565 memory output
→ Initialize → Start → WriteCodedDataL → NextPictureL
```

该实验回答“公开 DevVideo memory-output 初始化/提交本身是否可行”，不回答音画同步。E7 三次受控运行均未越过初始化：
真实样本 A 的 Header（640×360）、Configure、5 项 output list 和 YUV420 planar `SetOutputFormatL` 均成功；
`EFalse` + 时钟、`EFalse` + `SynchronizeDecoding(EFalse)` 均在 `MdvpoInitComplete` 返回 `-44` 且 0 picture，
`ETrue` 则 `SetVideoDestScreenL` 直接返回 `-5`。这组结果只证明 decoder-only memory contract 未建立，
因此按计划做一次 PP 结构性补测，而不再继续调 clock/flag。

### C1-PP（唯一结构性补测，已完成）

`e7devvideomemorypp1` 仍禁用 FFmpeg，并在同一 `CMMFDevVideoPlay` 会话中动态枚举、选择 E7
实际提供的 post-processor，再按公开 DevVideo contract 协商：

```text
SelectDecoderL(0x10204C21) + SelectPostProcessorL(E7 UID)
→ SetInputFormatL(video/h264) + real Header/Configure
→ decoder output ∩ PP source formats
→ SetOutputFormatL(decoder) + SetInputFormatL(PP)
→ SetPostProcessTypesL(PP, 0)
→ GetOutputFormatListL(PP) + memory destination
```

E7 枚举到 1 个 PP（`0x10273417`，accelerated/direct-display 均为 true，28 个 source formats）。
真实样本 A 与 decoder/PP 存在 3 个共同 YUV 格式，选中的 planar YUV、decoder output 和 PP input
设置均返回 0；PP 的 optional `GetOutputFormatListL` 返回成功且 **0 项**，但这不能单独证明不存在
默认 memory endpoint。最终补测继续使用默认合约：`SetBufferOptionsL`、`SetPostProcessTypesL(PP,0)` 和
`SetVideoDestScreenL(EFalse)` 均返回 0，`Initialize()` 实际发出，随后异步 `MdvpoInitComplete=-44`，
无 picture callback。故 C1 的关闭依据是 E7 在共同格式与默认 memory 合约后仍无法初始化硬件，
不是把 optional output-list 为空误判为未初始化；不再追加 clock、flag 或 PP 参数。PP 的 direct-display
标志仅保留为 C2/C3 情报，不重新开启已封存的 Broadcom Direct DevVideo/DSA 正式路径。原始最终日志：
`.tmp/e7-coda/install-c1pp-default-memory-com4.jsonl`；设备事实见矩阵。

### C1-MMF retained

仅在 C1-Solo 能稳定收到 picture 后启用 `e7devvideomemory1`：MMF 保留用于 AAC 和主时钟，
其视频轨在 Prepare 前关闭；DevVideo picture 交给同一持久不透明 native surface。若 solo 成功而
retained 失败，优先归因于 MMF 视频资源/会话冲突，而不是 display-window。本轮 C1-Solo 与 C1-PP
均未产生 picture，因此该 retained 变体不进入测试。

## C2：MMF graphics-surface 绑定（已关闭，失败）

`e7mmfsurface1` 保留现有 `CVideoPlayerUtility2`、MMF AAC 主时钟、player controller、原生 video host
和透明 overlay；它不创建独立 Player，也不启用 DevVideo probe。为与传统 display-window 路径作出真正
区分，Open 成功后直接 `Prepare()`，不调用 `AddDisplayWindowL`。在 Prepare 成功且 `VideoEnabledL()` 为
true 后，以当前 `ScreenDevice()->GetScreenNumber()` 调用 `AddDisplayL(WsSession, displayId, handler)`；
`MmsehSurfaceCreated` 回调中仅把返回的 `TSurfaceId` 附到既有 `RWindow::SetBackgroundSurface()`，并 flush
WSERV。退出时先 `RemoveBackgroundSurface(ETrue)`、再 `RemoveDisplay(displayId)`、最后 `Close()`。

本次只检验该公开 MMF surface 输出链。通过必须同时满足：Open/Prepare 可用、视频轨启用、`AddDisplayL`
成功、收到 surface-created、background attach 返回 0、音频与 position 推进、用户在 E7 上确认实际视频
 可见且能正常退出返回。仅有 controller、surface 或 position 日志均不能证明出画。

### C2 真机结果（已关闭）

诊断包：`e7mmfsurface1`，SIS
`symbian/out/wiliwili-symbian-debug-e7mmfsurface1/NIKINIKI_1.2.0_debug_e7mmfsurface1.sis`；
设备为 E7 / RM-626 / Belle Refresh `111.040.1511`，媒体为本地样本 A
`E:/test/test.mp4`（18,127,827 字节，SHA-256 见设备矩阵）。本次通过 USB CODA 安装并运行 90 秒，
固定使用现有 NIKINIKI `CVideoPlayerUtility2`、播放器 controller、AAC/主时钟、原生 video host
和 overlay，没有创建独立 Player，也没有启用 DevVideo probe。

关键日志边界：

```text
NATIVE_MMF_OPEN_COMPLETE 0
E7_MMF_SURFACE_PREPARE_WITHOUT_DISPLAY_WINDOW
NATIVE_MMF_PREPARE_COMPLETE -12017
NATIVE_MMF_TRACKS true 0 true 0 true
E7_MMF_SURFACE_ADD_BEFORE 0
E7_MMF_SURFACE_ADD_AFTER 0 0
NATIVE_MMF_PLAY
NATIVE_MMF_AUDIO_STATE play ... enabled true ...
```

运行期间没有 `E7_MMF_SURFACE_CREATED`、`E7_MMF_SURFACE_PARAMETERS`、
`E7_MMF_SURFACE_REMOVED` 或首帧/renderer 提交计数。零PP统计的实际阶段是Prepare回调之前，
不是90秒播放后的累计遥测；实例归属限制见设备矩阵。用户实机确认“无画面、有声音”，本次不再继续调试。

结论：公开 `AddDisplayL` 调用及音频时钟路径可以建立，但 E7 本次没有交付 surface 或视频帧，
graphics-surface 路线不构成可用显示链。C2 不能把黑屏归因收窄为传统 display-window，
也不能作为 C3 正向进入条件；C3 与 BD 保持未启动，后续是否重开必须有新的独立证据和路线决策。

## 每次真机验收必须记录

- 构建 variant、SIS/EXE 身份、设备固件和媒体样本；
- Header、Configure、Initialize、Start、WriteCodedData 返回码；
- 首帧延迟、picture 数量、`ReturnPicture` 是否持续、stream-end counters；
- 首帧实际宽高、crop、`iDataFormat`、YUV pattern/layout、stride/字节数；
- picture CRC 或稳定颜色特征，以及 native surface 的首次提交/绘制计数；
- 是否出现 `-44`、`-5`、buffer pool 停滞、fatal/picture-loss；
- 退出、再次进入和返回主界面的稳定性。

通过门槛是：同一媒体连续收到可读首帧，至少 20 秒无 buffer pool 卡死，画面可见且能正常返回；
性能路线另需 decode/present 分段数据。只有真机通过后，才能把路线提升为候选产品实现。

## 失败与停止条件

- C1-Solo 在真实 header、Configure 或 Initialize 阶段稳定返回不支持，或 C1-PP 在共同格式之后
  没有 PP memory output：停止 C1，转入 C2/C3 的 display 诊断，不伪造 SPS/PPS，也不把
  full-SPS fake 当方案；
- 能解码但 `NextPictureL` 永远无 picture：记录为 decoder/output contract 失败，先查 output
  format、buffer ownership 和 timestamps；
- solo 有 picture、retained-MMF 无 picture：保留 solo 结果，停止扩大 MMF 资源猜测；
- picture 可读但 RGB565 CPU 转换低于体验门槛：不否定 C1 解码，转 BD 做 memory→GLES 性能测量；
- 任一路线造成独立窗口、独立时钟或第二套 player 生命周期：视为越界并停止合入。

## 构建与记录规则

诊断 variant 通过 `symbian/Build-App.ps1` 显式传入：

```powershell
.\symbian\Build-App.ps1 -Configuration debug -Variant e7devvideomemorysolo1
.\symbian\Build-App.ps1 -Configuration debug -Variant e7devvideomemory1
.\symbian\Build-App.ps1 -Configuration debug -Variant e7devvideomemorypp1
.\symbian\Build-App.ps1 -Configuration debug -Variant e7mmfsurface1
```

它们不进入普通 debug/release 包，也不改变 `docs/STATUS_ZH.md` 的 E7 不支持公告。构建日志、
CODA 原始事件和逐次观察结果放在 `.tmp/e7-coda/` 或本目录；设备事实最终只汇总到
`docs/reference/DEVICE_TEST_MATRIX.md`。
