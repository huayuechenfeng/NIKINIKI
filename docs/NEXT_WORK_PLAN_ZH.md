# NIKINIKI 1.0 发布后工作计划

> 更新日期：2026-08-29  
> 开发基线：1.0.0 正式 Release，原生 MMF + 本机 FFmpeg 回退 + 原生横屏 + 独立不透明 RGB565 视频表面 + 上层单 ARGB UI/弹幕窗
> 当前工作：正式 SIS 已完成有效签名；继续原版 Symbian³ / Anna 公测、50 次 Release 重入、直播与外部播放器回退，不动已冻结解码器，也不拆分应用包。
> 详细证据：`docs/PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md`

> 1.0 冻结规则：不再探索 BCM2727/BCM2763 硬解边界、SPS ref/DPB 伪装、DSA/post-processor 直显或受限 UI 变体。唯一保留的是已实现的只读 Broadcom header preflight：它不初始化或显示 DevVideo，只以真实 SPS/PPS + 首个同步 AU 的 `GetHeaderInformationL()` 结果选择 MMF 或本机软解。详见 `docs/PLAYER_1.0_DECODING_POLICY_ZH.md`。相关后续方向只记录在 `docs/POST_1.0_BCM2763_HWDEVICE_RESEARCH_ZH.md`、`docs/POST_1.0_H264_REFERENCE_DPB_RESEARCH_ZH.md` 与 `docs/POST_1.0_DEVVIDEO_DISPLAY_RESEARCH_ZH.md`，1.0 前不执行。

> 软解路线决定：PPSSPP-FFmpeg/libavcodec 已是当前唯一内置候选。使用 GCCE 4.4.1 源码重编的 H.264-only 库；当前版面向 ARM1176JZF-S 启用 ARMv6/VFP（禁用 NEON），并使用可满足 Symbian 4 MiB 代码/只读段边界的 `-Os`。旧 PPSSPP 预编译库仍禁止直接链接或删除 EABI attribute。详见 `docs/PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md`。

## 1. 接手前必须确认的事实

- 0.7 的历史方案通过保持 360×640 并旋转 MMF `RWindow` 解决首次横屏无画面；0.9 当前源码已改为设备验证过的 native landscape；
- 视频画面、声音和弹幕覆盖已在 Nokia 603 首次播放中同时成功；
- 当前 MMF、CPU RGB565、弹幕、控制栏和输入都直接使用真实 640×360 坐标；旧离屏 ARGB 及 1:1 的 90° 像素旋转已删除；
- `-12017` 表示 partial playback；只有视频轨仍可用时才能直接继续。若 `video=false/audio=true`，必须进入编码兼容回退；
- 旧 one-shot 会话、`deleteLater()` 和旧会话析构已经由日志证明执行成功，但不能避免下一次进入崩溃；
- 尽管旧会话已经销毁，旧包第二次进入仍在 `PLAYER_REBUILD_BEGIN` 之前访问 `0x140` 并 data abort；
- Release 与 Debug 从手机独立启动都已复现，所以 CODA 和 UDEB-only 均已排除；
- `overlayreuse1` 真机仍在第二次播放卡死退出，说明仅持久化 ARGB 覆盖窗不够；
- `surfacepersist1` 让播放器 controller、native video host、MMF facade/observer、`CVideoPlayerUtility2` 和 ARGB overlay 全部跨播放复用；用户已确认它可以退出后再次播放；
- 工作样本 `BV1oyhM6AETw` 与故障样本 `BV1Uy8x6AETG` 都是 640×360 H.264 High/yuv420p；故障流使用 7 refs、4 reorder 和 weighted B-frame，工作流为 4/3/无 weighted；
- Q6 是文档保留的 HTML5 MP4 编号，但 2026-08-28 对六个正反样本、早期 AV、旧 PGC 样例及 `BV15EhG6qEAg` 四个分 P 的实测均被服务端提升为同一 Q16 对象。Q6 链路已按用户要求撤回；现在不多发清晰度请求，安全流留在 MMF，已知硬解风险流直接进入本机 FFmpeg；
- 新增 3 失败/3 成功真机矩阵完全复现两套 SPS 模板：失败均为 7 refs/4 reorder/DPB 7/weighted 1,2，成功均为 4/3/4/0,0；Q64 与 Q16 分组不变；
- 当前工程保留 `ffmpegsoft1` 纯 C 历史后端；原 `ffmpegsoft2` 已合并为 0.9 普通构建默认。它不启用失败的 late-drop；只有额外加入 `CONFIG+=ffmpeglatedrop1` 才能重现实验。Qt Mobility 仍会让兼容流使用 MMF，软解只接管已知风险模板。
- 用户明确要求视频兼容完全在手机本机实现，不采用桥接或远端转码；
- 正式发布只维护一个以 `Symbian3Qt474` 构建的应用 SIS。唯一的 Belle-only `cookiemanager.dll` 依赖已经移除，Debug/Release 均以 `sbs errors: 0` 完成；原版 Symbian³ / Anna 通过离线补齐 Qt 4.7.4、Qt Mobility 1.2.x，不另编应用；
- Belle SDK 提供 `CMMFDevVideoPlay`/`devvideo.lib`，可绕过 `CVideoPlayerUtility2` controller 直接探测和调用 AVC decoder；
- BCM `0x10204C21` 对正常 High@5.1/4-ref header 返回 0 和正确 640×360，对故障 High@3.0/7-ref/DPB7/weighted header 稳定返回 -5；输入契约和 MMF 资源占用已由控制组排除；
- ARM H.264 decoder `0x102073EF` 对同一故障 header 返回 0，但尺寸为 0×0，只能记为“header 接受”，不能记为“可播放”；
- `armsoftprobe1` 后续真机日志证明 `ConfigureDecoderL()=-5`，无法 Initialize/出帧；系统 ARM 路线已废止，不再编译或验证；
- `video/avc` 的 `-1/0` 是 MIME 不匹配，固件登记为 `video/h264`；Broadcom decoder 自身不支持 direct display，需继续枚举 post-processor 或采用内存 YUV→GLES。
- post-processor 真机结果已经排除直接显示正式路线：它们只能 `ERotateNone`，也没有可用缩放；正式硬解输出固定为内存 YUV→GLES；
- `headercontrol1` 已完成使命并退役；它会主动关闭 MMF，绝不能作为播放器基线。专用控制代码已删除，默认源码还用 `WILIWILI_ENABLE_UNSUPPORTED_BCM_DEVVIDEO` 显式门控旧 BCM 实验。

## 2. 第一优先级：保持回归基线并选择本机兼容后端

1. 已完成：普通构建让正常样本输出 `PROFILE_SKIP` 并持续 MMF 播放；实验构建对故障模板保留 MMF AAC，不再沿用会关闭整个媒体会话的旧诊断路径；
2. 已完成：`Mp4AvcProbeReader` 增加 `ctts` version 0/1 和 PTS，DevVideo 同时接收 DTS/PTS；
3. 已完成：系统 ARM 因 `ConfigureDecoderL()=-5` 触发止损；`armsoftprobe1` 构建变体已从当前入口移除；
4. 已完成：用 GCCE 4.4.1 从 PPSSPP-FFmpeg commit `b87f7c6d522d1edba77cfc4fac96ce48a236f806` 重编 H.264-only `libavcodec`/`libavutil`，通过符号、最小链接和应用 Debug 全量构建；
5. 已完成：`FfmpegH264Decoder` 在单独工作线程消费 Annex-B AU，使用 DTS/PTS，以 MMF AAC position 选择应显示帧；当前默认由 CPU 转 RGB565，再交给持久、不透明的原生子窗口；唯一透明 ARGB 顶层只绘制弹幕和控件并保持在视频上方；
6. 已完成：`ffmpegsoft1` 真机兼容性通过——原有声无画视频已出现软件解码画面；性能失败——用户观察约 2–3 fps；
7. 已完成三轮性能定位：初版 300 帧/57,479 ms；LUT2X2 版 300 帧/25,014 ms（约 12 fps），转换从 156 降至约 42 ms/帧；late-drop 控制组第一段显示 275、丢转换 491、47,701 ms（约 5.8 可见 fps），第二段显示 125、丢 153、17,529 ms（约 7.1 可见 fps），媒体推进均只有约 0.78×，该策略已止损；
8. 已完成 H.264 ARM 路径审计：当前 FFmpeg 的 qpel/chroma/pred/weighted/deblock/IDCT ARM 快路径均要求 NEON，CABAC 快路径要求 ARMv6T2；ARM1176 上除 start-code 扫描外仍主要运行 generic C；
9. 已完成输出路线止损：三平面 GLES2 真机过慢并退役；当前紧凑 Y/U/V 排队、stale/drop 后只转换选中帧为 RGB565，常态跳过非参考帧 loop filter；
10. 已定位并修复首轮 0.9 Debug 的 Range 路由失效：原日志在 MMF 同时打开 CDN 时以 `DEVVIDEO_RANGE_TIMEOUT 1 0 0 20 20` 结束，故 GLES 仅初始化、FFmpeg 从未启动。当前源码先完成 MP4 header/风险流首批 AU 的 Qt Network 预取，再启动 MMF AAC；安全/失败路径均明确回到 MMF。该改动已通过 Debug 编译，待 Nokia 603 验证；
11. 本机回退只能发生一次，失败时显示明确“当前视频编码不兼容”，不能循环 CDN/清晰度/后端；
12. 内置软解仍不支持或性能不足时，提供明确的外部播放器确认入口，优先安全地交付本地临时文件或不含 Cookie 的短期直链；
13. 不记录 Cookie 或完整签名媒体 URL，不把媒体或解码请求发送到任何桥接/转码服务。

正式路径已经加入轻量 MP4 `moov/avcC` Range 预读和 SPS/PPS 解析，并以 7/4/7/weighted 模板选择本机软解/外部回退路线。后续不要再用 Level、分辨率或 `VideoEnabledL()==true` 作为“必定有画面”的充分条件。

## 3. 验收顺序

1. 已完成：恢复当前有效签名材料，对 `Symbian3Qt474` Release 产物剥离 SDK 旧签名并重签为 1.0.0 正式 SIS；
2. 在 Belle 上完成当前包的发布回归；用户已确认 Nokia 603 当前测试没有异常，先前黑屏/闪退是错包，禁止计入回归结果；
3. 公测前为原版 Symbian³ 取得并核对可再分发的完整 Qt 4.7.4 / Qt Mobility 1.2.x 离线包；当前 SDK 只有 stub 和已失去服务端保障的 Smart Installer，不能作运行库。Anna 已有本机离线候选；
4. 公测用户在原版 Symbian³、Anna 安装与 Belle 完全相同的应用 SIS，并回传系统版本、运行库版本和结果；
5. 各系统回归冷启动、首页 HTTPS/图片和二维码登录；`WW:COOKIE_PARTS` 应捕获非零字段，`WW:LOGIN_COOKIE_SUMMARY` 必须为 `true true true`，重启后会话仍有效；
6. `BV1oyhM6AETw`：必须保持直连 MMF，画面、声音、弹幕和清晰 UI 不回归；
7. `BV1Uy8x6AETG`：必须在命中风险 SPS 后直接进入本机 FFmpeg/CPU-RGB565 软解并得到画面，不能多发 Q6 请求、循环或停留在 AUDIO；
8. 故障样本日志必须出现 `PLAYER_SOURCE_DEFER_MMF`、`DEVVIDEO_MP4_AVC ... true`、`PLAYER_SOURCE_DEFER_MMF_DONE soft-prefetch-ready`、`FFMPEG_SOFT_READY ... RGB565_LUT2X2`、`SOFT_SURFACE_ACTIVE` 和 `SOFT_SURFACE_FIRST_PAINT`；不应出现 `PLAYER_240P_HARDWARE_FALLBACK`/`PLAYBACK_240P_RESPONSE` 或 soft `GLES_YUV_FIRST_FRAME`、持续 GLES upload；
9. 每次横屏应出现 `PLAYER_NATIVE_PORTRAIT_CHROME_READY`、`PLAYER_NATIVE_LANDSCAPE_640X360_READY`、`PLAYER_NATIVE_LANDSCAPE_VISIBLE`；退出应出现 `PLAYER_NATIVE_PORTRAIT_FULLSCREEN_READY`，且不出现 `PLAYER_NATIVE_ORIENTATION_TIMEOUT/FAILED`；
10. 两条视频交替打开、退出、重入至少 10 次，并验证暂停、seek、音量、弹幕开关、控制栏与回退源时钟一致；
11. 最终 Release 50 次循环，无崩溃、残留透明窗、黑屏、后台声音或持续内存增长。

## 4. P1 工作

### 媒体兼容矩阵

- 360P、480P、720P 渐进 MP4；
- H.264 profile/level、AAC 轨道、分辨率和码率；
- `NATIVE_MMF_TRACKS` 中完整播放与 partial playback；
- HTTP/HTTPS、CDN 备用地址和本地下载兜底；
- 无视频轨时的单步 DevVideo/软解回退，不能循环回退。

### P1：播放器交互与覆盖层耐久

- 弹幕密集片段连续十分钟；
- 控制栏反复隐藏/显示；
- 暂停、拖动、音量、倍速、画质切换和弹幕开关；
- 前后台切换、锁屏恢复和异常网络；
- 内存峰值和退出后的窗口/任务残留。

### P1：客户端功能回归

- WBI 视频/用户搜索相关性和输入法焦点；
- 二维码登录、Cookie 持久化与重启后的资料页；
- 评论根列表、楼中楼和分页；
- 手机图标冷启动、应用内退出和异常退出恢复。

### P2：直播

普通视频达到稳定门槛后，再处理直播证书、HLS/FLV 容器、画质切换和直播弹幕。不要用直播问题干扰当前普通视频覆盖层 P0。

## 5. 不要重复的方向

- 不恢复旧的 soft-only native-landscape 归档实现，也不恢复 `AA_S60DontConstructApplicationPanes`、过早隐藏主 QGL 或在方向切换期的 `resizeEvent()` 中修改窗口树；当前只使用已经通过 20 轮的 panes + `workAreaResized()` + dynamic fullscreen 主线状态机；
- 不恢复 `QMediaPlayer + QVideoWidget` 作为正式 Symbian 后端；
- 不把 raster 播放器/弹幕控件嵌进主 QGLWidget 子树；
- 不用大量透明顶层窗口分别绘制弹幕；
- 不把所有 `-12017` 都当成致命错误，也不忽略其中 `video=false/audio=true` 的真实不兼容；
- 不再尝试把故障视频统一降到 360P；它的 Q16 就是不兼容样本；
- 不把 Qt Mobility 当作独立软件解码器；当前它仍选择系统 MMF；
- 不以切 CDN、HTTP/HTTPS、本地下载或 DASH 代替码流兼容处理；已验证这些变体保持同一 SPS 结构；
- 不继续围绕 MMF 延时、CDN 或 H.264 修改来解释当前第二次进入崩溃；
- 不因为 C++ 对象地址相同就认定旧会话未销毁，必须结合 `DESTROY_READY` 和原生控件地址判断；
- 不把 Qt Creator/CODA 成功或失败直接等同于完整 SIS 独立运行结果。

## 6. 当前安装包

| 配置 | 文件 | SHA-256 |
|---|---|---|
| Release 1.0.0（正式发布包） | `symbian/out/releases/v1.0.0/NIKINIKI_1.0.0_release.sis` | `49748F5B4061F1F3D776B9FA7275B0092F2BA75F9F46872AE2FF88BB2B641095` |
| Release surfacepersist1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_surfacepersist1_currentcert.sis` | `4BEBC2E8151FD688E4A7252E3DC86C89E3C562D5AB13DD4D8FFF5CF3DDD50190` |
| Debug surfacepersist1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_surfacepersist1_currentcert.sis` | `78667C76978D27BCF331AE41C6A11A7B8921DCD4ED00854A2960515F2BB6DB0E` |
| Release devvideoprobe1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_devvideoprobe1_currentcert.sis` | `34D83F9F4D8AE4CAF8AA211A0EE9A4A1FE5886F934A822D77B9D81DC0A1450C0` |
| Debug devvideoprobe1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_devvideoprobe1_currentcert.sis` | `27360039D7DD1CD32601F8477FF4FEF0E61CC886260AB40646DB7149F793C753` |
| Debug ffmpegsoft1（历史纯 C 性能基线） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_ffmpegsoft1_currentcert.sis` | `5FCA1231349D67088C93F0A2D7AEF5D6A90E2DB0E6ECBCD382EC8ED40B7E609A` |
| Debug armsoftprobe1（历史失败候选） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_armsoftprobe1_currentcert.sis` | `17730DC83BAD4FDC66AF35FE721AE6A2C6D56D772EC0B1CEDCF7B370F2E0CF3E` |
| Release codeccompat1（历史候选） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_codeccompat1_currentcert.sis` | `08C9F75DCD4025E2680B41B5A4B908BFF5439E06D46218C88A32047AA048B2D0` |
| Debug codeccompat1（历史日志包） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_codeccompat1_currentcert.sis` | `61F87FBE6B3A2B9D014CBC569C1F798733655E54C010577EB08EDE667D5EC9FA` |

`surfacepersist1` 两份包是已确认可以重复播放的回归基线。`ffmpegsoft1` 已证明 libavcodec 能覆盖原故障码流，但约 2–3 fps，仅保留作纯 C 性能基线；`ffmpegsoft2` + CPU RGB565 已进入 1.0 主线，旧 GLES-YUV 包仅作历史性能证据。故障流 READY 应为 `ARM11_GENERIC_H264_RGB565_LUT2X2`；不要加入 `ffmpeglatedrop1`。`armsoftprobe1`、`codeccompat1` 与 `headercontrol1` 都是历史诊断证据。50 次 Release 压力测试继续作为发布后覆盖项。

历史低帧率审计结论：CPU 与 GLES 均曾约 3 fps，40 ms overlay timer 本身不是 200–330 ms 固定节流；共同候选是 UI/WSERV repaint 与同步 `CVideoPlayerUtility2::PositionL()`。RGB565 路径随后改为先排队紧凑 YUV、由 `takeOutput()` 先做 stale/drop，再只转换选中的帧；单周期 position 去重也已完成。最新的 surface/500 ms 时钟方案见本文末尾，本轮仍不调整 decoder、network 或 catch-up 阈值。

注意：日志 2734939 混合了旧 RGB565/GLES 会话和末尾的新延迟转换会话。旧会话首帧为 `RGB565`、`repackCopyMs=0`；末尾会话已出现 `RGB565_DEFERRED` 和非零 `repackCopyMs`，说明延迟转换已经生效。末尾会话仍只有约 3–4 fps 的 consumer rate，下一轮只测量/缓存单周期 `PositionL()` 及 `paintEvent()`，不再改 decoder、网络或 RGB565 LUT。

日志 2736142 已完成时钟缓存后的 A/B：两段 `RGB565_DEFERRED` 会话仍分别收敛到约 3.3/3.1 fps，说明 PositionL 去重不是最大节流。下一轮只测量 timer 到达间隔、`paintEvent()`、RGB565→ARGB 绘制和 WSERV repaint 边界；catch-up、decoder、网络和 queue 暂停修改。

日志 2740509 完成了旧 portrait+virtual 路线的边界定位：CPU RGB565 会话解码约 19 fps、选中帧转换约 68 ms/frame，overlay `paintEvent()` 约 333–344 ms/次，consumer 约 3.2 fps；单次 `PositionL()` 约 3.5–5.1 ms。日志 2738898 进一步把约 224 ms/frame 定位到整帧 90° rotation。当前原生横屏源码已删除该 pass；decoder、catch-up、network、queue、RGB565 LUT 和 GLES 保持冻结，下一步以主线真机 `SOFT_STATS` 判断实际收益。

分段计时继续由 `User::FastCounter()` 累计 `overlayClearMs`、`overlayVideoDrawMs`、`overlayIntermediateMs`、`overlayRotateMs`、`overlayDanmakuMs`、`overlayControlsMs`、`overlayPainterEndMs` 与 `overlayOtherMs`。原生横屏下 `overlayIntermediateMs`/`overlayRotateMs` 应为 0；若非 0 或画面方向错误，视为合入回归。

日志 2738898 已确认 fixed 90° software rotation 是第一瓶颈（约 224 ms/frame）。最终 app-shell 探针随后证明可靠 native landscape；当前源码已彻底删除 ARGB 旋转 buffer/copy/二次 draw，并把 MMF rotation、UI 和输入都切到原生坐标。真实软解 fps 尚未测量，不能把 224 ms 理论节省直接写成已取得的帧率。

当前源码已把上述测量合并到既有低频 `SOFT_STATS`：检查 `overlayPaintMs`、`overlayPositionMs`、`overlayTimerMaxGapMs` 及其对应计数即可，不增加逐帧日志。

日志 `2746319` 已完成原生横屏主线的四视频集成验证：前两条走 FFmpeg soft，后两条走 MMF `PROFILE_SKIP`，同时覆盖横向和竖向编码内容；用户确认除软解卡顿外，画面、方向、比例、弹幕/控件、返回和最终退出均正常。两条 soft 会话的 `overlayIntermediateMs/overlayRotateMs` 都为 0，证明旧 90° 整帧 pass 已彻底移除；但 640×360 soft 后段仍约 155–179 ms/帧花在 `overlayVideoDrawMs`，带弹幕的 overlay paint 约 183–241 ms/帧，竖向编码 soft 的 video draw 约 102 ms/帧。

当前源码针对这一边界只做两项显示/时钟优化：新增持久、不透明、`WA_NativeWindow` 的 RGB565 视频子窗口，透明 ARGB overlay 仅画弹幕和控件并始终 `raise()` 到视频之上；soft audio clock 改为每 500 ms 调用一次 `PositionL()`，中间按实际倍速外推，暂停、seek、倍速与会话切换都会失效缓存。解码器、loop filter、catch-up、queue、Range、RGB565 LUT 和编码路由均未改动。下一次真机以 `softSurfacePresented>0`、`overlayVideoDrawMs=0`、`overlayPositionCacheHits>0`、UI/弹幕可见且可点击为通过条件。
