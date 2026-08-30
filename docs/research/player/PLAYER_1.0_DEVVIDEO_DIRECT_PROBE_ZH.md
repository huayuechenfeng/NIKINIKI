# DevVideo Direct Screen Access 最终支线探针（已封存）

> 文档状态：Historical experiment。本文只证明 DSA Phase A 边界，不是当前显示后端。

> 状态：2026-08-28 Nokia 603 真机 Phase A 硬失败；1.0 前不制作第二包、不接 FFmpeg。  
> 边界：本探针不修改 MMF AAC、PTS、Range、队列、catch-up 或 CPU RGB565 fallback。普通构建未编入其源文件。

## 真机最终结果

`devvideodirectprobe1` 的 Nokia 603 / CODA 日志完成了预定的 Phase A 硬门：

| 观察 | 日志证据 | 结论 |
|---|---|---|
| DSA 对象可创建 | `DIRECT_PHASE_A_BEGIN ... dsaError 0` | API/原生窗口不是立即创建失败 |
| overlay 隐藏时无区域 | `DIRECT_REGION overlay=0 rects 0 area 0` | 初始样本无可用区域；它单独不足以断言全系统 DSA 不可用 |
| 正式 ARGB overlay 显示后仍无区域 | `DIRECT_ABORT reason 1` → `DIRECT_RESTART ... error 0 rects 0 area 0`，随后 `overlay=1 ... area 0` | 产品所需的“DSA 视频下方 + 顶层 ARGB UI/弹幕”不共存 |
| Phase B 未进入 | 无 `DIRECT_PP`、`DIRECT_INPUT_*`、`DIRECT_PIPELINE_STARTED` | 硬门正确阻止了 DevVideo/post-processor 初始化 |
| 收尾 | `DIRECT_RESULT phaseA NO phaseB NO`，随后 `PLAYER_NATIVE_PORTRAIT_FULLSCREEN_READY` | 探针安全恢复，主线对象图未损坏 |

真机上可见的标题、控制条、模拟弹幕和触摸反馈来自 **ARGB overlay 自身**；Phase A 尚未创建 DevVideo，因此它们不是 DSA/YUV 画面。该现象恰好说明透明 ARGB 顶层窗在 Window Server 的 DSA 遮挡模型中仍按整个窗口区域遮挡底层视频窗；透明像素不会把 DSA 写入权让回去。

因此本结果只否定“全屏透明 ARGB overlay 与 DSA 直显并存”的正式播放器架构，不声称两个 post-processor 不具备 YUV 能力——Phase B 根据硬门从未运行。后续仅作为 1.0 后研究的受限 UI 方向，见 `POST_1.0_DEVVIDEO_DISPLAY_RESEARCH_ZH.md`。

## 最终执行计划

1. Phase A 只在已验证的原生 640×360 `SoftVideoSurfaceWidget/RWindow` 上启动 `CDirectScreenAccess`，让正式 ARGB overlay 经历隐藏、显示、raise、持续 UI/弹幕重绘和触摸；DrawingRegion、Abort/Restart 任一硬门失败即结束，绝不创建 DevVideo。
2. Phase A 通过后，Phase B 依次探测 `0x2003162A`、`0x10273417`，以最多三种合理 planar YUV420 描述提交预生成黑、白、色条和移动扫描线；目标只有 `YUV420 -> post-processor -> DSA`，不要求 RGB memory output 或 `EPpYuvToRgb`。
3. Phase B 使用三个固定 345600-byte 槽，图片返回前不修改、不释放、不复用；运行约十秒并记录提交、返回、写入耗时、返回延迟、busy drop、DSA 中断/恢复、region、picture counters 和 playback position。
4. 只有 Phase A、Phase B 和人工目视同时为 YES，才允许制作第二包 `devvideodirect1`；否则立即封存此路线，普通构建继续 CPU RGB565。

## SDK 实签名修订

- `CDirectScreenAccess::NewL(..., ETrue)` 的 Phase A 只要求 `DrawingRegion()`；region-tracking-only 模式不强求 drawing GC/`ScreenDevice()`。
- Phase A 通过后销毁该 DSA，并用同一视频窗口、`EFalse` 重建完整 DSA，取得 DevVideo `StartDirectScreenAccessL()` 所需的 `CFbsScreenDevice`。此重建失败也是 Phase B 硬失败。
- 实际配置顺序为 `SelectPostProcessorL -> SetInputFormatL -> GetOutputFormatListL（只记录） -> SetVideoDestScreenL(ETrue) -> Initialize -> StartDirectScreenAccessL -> Start -> WritePictureL`。
- 未设置 clock source，因此静态探针按应用 40 ms 定时器供帧；这避免把 MMF AAC 引入第一包，同时保留每帧 presentation timestamp 和 `PlaybackPosition()` 遥测。
- `AbortNow()` 只停止提交、置 aborted，并调用已初始化 DevVideo 的 `AbortDirectScreenAccess()`；不调用 Window Server。`Restart()` 才重新 `StartL()`、读取新 region，并重启 DevVideo DSA。
- FatalError 只记状态并投递零延迟 Qt 事件，绝不在 DevVideo 回调栈删除对象。

## 历史真机操作（不再于 1.0 前重复）

1. 在 Qt Creator 打开 `symbian/app/wiliwili_devvideo_direct_probe.pro`，选 Symbian Device / Debug，运行 qmake 后构建并启动。
2. 应用约 1.5 秒后自动进入横屏；不需要登录或选视频。
3. overlay 出现后在画面上点按或拖动一次，确认触摸反馈；观察文字、控制条和移动“弹幕”始终在视频层上方。
4. 若 Phase A 通过，随后观察约十秒：黑、白、彩条及移动白色扫描线；结束后应用自动恢复竖屏主页。
5. 复制所有 `WW:DIRECT_` 日志。若发生横竖屏恢复问题，再一并复制 `WW:PLAYER_NATIVE_`。

## 历史判据

Phase A 日志 YES 必须同时满足：

- `WW:DIRECT_PHASE_A YES`；
- overlay=0 与 overlay=1 的 region 都至少为视频区 80%；
- 没有持续 aborted，也没有超过 12 次的 Abort/Restart 风暴；
- 人工确认 overlay 始终在视频层上方且触摸有效。

Phase A 任一项不满足即 NO；日志中不应出现 `WW:DIRECT_PP`。

Phase B 日志 YES 必须同时满足：

- 至少一个 PP 完成输入格式、初始化和 DSA 启动，并出现 `WW:DIRECT_PHASE_B YES`；
- 十秒内至少提交 25 帧、有 ReturnPicture、无 fatal/aborted；十秒边界不要求 submission pool 清空，因为 renderer 可以合法持有当前显示帧和刚接收的帧；
- 三槽全 busy 后，只有在 1.6 秒内 ReturnPicture、displayed counter 和 playback position 都停止增长才算死锁；
- 人工确认方向、黑白/色彩、移动扫描线、overlay 层级与触摸均正常，无持续黑屏或严重闪烁。

最终只有 `WW:DIRECT_RESULT phaseA YES phaseB YES` 加上述人工确认才算该架构成立。本包实际结果为 `phaseA NO phaseB NO`。

## 历史构建

命令行：

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-App.ps1 -Configuration debug -Variant devvideodirectprobe1
```

普通 `.pro` 未加入探针源文件；只有 `CONFIG+=devvideodirectprobe1` 才定义入口。关闭该 CONFIG 后，播放器仍使用原 CPU RGB565 soft surface。

2026-08-28 最终构建结果：探针 Debug 为 `sbs errors: 0`（34 条既有 SDK/编译器警告）；关闭探针宏的普通 Debug 回归也为 `sbs errors: 0`（34 条既有警告）。当前证书安装包：

```text
symbian/out/wiliwili-symbian-debug-devvideodirectprobe1/
  wiliwili_symbian_0.9.0_debug_devvideodirectprobe1_currentcert.sis
```

大小 `9,738,096` bytes，SHA-256：

```text
D9715E71A80343059D1206840541B57EF53C8EEDCD61010F6E5CEBE2921B4520
```

`signsis -o` 已确认签名为 Qt Development Frameworks 当前证书（2026-08-24 至 2036-08-21）；`dumpsis -l` 已确认主 EXE capabilities 与 SIS 头一致，为 `NetworkServices ReadUserData`。
