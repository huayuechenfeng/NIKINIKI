# E7 黑屏：backend 对照、绑定时机与完整应用兼容候选重新评估

> 审计截点：2026-09-06；2026-09-07 产品方向调整后，本研究及完整应用兼容候选均已封存。
> minimal backend A/B、绑定时机与 frozen-C cold/warm 三组四次预算均已停止，不启动额外观察、
> 不修改正式播放器行为；兼容候选的最终存档见
> [实验性视频兼容模式存档](EXPERIMENTAL_VIDEO_COMPATIBILITY_ARCHIVE_ZH.md)。
> 当前评估入口；[第一阶段总审计](E7_BLACK_SCREEN_ROOT_CAUSE_AUDIT_ZH.md)保留当时证据与原计划。
> 真机事实分别见设备矩阵的[最小 backend A/B](../../reference/DEVICE_TEST_MATRIX.md#e7-minimal-backend-ab-20260906)
> 、[绑定时机预算](../../reference/DEVICE_TEST_MATRIX.md#e7-bind-timing-observations-20260906)与
> [cold/warm 配对](../../reference/DEVICE_TEST_MATRIX.md#e7-c-lifecycle-pair-observations-20260906)；方法定义见
> [诊断方案](../../reference/E7_BLACK_SCREEN_DIAGNOSTIC_PLAN_ZH.md)。

同一 minimal-P 宿主、同一 EXE 的 A→B→B→A 已命中预设第一分流：A 两次稳定有画有声，B 两次
稳定内容区全黑但有声。后续同一 C 出现黑屏与出画翻转，证明普通 native 输出至少一次可行，
但延后绑定尚不是稳定修复。新增 C cold/warm 四次均 Prepare 0、有画有声，未支持“重启后首次与
terminate 后紧邻会话稳定相反”的生命周期二分。当前优先补齐同一冻结包缺失的 **B/C 绑定时机
负控制**，其次才比较 Qt 输出合约对稳定性的作用。DLL 导入证明候选插件具备额外 surface/EGL
能力，不证明这些能力是本次成功的原因。完整应用环境不是复现 B 黑屏的必要条件；单一 API、
厂商 bug 与有效帧状态仍未定位。

## 本轮交付摘要

### 实际实现与计划偏离

- 新增显式诊断 CONFIG `e7backendab1`，只存在于 `symbian/probes/e7-qt-window/`。同一 EXE
  要求 `--e7-backend=A|B`；缺失或无效参数不创建媒体。每个新进程只创建 A 或 B 一种后端。
- A 复用成功 P v2 的 `QMediaPlayer + QVideoWidget`；B 复用产品 `VideoPlaybackBackend` 的
  `CVideoPlayerUtility2`、shared `RFile`、resolver、普通 `AddDisplayWindowL` 和既有 partial-playback
  处理。只在显式诊断宏内增加稀疏 controller identity 日志。
- 样本 A、主 QGL、YUV/NanoVG512/内置字体资源、外层宿主、fullscreen→maximized→半宽 QGL
  和窗口事件过程固定。没有加入 C2、sanitize、atlas 调整、额外延时或 decoder 策略。
- A 的 Qt 内部 `S60VideoWidget` child 与 B 的普通 native child 被逐次记录；它们不是同一个
  `RWindow`，所以本实验比较的是“后端连同输出适配”，不是只换 Utility 的单变量实验。
- 偏离冻结提交的唯一原因是接手时工作树已 dirty 且包含用户研究改动。为不清理、覆盖或误提交，
  建立分支 `codex/e7-minimal-backend-ab-20260906`，但不创建混合提交；改以 HEAD 加逐文件 SHA-256
  冻结源码。这降低了 commit 级复现便利性，不影响本轮包内 payload 与本地冻结 EXE 的一致性。

普通播放器工程没有加入 `e7backendab1`；产品 backend 的新增代码全部受
`WILIWILI_ENABLE_E7_BACKEND_AB_DIAGNOSTIC` 保护。因而普通播放行为未改变，也没有提交正式修复。

### 既有 P/R 调用差异

无法由源码、旧日志或本轮符号证明的字段保持 UNKNOWN：

| 阶段 | 成功 P / 本轮 A | 失败 R 正式路径 / 本轮 B |
|---|---|---|
| Utility 类型 | 公共层为 `QMediaPlayer`；Symbian 内部 Utility 类型 UNKNOWN | `CVideoPlayerUtility2` |
| controller | UNKNOWN | 旧 R 为 UNKNOWN；本轮 B 实测 `0x101F8514` Real Video Player / Real / v1 |
| 输入重载 | `QMediaContent(file URL)`；插件内部 Open 重载 UNKNOWN | shared `RFile`，`OpenFileL(RFile, KNullUid)` resolver 路径 |
| NewL | `new QMediaPlayer`；插件内部 NewL 参数 UNKNOWN | observer、priority 0、preference `TimeAndQuality`（运行值3） |
| Open | `setMedia()` 内部行为与返回 UNKNOWN | Open 调用同步返回0，`OpenComplete(0)` |
| Prepare | 内部调用/回调/返回 UNKNOWN | `Prepare()` 异步发出，`PrepareComplete(-12017)`；音视频轨均存在并按既有规则继续 |
| 窗口创建与绑定 | `QVideoWidget` 后由 Qt 创建 `S60VideoWidget`；`setVideoOutput()` 的内部真实绑定 API/返回 UNKNOWN | 单独 native-video `QWidget/RWindow`；`AddDisplayWindowL` 返回0，随后 geometry、rotation、scale/extent/clip 均返回0 |
| Play | `QMediaPlayer::play()`；状态 Playing/Buffered | `CVideoPlayerUtility2::Play()` 返回0，position 前进 |

这些成功返回、track/available 和 position 都不是首帧证据。旧 R 的 PP 零统计属于播放 Utility 创建前
的 DevVideo 枚举；本轮 B 同样没有正式 renderer 的 Received/Displayed/Skipped 计数。A 的 Qt 内部
PP 实例和统计仍为 UNKNOWN。

### 源码固定点与构建验证

- 基础 HEAD：`fc9a0ead684f0e2b32b2e55ac87f161522be2ca5`；配置：Debug / `e7backendab1`；
  `Symbian3Qt474`、Qt 4.7.4、GCCE 4.4.1 ARMv5 UDEB；UID3 `0xE000B153`。
- GCCE：`sbs errors: 0`、warnings 9；SIS 打包和签名成功。`dumpsis` 载荷 EXE 与冻结 EXE
  SHA-256 完全一致；map 中存在 `ProbeController::startPlayer()` 和
  `VideoPlaybackBackend::setMedia`。证书为 Qt Development Frameworks，有效期
  2026-08-24 至 2036-08-21。
- 冻结 EXE：1,916,810 bytes，SHA-256
  `A1C77C7890FE572B83F1C5CB4CA2C7FD40939E6AEF963D9616BB98B188A918CC`；符号文件 SHA-256
  `1D850BA07C4E55418CCDD4547DE60F9A7978584E326DABE29DD2910A55F93C46`；map SHA-256
  `AD86D51C05684F4D9E8D5AE21B636F657824211021BC5AB669DF45DD5B9BF03D`。
- 最终 SIS：`NIKINIKI_E7_BACKEND_AB_V1_debug.sis`，1,757,076 bytes，SHA-256
  `32B107D7EC3E12C146B25F3E50DECFF08C374EA99B63F6B4833717AE1FDD54C3`。
- 完整逐文件、MMP/Makefile、包和四份日志哈希保存在本地
  `symbian/out/e7-backend-ab/e7-backend-ab-v1/build-manifest.json`。`git diff --check`
  除既有 CRLF 提示外通过；生成 MMP 只含预定诊断宏。手机 `E:/sys/bin` 读回被 CODA Code -21
  拒绝，因此实装 EXE SHA-256 为 UNKNOWN；安装回执和四次 runtime identity 均匹配冻结包。

### 四次结果索引

逐次包身份、模式、PID、前序/退出、窗口对象与 geometry、调用返回、人工画面/声音、日志文件和
关键行只在[设备矩阵的四次结果卡](../../reference/DEVICE_TEST_MATRIX.md#e7-minimal-backend-ab-20260906)
维护。本轮顺序严格为 A→B→B→A，配对期间不重装、不重链接、不增加变量；预算已耗尽并停止。

## FACT、HYPOTHESIS、UNKNOWN

**FACT**：同一个新包的新 A 在首尾两次均连续有画有声；B 在中间两次均进入有效播放，内容区全黑、
系统栏可见且声音正常。B 的 Open、AddDisplayWindow、显示配置和 Play 返回0，Prepare 回调为
`-12017`，音视频轨存在，position 前进。相同 minimal-P 外层环境足以同时容纳成功 A 和失败 B。

**HYPOTHESIS**：主失败族位于 native MMF backend 与视频输出适配/目标建立的差异中，可能涉及
Utility/协商分支、绑定时机、native window/surface 交接或视频分支未交付有效帧。这是范围收窄，
不是已定位根因。

**UNKNOWN**：A 的实际 Symbian Mobility 分支、Utility、controller、NewL/Open/Prepare 和真实绑定
调用；两条链的第一个底层差异；B 是否产生过有效解码帧；renderer 实例与首帧；手机实装 EXE
哈希、boot identity/uptime；完整应用是否还存在额外窗口/方向/overlay/历史问题。

## 新证据改变了什么

| 新证据 | 支持的判断 | 不能推出的判断 |
|---|---|---|
| R 的 full/empty ABBA 都黑，empty 实际提交计数为零 | 非空首页 draw 不是这组黑屏的必要条件；降低 H1 与继续拆 rectangle/text/image 的优先级 | NanoVG 初始化、空 frame、PNG 预上传、窗口历史或播放器 overlay 全部无关 |
| P v1 未启动，P v2 clear 校准成功 | 第5次属于诊断入口故障；第6次重新建立成功 Qt 参照 | P-empty 已通过，或 clear/empty 已完成配对；这条边界仍未测 |
| C1-Solo/PP 均未取得 picture | 当前应用环境下公开 memory-output 合约没有建立，停止继续猜 clock/flag/PP 参数合理 | E7 硬件永久不可用，或它在成功 Qt 会话中也没有解码；Solo 仅排除了 MMF/FFmpeg，仍有真实应用的 QGL/资源环境 |
| C2 Open/注册可达，但 partial prepare、无 surface 回调且黑屏 | C2 方案未建成可用输出链；优先查看 prepare/输出目标建立的边界 | AddDisplayL 成功证明 decoder/renderer 正常，或所有 display-window 问题已被排除 |
| 成功 P 的退出日志有非零接收/显示统计 | 与人工可见画面共同支持该会话实际走过视频输出 | 能确定第一帧时刻、像素正确性，或可以把这份统计移给 R |

六次的数量完成了，最初设想的全部对照没有完成；不能为了“补齐清单”自动把 P-empty 排第一。
它保留为次级对照。源码已经明确空 frame 无主要 GLES draw，而新的结果更需要解释输出建立差异。

## 本次核查纠正的三个边界

**零统计必须按实例和阶段解释。** R 四次日志里的零统计位于 DevVideo post-processor 能力枚举期间，
在正式 MMF utility 创建之前，不能当作该播放会话没有帧的证明。C2 的零统计位于 Prepare 过程中、
`PrepareComplete(-12017)` 与 `AddDisplayL` 之前，不能描述为同一 renderer 播放90秒的累计测量。
尚无 PP 实例标识；初始化失败并提前清理是候选解释，不是已捕获的调用栈事实。

**C2 已经控制过更多条件。** 源码宏及运行日志共同确认 C2 跳过 DevVideo 枚举，采用文件名输入、
priority/preference 0，并实际打开 Helix `0x101F8514`。这组条件加 graphics-surface 仍失败，
不能再把“改用 Helix”“只换文件名”“关枚举”描述成尚未尝试的组合修复。它不逐项证伪每个因素
在其他输出路径中的作用；更不能把 C2 的 controller 身份追认给普通 R 或成功 P。

**C2 不等于完整复制成功 Qt 输出路径。** 上游 v1.2.0 本身就有 Utility2 与 legacy Utility 两个
编译分支；固定比较版还在 AddDisplayL 后保留 dummy display window、执行 pending window 更新。
本次 C2 刻意不调用 AddDisplayWindowL，和这些完整流程不同。该差异值得核对，但手机 Mobility 1.2.2
的实际分支仍未知，因此“补一个 dummy window”仅是候选，不能直接作为修复。
[v1.2.0 源码](https://raw.githubusercontent.com/qtproject/qt-mobility/v1.2.0/plugins/multimedia/symbian/mmf/mediaplayer/s60videoplayersession.cpp)、
[固定比较版源码](https://github.com/qtproject/qt-mobility/blob/169da60c8f657b3b61309c0a570d296107181411/plugins/multimedia/symbian/mmf/mediaplayer/s60videoplayersession.cpp)。

`-12017` 在 R 四次均出现，值得追究其视频分支为何未正常建立；但它仍不是通用致命错误，不改变
603 已验证的可恢复处理。`-44` 在 SDK 中是 KErrHardwareNotAvailable，只描述该操作的失败，
不证明硬件损坏或某机型普遍不支持。603 的软件帧率也不能替代 E7 软件路径的实测验收。

## 最小 backend A/B 结束时的假说排序（后续优先级见文末）

| 假说 | 当前信心 / 优先级 | 理由 |
|---|---|---|
| H2/H3/H6：native/EGL 输出建立、绑定时机、事件顺序 | High / 第一 | 同一 minimal-P 宿主中 A 2/2 有画、B 2/2 黑；差异已收窄到后端连同输出适配，但底层历史尚未对齐 |
| H5：视频分支未产生或交付有效帧 | Medium–High / 第一，与上一项并行区分 | B 重复 partial prepare 且无同会话 renderer/首帧证据；仍不能区分未出帧和出帧未呈现 |
| H8：输出控制/Utility/协商分支不同 | High / 第一 | 同包对照稳定分离；B 实测 Helix 仍黑，但 A controller UNKNOWN，不能把 controller 名称单独归因 |
| H9：窗口映射/背景 surface/裁剪 | Medium–High / 第一 | B 的独立 native child 可见且绑定成功仍黑；窗口/surface 交接仍是输出适配的一部分，不等于已排除 |
| H7：完整应用资源、加载顺序和组合 | Low / 降级为额外因素 | 最小 P 环境已稳定复现 B 黑屏，完整应用资源不是这次失败的必要条件；仍可能叠加其他问题 |
| H1：非空首页绘制遗留的普通 GLES state | Low / 继续降级 | 新 B 在 P clear 环境也稳定黑，且旧 ABBA 没有恢复；不优先做 sanitize |
| H4：完整应用全局状态/分配历史 | Low / 保留记录 | 本轮无需完整应用即可稳定分离；历史翻转仍未解释，但不再是当前首要范围 |

这里的优先级是下一步投入次序，不是根因概率。多项可能属于同一条初始化链。

## 已执行的四次对照契约

先整理两边真实调用契约，不消耗新的人工播放观察：确认 P 的输出 control/内部 native widget，
能取得时记录实际 Utility 类型、controller、加载模块身份；列出 NewL、输入重载、Open、Prepare、
Add/SetDisplayWindow、AddDisplay、Play 的顺序、参数和返回码。无法从现有日志/匹配符号确定的
字段保持 UNKNOWN，并列为下一次启动的稀疏取证目标；不反编译整个系统或替换手机插件。

**已按同一最小宿主、同一 EXE 的 Qt/native 对照执行四次：**

1. A：P 的成功 Qt/QVideoWidget 路径，新带开关包先校准。
2. B：同包、同样本、同一主 QGL/资源/宿主过程，改为 R 的正式 native backend 与普通
   AddDisplayWindow 路径；保留适配需要的独立 native video child，不暗中继承 C2 实验输出。
3. B：不重装，新进程重复。
4. A：不重装，回到 Qt 路径。

这里只在诊断包中通过参数选择后端；共享链接内容不等于同时启动两个媒体会话。冻结各分支的
窗口树，记录 Qt 内部 child 与 native child 的差异；不声称两者绑定了完全相同的 RWindow。
这组比较的是**后端连同输出适配**，用于划分调查范围，尚不能归因某一个 API。窗口位置、主 QGL、
资源、媒体和前序条件保持一致，不把正式横屏/overlay 一并加到 P。新包 A 若也失败，停止并处理
校准，不能拿旧 P 通过代替；意外失败计入这四次，不隐含追加。

它与旧“另一个 MMF probe 失败、Qt probe 成功”相比，新增信息是同包控制和当前正式适配在
已校准宿主里的表现，避免继续只比较两个相差很大的完整程序。每次记录同会话 Open/Prepare/
Play、真实绑定、关键错误和人工可见性；PP 统计按来源/阶段归属，不能为凑首帧证据移用旁路实例。

| 新对照结果 | 下一项最小工作 |
|---|---|
| Qt 可重复有画，native 可重复黑 | 优先比较两条 Utility/输出目标建立过程；在实际匹配的 Qt 分支上找首个差异，再单项复刻和反向验证。暂停首页资源细分 |
| 两者都可重复有画 | native 本身在当前 P 环境可工作，转真实应用的窗口/方向/overlay/初始化历史；一次移入一个因素。不能据此把 P 的尺寸直接当修复 |
| Qt 基线或同模式翻转 | 停止机制归因，记录前序/退出/boot/CODA，并在已有预算中复验，不追加参数矩阵 |
| native 未到播放、错误或窗口未准备 | 实验适配无效，先修主机上的适配问题；不记为黑屏机制证据 |

这四次不是原六次续号，现已完成且不补跑。P-empty 仍未执行；当前结果没有把问题指回 UI frame
housekeeping，因此不因它缺失而机械追加。

## 继续止损的范围

- C1 memory/PP、BD、Direct DevVideo/DSA 和全局 atlas 参数继续保持关闭/搁置，不靠不同 flag
  重新打开相同失败合约。C2 当前构建也不重复跑。
- 原路线把 C3 排在 C2 正向结果之后，是当时的实验管理门槛，不是 API 依赖：C2 没有出画不能
  排除其他 native-window 路径。若采纳上述新对照，单独按诊断方案评审；本次不自动启动原 C3。
- 精确取帧或 renderer hook 仍有价值，但必须属于同一失败会话。GetFrameL 会干预播放，仅作为
  单独、计入预算的后续诊断；不以新 DevVideo 会话的成败替代正式 MMF 首帧。
- 成功必须最终回到完整 NIKINIKI、正式横屏与完整功能，独立菜单 Release 启动，并完成 E7/N8
  各自验收及603回归。现阶段不解除1.2的不支持公告，也不声称已经得到修复。

## 四次对照后的源码与 SDK 差异核查

本节是主机静态分析，没有新增设备观察，也没有改变正式播放逻辑。当前源码只能证明 B 的实现
与上游候选实现不同，不能证明 E7 已安装 Mobility 1.2.2 必然执行该候选分支。

| 环节 | 当前 B / 已关闭 C2 | Qt 源码候选 | 解释边界 |
|---|---|---|---|
| 首次显示绑定 | B 在 OpenComplete 内 AddDisplayWindowL、configureDisplay，然后 Prepare | v1.2.0 与固定比较版均在 PrepareComplete 内 applyPendingChanges(true) | 这是可直接隔离的调用时机差异；B 在 Open 完成后绑定并不因此成为非法 API 用法 |
| graphics-surface 注册 | B 无 AddDisplayL；C2 在 PrepareComplete 后调用，刻意不加 display window | 固定比较版 AddDisplayL 后添加空 extent/clip 的 dummy window，再更新真实输出 | C2 失败没有检验完整 Qt 合约；AddDisplayL 也不是所有 Utility2 窗口播放都必须调用的通用修复 |
| 窗口交接 | B 使用自己的普通 native child | applyPendingChanges 根据当前 windowHandle 先加入新窗口，成功后移除旧窗口，再处理 extent/clip、scale、rotation | 需要记录真实 RWindow 的代次和 Add/Remove 顺序，QWidget 指针稳定不足以证明窗口稳定 |
| 防止 surface 丢失 | B 没有保留 dummy display window | 固定比较版注释明确：native→EGL 切换时，避免移除窗口使 MMF 销毁 surface | 未捕获切换或 RemoveDisplayWindow 前，不能认定首次黑屏是缺少 dummy；它的已知目的不是强制 decoder 产帧 |
| 原生绘制 | B child 使用 WA_NativeWindow、WA_NoSystemBackground、关闭 autoFillBackground | 固定比较版 S60VideoWidget 在 graphics-surface 模式调用 setNativePaintMode(Disable)，同时有 _q_DummyWindowSurface 和窗口事件处理 | 普通 QWidget 属性不能代替该私有绘制契约；作用是否启用还取决于 USE_PRIVATE_QTGUI_APIS。_q_DummyWindowSurface 与 MMF dummy RWindow 是两个不同机制 |

代码定位：产品 `symbian/source/platform/video_playback_backend.cpp` 的 MvpuoOpenComplete /
MvpuoPrepareComplete；探针 `symbian/probes/e7-qt-window/ProbeController.cpp` 的 B child 创建；
Qt 比较版的 S60VideoPlayerSession::MvpuoPrepareComplete / applyPendingChanges、
S60VideoWidget::setPaintingEnabled 与 S60VideoWidgetDisplay::eventFilter。
[固定比较版 session](https://github.com/qtproject/qt-mobility/blob/169da60c8f657b3b61309c0a570d296107181411/plugins/multimedia/symbian/mmf/mediaplayer/s60videoplayersession.cpp)、
[固定比较版 widget](https://github.com/qtproject/qt-mobility/blob/169da60c8f657b3b61309c0a570d296107181411/plugins/multimedia/symbian/videooutput/s60videowidget.cpp)。
本地只读缓存位于 `.tmp/e7-root-cause-audit/upstream/`；v1.2.0 的 videooutpututils 显示
setNativePaintMode 经 QWidgetPrivate 修改原生绘制模式，并受私有 API 宏控制；不能把该版本的
实现细节无条件回填到手机二进制。

SDK `Symbian3Qt474/epoc32/include/platform/surfaceeventhandler.h` 明确 AddDisplayL 的 observer
服务于 window-less surface 客户端：MMF 仍拥有 surface，回调前已向 Window Server 注册。
因此注册成功、surface-created、窗口附着成功、显示有效视频帧必须分别记录。
`epoc32/include/w32std.h` 还提供只读 `RWindow::GetBackgroundSurface(TSurfaceConfiguration&) const`，
可作为下一诊断包的稀疏查询候选，无需为观察普通 B 而先改变它的 AddDisplayL 注册行为。
查询成功只证明该窗口存在背景 surface 配置，不证明来源是 MMF 或含有效帧；A 若实际走 EGL，
视频 child 上没有 MMF 背景 surface 也可能正常。SDK 声明不代替设备导出及查询行为验证。

## 绑定时机试验的原建议与实施记录（已执行）

四次真机契约已经结束。下一步先在主机上匹配手机实际安装的 Qt Mobility 输出分支，并以
只读/稀疏取证补齐 A 的 Utility 类型、controller、NewL priority/preference、Open 重载、Prepare 和
精确 child/RWindow 绑定顺序；逐项与 B 对齐。优先稀疏记录 Prepare 回调、真实窗口代次、
背景 surface 查询结果，以及已注册 observer 路径上的 created/removed；不把添加 observer
本身当作完全无干预的观察。

2026-09-06 已尝试通过 USB CODA 只读获取设备 ROM 的
`Z:/sys/bin/qtmultimediakit_mmfengine.dll`，`FileSystem.open` 返回 Code -21；实际 Mobility 分支
继续为 UNKNOWN。随后按下述后备路线完成 `e7bindtiming1` 主机实现：同一 EXE 保留 A、原 B 和
候选 C，C 只后移 `AddDisplayWindowL + configureDisplay`，并给三模式加入一致的只读背景 surface
查询。GCCE 为0 errors / 9 warnings，当前证书重签、payload 与冻结 EXE 核对均通过；尚未把这组
主机构建记成真机结果。运行契约见
[诊断方案](../../reference/E7_BLACK_SCREEN_DIAGNOSTIC_PLAN_ZH.md#e7-bind-timing-20260906)。

若运行库匹配暂时做不到，不必因此停止所有推进：首个可独立验证的候选是仅把 B 的首次
AddDisplayWindowL 与配套 configureDisplay 从 OpenComplete 移到可接受的 PrepareComplete
之后、Play 之前，其余 Utility、输入、priority/preference、child、资源均保留。这是绑定与
配置阶段整体移动，不能进一步声称只改变一条 API。必须放在新诊断开关下，并检查期间的
resize/configure 回调没有偷偷提前绑定；保留同 EXE 原 B 与成功 A 校准，另定新观察契约。
它检验候选时机的因果作用，不以手机已执行某一 Qt 分支为前提。

若该项仍黑，下一候选才是独立比较 AddDisplayL / dummy 保留及原生绘制契约。若选择先完整复刻
Qt 输出契约以寻找 native 正向基线，须明确这是多因素组合试验；一旦出画，必须撤回单项做反向
验证，不能把组合成功写成 dummy 根因。已有四次预算不自动扩展，本次仅完成静态比较。

在此之前不重开 C1/C2、BD/DSA，不加入 sanitize、atlas、延时、decoder 策略或完整应用因素，
也不把 B 的 controller 身份回填给 A。完整 NIKINIKI 的窗口、方向、overlay 与初始化历史仍是
后续回归范围，但已降级为“可能的额外贡献者”，不是复现本轮 B 黑屏的必要条件。

<a id="e7-bind-timing-result-20260906"></a>
## Prepare 后绑定时机试验结论

本节是上节最小建议的执行结果。逐次设备身份、PID、窗口、日志行和人工反馈只在
[设备矩阵](../../reference/DEVICE_TEST_MATRIX.md#e7-bind-timing-observations-20260906)维护；这里记录实现、
证据解释与后续决策。

### 实际实现、冻结点与偏离

- 新增显式诊断 CONFIG `e7bindtiming1`，同一 EXE 要求 runtime switch
  `--e7-backend=A|B|C`；每个进程只创建一种后端。A 保留 Qt/QVideoWidget 校准，B 保留
  OpenComplete 绑定，C 只把同一 `AddDisplayWindowL + configureDisplay` 整体后移至可接受的
  PrepareComplete 后、Play 前。
- C 没有加入 AddDisplayL、dummy RWindow、私有 native paint、sanitize、atlas、额外延时、输入、
  decoder、Utility、priority/preference 或 child 改动。三模式只额外做相同的只读
  `RWindow::GetBackgroundSurface` 查询；查询结果不作为首帧。
- 普通播放器行为不变：C 逻辑只在
  `WILIWILI_ENABLE_E7_DELAYED_DISPLAY_BIND_DIAGNOSTIC` 下编译，CONFIG 只由诊断 probe 工程加入。
- 冻结基础仍为 dirty 工作树上的 HEAD
  `fc9a0ead684f0e2b32b2e55ac87f161522be2ca5`，分支
  `codex/e7-minimal-backend-ab-20260906`；为保留用户修改没有建立混合提交，以逐文件 SHA-256
  固定源码。构建为 Debug / Symbian3Qt474 / Qt 4.7.4 / GCCE 4.4.1 ARMv5 UDEB，0 errors、
  9 warnings；SIS 打包、当前证书签名、payload 与冻结 EXE 一致性和 map 符号核对通过。
- 冻结目录为 `symbian/out/e7-bind-timing/e7-bind-timing-v1/`；EXE SHA-256 为
  `915E8B095B3BEBF40FC721EBE4E83A16BBC76B700957721939728555C3A19481`，签名 SIS SHA-256 为
  `2F6C7BC3CE7A3DF9D99A8C190110F58DC3778507C2768FF35F7DF2E60F595D3A`。完整源码、MMP、Makefile、
  symbol/map 与本地日志哈希见该目录 `build-manifest.json`。手机 EXE 无法读回，实装哈希 UNKNOWN；
  有效进程的 runtime identity 与冻结构建一致。
- 预定 A → C → B → C 中，第3次 B 的主机启动参数写法错误，在设备进程创建前即失败。依照事先
  规则仍计一次，未补跑；因此这组没有取得当前包 B 的有效负控制。这是实质性方法偏离，不能用
  上一包的 B 代填。

### FACT、HYPOTHESIS、UNKNOWN

**FACT**：A 校准进入有效播放并由人工确认有画有声。完全相同的 C 在两个新进程中均完成
Open、延后绑定、显示配置和 Play，position 均持续前进；但第一次 C 人工确认为无画有声，第二次
C 为有画有声且视频窗口外背景变白。第一次 C 的 PrepareComplete 为 `-12017`，第二次为0；两次
的 AddDisplayWindow 与配置调用都成功。第3次 B 没有设备进程，不能产生 B 的播放事实。

**FACT**：C 在 PrepareComplete 回调内部、收到 error 参数之后执行绑定；不是回调返回之后。
该次后续绑定不可能倒推改变已收到的 error。不过，相对 B，C 同时移除了 Prepare 前的绑定，
这个前置状态变化仍可能影响 Prepare；不能据回调先后排除全部绑定时机作用。
现有同模式翻转证明它没有建立稳定
结果。成功 C 同时证明：这条产品 native MMF backend 加普通 native child 的组合在 minimal-P
环境中至少一次可以显示视频；完整应用环境既不是先前 B 黑屏的必要条件，也不是 native 成功的
必要条件。

**FACT**：逐消息归一化比较到 PrepareComplete 为止，除进程元数据外，两次 C 的首个可见语义差异
不是窗口、Open 或应用发出的 Prepare 序列，而是失败侧在 `Prepare()` 异步调用返回后、回调前多出
一组无 Qt 前缀的 PP 零统计。失败侧 `obs02-C` 的 Prepare 调用返回在427行（应用
`elapsed-ms=2938`，主机接收 `ms=3984`），第二组统计在429～434行（只有主机接收
`ms=7312～7375`），`PrepareComplete(-12017)` 在441～442行（应用 `6387`，主机 `7406`）。成功侧
`obs04-C` 的调用返回同为427行（应用 `2951`，主机 `3937`），没有第二组统计，回调0在435～436行
（应用 `6432`，主机 `7390`）。两侧应用计时的调用返回至回调分别约3449/3481 ms，不支持只在失败侧
出现一个明显更长的固定超时。

**HYPOTHESIS**：第二组统计可能来自 Prepare 视频分支创建后又清理的 PP 实例，而成功 C 可能保留了
该实例，所以同一阶段没有析构/汇总输出。这一解释比“延后绑定已经修复”更贴近最早分叉，但仍只是
相关性：三次已观察的 native 黑屏（旧包两次 B、本包一次 C）都在 `-12017` 前出现第二组，成功 C
没有；样本数小，且统计没有实例身份。`-12017` 本身导致黑屏、PP 初始化失败、资源残留或厂商竞态
均未被分别证明。纯粹“OpenComplete 绑定太早”继续降级；白色外框作为次级窗口/覆盖问题分开处理。

**UNKNOWN**：第二组统计的模块、发出函数、线程、PP UID、实例、PID 和媒体会话归属；主机接收次序
是否等于底层执行次序；两次 C 的 Prepare 结果为何不同；设备 uptime、系统媒体服务和上一进程资源
状态；当前包 B 在同一阶段会怎样；A 的实际 Utility/controller/NewL/Open/Prepare/真实绑定；实际
加载的 Mobility 插件；两次 C 的首帧时刻和 renderer 帧统计；`GetBackgroundSurface(-1/null)` 的
目标语义；完整应用是否还有附加窗口/方向/overlay 问题。

### 对照是否成立与下一项最小建议

该试验的 A 校准成立，C 路径也两次进入有效播放，但**绑定时机因果对照不成立**：同一 C 翻转，
且本包 B 负控制缺失。按预设规则必须停止增加变量；不能宣称 C 修复黑屏，也不能推进到正式播放。
原先“native MMF/输出适配而非完整应用”这一范围判断仍成立，但内部优先级从单纯输出绑定时机移向
Prepare 视频分支及跨进程生命周期/资源状态；完整应用环境继续只作为可能的附加因素。候选 DLL 的
有限静态追踪已经完成，结论见下节；不再以“先反编译整个插件”为下一动作。

后续获准的[冷/暖生命周期配对](../../reference/E7_BLACK_SCREEN_DIAGNOSTIC_PLAN_ZH.md#e7-c-lifecycle-pair)
已执行完成；结果与解释见本文“冷/暖生命周期配对结论”。它没有进入 AddDisplayL/dummy/private
paint，也没有把任何运行补记为旧预算第五次。

<a id="e7-mobility-binary-match-20260906"></a>
## E7 Mobility 输出插件候选副本静态匹配

本节完成上段约定的主机工作，不包含设备启动、安装或替换。原始 DLL 与工具输出仅保存在本地
忽略目录；设备矩阵只记录所提供文件的身份事实和来源限制。

### FACT

- 用户提供的候选 `qtmultimediakit_mmfengine.dll` 为 ARMv5 E32 DLL，59,797 bytes，SHA-256
  `44EE2383965365B40D35CEE9EB0FB8D5C9A7A18F0343533A3CE953C87B8D86B4`，SID/UID3
  `0x2002AC76`，非 debug、BYTEPAIR 压缩、VFPv2、paged code/data；E32 header 显示26个 DLL
  引用、2个导出。它链接 `QtMultimediaKit{00010202}[2002ac77].dll`；`elf2e32` 把对应 module
  version 显示为 `1.514`，本文不据此另造产品版本号。
- 该 DLL 导入 `libEGL` 5项、`libGLESv2` 4项、`libOpenVG` 1项和 `ws32` 16项。它从
  `mediaclientvideo{000a0001}` 导入31项，其中 SDK ordinal 映射明确包含
  `CVideoPlayerUtility2::NewL`、`AddDisplayWindowL`、`RemoveDisplayWindow`、
  `SetVideoExtentL`、`SetWindowClipRectL`、按窗口参数的 `SetScaleFactorL` / `SetRotationL`，以及
  ordinal 156 `AddDisplayL(RWsSession&, int, MMMFSurfaceEventHandler&)`；同时包含基类的
  OpenFile/OpenUrl、Prepare、Play、轨道与 position 查询。导入存在证明该二进制具备调用能力，
  不证明某次媒体会话实际执行过该调用。
- 同一 SDK 的 UREL 插件是52,553 bytes，SHA-256
  `F4B4D99C90CF6C71186AAC7AFC1E10A90A357D03E24A1A9B0EC891C193B31F50`，链接
  `QtMultimediaKit{00010201}`，只有24个 DLL 引用、30项 `mediaclientvideo` 和8项 `ws32`；其导入
  表没有 ordinal 156，也没有 EGL/GLESv2/OpenVG 引用。故所提供 DLL **不是**本机
  Symbian3Qt474 SDK 中这份 mmfengine 二进制，并且至少编入了 SDK 版本没有的 graphics-surface/
  EGL 能力。
- 从 `elf2e32` 解出的 code/rodata 中可以直接找到 `_q_DummyWindowSurface`、
  `S60VideoPlayerSession`、`S60VideoWidget`、`setPaintingEnabled` 和 `eglCreateEndpointNOK` 字符串。
  这证明相关组件/诊断文本存在于候选二进制，不证明这些路径在 A 会话中被执行。
- 精确字符串 `Statistics of Post Processor` 在候选 DLL、解压 code（ASCII/UTF-16）和两份缓存 Qt
  Mobility 源码中均不存在；它也不是应用的 `qDebug` 文本。现有材料不能把该统计归给 Qt 插件。
- Qt Mobility 候选源码的 graphics-surface 构建会在 PrepareComplete 注册 `AddDisplayL`，加入
  一个零矩形 dummy display window，再由 `applyPendingChanges(true)` 绑定真实窗口并设置
  extent/clip/scale/rotation；videooutput 构建文件同时链接 EGL，并在 Qt 配置允许时加入 OpenGL/
  OpenVG。候选 DLL 的静态导入集合与这种构建轮廓相符。
- 候选源码还暴露了两个更早的合约差异：Qt 的 graphics-surface 路径用 priority 0 /
  `EMdaPriorityPreferenceNone`（SDK 值0）创建 Utility2，并以**文件名 + 显式 `KHelixUID`**调用
  `OpenFileL`；冻结 C 用 priority 0 / `EMdaPriorityPreferenceTimeAndQuality`（日志值3），并以
  **shared `RFile` + resolver**打开，随后虽同样选到 Real/Helix
  controller，NewL/Open 合约与建立历史仍不相同。旧 C2 已同时使用文件名、显式 Helix、priority/
  preference 0并跳过 DevVideo 枚举，仍然失败，所以这些差异不是已证明的充分修复；它们只是当前 C
  与候选 Qt 在 Prepare 前首个已知静态合约差异组。
- 第一组 PP 零统计的运行时接收位置可被应用源码括住：`GetPostProcessorListL` 后打印列表，再对 UID
  调 `PostProcessorInfoLC`；统计出现在列表标记之后、应用打印该 PP 信息之前。应用随后删除 info、
  关闭数组、删除 probe，再创建正式 Utility。故第一组属于独立枚举阶段这一点可信；它仍不揭示
  统计文本的具体发出函数。第二组发生时独立 probe 按应用顺序已删除，但仅凭主机接收顺序不能证明
  它来自正式 renderer。

### HYPOTHESIS 与解释升降级

**HYPOTHESIS（能力轮廓升级，因果未升级）**：候选插件编入 graphics-surface/EGL 支持的证据增强，
但这不等于“缺少完整 Qt 合约导致黑屏”的证据增强。C 没有添加 AddDisplayL、dummy 或 private
paint 仍曾出画，已经反驳“应用必须增加这些调用才能 native 出画”的绝对命题。Qt 的这些操作位于
已收到 PrepareComplete 之后，不能解释同一 C 为什么先得到0或 `-12017`；更早的 Open 合约和未记录
生命周期状态当前优先于回调后的 surface 组合。

**HYPOTHESIS（降级）**：仅把 `AddDisplayWindowL + configureDisplay` 从 OpenComplete 后移到
PrepareComplete 后不足以代表 Qt 路径。C 同模式已经出现一次黑、一次有画，二进制匹配又显示 Qt
插件具备额外 surface/EGL 能力，因此不能再把“绑定过早”当作单一首因，也不能把成功 C 当成 Qt
合约的等价复刻。

### UNKNOWN

- CODA 无法读回受保护的 ROM 文件；用户所提供副本是否正是 A 运行时加载的文件、加载盘符和
  ROM/补丁覆盖关系仍未独立核实。
- 静态导入不能给出 A 的实际 Utility 实例、controller、priority/preference、Open 重载、
  Prepare 返回，也不能证明本次 `QVideoWidget` 会话实际调用了 AddDisplayL、dummy window、
  EGL endpoint 或 private native-paint 分支。
- 候选二进制的准确源码提交、厂商补丁和编译宏尚未匹配。最初仅把 BYTEPAIR code 段提取为
  `0x14C08` bytes 的 Thumb 反汇编，不能凭字符串证明调用 xref；后续已用 `.ARM.exidx`、veneer
  ordinal、本地 DSO 导出和控制流有限恢复 Prepare/apply/Play，见
  [内部会话观测映射](E7_QT_INTERNAL_SESSION_OBSERVATION_ZH.md)。该恢复只证明候选静态链；候选与
  设备实际加载模块的关联仍为 UNKNOWN。
- 要确认统计发出者，缺少 E7 的 PP UID `0x10273417` 实现 DLL（或 ECom implementation→DLL 映射）
  及其符号/源码，或者带实例标识的动态模块跟踪。候选插件仍缺少匹配的未剥离 map/symbol 或准确
  源码提交；但当前固定问题所需的关键 import ordinal 已有限恢复，无需扩大为全插件反编译。剩余
  阻塞是设备端 module/fingerprint 关联与 ROM breakpoint/register/memory 实际能力，不能由候选
  静态分析代替。
- 哪个缺失环节会使 B 黑屏仍未知；C 的 Prepare 0 / `-12017` 翻转与 graphics-surface 合约之间
  是否有关也未知。

### 对照结论（生命周期试验前判断）

静态结果加强了原范围结论：**完整应用环境不是复现 B 黑屏所必需。** 当时应先控制跨进程生命周期
并复现 Prepare 分支；该有界实验现已完成，见下节。当前最早的既有失败/成功差异仍位于 Prepare
区间，候选 Qt 的已知回调后 `AddDisplayL/dummy/real-window` 顺序晚于这个分叉。静态材料没有把根因
缩小到 AddDisplayL、dummy、EGL、private paint、shared `RFile` 或 resolver 中任何单项，也没有形成
正式修复。

### 综合评估：Prepare 分叉优先，surface 合约作为稳定性候选

离线复核新增的 PP 统计阶段差异见[设备矩阵](../../reference/DEVICE_TEST_MATRIX.md#e7-bind-timing-observations-20260906)：
失败 C 在 Prepare 期间多出一组零统计，成功 C 没有对应块。它与随后 error 分叉共同提示 Prepare
内部可能有一次失败分支清理；但缺少实例和调用栈，不能确认析构、decoder 初始化失败或统计归属，
也不能排除延迟送达的其他实例输出。字符串追踪已排除“文本直接位于候选 Qt 插件/缓存 Qt 源码”，
但在缺少 PP 实现模块或实例跟踪时不能继续定位调用者。

两次 C 的应用 elapsed-ms 从 Prepare enter 到回调分别约3476 / 3507 ms，均约3.5秒。
这不足以支持仅失败侧发生固定超时，也不给新增经验延时提供依据。B/C 的背景 surface 查询只在
Open/Prepare/Play 前进行；成功 C 同样返回 -1/null，故该指标目前不能区分出画与黑屏，更不能
证明播放期间没有 surface。白色外框单列为呈现/覆盖问题，暂不与视频初始化失败一起修。

<a id="e7-c-lifecycle-pair-result-20260906"></a>
## 冷/暖生命周期配对结论

逐次设备事实、人工反馈、日志行与哈希只在
[设备矩阵](../../reference/DEVICE_TEST_MATRIX.md#e7-c-lifecycle-pair-observations-20260906)维护。本节只更新解释和
后续决策；冻结 C、样本、安装与正式播放行为均未改变。

### FACT

- cold-1 → warm-1 → cold-2 → warm-2 四次均进入有效播放，PID 依次为711、747、703、745；两次
  cold 前由用户明确重启，两次 warm 前固定 CODA terminate 并确认目标 PID 消失。设备 uptime / boot
  identity 取不到，仍为 UNKNOWN。
- 四次均为相同 Utility2、priority/preference、shared `RFile` resolver、Real controller、窗口 geometry
  和 generation 1 绑定；OpenComplete=0、PrepareComplete=0、AddDisplayWindow/configure/Play 成功，
  position 前进。用户四次均确认完整运动画面、声音正常、视频外背景白色。
- 四份日志均只有正式 Utility 创建前的枚举阶段 PP 统计，Prepare 返回至回调区间都没有失败 C 曾有的
  第二组。统计仍无实例/PID/会话标识，不能归给 renderer，也不能作为首帧计数。
- 四次上限已经用尽，最终 terminate 后 inspect 确认目标进程为空；没有第五次启动，也没有加入
  AddDisplayL、dummy、private paint、延时、重试、忽略错误或新 decoder 策略。

### HYPOTHESIS 更新

本组命中“四次相同”，所以“重启后首次 C 与 terminate 后紧邻 C 会稳定落入相反 Prepare 分支”的
生命周期二分假说被**降级**。它不排除更细的厂商服务竞态、资源压力或未记录前序状态，只说明这套
cold/warm 操作没有复现它。此前第二组 PP 块与 `-12017`/黑屏的相关性没有被反例推翻，但本组没有
失败样本，故也没有把它升级成因果或正式 renderer 归属。四次 C 成功增强了“普通 native window 路径
本身可用”的事实范围；仍不能把 Prepare 后绑定称为已修复，因为同一 C 先前确有失败且当前包 B
负控制至今未有效执行。

### 当时的 UNKNOWN 与已取代建议

此前 C 为什么进入 `-12017`/黑屏、PP 第二组的实际模块与实例、terminate 后媒体服务是否完全复位、
设备实际 Qt 插件分支、首帧和 renderer 帧统计仍为 UNKNOWN。白色外背景在五次成功 C 观察中一致，
是独立呈现/覆盖现象，不并入视频初始化根因。

当时若另行批准下一轮，建议优先补齐**当前冻结 EXE 中缺失的 B/C 绑定时机负控制**，而不是立刻叠加完整 Qt
surface 合约。当时的单一研究问题是：同一 `e7bindtiming1` 中，仅“OpenComplete 后、Prepare 前绑定”的 B
与“PrepareComplete 后、Play 前绑定”的 C，是否稳定改变 Prepare 结果、第二组 PP 标记和人工画面。
保持 SIS/EXE、样本、Utility/controller、native child、方向、启动与 terminate 方式不变；使用已验证的
joined runtime 参数，固定最多四次新进程，顺序 B → C → C → B，期间不重启、不重装、不运行其他
媒体应用。每次确认 PID 消失；无效启动也计数。

只有 B 两次一致 `-12017`/第二组 PP/黑而 C 两次一致0/无第二组/有画，才提升 Prepare 前绑定这一
变量；两模式都成功则降低它并转向完整应用窗口/方向/overlay；任一同模式翻转即停止归因；B 未进入
有效 Open/Prepare/Play 只算适配失败。四次后无条件停止，不补跑、不自动进入 surface/dummy。完整 Qt
输出合约仍只是后备组合诊断；若日后采用并出画，必须在独立预算中反向撤除单项，不能宣称 dummy
根因。该建议已被下方2026-09-07完整应用 Qt 同媒体结果取代，不再是当前待执行项。

## 完整应用兼容候选首次反馈评估

本节对应2026-09-07完整应用首次兼容实践。唯一设备事实记录见
[设备矩阵](../../reference/DEVICE_TEST_MATRIX.md#e7-full-app-compat-first-20260907)；这里只解释边界与下一步。

**兼容实现已执行，本次效果失败。** 上层与MMF均锁定开启，OpenComplete延后首次绑定，
PrepareComplete收到-12017后才执行绑定与配置，随后Play。没有证据支持开关未生效或首次绑定
仍发生在Prepare前；也没有本次错误路径触发兼容绑定状态保护的记录。session0的active=false
只是媒体会话锁定前的偏好日志，不能脱离后续session1解释。

**失败阶段与旧黑屏C相似。** 第二组无归属PP零统计先于PrepareComplete(-12017)，后续绑定和
配置成功仍黑。这增强了相同失败标记在不同宿主下出现的观察范围，仍不证明同一PP实例、同一
底层根因或零有效解码帧。只改绑定时机不足以覆盖这次实际点播场景；目前不能作为已验证兼容修复
扩大启用，也不应通过强制忽略-12017、自动重试或延时掩盖结果。

**这不是只增加完整应用窗口的单因素移植。** 本次实际解析视频640×360，页面1920×1080元数据
不能用于判断硬解超限。下载完成字节数与样本A不同，媒体内容未按SHA对齐；同时采用自动header
preflight、共享增长缓存、真实640×360横屏与overlay。缓存下载在OpenComplete前已结束，因此
不能简单认定Prepare失败是因为当时文件仍在增长；Open起点的解析历史仍有待控制。preflight
接受只证明header接受，不保证后续新MMF会话完成视频初始化。

**原拟进行的同媒体、完整本地输入桥接对照已取消并封存。** 当时的执行条件是保持完整应用兼容
开关开启、横屏、QGL与overlay，使用与成功最小C相同且哈希核实的完整样本A，并以冻结最小C校准；
这些条件仅保留作历史设计，不再构成待执行清单。对应诊断CONFIG及主机产物已经从产品树移除，
未安装、未启动，也没有设备结论。

当前同包B/C时机对照仍是研究线的独立历史缺口，不能用这次不同媒体的完整应用结果补作B/C结果。
兼容开关已从主线移除，N8与603状态不由这次E7反馈改变；初代设备不支持公告维持不变。若未来
重新开启，必须取得新的明确授权并重新定义单变量预算，不能沿用本节作为现行计划。

<a id="e7-full-app-qt-same-media-result-20260907"></a>
## 完整应用 Qt 同媒体对照结论

逐次设备事实、身份、日志行与哈希只在
[设备矩阵](../../reference/DEVICE_TEST_MATRIX.md#e7-full-app-qt-same-media-20260907)维护。本节收敛解释和
唯一方向，不覆盖上文 native B/C、C1/C2 或旧兼容候选的历史事实。

### 已证明

- 冻结 `e7bindtiming1` A 与当前完整应用 Qt 使用相同完整样本 A、同一设备运行库且 UID 可共存；
  参照未重编译，完整应用从真实首页进入正式播放器。顺序 A/完整/完整/A，四次均为新进程，期间
  不重装、不换媒体、不运行其他媒体应用。
- 人工结果为可见/黑屏/黑屏/可见：两次 A 均连续运动、有声、完整、正常黑背景；两次完整应用均
  黑屏有声且播放器 UI 正常。参照没有同模式翻转。
- 两次完整应用均实际选择 Qt 后端，创建并显示640×360 `QVideoWidget` 与内部 `S60VideoWidget`；
  `setVideoOutput`、`setMedia(file URL)`、`play` 的公开调用已经返回，音视频轨被识别，状态到
  Playing/Buffered 且 error 0。约10秒前的已记录 position 均为0；之后因 logger 只按
  state/status/error/available 变化输出而为 UNKNOWN。两次 A 的定时采样则推进到约51.8秒。
- 最早可证明的运行分界因此在公开 Play 请求返回后、约8～10秒时首次已记录 position 推进/人工
  可见帧之前。
  这把优先级放在当前 Qt 会话/视频初始化链，不支持直接跳到 overlay、QGL、surface 或驱动修复。
- 诊断为固定 Qt/完整本地输入而用全硬解跳过 preflight；这是与真实自动点播的显式差异，不是产品
  修复。原契约要求 Play 后60秒，但采集器实际从进程启动计时，四次主日志只证明约57～58秒的
  Play 后窗口；预算已耗尽且没有补第5次。

### 两侧实际差异

| 项目 | 冻结最小 Qt A | 当前完整应用 Qt 诊断 |
|---|---|---|
| 对象创建 | 每个进程创建 `QMediaPlayer`、`QVideoWidget` 后直接播放；player parent 为 `ProbeController` | 真实首页先创建正式顶层 `VideoPlayerWidget`、native video host、soft surface 与持久 overlay；物理横屏确认后按需创建 backend/player/output；player parent 为 native video host |
| 输出父子/尺寸 | `QVideoWidget` 是最小 host 子控件；AVKON 工作区约360×554，最小 P/QGL 宿主 | `QVideoWidget` 是正式 native video host 的子控件；播放器顶层、host 与 output 为物理640×360，主 QGL、overlay、controls 保留 |
| 调用顺序 | 单次 `setVideoOutput` → `setMedia(file)` → `setVolume(80)` → `play` | 创建阶段 `setVideoOutput` → volume/rate；紧接 setMedia 阶段再次对同一 output 执行 `setVideoOutput` → volume/rate → `setMedia(file)`；随后 `play` |
| preflight/输入 | 无产品 preflight；直接完整本地 file URL | 全硬解诊断性跳过 preflight；同一完整本地 file URL，不经下载或增长缓存 |
| 事件/观测 | 直接连接 Qt state/status/error/available/duration 信号并每2秒取样 | 复用产品轮询、控件和窗口/前后台事件；公开层无 Prepare/自然首帧事件，均记 UNKNOWN |
| 资源与层级 | 最小 QGL/P 资源；没有真实首页、正式横屏顶层或产品 overlay/control 对象图 | 真实首页 NanoVG/QGL、640×360正式顶层、Qt output、ARGB overlay、控制及输入均存在 |
| 生命周期/退出 | 对象只活到本次新进程；CODA terminate | 产品设计为 controller/backend/Qt output 跨会话复用、正常 Back 只 stop/解绑/隐藏；本组为对齐新进程也使用 CODA terminate，未验证正常返回/复用 |

已有实验对窗口、QGL/NanoVG 资源、native B/C 绑定、C1/C2 和 atlas/sanitize 的结果只沿用既有证据；
本组没有重新运行它们，也没有把其结论套到 Qt 失败。

### 推测与未知

- position 0 与稳定黑屏相关，但 position、available、Buffered 都不是首帧证据；人工听到声音也
  不能反推视频 Prepare 或 decoder 初始化成功。Qt 公开层没有 Prepare 事件，四次均为 UNKNOWN。
- 完整应用首会话在对象创建时先 `setVideoOutput` 并设置 volume/rate，随后 `setMedia` 路径又对同一
  output 重复绑定并重复设置；冻结 A 是单次 `setVideoOutput → setMedia → setVolume → play`。
  这是当前 Qt 链最早的静态事务差异之一，但本组没有隔离它，不能直接宣布删除重复调用即可修复。
- 两次完整应用各有一组 Prepare 时间附近的无归属 PP 0/0/0/0 统计。没有模块、实例、PID或会话
  标识，不能归给 renderer，也不能证明没有有效视频帧。
- 实际 `S60VideoPlayerSession` 的 Utility/controller/Open 重载、Prepare error、
  `applyPendingChanges(true)`、AddDisplayL/AddDisplayWindow、dummy/endpoint 与自然首帧仍 UNKNOWN；
  插件导入相关 API 或包含相关字符串不证明本次执行。

### 内部取证结果与后续入口

后续四次新进程已按 A/完整/完整/A 完成相同内部断点观测。实际加载
`qtmultimediakit_mmfengine.dll` 的基址均为 `0x7BA40000`，三个 ROM code fingerprint 均匹配候选；
Prepare、`applyPendingChanges(true)` 返回点和实际 MMF Play 在四次均命中同一 session，pending
flags 均为 `3→7→0`、真实 output window 均从 null 变为非 null，apply 前路径 TRAP error 均为0。

人工结果变为可见/黑屏/黑屏/黑屏。唯一可见的首个 A 在
`MvpuoPrepareComplete` 收到0；两个完整应用和黑屏的末个 A 均收到实例归属明确的 `-12017`，随后仍
完成绑定并进入 Play。它把最早观察到的结果相关分界前移到 Qt session 的 Prepare completion，也
证明本次 `-12017` 不是借用旧 native 或无归属 PP 日志。但冻结 A 同模式发生翻转，已触发预设停止
条件：该相关性不足以证明 `-12017` 导致黑屏，更不能把完整应用对象图、overlay、QGL、窗口或重复
`setVideoOutput` 宣布为根因。完整逐次表和断点干预时长见
[内部会话观测映射](E7_QT_INTERNAL_SESSION_OBSERVATION_ZH.md)。

后续主机阶段已取得 RM-626 / SW 111.040.1511 core 候选，确认 `MediaClientVideo.dll` ROM-FS
条目和 HxMmfCtrl/Real 静态 ECom 候选；目标手机逻辑文件副本随后恢复出 E7 utility 的 Prepare
转发代码。后续设备取证与旧寄存器复核已证明`-12017`由controller event带入，且失败会话未出现
成功会话都有的Helix视频服务线程；原始controller初始化返回码仍未取得。因此本文不再维护下一步
细节；当前唯一方向、Prepare error传播边界和精确缺口统一见
[MMF Prepare 错误来源](E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。在controller模块/函数映射成立前停止
猜地址；不实施 GetFrameL、新 DevVideo 会话、dummy/AddDisplayL、延时、重试、系统 DLL 替换、
窗口重构或删除重复 `setVideoOutput`。
