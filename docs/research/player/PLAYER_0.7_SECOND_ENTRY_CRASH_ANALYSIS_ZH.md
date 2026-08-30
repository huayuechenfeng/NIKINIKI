# 0.7 播放器第二次进入崩溃分析

> 文档状态：Historical evidence。当前生命周期决定见
> `docs/decisions/0002-persistent-player-lifecycle.md`。

> 记录日期：2026-08-25  
> 适用版本：wiliwili for Symbian³ 0.7.0  
> 状态：已解决。用户真机确认 `surfacepersist1` 可以退出后再次进入播放器；50 次 Release 压力门槛仍待完成  
> 关联总报告：`docs/archive/wiliwiliforsymbian3/DEVELOPMENT_STAGE_REPORT_2026-08-25_ZH.md`

## 1. 当前结论

0.7 已在 Nokia 603 真机确认横屏视频、声音、弹幕覆盖和清晰 UI 均正常。用户随后确认 `surfacepersist1` 可以退出后再次播放，原先稳定复现的“第二次进入播放器”P0 已功能修复。

旧日志证明 one-shot 会话与 `deleteLater()` 已按设计完整执行，因此“旧 `VideoPlayerWidget` 没有销毁”不成立。故障发生在第二次播放器页面准备完成之后、延迟 MMF 启动之前，属于原生对象生命周期边界。

随后真机测试确认：即使顶层 ARGB 覆盖窗已经提升为应用级持久对象，`overlayreuse1` 仍会在第二次播放时卡死退出。因此“只因为第二个透明覆盖窗被重新创建”不是充分原因。最终修复把 `VideoPlayerWidget`、native video host `QWidget/CCoeControl/RWindow`、后端 observer、`CVideoPlayerUtility2` 与 overlay 作为完整对象图跨会话复用；真机结果支持这一生命周期根因。

用户随后脱离 CODA 分别测试 Release 和 Debug，两者都会崩溃。因此 CODA 已被排除为必要条件，UDEB-only 假设也被排除；Qt Creator 显示的 `SIGSEGV` 仍只是对 Symbian exception/panic 的通用 GDB 映射，底层手机确实报告了访问 `0x140` 的 data abort。

## 2. 最新日志的完整生命周期证据

第一次进入成功：

```text
WW:PLAYER_SESSION_OBJECT_NEW 0x4d97930
WW:PLAYER_REBUILD_BEGIN true QSize(360, 640) true
WW:NATIVE_MMF_OPEN_COMPLETE 0
WW:NATIVE_MMF_WINDOW ... true
WW:DANMAKU_READY 1200
WW:NATIVE_MMF_PREPARE_COMPLETE 0
WW:NATIVE_MMF_TRACKS false 0 true 0 true
WW:NATIVE_MMF_PLAY
```

第一次退出和销毁成功：

```text
WW:PLAYER_NATIVE_SURFACE_PARKED
WW:PLAYER_SESSION_RELEASED
WW:PLAYER_CLOSED_RESTORE
WW:PLAYER_SESSION_DELETE_LATER 0x4d97930
WW:PLAYER_SESSION_DESTROY_BEGIN 0x4d97930
WW:PLAYER_SESSION_DESTROY_READY 0x4d97930
```

第二次进入创建了新会话和新的原生覆盖控件：

```text
WW:PLAYER_SESSION_OBJECT_NEW 0x4d97930
WW:PLAYER_SESSION_ACTIVE
WW:PLAYER_VIRTUAL_ORIENTATION true true QSize(360, 640)
WW:PLAYER_BACKEND_NATIVE 1 true
WW:PLAYER_OVERLAY_ALPHA 0x4d7eb88 0
WW:PLAYER_PAGE_PREPARED QSize(360, 640) QRect(0,0 360x640) false 2880 2160 true true
WW:PLAYBACK_READY 64 "mp4720" 4 2880 2160
```

第二个 `VideoPlayerWidget` 恰好再次获得 `0x4d97930`，只是分配器复用了已经释放的地址，不能据此认定旧 C++ 对象仍存活。第二次覆盖控件地址 `0x4d7eb88` 与第一次不同，也证明新的原生覆盖层已经创建。

## 3. 崩溃位置

第二次日志没有到达：

```text
WW:PLAYER_REBUILD_BEGIN
```

因此以下路径在第二次会话中尚未开始：

- `CVideoPlayerUtility2` 创建；
- `OpenUrlL`；
- `AddDisplayWindowL`；
- rotation/scale；
- `Prepare()` 与音视频轨探测；
- `Play()`。

CODA 报告：

```text
Thread 1000 stopped: 'Exception':
A data abort exception has occurred accessing 0x140.
signal-name="SIGSEGV"
frame={addr="0x7d4d55c2",func="??"}
```

故障点附近汇编：

```text
0x7d4d55c0: ldr r0, [r5, #0]
0x7d4d55c2: ldr r1, [r0, #84]
0x7d4d55c4: movs r0, r5
0x7d4d55c6: blx r1
```

这是典型的 C++ 虚函数调用序列。访问地址 `0x140` 表明对象的虚表指针已经无效或被覆盖。PC 位于只读共享库区，但 QtGui 等设备 DLL 没有加载符号，当前不能可靠命名具体 Qt 函数。结合发生时序和 `VideoOverlayWidget::paintEvent()` 一进入就构造 `QPainter`，故障更符合 Qt 窗口绘制/paint engine/backing store 生命周期，而不是媒体网络或解码错误。

## 4. CODA 与日志警告的判读

仓库内保留的 Qt Creator 2.4.1 源码显示：

- `codamessage.cpp` 把 `Data abort exception` 和 `Thread has panicked` 归为 crash；
- `codagdbadapter.cpp` 再把包含 exception/panic 的停止原因映射为 GDB `SIGSEGV`。

因此 Qt 弹窗里的 `SIGSEGV` 名称是通用翻译，但 data abort 和访问地址来自手机端。

以下两条不是根因：

- `limiting remote suggested packet size ...`：只是 GDB remote packet 大小协商；
- `Could not load shared library symbols ...`：它在崩溃后执行 `info shared` 时出现，只会妨碍调用栈符号化。

CODA 确实会改变事件调度、线程停顿和堆地址复用，但后续 Release/Debug 独立启动均已复现，证明它不是本次故障的必要触发因素。

## 5. A/B/C 对照结果

| 组别 | 启动方式 | 结果 | 结论 |
|---|---|---|---|
| A | Release SIS，手机独立启动 | 崩溃 | 正式优化构建也受影响 |
| B | Debug SIS，手机独立启动 | 崩溃 | 不是 UDEB-only 差异 |
| C | Qt Creator + CODA | 崩溃，并取得 `0x140` data abort | CODA 便于取日志，但不是触发故障的必要条件 |

三组结果共同把故障边界收敛到应用/Qt/WSERV 的原生窗口生命周期。继续调整 CODA packet、共享库符号搜索路径、CDN 或 MMF 延时不会直接消除该崩溃。

## 6. 已否定的候选：只持久化 ARGB 覆盖层

`overlayreuse1` 曾做以下隔离：应用级复用同一个透明覆盖窗，安全停止 timer、清空 `QPointer` owner，并拒绝迟到 paint/input 事件；播放器控制器、视频宿主和 MMF 后端仍按 one-shot 模式销毁重建。Debug/Release 都已零错误构建并签名。

Nokia 603 真机随后确认第二次播放仍卡死退出。这个结果说明：

- 单独重建 ARGB overlay 不是崩溃的充分条件；
- 已验证的 alpha、弹幕、640×360 离屏清晰化逻辑继续保留；
- 调查边界应下移到播放器 controller、native video host 和 MMF observer/utility 的跨会话销毁重建。

`overlayreuse1` 只保留为失败对照包，不再作为当前测试候选。

## 7. 最终修复：完整原生播放表面复用

`surfacepersist1` 在保留持久 ARGB overlay 的基础上进一步修改：

1. `WiliwiliWidget` 不再在播放退出时把 `m_videoPlayer` 置空或 `deleteLater()`；`VideoPlayerWidget` 只在整个应用退出时析构；
2. 内部 native video host `QWidget/CCoeControl/RWindow` 跟随同一个播放器控制器长期存在，退出只 `hide()`，第二次进入复用原地址；
3. `VideoPlaybackBackend` 和 MMF observer 不再跨会话销毁；
4. `CVideoPlayerUtility2` 只创建一次。切换媒体时按公开 API 执行 `Stop → RemoveDisplayWindow → Close → OpenUrlL/OpenFileL`，而不是 `delete → NewL`；
5. 关闭播放器只停止 MMF、解绑/隐藏 overlay、隐藏视频 host、取消下载并清空会话标志，不销毁 Qt/WSERV/MMF 原生对象；
6. landscape 改变只在 display window 已真正绑定时重新配置，避免对已经关闭 controller 调用 rotation/scale；
7. 新增 `PLAYER_SESSION_RETAINED`、`PLAYER_SESSION_PARKED`、`PLAYER_BACKEND_REUSE`、`NATIVE_MMF_MEDIA_CLOSED` 和 `NATIVE_MMF_UTILITY_REUSE` 日志，可直接核对第二次播放是否复用同一对象地址。

Debug 与 Release 都由 Qt 4.7.4/GCCE 完整编译，`sbs errors: 0`。六个相关 Debug/Release 对象文件已核对包含复用日志；SIS 已检查包含主 EXE、AppArc 注册资源、字体和许可证。用户已在 Nokia 603 上确认可以重复播放，因此该确定性 P0 已解决；仍需 Release 50 次压力验收。

## 8. 当前修复候选产物

| 配置 | 文件 | 大小 | SHA-256 |
|---|---|---:|---|
| Debug surfacepersist1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_surfacepersist1_currentcert.sis` | 9,163,400 | `78667C76978D27BCF331AE41C6A11A7B8921DCD4ED00854A2960515F2BB6DB0E` |
| Release surfacepersist1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_surfacepersist1_currentcert.sis` | 9,168,428 | `4BEBC2E8151FD688E4A7252E3DC86C89E3C562D5AB13DD4D8FFF5CF3DDD50190` |

两份产物都使用有效期 2026-08-24 至 2036-08-21 的当前证书。`surfacepersist1` 已取得可重复播放的真机结果；Release 达到 50 次前仍不得标记为完整压力稳定版。旧 `release_full` / `debug_mmftrace5` / `overlayreuse1` 均保留为已确认失败的对照基线。当前有声无画问题是独立的码流兼容 P0，见 `docs/research/player/PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md`。
