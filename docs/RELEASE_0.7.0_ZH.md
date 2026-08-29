# wiliwili for Symbian³ 0.7.0 播放器正式集成基线

> 2026-08-26 更正：本页下列 `codeccompat1` 与 `armsoftprobe1` 均为历史诊断产物。BCM2727 拒绝目标 7-ref/DPB7/weighted 流，系统 ARM 又在 Configure 返回 -5。当前回归基线仍是 `surfacepersist1`；`ffmpegsoft1` 已真机出画面但约 2–3 fps，只是纯 C 性能基线。当前优化入口是从源码以 `CONFIG+=ffmpegsoft2` 构建，不是本页既有签名包。1.0 前的新路线见 `docs/PLAYER_1.0_DECODING_POLICY_ZH.md`。

## 安装包

历史系统 ARM Debug 诊断包（已失败，不再安装作当前候选）：

`symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_armsoftprobe1_currentcert.sis`

- 大小：9,241,072 字节
- SHA-256：`17730DC83BAD4FDC66AF35FE721AE6A2C6D56D772EC0B1CEDCF7B370F2E0CF3E`
- 构建：GCCE ARMv5 Debug，`sbs errors: 0`
- 签名证书：2026-08-24 至 2036-08-21

此包仅对 7/4/7/weighted 风险模板选择系统 ARM H.264 UID `0x102073EF`，保留 MMF AAC；真机 `ConfigureDecoderL()=-5`，路线已废止。

历史 `codeccompat1` Release 诊断包（不要作为当前播放器优先安装）：

`symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_codeccompat1_currentcert.sis`

- 大小：9,220,876 字节
- SHA-256：`08C9F75DCD4025E2680B41B5A4B908BFF5439E06D46218C88A32047AA048B2D0`

对应的历史 Debug 诊断包：

`symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_codeccompat1_currentcert.sis`

- 大小：9,205,204 字节
- SHA-256：`61F87FBE6B3A2B9D014CBC569C1F798733655E54C010577EB08EDE667D5EC9FA`

两份包均由 Belle Qt 4.7.4/GCCE 构建，Debug/Release 均为
`sbs errors: 0`。SIS 版本仍为 0.7.0，当前自签名证书有效期为
2026-08-24 至 2036-08-21。

对应 unsigned 归档：

| 配置 | 文件 | 大小 | SHA-256 |
|---|---|---:|---|
| Debug unsigned | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_codeccompat1_unsigned.sis` | 9,204,488 | `B98ECD839C1907B8990BF28C73B4DF8427C6CB15B48E1025F4E5D13E9FEB9112` |
| Release unsigned | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_codeccompat1_unsigned.sis` | 9,220,160 | `F0A38A710B0EE4714F2B59D283664091BA483954BF9BEC847F431F3BC5052D68` |

双配置均为 `sbs errors: 0`。`signsis -o` 已确认签名证书为当前 Qt Development Frameworks 自签名证书，有效期 2026-08-24 至 2036-08-21。这些包曾完成构建和签名，但后续真机控制实验已经否决其 BCM 兼容路线，因此只保留用于追溯，不再标记为“正式候选”。

## 0.7 播放架构

0.7 不再通过 Qt Mobility 的 `QMediaPlayer + QVideoWidget` 间接控制
Symbian 视频输出，而是直接使用 Belle 的 `CVideoPlayerUtility2`：

- 主 AVKON/QGL 窗口始终保持 360×640 竖屏，不再调用系统方向切换；
- MMF 直接绑定播放器内部拥有的原生 `RWindow`；
- 横屏视频只对该显示窗口调用 `SetRotationL(...Clockwise90)`；
- `SetAutoScaleL(...BestFit)` 负责在物理屏幕内等比缩放；
- 播放、暂停、停止、进度、音量、倍速与错误状态由新的原生后端统一封装；
- `AddDisplayWindowL` 只在媒体 `OpenComplete(0)` 后调用，避免 controller 尚未就绪时的 `-18`；
- `KErrMMPartialPlayback (-12017)` 作为可恢复状态继续探测音视频轨并播放；
- 播放器 controller、native video host `QWidget/CCoeControl/RWindow`、MMF facade/observer 与 `CVideoPlayerUtility2` 在应用期只创建一次；退出只停止、解绑和隐藏，第二次进入复用同一对象图；
- 切换媒体时对现有 MMF utility 执行公开的 `Stop → RemoveDisplayWindow → Close → OpenUrlL/OpenFileL` 流程，不再跨会话 `delete → NewL`；
- ARGB 控制/弹幕覆盖窗同样由主应用持有并跨播放复用；退出时先停覆盖层 timer、清空受保护 owner 和输入状态，再隐藏窗口，避免第二次销毁/创建 Qt/WSERV backing store。

对于 MMF 只能播放声音的 7 refs / 4 reorder / DPB 7 / weighted B-frame AVC，`codeccompat1` 增加完全本机的兼容路径：

- `mp4` 和 `mp4720` 都会进入轻量 Range 预读，正常 4/3/4/0,0 模板仍由 MMF 播放；
- 风险模板按 MP4 sample table 从同步帧开始分批读取，每个 sample 独立转为 Annex-B access unit；
- BCM2727 `CMMFDevVideoPlay` UID `0x10204C21` 使用 `EDuCodedPicture`、DTS 和 sequence number 持续硬解；
- MMF 媒体保持打开并继续输出 AAC，`CSystemClockSource` 以 MMF position 为主时钟，超过 150 ms 漂移时校正；
- DevVideo 的内存 YUV/RGB 帧先绘入持久 ARGB 层，再绘弹幕和控制，因此视频不会遮挡弹幕；
- pause/resume、关键帧 seek、分批补流、InputEnd、退出和重复进入均进入同一生命周期；
- 超出 BCM2727 1280×720 上限或硬解失败时只安全降到 Q16，不使用桥接或远端转码。

网络媒体仍先尝试 Bilibili CDN 地址。普通视频在所有直连地址失败后会使用
带 User-Agent、Referer、Origin 和 Cookie 的 Qt 网络请求下载到临时 MP4，再交给
原生 MMF 播放；临时文件上限为 96 MiB。直播不会走完整文件下载。

## 弹幕覆盖方案

0.7 取消了“一条弹幕一个 WSERV 窗口”的诊断实现，控制栏和全部弹幕只使用
一个覆盖整个播放器的顶层 ARGB 窗口：

- 覆盖窗在应用生命周期内只创建一次，播放会话通过 `attachOwner()` / `detachOwner()` 安全接管和释放；
- 创建前设置 `WA_TranslucentBackground`；
- 创建后显式调用 WSERV `SetTransparencyAlphaChannel()`；
- 使用 `WindowStaysOnTopHint`，并在播放器进入、表面重建和尺寸变化时重新置顶；
- 每帧先把未使用区域清为完全透明，再绘制弹幕和控件；
- 横屏布局使用 640×360 虚拟坐标，触摸输入做对应逆变换；
- 文字、控件和弹幕先在复用的 640×360 ARGB 缓冲区中以正常方向清晰栅格化，再进行无缩放、1:1 的 90° 像素旋转，避免 Qt 4 直接旋转字体造成模糊；
- 单帧最多绘制 18 条弹幕，避免 Nokia 603 因文字栅格化和窗口合成过载；
- 播放期间暂停主 NanoVG 页面内容绘制，只保留 EGL surface，减少与 MMF/弹幕层竞争。
- 覆盖层 owner 使用 `QPointer` 保护；退出先停止 50 ms timer，迟到的绘制和触摸事件在 owner/player 为空时直接返回；
- 第二次进入复用已经完成 alpha 设置的原生窗口，并取消覆盖窗反复 `activateWindow()` / `setFocus()`。

这套结构同时规避了 0.6.x 已确认的两个故障边界：AVKON 旋转后 MMF 显示
设备失效，以及普通 raster 子控件位于 QGLWidget 子树时没有 paint engine。

## 已确认旧故障与当前修复状态

旧 `release_full`、`debug_mmftrace5` 和 CODA 路径共同记录了以下事实：

- 第一次视频、声音、弹幕和清晰 UI 正常；
- 退出后依次出现 `PLAYER_SESSION_DELETE_LATER`、`DESTROY_BEGIN` 和 `DESTROY_READY`；
- 第二次进入创建了新播放器会话和新的原生覆盖控件；
- 在 `PLAYBACK_READY` 之后、`PLAYER_REBUILD_BEGIN` 之前，主 UI 线程发生访问 `0x140` 的 data abort；
- 第二次 MMF 后端、`OpenUrlL`、`AddDisplayWindowL`、rotation、prepare 和 play 尚未开始。
- 用户确认 Release 和 Debug 脱离 CODA 独立启动也会崩溃，因此 CODA 与 UDEB-only 均已排除。

故障地址处是通过无效对象/虚表进行虚函数调用的 ARM Thumb 指令序列。设备 Qt 共享库未加载符号，因此不能精确命名 Qt 函数。`overlayreuse1` 真机仍失败，而 `surfacepersist1` 进一步保留 controller、native video host、MMF observer/utility 和 overlay 的全部原生对象身份；用户已确认后者可以退出后再次播放，故确定性的第二次进入 P0 已功能解决。

Qt Creator 的 CODA 适配器会把 Symbian exception/panic 通用映射成 GDB `SIGSEGV`。CODA 会改变时序，但 Release/Debug 独立启动均已复现，证明它不是必要触发因素；手机报告的 data abort 也不是 packet-size 或缺少共享库符号警告造成的。详细证据和后续调查边界见 `docs/PLAYER_0.7_SECOND_ENTRY_CRASH_ANALYSIS_ZH.md`。

## codeccompat1 历史真机验收计划（已作废）

以下步骤保留用于解释旧包设计，不能作为当前测试指令。当前 1.0 路线以 `PLAYER_1.0_DECODING_POLICY_ZH.md` 为准。

1. 先安装 Debug `codeccompat1`，确认应用管理器显示 0.7.0。
2. 打开正常样本 `BV1oyhM6AETw`，应 `PROFILE_SKIP` 并保持 MMF 画面、声音、弹幕和清晰 UI。
3. 打开故障样本 `BV1Uy8x6AETG`，应记录 `PLAYER_HANDOFF → INIT 0 → FIRST_PICTURE`，同时保留声音、弹幕和控制。
4. 在故障样本播放 30 秒，测试 pause/resume 和拖动 seek；记录 `CLOCK_RESYNC` 是否频繁、音画是否可感知错位。
5. 正常/故障样本交替进入退出至少 10 次，确认没有卡死、残留透明窗或后台声音。
6. 若失败，保留从 `DEVVIDEO_MP4_AVC` 到 `PLAYER_FATAL/FAILED` 的完整日志，再根据第一个非零错误处理；不要退回旧探针包。
7. Debug 功能通过后安装 Release `codeccompat1`，独立运行达到 50 次进入/退出，0.7 才能标记为稳定版。

预期关键日志：

```text
WW:PLAYER_VIRTUAL_ORIENTATION true ...
WW:PLAYER_BACKEND_NATIVE 1 true
WW:PLAYER_OVERLAY_PERSISTENT_NEW ...
WW:PLAYER_OVERLAY_ATTACH ...
WW:NATIVE_MMF_WINDOW ... true
WW:PLAYER_OVERLAY_ALPHA ... 0
WW:NATIVE_MMF_OPEN_COMPLETE 0
WW:NATIVE_MMF_ROTATION true 0
WW:NATIVE_MMF_SCALE 0
WW:NATIVE_MMF_PREPARE_COMPLETE 0
WW:NATIVE_MMF_TRACKS false 0 true 0 true
WW:NATIVE_MMF_PLAY
WW:PLAYER_NATIVE_SURFACE_PARKED
WW:PLAYER_OVERLAY_DETACH ...
WW:PLAYER_SESSION_PARKED 1 <controller> <video-host> <backend>
WW:PLAYER_SESSION_RETAINED <same-controller>
WW:PLAYER_OVERLAY_ATTACH ...
WW:PLAYER_SESSION_ACTIVE 2 <same-controller> <same-video-host> <same-backend>
WW:PLAYER_BACKEND_REUSE <same-backend> true
WW:PLAYER_OVERLAY_NATIVE_REUSE ...
WW:PLAYER_OVERLAY_FIRST_PAINT ...
WW:NATIVE_MMF_UTILITY_REUSE <same-utility> <same-window>
```

旧版本已知失败签名：

```text
WW:PLAYER_SESSION_DESTROY_READY ...
WW:PLAYER_SESSION_OBJECT_NEW ...
WW:PLAYER_OVERLAY_ALPHA ... 0
WW:PLAYER_PAGE_PREPARED ...
WW:PLAYBACK_READY ...
A data abort exception has occurred accessing 0x140
```

如果第二次失败发生在 `PLAYER_REBUILD_BEGIN` 之前，不要把它归因于 MMF、CDN 或视频编码。

若横屏仍无画面，优先保留 `NATIVE_MMF_WINDOW`、`OPEN_COMPLETE`、
`ROTATION`、`PREPARE_COMPLETE` 和错误码。若视频正常但弹幕被挡住，保留
`PLAYER_OVERLAY_ALPHA` 及播放器进入后 20 秒日志；不要发送完整 Cookie 或
带签名的媒体 URL。

## 当前验证边界

Nokia 603 真机已经确认横屏播放的画面、声音和弹幕正常，控制/UI 清晰，且
`surfacepersist1` 可以退出后重复进入。旧 Release、Debug、CODA 与 `overlayreuse1`
仍是失败对照。Release 连续进入/退出 50 次前，0.7.0 仍不是完成压力验收的稳定版。

当前独立待验收项是 H.264 编码覆盖：`BV1Uy8x6AETG` 的最低 360P AVC 使用
7 个参考帧、4 帧重排和 weighted B-frame，MMF 只能播放音频；正常对照
`BV1oyhM6AETw` 为 4/3/无 weighted。后续对照已经证明 BCM2727 拒绝最低 Q16 的 7/4/7/weighted 故障流，所以 `codeccompat1` 的 Direct DevVideo 接管不能成为正式解法。1.0 前直接进入本机软件解码路线，仍无法软解时允许用户确认后调用外部播放器；不使用桥接或远端转码。详见 `docs/PLAYER_1.0_DECODING_POLICY_ZH.md`。
