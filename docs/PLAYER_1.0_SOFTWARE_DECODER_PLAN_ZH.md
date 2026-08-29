# 1.0 本机软件解码开发计划

> 决策日期：2026-08-26，最新更新：2026-08-27  
> 状态：系统 ARM 路线已真机止损；`ffmpegsoft1` 已在 Nokia 603 为原有声无画视频输出真实画面。`ffmpegsoft2` 的 CPU RGB565 输出达到约 11.4–12.0 fps，180 ms late-drop 控制组失败；GLES2 三平面 YUV420 候选已实测约 216 ms/帧上传、约 321 ms/帧提交并退出主线，当前 soft renderer 恢复 CPU RGB565。  
> 结论：当前唯一内置软解候选是用 GCCE 4.4.1 从 PPSSPP-FFmpeg 源码重编的 libavcodec；不使用旧预编译库，不再继续系统 ARM/DevVideo 路线

## 1. 结论

系统 ARM H.264 decoder `0x102073EF` 曾因能复用 DevVideo 管线而作为第一候选。真机后续结果已给出明确止损依据：它虽对故障 SPS/PPS/IDR 完成 `SelectDecoderL`、`SetInputFormatL` 和 `GetHeaderInformationL()==0`，但 header 尺寸仍为 0×0，随后 `ConfigureDecoderL()` 稳定返回 `KErrNotSupported (-5)`，无法进入 Initialize 和真实首帧。该路线已废止，不再作为当前实现或测试入口。

PPSSPP-FFmpeg 的 H.264 解码能力预计更完整，也更不依赖具体固件，但它的现成 Symbian 库不能直接链接到本工程。当前 QtSDK 的 GCCE 4.4.1 对预编译 `h264.o` 的实际链接结果是：

```text
Unknown mandatory EABI object attribute 44
failed to merge target specific data
```

PPSSPP-FFmpeg 的 `symbian-build.sh` 明确使用 Sacha GCC 4.8.3、`armv6zk/arm1176jzf-s`、softfp/VFP 和静态库；仓库版本为 FFmpeg 3.0.2。旧预编译包也不是 GCCE 4.4.1 产物。不能靠重命名、删除 object attribute 或混用编译器规避这个 ABI 问题；安全路线是用 GCCE 4.4.1 从源码重编裁剪版。

当前执行顺序固定为：

1. 经 Broadcom `GetHeaderInformationL` preflight 接受的码流继续使用 MMF 硬解；
2. 被同一 preflight 明确拒绝的码流转 libavcodec H.264 工作线程，AAC 与主时钟仍由 MMF 提供；
3. `ffmpegsoft1` 已通过首帧/画面兼容性验证但约 2–3 fps；`ffmpegsoft2` 已验证快速 RGB565 有效，late-drop 无效；GLES-YUV 输出已因真机上传/提交耗时退出主线，当前默认恢复 CPU RGB565，不再继续扩展解码架构；
4. 内置 FFmpeg 仍不支持或性能不足时，执行已批准的外部播放器回退。

## 2. 两条路线比较

| 项目 | 系统 ARM H.264 `0x102073EF` | PPSSPP-FFmpeg |
|---|---|---|
| 当前真机证据 | header 接受，但尺寸 0×0；`ConfigureDecoderL()=-5`，路线已废止 | 已在 Nokia 603 解出原故障视频画面；纯 C 基线仅约 2–3 fps，兼容性通过、性能失败 |
| 工具链 | 现有 Belle SDK、GCCE、`devvideo.lib` 可直接编译 | 旧 `.lib` 与 GCCE 4.4.1 ABI 不兼容；已用 GCCE 4.4.1 重编精简库 |
| 现有代码复用 | 很高：DevVideo observer、buffer feed、YUV/RGB 转换、时钟和 overlay 路径都已存在 | 中等：MP4 sample/Annex-B、时钟和 overlay 可复用；decoder API、构建和内存管理需新写 |
| 包体积 | 基本不增加，decoder 在固件中 | 增加静态 H.264 decoder 与最小 `avutil`；不能照搬完整约 18 MB `avcodec.lib` |
| 设备覆盖 | 依赖目标固件是否存在该 UID/能力 | 编译成功后更可控，也可覆盖没有 Nokia ARM plugin 的设备 |
| 性能 | 无首帧，不再评估 | 纯 C 真机约 2–3 fps；CPU RGB565 约 11.4–12.0 fps；GLES-YUV 真机约 216 ms/帧上传、约 321 ms/帧提交，已退役 |
| 维护与合规 | 小；主要是运行时能力检测与生命周期 | 大；还包含 FFmpeg 安全更新、LGPL/GPL 配置、静态链接交付义务 |
| 主要风险 | header 返回 0×0；可能 Configure/Initialize 失败或 360P 帧率不足 | GCCE 兼容补丁、汇编、libc/POSIX 差异、包体积和性能都在首帧前形成工作量 |
| 相对工作量 | `1×`，先做最小可行实验 | 约 `3–5×`，且先有一个独立工具链阶段；这是相对量级，不是日历工期承诺 |

系统路线虽失败，但其中 MP4 `ctts`/PTS、分批 Range、MMF 主时钟和软件帧合成已直接被 FFmpeg 路线复用，因此试验并未造成架构反复。

## 3. 两条路线共同需要补齐的基础

### 3.1 正确的 DTS 与 PTS

旧版 `Mp4AvcProbeReader` 只解析 `stts` 并输出 DTS，没有解析 `ctts`。故障模板包含 4 帧重排，软件解码不能把解码顺序直接当显示顺序。`armsoftprobe1` 已补齐下面这些公共基础，但仍需用真机输出验证时间戳行为。

已完成：

- 解析 `ctts` version 0 的无符号 composition offset；
- 解析 `ctts` version 1 的有符号 composition offset；
- `Sample` 与 `AccessUnit` 同时保存 `dts` 和 `pts`；
- 对 DevVideo 输入同时设置 `EDecodingTimestamp`、`EPresentationTimestamp`；
- 对 FFmpeg packet 同时传入 DTS、PTS，并以输出 frame PTS 对齐 MMF position；
- seek 从目标时间之前最近的 IDR 开始，目标点之前的输出只做 preroll，不显示。

### 3.2 解码器边界

在系统 decoder 通过首帧实验后再抽象公共接口，避免首帧前过度重构。接口保持 Qt 4/C++98 可编译，只表达：

- 打开 H.264 decoder 和输出格式；
- 提交带 DTS/PTS 的一个完整 access unit；
- 异步交付 YUV420/RGB 帧及其 PTS；
- flush、seek reset、pause/resume 和 close；
- 返回明确的“不支持、资源不足、损坏数据、性能降级”状态。

系统实现可命名为 `SymbianArmH264Decoder`，未来 FFmpeg 实现为 `FfmpegH264Decoder`。播放器只负责策略和时钟，不直接依赖某个 decoder 的内部对象。

### 3.3 有界内存与统一画面出口

- 压缩 sample 继续按 Range 小批量读取，提交后及时丢弃；
- 最多保留 2 张待显示软件帧，积压时优先丢弃已经晚于音频时钟的非关键帧；
- 视频帧进入持久、不透明的原生 RGB565 640×360 子窗口；弹幕和控制由独立的单一透明 ARGB 顶层在其上方绘制；
- 保留现有 1:1 旋转，软解不能重新引入模糊横屏 UI；
- GLES-YUV/ping-pong 已完成真机否定；当前从 planar YUV420/GLES2 切回已有 CPU RGB565 输出，避免三平面 texture upload，不引入 swscale。

## 4. 历史第一阶段：系统 ARM decoder（已止损）

该阶段使用显式的 `WILIWILI_ENABLE_ARM_SOFT_DECODER_PROBE` 构建开关，默认关闭，不进入日常 MMF 构建。只对已知风险模板触发，正常样本必须继续输出 `PROFILE_SKIP` 并由 MMF 连续播放。

### 4.1 实验 A：隔离首帧

复用现有 DevVideo 代码，只把 decoder 选择参数化为 `0x102073EF`，并按以下顺序逐步记录错误：

1. `SelectDecoderL`；
2. `SetInputFormatL(video/h264, EDuCodedPicture, EDuElementaryStream, ETrue)`；
3. `GetHeaderInformationL`；
4. `ConfigureDecoderL`；
5. `GetOutputFormatListL` / `SetOutputFormatL`；
6. `SetVideoDestScreenL(EFalse)`；
7. `Initialize` / `MdvpoInitComplete`；
8. 提交一个 IDR 起始、约 2 秒且不超过 60 个 access unit 的批次；
9. `MdvpoNewPictures`、`NextPictureL`、像素格式、尺寸、首帧耗时和 picture counters。

header 的 0×0 只能记为诊断信息，不能手工伪造 `TVideoPictureHeader` 字段。实际 coded size 640×360 可以作为应用输出缓冲和日志的预期值，但 Configure 必须遵守插件返回的结构。

### 4.2 实验 B：与 MMF AAC 共存

只有实验 A 得到真实视频帧才执行：

- 保留故障流的 MMF audio-only controller，不调用旧 BCM 实验中的 `closeMedia()`；
- MMF `PositionL()` 是唯一主时钟；
- `CSystemClockSource` 初始化到 MMF position，偏差超过 150 ms 才校正；
- 使用 PTS 显示/丢弃帧，不能继续只用 DTS；
- 连续运行 30 秒，记录 decoded/displayed/skipped、首帧时间、有效帧率、最大音画偏差、Range 队列和 heap 变化；
- 验证 UI/弹幕仍能响应，不能出现“有画面但主线程被软解占满”。

### 4.3 止损门槛

满足以下全部条件才进入正式集成：

- Configure、Initialize 和第一张非空 640×360 图片均成功；
- 30 秒无 fatal callback、panic、持续内存增长或音频中断；
- 25 fps 输入至少能长期达到约 20 fps，或达到源帧率的 80%，且可通过丢帧维持音画同步；
- 稳态音画偏差可控制在约 150 ms，控制栏和弹幕操作没有明显冻结；
- pause/resume 后继续出帧，关闭后可以再次创建或重置 decoder。

以下任一情况出现就停止在系统插件上继续消耗时间，转 FFmpeg：

- Configure/Initialize/输出格式不可用；
- 有界 2 秒/60 AU 输入仍没有首帧；
- 持续帧率低于约 12–15 fps，或导致 UI、音频明显卡顿；
- 重复进入出现无法可靠清理的 panic、泄漏或资源占用。

12–20 fps 属于一次性的优化评估区间，只允许比较输出格式、减少颜色转换和主动丢帧；不得重新转向 BCM 私有接口研究。

### 4.4 2026-08-26 实现状态

`armsoftprobe1` 已按本节方案落地：

- `Mp4AvcProbeReader` 已解析 `ctts` version 0/1，`Sample`/`AccessUnit` 同时携带 DTS 和 PTS，按 PTS 选择 seek 目标之前的同步样本；
- DevVideo 输入同时设置 `EDecodingTimestamp` 与 `EPresentationTimestamp`；
- 构建开关 `CONFIG+=armsoftprobe1` 选择系统 ARM decoder UID `0x102073EF`，默认普通构建不定义该宏；
- 风险模板以约 2 秒一批持续 Range 取样并送入 decoder，正常模板仍 `PROFILE_SKIP`、完全走 MMF；
- ARM 路径保留现有 MMF audio-only 会话，不调用旧 BCM 实验的 `closeMedia()`；输出帧继续进入现有视频/弹幕/控制合成层；
- 日志新增 `WW:DEVVIDEO_MP4_CTTS`、`WW:ARM_SOFT_MMF_AUDIO_RETAINED`、首帧耗时和每 25 帧一次的 `WW:ARM_SOFT_PROGRESS`；
- `armsoftprobe1` Debug 与不带实验宏的标准 Debug 均完成 GCCE ARMv5 全量构建，结果都是 `sbs errors: 0`；这只证明代码和默认门控可编译，不证明系统插件能出帧。

当前签名候选为 `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_armsoftprobe1_currentcert.sis`，大小 9,241,072 bytes，SHA-256 为 `17730DC83BAD4FDC66AF35FE721AE6A2C6D56D772EC0B1CEDCF7B370F2E0CF3E`。它是诊断候选，不是正式兼容版本。

### 4.5 真机最终结果

`armsoftprobe1` 在 Nokia 603 上对故障码流完成 header 解析后，`ConfigureDecoderL()` 稳定返回 `KErrNotSupported (-5)`，因此没有 Initialize、首帧或性能数据。用户后续多视频测试仍是原有故障码流只有声音。`armsoftprobe1` 与其构建变体已从当前项目入口移除，仅保留日志和安装包作历史证据。

## 5. 历史第二阶段：系统路线正式集成（取消）

以下是早期计划；因 `ConfigureDecoderL()=-5` 已取消，不得再作为当前工单：

1. `video_player_widget.cpp` 只在已知风险模板或 MMF 明确 audio-only 时选择一次软解；
2. `video_playback_backend.cpp` 保留 MMF AAC，不再沿用 BCM 诊断代码关闭整个 MMF media 的行为；
3. decoder 输出的帧进入当前 `devVideoFrame`/overlay 合成位置，先画视频、再画弹幕、最后画控制；
4. pause 同时暂停 MMF、decoder 和软件时钟；
5. seek 先移动 MMF，再 flush decoder，从前一 IDR 重新 Range 取样并 preroll；
6. 返回页面时取消 Range reply、停止输入、归还所有 picture，并验证第二次进入；
7. decoder 缺失、初始化失败或性能守卫触发时，只允许一次转入外部播放器提示，不循环重试清晰度/CDN/后端。

集成包先完成故障样本 30 秒、正常样本不回归、两类视频交替 10 次；进入 1.0 候选后再执行 50 次 Release 循环和长时间温度/内存观察。

## 6. 当前阶段：PPSSPP-FFmpeg/libavcodec 路线

系统路线已触发止损条件，本阶段已启动。PPSSPP 的价值是提供 Symbian 配置、ARMv6 参数和历史补丁，不是直接提供可链接二进制。

### 6.1 构建证明

1. 固定当前研究版本和补丁清单，不直接切换整个 Qt 应用到 GCC 4.8.3；
2. 用 QtSDK 自带 GCCE 4.4.1 构建独立的 H.264-only 静态库；
3. 从 `--disable-everything` 开始，只启用 H.264 decoder 和最小 `avutil`；现有代码已经拆 MP4 和生成 Annex-B，因此第一版不需要 `avformat`、网络、音频、`swresample` 或 `swscale`；
4. 第一轮关闭汇编和线程，只验证 C 路径能编译、链接并在手机上解出测试帧；
5. 通过后再逐项恢复 ARMv6/VFP/DSP 汇编，以每次可回滚的方式解决 GCCE assembler 差异；
6. 不能通过删除 EABI attribute 或直接混入 GCC 4.8 object 来制造“能链接”的假成功。

上述构建证明已完成：

- 源码固定为 `hrydgard/ppsspp-ffmpeg` commit `b87f7c6d522d1edba77cfc4fac96ce48a236f806`（FFmpeg 3.0.2）；
- `Build-Gcce-H264.ps1` 用 GCCE 4.4.1 构建 ARM1176JZF-S/ARMv6K/softfp/VFPv2 的 H.264-only `libavcodec` + 最小 `libavutil`；首版关闭汇编，第二版恢复 ARM/ARMv5TE/ARMv6 汇编与 inline asm，明确禁用 Nokia 603 不支持的 NEON；
- 库已通过必需符号检查和最小 GCCE 链接探针；应用 Debug 全量链接 `sbs errors: 0`；
- `CONFIG+=ffmpegsoft1` 保留纯 C 历史基线；0.9 主线默认启用 `ffmpegsoft2` 的 CPU RGB565 实现，普通 qmake 会自动带入 FFmpeg 宏、源文件和静态库，不需要额外 CONFIG 参数；
- `FfmpegH264Decoder` 在有界 `QThread` 中解码完整 Annex-B access unit，接收 DTS/PTS，默认在线程内输出紧凑 YUV420 平面，最多缓存 6 帧；consumer 选出要显示的帧后才复用 `RGB565_LUT2X2` 转换，YUV420 三平面 GLES 只保留历史诊断路径；
- 现有 MMF 会话继续播 AAC 并提供主时钟；软解 RGB565 帧进入持久 opaque native surface，弹幕/控件仍由唯一 ARGB overlay 在其上方绘制和接管输入；
- `ffmpegsoft1` 已在 Nokia 603 为原故障视频输出画面，但用户观察只有约 2–3 fps，因此不能转为正式后端。

软解的横屏显示代价另见 `PLAYER_ORIENTATION_PROBLEM_AND_WORKAROUND_ZH.md`：主线暂时保持 portrait + virtual 90°，真实 native-landscape 实验因 Nokia 603 横屏闪退已归档。后续只保留两条低侵入方向——修复真实横屏，或在 YUV→RGB/最终写入阶段融合固定 90° 映射；不得把 native-landscape 实验重新混入当前 decoder、队列或硬解架构。

### 6.2 当前性能优化：`ffmpegsoft2`

- FFmpeg 静态库保留 `--enable-small/-Os`：全库 `-O2` 虽能编译，却让最终 EXE 的 `.rodata` 延伸到 `0x4A50BB`，与 GCCE 固定从 `0x400000` 开始的 `.data` 重叠；`-O3` 还会在 `libavutil/tea.c` 触发编译器 ICE。下一轮只能给 H.264 核心对象局部提高优化级别，并同时检查最终段边界；
- 目标保持 `arm1176jzf-s`、softfp/VFPv2，禁用 NEON。源码审计确认“库内存在 ARM 汇编”不等于“H.264 已为 ARM11 加速”：qpel、chroma、intra prediction、weighted/biweighted prediction、deblock 和 H.264 IDCT 的 ARM 快路径均只为 NEON 注册；CABAC inline 路径要求 ARMv6T2。ARM1176 两者都没有，现有 ARMv6 收益主要是 start-code 扫描；
- 640×360 YUV420→RGB565 常用路径移除逐像素除法，并让相邻两个像素复用一次 U/V 乘法；
- 启用 `AV_CODEC_FLAG2_FAST`，常态跳过非参考帧 deblocking；当视频 DTS 落后 MMF 音频超过 500 ms 时进入追帧，跳过非参考帧并关闭全部 deblocking，追回到 150 ms 内退出；
- 每 25 张输出记录 `WW:FFMPEG_SOFT_TIMING pictures wall_ms decode_ms convert_ms`，并用 `WW:FFMPEG_SOFT_CATCHUP` 记录追帧状态，下一次真机日志可以区分 H.264 核心与颜色转换成本。

首轮 `ffmpegsoft2` 真机数据已经定位瓶颈：300 张输出耗时 57,479 ms，即约 5.2 fps；其中 libavcodec 解码累计 9,583 ms（约 32 ms/张），YUV420→RGB565 累计 46,785 ms（约 156 ms/张、占墙钟约 81%）。Range、队列和等待只占很小比例。保持 640×360 后，`RGB565_LUT2X2` 第二轮把 300 帧降到 25,014 ms（约 12.0 fps），转换降到 12,555 ms（约 42 ms/张），相对首轮总速度 2.3 倍、转换速度 3.7 倍；600 帧时仍约 11.4 fps，说明性能进步真实但尚不足以实时。

第三轮 late-drop 控制实验已经完成并止损。第一段在 47,701 ms 内显示 275 张、跳过 491 次转换，约 5.8 可见 fps；第二段在 17,529 ms 内显示 125 张、跳过 153 次转换，约 7.1 可见 fps。两段媒体 PTS 都只以约 0.78× 实时推进，说明该策略减少了 RGB 工作，却没有解除 H.264 解码和显示总成本，还显著破坏观感。普通 `CONFIG+=ffmpegsoft2` 已关闭该逻辑，READY 改为 `ARM11_GENERIC_H264_RGB565_LUT2X2`；只有额外指定 `CONFIG+=ffmpeglatedrop1` 才会得到 `_LATEDROP` 诊断标识。

第四轮 GLES 输出优化已完成实现与真机构建，但已因约 216 ms/帧上传、约 321 ms/帧提交退出发布主线；当前源码恢复已有 CPU RGB565 `QImage` 输出，并把它移到持久 opaque native surface，overlay 只画弹幕/控制。常态 `skip_loop_filter=AVDISCARD_NONREF`，默认 READY 为 `ARM11_GENERIC_H264_RGB565_LUT2X2`。详细历史边界见 `PLAYER_0.9_GLES_YUV_OPTIMIZATION_ZH.md`。

### 6.2.1 2026-08-27 延迟转换会话的最新结论

日志 `2734939` 的末尾会话已经打印 `RGB565_DEFERRED`：decoder 只复制紧凑 YUV，consumer 先执行 `takeOutput()` 的 stale/drop，再转换被选中的帧。`pts=3400/10366 ms` 时，`pictures/outputDrops/queueDepth` 为 `90/68/6`、`210/168/6`，推算选中帧约 16→36，实际 consumer 仍只有约 3–4 fps。`convertMs` 由 1125 增至 2380，而 `repackCopyMs` 由 752 增至 1612，说明转换成本已从“每个 decoded picture”收缩到“实际选中 picture”；`queueMutexWaitMs=0`，输出锁不是主卡点。

CPU 的 `presented=0/uploaded=0` 在此路径不表示没有画面，这两个计数由 GLES presenter 维护。当前共同低速应优先归因于 UI/WSERV repaint、同步 `CVideoPlayerUtility2::PositionL()`（旧实现一次 timer 周期最多取帧/弹幕/控件三次）以及 RGB565→ARGB overlay 的潜在第二次格式转换。源码已加入单周期时钟缓存，下一次真机只需验证 consumer fps/AV lag 是否改善，并分段测量 `paintEvent`/`PositionL`；不再修改 decoder、catch-up 或 GLES。

日志 `2746319` 已补齐这一判断：原生横屏删除 90° pass 后，两条 soft 与两条 MMF 视频都能正确显示和返回，但 640×360 soft 的 `overlayVideoDrawMs` 后段仍约 155–179 ms/显示帧，竖向编码 soft 约 102 ms/帧；`PositionL()` 也出现约 14–39 ms/次的波动。当前源码因此将视频移到独立 opaque native surface，并让 ARGB overlay 只绘制 UI/弹幕；同时每 500 ms 校准一次 `PositionL()`、中间按倍速外推。新 telemetry 使用 `softSurfacePresented/softSurfacePaintMs/softSurfaceVideoDrawMs` 和 `overlayPositionCacheHits`，而 `overlayVideoDrawMs` 应保持 0。该改动不涉及 decoder、loop filter、queue、catch-up、Range 或编码路由，待 Nokia 603 验证。

### 6.3 UCPlayerEx 参考证据的正确边界

用户提供的 UCPlayerEx V3.0.0 patch2 报告证明主程序静态包含完整 FFmpeg/libavcodec/libavformat 痕迹、H.264 ARM qpel 符号、解码线程和 framebuffer 路径，这足以证明 2011 年商用 Symbian 播放器具备高度优化的本机软件引擎。用户还报告第二个测试视频在 UC 中可接近 30 fps，这说明 Nokia 603 不应仅凭本项目当前帧率被判定为“硬件性能不足”。

但定向导入表核验也显示，UC 同时使用 `CVideoPlayerUtility::NewL/OpenUrlL/OpenFileL/Prepare/Play/Stop`、显示窗口/旋转、`StartDirectScreenAccessL/StopDirectScreenAccessL`，并使用 `CDirectScreenBitmap::NewL()`。因此最严谨的架构结论是“MMF + 静态 FFmpeg + direct-bitmap 的混合播放器”，不是所有 H.264 一律纯软解。只有用完全相同的 7-ref/weighted 文件确认 UC 能出图，才能把接近 30 fps 当作这条故障码流的软件解码上限。

UC 二进制只用于行为和架构对照，不复制、反编译移植或再发布其中的专有代码。可实施顺序是：先尝试 H.264 核心对象局部 `-O2` 并守住 4 MiB 段边界；再寻找许可证清楚、与 ARMv6/ARM11 匹配的 qpel/颜色转换实现；最后才评估 direct-bitmap 显示，因为现有产品仍必须保证弹幕和控件覆盖。

已检查当前 PPSSPP-FFmpeg 3.0.2 以及官方 FFmpeg n0.5.15/n0.8 源码，未找到 UC 字符串中的非 NEON `put_h264_qpel8_arm` 实现；这些公开版本的 ARM H.264 qpel 仍是 NEON 路径。这一符号更像 UC 自有分支或第三方补丁，不能假设存在可直接 cherry-pick 的上游实现。

### 6.4 解码证明与集成

- 独立小程序先用故障样本的 SPS/PPS/IDR 解出一张 YUV420 帧并校验尺寸；
- 再解连续 10 秒，记录平均帧率、峰值 heap 和输出顺序；
- 同样通过公共 DTS/PTS、两帧队列、MMF 主时钟和 overlay frame sink 接入；
- 同步解码不能运行在 UI 线程，使用一个有界、可停止的工作线程；
- 不开启多线程 H.264，除非真机证明收益大于额外栈和同步成本；Nokia 603 的目标是稳定 360P，而不是追求 720P/1080P 软件解码。

### 6.5 发布前额外门槛

- 明确 FFmpeg 配置最终是 LGPL 还是包含 GPL 部件；
- 静态链接必须准备许可证、对应源码/补丁和满足重新链接要求的交付方式；
- 记录库版本和已知解码安全问题，媒体输入按不可信数据处理；
- 比较精简后的 SIS 增量、启动内存和 30 分钟温度；
- 只有 FFmpeg 路线真机明显优于系统路线，才允许在有系统 ARM plugin 的设备上取代它。

## 7. 建议的代码落点

| 文件/模块 | 计划改动 |
|---|---|
| `mp4_avc_probe_reader.h/.cpp` | 增加 `ctts`、PTS、seek preroll 所需信息 |
| `video_playback_backend.cpp` | 已接入 FFmpeg 工作线程，保留 MMF AAC/主时钟并交付软件帧 |
| `video_player_widget.cpp` | 保持 MMF 优先，仅对一次性兼容回退进行路由 |
| `symbian_arm_h264_decoder.*` | 取消；系统 ARM 路线已止损 |
| `ffmpeg_h264_decoder.*` | 已建立；有界输入/输出、PTS 同步，CPU RGB565 为默认 soft 输出，YUV420 三平面仅保留历史路径 |
| `video_player_widget.cpp` 的 soft native surface + ARGB overlay | 已建立；opaque RGB565 原生子窗口显示视频，唯一透明 ARGB 顶层显示弹幕/控件并接管输入；待真机验证 |
| `symbian/app/wiliwili_symbian.pro` | 0.9 普通构建默认启用 FFmpeg/CPU RGB565；`ffmpeglatedrop1` 仅诊断 |
| `symbian/third_party/ppsspp_ffmpeg/` | 可复现 GCCE 构建脚本、公开头文件、静态库和 LGPL 文件 |
| `docs/DEVICE_TEST_MATRIX.md` | 记录每个阶段的首帧、帧率、同步、内存和重复进入结果 |

## 8. 下一步唯一执行入口

GLES-YUV 真机验收已经完成并否定。原生横屏主线的四视频矩阵也已通过；下一步只验证 opaque RGB565 surface 与上层 ARGB UI/弹幕的层级、性能、同步和重复进入。不要再通过 late-drop 追求数字，也不恢复 `armsoftprobe1`、`headercontrol1` 或旧 BCM `devvideosample1`。故障流的 READY 应为 `ARM11_GENERIC_H264_RGB565_LUT2X2`，且 soft playback 不应调用 GLES YUV upload。

Qt Creator 操作：打开 `symbian/app/wiliwili_symbian.pro`，选择 Symbian Device/GCCE kit 和 Debug/Release，直接执行一次“运行 qmake”再构建项目。0.9 主线已默认包含 `ffmpegsoft2`；不要加入 `ffmpeglatedrop1`。切回历史 MMF-only 对照包需要回到旧版本源码，而不是在 0.9 qmake 中删除参数。

若局部 `-O2` 与一轮有许可证的 ARM11 热点优化仍不能稳定超过最低约 12–15 fps，就按既定止损门槛把该设备上的内置软解标为性能不足，进入外部播放器回退实现；不得复制 UC 专有实现，也不得重新开启 1.0 前的 BCM 硬解边界研究。
