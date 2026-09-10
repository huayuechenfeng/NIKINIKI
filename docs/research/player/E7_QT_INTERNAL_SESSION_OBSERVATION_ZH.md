# E7 Qt 内部会话观测映射

> 状态：Device comparison complete / structural attribution paused
> 日期：2026-09-07
> 范围：成功冻结最小 Qt A 与失败完整应用 Qt 的同媒体内部链对照

本文承接[完整应用 Qt 同媒体结论](E7_BLACK_SCREEN_REASSESSMENT_ZH.md#e7-full-app-qt-same-media-result-20260907)。
不重新审计旧 native、B/C、Direct DevVideo、C1/C2、系统补丁或窗口矩阵，也不修改正式播放逻辑。
本轮完成主机身份核实、候选模块有限映射及一次有界设备内部对照；没有安装包、替换 DLL、修改
正式播放逻辑或启动额外播放器方案。

## 身份核实与记录修正

冻结参照原件、原 `build-manifest.json` 和 SIS payload 三者一致：

| 对象 | 大小 | SHA-256 | 结论 |
|---|---:|---|---|
| 签名 `NIKINIKI_E7_BIND_TIMING_V1_debug.sis` | 1,762,168 | `2F6C7BC3CE7A3DF9D99A8C190110F58DC3778507C2768FF35F7DF2E60F595D3A` | 本轮四次对照使用的冻结参照原件 |
| 未签名 `e7-bind-timing-v1_unsigned.sis` | 1,761,448 | `5AF22D9CCBDBE4C4DC5F3ABC8032D7EC18769679B22667549A304F8232799060` | 与签名包区分记录 |
| payload `nikiniki_e7_qt_window_probe.exe` | 1,922,568 | `915E8B095B3BEBF40FC721EBE4E83A16BBC76B700957721939728555C3A19481` | 原 manifest 标记 `payloadExeMatchesFrozenExe=true` |

设备矩阵及完整应用冻结清单一度写成 SIS `1,922,568` bytes / `2F6C…E7FC…`、EXE
`915E…50CD…`。前者实际是 EXE 大小；两组摘要均不匹配现存原件，也不匹配原冻结 manifest。
因此这是**记录转写错误**，不是已证明的重签或包变化。两次参照运行日志分别以 PID 2621、2687
记录 `package=ABC config=e7bindtiming1 build=e7-bind-timing-v1 mode=A`；受保护设备 EXE 的独立
readback 仍为 `UNKNOWN`，不能用运行标签代替设备文件哈希。

完整应用签名 SIS 为 9,903,372 bytes / `39DF195E77B897D1950C9D576DED7A31FDE147A6C2986C400CE10803B286F076`；
payload EXE 为 2,939,233 bytes / `3DE617ECBF47F532DB1B8128FB1E0F1F84A431BE0E568FDFE58055A4050DC1B6`。
未签名 SIS 的实测摘要应为
`DFBBBBCC91ABA87041EAB1D535D302E41524D3EE74A0169E8E0931D9D260E464`；冻结清单中的旧值也已修正。

候选 `.tmp/e7-mobility-runtime/qtmultimediakit_mmfengine.dll` 为 59,797 bytes，SHA-256
`44EE2383965365B40D35CEE9EB0FB8D5C9A7A18F0343533A3CE953C87B8D86B4`，E32 身份为 ARMv5、
UID3/SID `0x2002AC76`、QtMultimediaKit ABI `{00010202}`。其解压 code 段为 `0x14C08` bytes，
SHA-256 `9FAEC742872CD5F8377D28827500A5C6D4159933F10E840434AA1653FF6F7792`。CODA 先前读取
`Z:/sys/bin/qtmultimediakit_mmfengine.dll` 返回 `-21`，故候选与设备实际加载模块的关联目前仍为
`UNKNOWN`。UID、版本字符串、文件名和导入能力都不补足这一缺口。随后设备对照在实际模块
`CodeAddress=0x7BA40000` 逐次读取下述三个函数块，四次有效运行均与候选 fingerprint 完全一致，
因此候选与这四次实际加载 code 的关联现已建立；这不等于设备整份 DLL 已被回读并取得完整哈希。

## 有限函数恢复

以下地址只属于上述候选 code 段。恢复依据同时包括：候选 E32 的 `.ARM.exidx` 函数边界；本地
Symbian3Qt474 UREL map 的函数相对顺序和大小；候选 Thumb 控制流与成员访问；候选导入 veneer
中的 ordinal 及本地 `mediaclientvideo.dso` 导出表。SDK 二进制与候选不同，网上源码也不是设备
地址来源；它们只用于为候选反汇编命名，不能单独授权设备断点。

| 函数/点位 | link 地址 | code 偏移 | 身份与参数依据 |
|---|---:|---:|---|
| `S60VideoPlayerSession::applyPendingChanges(bool)` | `0xE868` | `+0x6868` | 函数入口保存 `r0` 为 session；`r1` 为 force；读取 state `+0x2C`、pending `+0xAC`，随后走真实输出窗口绑定和几何设置 |
| `S60VideoPlayerSession::doPlay()` | `0xF1A0` | `+0x71A0` | `r0` 为 session；`0xF1A4` 读取 utility `+0x78`，`0xF1A6` 经 ordinal 22 调用无参 `CVideoPlayerUtility::Play()` |
| `S60VideoPlayerSession::MvpuoPrepareComplete(TInt)` body | `0xF4D2` | `+0x74D2` | 入口 `r0=session`、`r1=aError`；`0xF6FE` 的 observer thunk 先 `r0-=0x50` 再跳回该 body，交叉证明完整对象地址和参数约定 |
| Prepare 内 apply 返回点 | `0xF6CC` | `+0x76CC` | `0xF6C4` 置 `r1=1`，`0xF6C8` 直接调用 `0xE868`；命中该点证明 `applyPendingChanges(true)` 已返回 |

Prepare 接受分支内，`0xF64A` 经 ordinal 156 调用 `CVideoPlayerUtility2::AddDisplayL`；若无 leave，
`0xF6A0` 经 ordinal 137 对 dummy window 调用 `AddDisplayWindowL`，之后才到
`applyPendingChanges(true)`。apply 内对真实输出窗口的关键调用为：`0xE934` / ordinal 137
`AddDisplayWindowL`、`0xE96C` / 139 `RemoveDisplayWindow`、`0xE9B6` / 135
`SetVideoExtentL`、`0xE9F6` / 138 `SetWindowClipRectL`、`0xEB0C` / 147
`SetScaleFactorL`、`0xEB70` / 143 `SetRotationL`。这证明候选二进制的实际静态链，不证明此前
任一设备运行已经执行它。

可在同一 session 上读取的候选成员为：base error `+0x38`、`RWsSession* +0x6C`、
`CWsScreenDevice* +0x70`、utility `+0x78`、dummy window object `+0x8C`、output control
`+0x90`、output display `+0x94`、实际 `RWindow* +0x98`、pending flags `+0xAC`。
在 `0xF6CC`，`r4=session`，`[sp+0x38]` 是 AddDisplayL/dummy-window 路径共用的 TRAP error；
它为非零只能证明 apply 前两步之一 leave，不能在没有更细命中时区分是哪一步。

主机只读校验器为 `tools/research/e7/qt_mmf_internal_map.py`。它核对候选完整摘要、code 段摘要及
三个函数块 fingerprint，并可用 `--code-address` 纯算术输出运行地址；不含 CODA transport、安装、
启动、断点或写设备能力。当前三个 fingerprint 均匹配：

```powershell
$python = 'python'
& $python tools\research\e7\qt_mmf_internal_map.py `
    --candidate '<private-input>\qtmultimediakit_mmfengine.dll' `
    --code '<private-input>\qtmultimediakit_mmfengine.code.bin'
```

只有设备内 fingerprint 已匹配后，才可在同一命令末尾加
`--code-address <CODA 返回的十六进制 CodeAddress>`。

| 候选块 | 长度 | SHA-256 |
|---|---:|---|
| `+0x6868` apply | 64 | `C316DF978DC6E221E7F04EF78D21047D8ADF4DA2A31F604D972AF085A05A2134` |
| `+0x71A0` play | 32 | `1CE9B30E1C30118D22A4CC59618DA62D56CE48760A155CB3D0B2D89111FFFF5D` |
| `+0x74D2` prepare | 64 | `2C2447CC82E9C0EB514ED8416717BA6503168D63263DB61A8CA761F83C25B76B` |

## 设备能力与实际加载身份

CODA 在每次进程的 Shared Library 停止事件中都报告实际
`qtmultimediakit_mmfengine.dll`、`CodeAddress=0x7BA40000`，并允许读取 ROM code、通用寄存器、
session 内存以及设置/移除 Thumb breakpoint。`Breakpoints.getCapabilities/getStatus` 返回空结果，
所以具体硬件/软件槽类型仍不可枚举；但四次均实际命中并自动继续。运行地址按
`CodeAddress + (linkAddress - 0x8000)` 换算，四次的 apply/play/prepare 三块设备内 fingerprint
全部匹配候选。没有 fault/exception，未主动调用 `PositionL` 或其他播放器方法。

脚本在正式四次前有三项无效记录：Python `serial` 依赖在连接前失败一次；PID 2761 在模块加载暂停
处因 CODA 把寄存器命名为 `rR0` 而退出，未到媒体 Play；PID 2910 因主机误传
`--e7-backend=qt` 而得到 `modeValid=0 / MEDIA_ENTRY_REJECTED`，未创建媒体会话。用户明确这些
工具/参数预启动失败只记录、不计四次有效预算。寄存器归一化离线校验后，完整应用按源码要求改用
`--e7-mode=full`。这些错误不作为黑屏机制证据。

## 四次设备内部对照

有效顺序为冻结 A → 完整 Qt → 完整 Qt → 冻结 A；PID 依次为 2787、2916、2933、2950。两侧继续
使用 `E:/test/test.mp4`，没有重装、换包、换媒体、下载、preflight 或其他媒体应用。每次在内部
MMF Play 命中并继续后固定观察60秒，再由 CODA terminate；相邻有效启动间隔均超过15秒。该退出
方式仍不同于产品正常 Back/持久复用。

| 运行 | Prepare 回调 | pending/output 应用 | MMF Play | 断点暂停 | 人工结果 |
|---|---|---|---|---|---|
| #1 冻结 A / PID 2787 | session `0x2A32DE8`，utility `0x803B28`，`aError=0` | 同一 session；pending `3→7→0`；output control `0x861850`、display `0x2A317A8`；真实 window `0→0x85FE98`；apply 前路径 TRAP error 0 | 同一 session/utility 命中 | prepare/apply/after/play 为 15/15/78/15 ms | 画面正常、声音流畅、视频外背景黑色；完整性未单独确认 |
| #2 完整 Qt / PID 2916 | session `0x4D7EEE0`，utility `0x969434`，`aError=-12017` | 同一 session；pending `3→7→0`；output control `0x962BF0`、display `0x4D7DED0`；真实 window `0→0x969040`；TRAP error 0 | 同一 session/utility 命中 | 32/16/15/16 ms | 无画面、声音正常、播放器 UI 正常 |
| #3 完整 Qt / PID 2933 | session `0x4D7EEC8`，utility `0x969434`，`aError=-12017` | 同一 session；pending `3→7→0`；output control `0x962BF0`、display `0x4D7DEB8`；真实 window `0→0x969040`；TRAP error 0 | 同一 session/utility 命中 | 16/15/16/16 ms | 无画面、声音正常、播放器 UI 正常 |
| #4 冻结 A / PID 2950 | session `0x2A32DE8`，utility `0x803B28`，`aError=-12017` | 同一 session；pending `3→7→0`；output control `0x861850`、display `0x2A317A8`；真实 window `0→0x85FE98`；TRAP error 0 | 同一 session/utility 命中 | 15/15/15/15 ms | 无画面、声音正常、背景黑色 |

四次设备内 code 身份和断点格式相同。唯一出画的 #1 在 `MvpuoPrepareComplete` 收到0；三个黑屏
运行都收到实例归属明确的 `-12017`。这使 Prepare completion 成为本轮**最早观察到的结果相关
分界**，并首次把 `-12017` 归属到本次 Qt session；它不再是旧 native 或无归属 PP 日志的套用。
但 #4 冻结 A 在同模式、同媒体、相同观测下从此前成功翻为黑屏，已触发预设停止条件。因而不能据此
宣布完整应用对象图、窗口或重复 `setVideoOutput` 是根因，也不能把 `-12017` 从相关标记提升为已
证明的黑屏原因。断点发生在回调入口、错误值产生之后，所以该次 prepare 入口暂停不能倒因产生该
错误；模块加载暂停或跨进程媒体服务状态是否参与仍为 UNKNOWN。

原始日志保留在本地忽略目录 `.tmp/e7-qt-internal-20260907/`。`run01-A.jsonl` 与
`run02-FULL.jsonl` 分别包含一次上述无效记录及随后有效记录；四文件 SHA-256 依次为
`DE4C9EC4108CFBA31B3B68824E2F655450EA852B92DB27824C0193CF9D8CFEB3`、
`9039CE869E7EC375714CABF838821CF8D8B54729B3A44C30B215C0DFAFE85915`、
`FC7CA2D42AF4F12BC27ECD3580EA9FF772D5E68F766FE8A566C9402D2B737832`、
`D8F11C5F384D147C1F8CAABF40E484B8252B33F8AC919159C83650DB5ADE6AA2`。

## 后续 MMF Prepare 来源调查

暂停完整应用结构性修复。用户随后提供了 RM-626 / SW 111.040.1511 core 候选；主机已经从
ROM-FS 可靠确认 `MediaClientVideo.dll` 条目，并从同一 core 确认 HxMmfCtrl / Real 的 ECom 静态
候选；随后用户提供的目标手机逻辑文件副本已恢复 E7 event handler。后续raw-event取证与本轮保存
寄存器的联合复核已证明旧三个`-12017`由controller event带入MediaClientVideo，utility只原样
转发；同时把最早运行差异收窄到Helix视频服务线程是否成功出现。具体身份限制、证据链和唯一下一点
统一见[E7 MMF Prepare 错误来源](E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)，不再在本文维护第二份下一步。

此前不删除重复 `setVideoOutput`、不重构窗口、不恢复封存开关，也不做 dummy/AddDisplayL、
GetFrameL、延时、重试或系统 DLL 替换。
