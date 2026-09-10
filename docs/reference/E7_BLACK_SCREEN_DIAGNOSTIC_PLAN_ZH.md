# E7 应用内视频诊断方案

> 状态：Closed / Historical reference
> 适用范围：已结束的 1.2.0 正式基线之后 E7 受控研究
> 本页职责：实验定义、观测要求和判定规则；执行优先级只见路线图，设备结果只见设备矩阵

本方向已封存，不再提供待执行设备清单。历史入口：[路线图](../ROADMAP_ZH.md)、[ADR-0008](../decisions/0008-e7-inprocess-qt-diagnostic.md)、
[E7 设备观察](DEVICE_TEST_MATRIX.md#e7-investigation-20260904)。
诊断构建入口和观测约定见下文；实际运行库、安装与播放结果只在设备矩阵维护。
下方[六次观察契约](#e7-six-observations)已结束，保留为执行定义；旧 R0～R5/atlas 表也不是
继续执行的任务清单。后续 minimal-P backend A/B 四次契约也已严格按 A→B→B→A 完成并停止；
设备事实见[四次结果卡](DEVICE_TEST_MATRIX.md#e7-minimal-backend-ab-20260906)，实现、方法偏离和解释见
[重新评估](../research/player/E7_BLACK_SCREEN_REASSESSMENT_ZH.md)。绑定时机预算和 frozen-C cold/warm
配对也均已按各自四次上限停止；原计划的 P-empty 未执行，不自动续跑。

<a id="e7-bind-timing-20260906"></a>
## Prepare 后绑定时机对照契约

状态：四次预算已按契约计数并停止；实际序列为 A → C → B（主机启动无效）→ C，未补跑第五次。
设备 ROM 中
`Z:/sys/bin/qtmultimediakit_mmfengine.dll` 的 CODA 只读请求返回 Code -21，所以实际 Mobility
分支仍为 UNKNOWN；这组试验只检验绑定阶段，不声称复刻了手机 Qt 内部实现。

同一 `e7bindtiming1` EXE 通过必填 `--e7-backend=A|B|C` 每进程只创建一种媒体后端：

| 模式 | 行为 | 变量边界 |
|---|---|---|
| A | `QMediaPlayer + QVideoWidget` | 新包 Qt 校准；内部 Utility/绑定保持 UNKNOWN |
| B | 正式 native backend，在 `OpenComplete` 调用 `AddDisplayWindowL + configureDisplay` | 原失败路径负控制 |
| C | 与 B 相同，但把上述绑定与配置整体移至可接受的 `PrepareComplete` 后、`Play` 前 | 唯一行为差异是绑定阶段；不是单条 API，也不加入 Qt surface 组合 |

三种模式共用样本 A、minimal-P 外层宿主、主 QGL 和 YUV/NanoVG512/字体资源；A 的 Qt video child
与 B/C 的 native child/RWindow 不同。B/C 之间共用 Utility2、shared `RFile` resolver、
priority/preference、controller 选择、native child 属性及 Play 流程。三模式统一加入只读
`RWindow::GetBackgroundSurface` 日志；查询本身不算 surface 创建或首帧证据。明确不加入
`AddDisplayL`、dummy RWindow、`_q_DummyWindowSurface`、私有 native paint、sanitize、atlas、
额外延时或 decoder 变化。

若进入真机，建议新预算最多四次，固定顺序 **A → C → B → C**，每次新进程，同包一次安装，
期间不重新链接、重装或追加运行。A 用于新包校准；中间 B 验证原失败链仍成立；C 前后重复并跨过 B，
用于区分候选效果与前序状态。无效启动、超时或人工不确定同样计一次。A 失败或同模式翻转时停止
机制归因，在剩余预算内只处理校准；B 未进入有效播放则先判适配无效；C 未进入有效播放不能记作
“延后绑定仍黑”。

每次沿用上一组的包身份、PID、session/seq、前序/退出、窗口代次、geometry/visibility、
Open/Prepare/Play、真实绑定返回、controller/Utility、position/轨道/错误和人工画面/声音字段；
另记录 `MMF_BACKGROUND_SURFACE`、`MMF_DISPLAY_BIND_DEFERRED`、
`MMF_DELAYED_DISPLAY_BIND_ENTER/RETURN`。人工结果只能由用户确认，available/position/surface 查询
都不能冒充首帧。四次之后停止；下一候选 AddDisplayL/dummy/private-paint 不自动进入。

执行偏离只发生在第 3 次：用于启动 B 的主机命令因参数形式错误，在发出设备进程启动请求前即被
参数解析器拒绝；依照契约仍消费一次，PID、媒体会话和设备日志均为 UNKNOWN/不存在，且没有补跑。
第 2 与第 4 次是同一安装、同一模式 C，却出现不同的人工可见性；因此命中“同模式翻转”的停止规则，
不能把延后绑定解释成稳定修复。逐次设备事实与日志行见
[设备矩阵](DEVICE_TEST_MATRIX.md#e7-bind-timing-observations-20260906)，解释与下一最小建议见
[重新评估](../research/player/E7_BLACK_SCREEN_REASSESSMENT_ZH.md#e7-bind-timing-result-20260906)。

<a id="e7-c-lifecycle-pair"></a>
### 已执行实验：冻结 C 的冷/暖生命周期配对

**状态：2026-09-06 已按四次上限执行完毕并停止。** 唯一研究问题是：冻结 C 的 Prepare 0 / `-12017` 分支是否随
重启后的首次媒体会话与同包 C 被强制结束后的紧邻会话而可重复改变。它不检验 dummy、AddDisplayL、
延时或完整 Qt surface 合约。

保持 `e7bindtiming1` 原 EXE/SIS、模式 C、样本 A、方向、滑盖、音量、CODA 启动方式和所有媒体参数
不变；全组不重新链接、不重装、不换媒体，也不运行系统播放器或其他媒体应用。冻结包没有已验证的
应用内正常退出路径，因此四次都固定使用 CODA terminate，确认目标 PID 消失后重连；这使“强制结束
后的服务/资源状态”成为被测前序的一部分，不能写成正常析构结果。每次记录 boot/uptime（取不到写
UNKNOWN）、前序程序、退出、PID、controller、Open/Prepare/Play、第二组 PP 标记及人工画面/声音。

次数上限严格为四次，每次新进程；无效启动、超时或无法判断都占一次：

| 次数 | 固定前序 | 目的 |
|---|---|---|
| 1 | 明确重启、CODA 稳定后，启动后的首次且唯一媒体应用为 C | cold-1 |
| 2 | #1 terminate、PID 消失并重连后立即启动 C，中间无其他媒体应用 | warm-1 |
| 3 | 再次明确重启，以相同条件启动首次 C | cold-2 |
| 4 | #3 terminate、PID 消失并重连后立即启动 C | warm-2 |

结果只按以下分流，不增加第五次：

- 两个 cold 一致、两个 warm 一致且两类相反：提升跨进程 lifecycle/媒体服务资源状态；下一步只查
  退出/服务所有权和 PP 实例生命周期，不进入 surface 组合。
- cold 内或 warm 内翻转：停止，把该前序下的 C 归为仍不稳定；不靠继续重复寻找“想要的”结果。
- 四次相同：本设计未支持 cold/warm 影响；若均为 Prepare 0/有画，另立预算再比较输出合约；若均为
  `-12017`/黑屏，先获取 PP 模块归属或实例跟踪，不强行忽略错误。
- 第二组 PP 块若始终只随 `-12017` 出现，只把它升级为分支标记，不升级为原因；若脱钩，则降低其
  诊断价值。任一次未进入有效 Open/Prepare/Play 只记启动/适配无效，不能当黑屏机制证据。

实际顺序为 cold-1 → warm-1 → cold-2 → warm-2，PID 依次为711、747、703、745。两个 cold 均有
明确人工重启和 CODA 重连，两个 warm 均在前一 C 被 CODA terminate、确认 PID 消失后紧邻启动；
全组未重装或重新链接。四次均为 PrepareComplete 0、绑定/Play 成功，且用户确认完整运动画面、
声音正常、视频外背景白色；Prepare 区间均没有第二组 PP 统计。因此命中“四次相同”分流：本设计
**未支持** cold/warm 前序会改变 C 的分支。详细设备事实和日志行见
[设备矩阵](DEVICE_TEST_MATRIX.md#e7-c-lifecycle-pair-observations-20260906)。

四次到限后已经停止。没有自动重试、经验延时、忽略错误、切换媒体或加入 AddDisplayL/dummy/private
paint。它也没有证明系统媒体服务必然在 terminate 后复位，更没有抹去此前 C 的翻转；只降低这里
定义的“重启后首次 vs terminate 后紧邻”二分假说。下一实验必须另立预算，不能续作第五次。

## 证据与代码固定点

以下是产品 Git 中的追溯点，提交存在不等于对应安装包已经核验。
封存实现仅供只读对照；不从并列研究仓库维护或复制第二套产品源码。

| 参照 | 固定提交 | 核对用途 |
|---|---|---|
| 正式 1.2.0 | `fc9a0ea` | 新研究起点，避免继承多轮诊断 CONFIG |
| QWidget + direct MMF | `2e2c385` | E7-01 的最小结构参照 |
| QGL-first + Qt 视频 | `c5f9ff9` | 首轮冻结校准参照 |
| 横屏 Qt 视频候选 | `471df25` | 后续方向对照；逐包对应关系待核验 |
| Qt 后端完整集成 | `ce51e26` | 与最小宿主比较，禁止整批移植 |
| 无 overlay / 默认视频属性 | `fb5cc85` / `7ece4b2` | 避免无新增观测地重复旧实验 |
| CONTROL / MIRROR | `3eded8d` | 比较实际创建顺序与启动链 |
| 独立 Player 封存 | `b9e1a8d` | 历史设备记录与日志实现参考，不作为新产品基线 |

`c5f9ff9` 的 `ProbeController.cpp` 中，普通 QWidget host 先 `showMaximized()`，8×8 子
QGLWidget 完成 show/native 化/makeCurrent/doneCurrent，然后创建 QMediaPlayer、连接信号，
再创建 QVideoWidget，geometry/show/raise/native 化后才 setVideoOutput、setMedia、play。
样本路径是 `E:/test/test.mp4`。源码中的 400/500 ms 延时只属于参照，不是已证明的修复条件。
小 QGL 的初始化不等同于完整 NanoVG 主界面持续渲染。

旧 MIRROR 先 native 化 QVideoWidget，再创建媒体对象，并非上述参照的逐调用复刻。
上层 `QTMAB:SET_VIDEO_OUTPUT` 也不能替代后端真实绑定位置的记录。

## 首轮输入与实验身份

- 记录 E7 型号/固件、Qt 与 Mobility 运行库、安装盘、起始方向、滑盖、当前补丁状态。
  CODA 的连接方式和端点只留本地；首轮不改变运行库、补丁、媒体或清晰度。
- 固定样本 A：历史成功参照使用的完整本地 MP4；样本 B：用户报告的黑屏完整 MP4。
  记录大小、SHA-256、时长、H.264 profile/level/ref、音轨。两文件原样使用，不转码或重封装。
  B 也须在同一参照测一次；若参照也失败，它暂时不能用来定位应用窗口差异。
- 每个候选记录提交、工作树差异、实际 CONFIG、EXE/SIS 身份和实装证据。
  最低 `Symbian3Qt474`、Qt 4.7.4、GCCE 4.4.1 是构建基线；若冻结参照原用不同 SDK，
  先核对原包再把 SDK 差异作为显式变量，不能静默换 SDK 后仍声称完全复现。
- 固定 Qt 视频路径，禁用本次实验的自动 preflight、FFmpeg 与媒体重试，确认实际输入为
  已完整存在的本地文件。Qt 播放出画首先只记为 Qt 路径通过；若无 controller/HwDevice
  证据，不进一步宣称已证明某个硬件 decoder。

## 实验定义

| ID | 内容 | 必须回答的问题 |
|---|---|---|
| R0 冻结校准 | 当前 E7 跑 `c5f9ff9`，保留其窗口与创建顺序 | 历史成功能否在本轮设备和样本上重现？ |
| R1 应用内最小宿主 | 真实 NIKINIKI 进程中接入简单 QWidget + QVideoWidget，先匹配 R0 的固定方向与可见几何；不创建播放器 overlay | 完整主程序环境是否仍允许最小 Qt 路径出画？ |
| R1B 主动激活宿主 | R1 创建 QMediaPlayer 前调用一次 `QApplication::setActiveWindow(host)` | 该调用是否保持到真实输出绑定，以及是否改变可见画面？ |
| R1C 主 QGL 半宽 | R1 主界面初始化完成并退出 fullscreen 后，将仍映射的主 QGL 宽度缩半；保持第二视频宿主 | 最小探针的半宽条件能否帮助真实 NIKINIKI 出画？ |
| R0W-S / R0W-T 窗口差分 | 共用同一探针源码，分别把 QVideoWidget 放在小 QGL 的原宿主或新建的第二顶层宿主 | 多顶层结构本身是否足以改变最小参照结果？ |
| R0W-L QGL 尺寸 | 保持 R0W-T 第二视频宿主，只把原 8×8 子 QGL 扩大到原宿主 rect | QGL 表面尺寸是否足以引入黑屏？ |
| R0W-H 半宽 QGL | 保持同一第二视频宿主，把 QGL 放在宿主左半部、宽度减半、高度不变 | 视频是否按 QGL 覆盖范围被遮挡，还是仍整幅黑屏？ |
| R0W-G 一像素空隙 | 保持第二视频宿主，只让 QGL 宽度比宿主少 1 像素、高度不变 | 几乎相同尺寸下，是否只有完整覆盖宿主才失败？ |
| R0W-I 初始化尺寸历史 | 子 QGL 先以 360×554 完成初始化，再缩到 180×554、doneCurrent 后才创建第二宿主和媒体对象 | 相同最终半宽下，先前的大尺寸初始化是否足以导致失败？ |
| R0W-Q 顶层 QGL | 用 QGLWidget 本身替换初始普通主宿主，保持先大后缩半及第二视频宿主 | 主 QGL 为顶层而非子控件时，最小路径是否仍出画？ |
| R0W-F 主窗口全屏往返 | R0W-Q 加入主程序同样的 retained-pane 隐藏和 showFullScreen，再 showMaximized、缩半 | 启动时的主窗口全屏状态转换是否足以触发失败？ |
| R0W-C 实际清屏 | R0W-F 的 QGL paintGL 中加入主程序相同的 glClearColor/glClear | 实际 GL 清屏是否足以改变空 QGL 的出画结果？ |
| R0W-N NanoVG 初始化 | R0W-C 加入产品同一份 NanoVG GLES2 backend 和相同创建 flags，尚不画图 | NanoVG shader/初始资源创建是否足以触发黑屏？ |
| R0W-NC 同二进制初始化对照 | 同一个 `nvgcontrol1` 包，默认初始化或通过启动参数跳过初始化 | 能否排除新增链接和两包构建差异？ |
| R0W-X 仅亮度纹理 | 保留链接，仅创建初始化用的 1×1 和 512×512 GL_LUMINANCE 纹理，不创建 NanoVG/shader | 这组纹理分配和状态调用本身是否足以触发黑屏？ |
| R0W-CPU NanoVG CPU context | 使用 NanoVG core 和空渲染回调创建 context，不发出 GLES 资源或绘制调用 | CPU 路径缓存/fontstash 初始化本身是否足以触发黑屏？ |
| R0W-SA NanoVG 小 atlas | 执行完整 GLES backend 初始化，仅把初始 font atlas 从 512×512 改为 1×1 | shader/buffer/backend 初始化在小 atlas 下是否仍触发黑屏？ |
| R0W-A256 NanoVG 256 atlas | R0W-SA 只把 atlas 提高到 256×256 | 完整 backend 与 atlas 是否存在尺寸阈值？ |
| R0W-A128 NanoVG 128 atlas | 1×1 通过与 256×256 失败之间的中点 | 阈值落在 128 以下还是以上？ |
| R0W-A192 NanoVG 192 atlas | 128×128 通过与 256×256 失败之间的中点 | 阈值落在 192 以下还是以上？ |
| R0W-A224 NanoVG 224 atlas | 192×192 通过与 256×256 失败之间的中点 | 阈值落在 224 以下还是以上？ |
| R0W-A240 NanoVG 240 atlas | 224×224 通过与 256×256 失败之间的中点 | 阈值落在 240 以下还是以上？ |
| R0W-A255 NanoVG 255 atlas | 240×240 通过后直接测 256 前一整数 | 256 边界值是否具有特殊性？ |
| R0W-A257 NanoVG 257 atlas | 紧邻 256 的更大非 2 次幂 atlas | 失败是否随面积单调，还是集中在 256 尺寸路径？ |
| R0W-Y `initializeYuvRenderer` 资源 | 在当前出画的默认 NanoVG 探针中，先创建真实主 QGL 同一 YUV shader/program 和六个未分配像素存储的纹理对象 | 真实程序在 NanoVG 前多出的 YUV 初始化资源是否足以恢复黑屏分界？ |
| R0W-P 内置首帧字体 | R0W-Y 只增加真实程序的 1.8 MiB 内置字体注册，不加载 UI 图片或执行 NanoVG 绘制 | 字体解析和内存驻留是否足以触发视频黑屏？ |
| R2 方向和全屏 | 保留已通过的宿主，先测原生横屏，再单独测 fullscreen/CBA 隐藏 | 首个破坏出画的动作是否是方向或全屏转换？ |
| R3 显示层 | 依次加入透明空层、基础控制、弹幕；每次保留上一个配置作对照 | 哪个覆盖层状态首次引入问题？ |
| R4 生命周期 | 持久对象 stop/解绑/隐藏，再进入；与出画测试分别记录 | 返回和重入是否稳定，是否保留主页面状态？ |
| R5 产品输入 | 同一样本的完整下载、增长文件、远程输入及现有解码策略 | 本地显示成立后，哪种传输或路由另有问题？ |

R0 与 R1 是环境边界对照，改变不止一个因素，因此 R1 失败不能直接定罪 QGL、NanoVG 或某个
widget。此时从冻结参照增量引入实际差异：独立顶层宿主、QGL 尺寸/像素配置、真实绘制负载、
前台与窗口树、媒体对象创建顺序；一次只改一项，并保持媒体和其他条件相同。
真实主 QGL 的“保持映射但暂停绘制”可作负载对照，不能改成预先 hide/delete。

R2 保留 panes、主 QGL 和物理工作区门槛。若测试启动前已横屏或滑盖已打开，要消费本次有效的
工作区状态，不可只等一个不会再次发生的旋转事件。R0 的系统栏允许作为诊断参照，不能算正式全屏。

后续机制确认需重复与反向控制；当前仅分配六次单独观察，按下方 ABBA 契约计数，不再套用
旧“每边三次”的额外次数。一次启动仅一种模式；R4 的 20 次循环属于未来生命周期验收。
若日志或断点改变结果，增加无断点、稀疏日志对照；手机重启与进程重启分开记录。
冻结 R0 本轮两次新进程加用户前一天一次通过的计数必须分开，不补写为本轮三次配对。

## 诊断代码入口

- 冻结 R0：`symbian/probes/qtmobility-qvideowidget-qgl-first/` 保留 `c5f9ff9` 源码；
  旧包证书过期时只重签，并逐个核对 SIS payload，不把重签记为重新编译。
- R1 / R1B：`symbian/Build-App.ps1 -Configuration debug -Variant e7qtminimal1` 或
  `e7qtactivehost1`；仅这些显式 CONFIG 编入 `E7QtMinimalProbe`。真实主界面完成绘制、资源
  准备与前台恢复后才启动，播放期间保持主 QGL 映射并让既有前台保护逻辑让出控制；固定竖屏、
  普通 QWidget/QVideoWidget、完整本地输入，无 overlay、preflight、FFmpeg 或网络加载。
  主页准备超过 15 秒会停止自动入口；主页就绪后点击可重试，超时本身不计播放失败。
- R1C：`-Variant e7qthalfmain1` 只继承 R1，不继承 R1B 的主动激活调用；在第二宿主显示前
  缩半主 QGL 宽度，并记录 `HALF_MAIN_GEOMETRY` 和持续 `mainRect`。它保留 QWidget/QGL 对象
  与映射，但上游 [Qt 4.8.0 的 Symbian resize 路径](https://raw.githubusercontent.com/qt/qt/v4.8.0/src/opengl/qgl_symbian.cpp)
  含 EGLSurface 重建，[顶层主动 resize 也会退出 maximized 状态](https://raw.githubusercontent.com/qt/qt/v4.8.0/src/gui/kernel/qwidget_s60.cpp)。
  尚未跟踪手机 DLL 内部调用，因此它是已知条件向主程序的迁移对照，不能称 EGL surface/窗口状态
  全程未变或只证明一个底层原因。
  固定竖屏诊断，未实现正式横屏适配或恢复验收，不将缩小主页作为普通用户流程。
- R0W-S / R0W-T：`symbian/probes/e7-qt-window/Build.ps1 -Variant sharedhost1` 或
  `separatehost1`，均为独立诊断探针 UID `0xE000B153`，不是产品独立 Player。
  两者保持 8×8 QGL、400/500 ms 阶段、媒体/视频对象顺序和默认窗口属性一致；后者仅在 QGL
  初始化后增加第二个 maximized QWidget，并把视频置于其中。两包分别留存，不能覆盖配对身份。
  新观测代码的共用宿主也必须实测，不把冻结 R0 的通过自动算到它名下。
- R0W-L：同一窗口差分工程的 `-Variant largegl1`，继承 `separatehost1` 的全部条件，
  仅在首次 show/native 化之前改变 QGL geometry；不新增绘制、纹理或 shader。
- R0W-H：`-Variant halfgl1`，仅把大 QGL 的宽度减半、位置仍在左上角。
  必须记录视频左半/右半的实际可见性，不能只询问是否“有画面”；部分出画支持调查覆盖/裁剪，
  仍不单独证明具体 compositor 或 MMF 内部机制。
- R0W-G：`-Variant gapgl1`，仅令 QGL width = 宿主 width − 1；与大 QGL 的像素面积近似，
  用于继续拆分覆盖完整性和尺寸条件。不得先写为内存或合成器根因已经确认。
- R0W-I：`-Variant latehalfgl1`，先按大 QGL 初始化，再缩半并 doneCurrent，确保创建媒体前
  的显式释放动作与初始半宽参照一致；新增 `GL_CURRENT` 只读观测实际当前 context，不能假定
  后续窗口事件不会再次绑定它。
  未引入真实 NanoVG 绘制或主窗口类型变化；结果不能直接外推到完整主程序。
- R0W-Q：`-Variant topgl1`，初始主窗口直接是 QGLWidget，首次 show 时即可初始化；
  保持后续 400/500 ms 阶段、先大后缩半和第二视频宿主。它是窗口结构迁移，不声称初始化
  时刻与子 QGL 完全相同；不引入 NanoVG、主程序资源或 fullscreen 往返。
- R0W-F：`-Variant fullcycle1`，按主程序隐藏仍存活的 StatusPane/CBA、showFullScreen、setFocus，
  确认物理竖屏且主窗口 360×640 后再 showMaximized；确认窗口和工作区恢复 360×554 才缩半和
  创建视频。每段 5 秒超时则停止，不能计播放失败。只测主窗口状态转换，不是视频真全屏通过。
- R0W-C：`-Variant cleargl1`，在 Qt 原有的 paintGL 回调中执行主程序相同的不透明背景色和
  color/stencil clear，记录最初五次尺寸与 GL error；不新增持续重绘 timer、NanoVG、字体或纹理。
- R0W-N：`-Variant nvginit1`，直接编译产品维护的 NanoVG C/GLES2 backend，在 initializeGL
  以 `NVG_ANTIALIAS | NVG_STENCIL_STROKES` 创建 context；未加入 NanoVG 绘制、字体/图片。
  创建失败则停止，不将缺少前提的播放计作黑屏。探针不维护第二份 NanoVG 源码。
- R0W-NC：`-Variant nvgcontrol1`，默认与 R0W-N 相同；仅传入 `--skip-nvg-init` 时跳过
  创建并输出 `NVG_INIT_SKIPPED`，同一包保留全部 NanoVG 链接内容。默认路径仍必须取得
  `NVG_CREATED true` 才进入媒体阶段；每轮用新进程，核对实际分支后才计入比较。
- R0W-X：`-Variant gltextures1`，只执行 NanoVG 默认 dummy/font-atlas 对应的亮度纹理
  分配、linear/clamp 参数及 unpack alignment 恢复，纹理保持到进程结束；不创建 shader、
  VBO、CPU fontstash 或 NanoVG context，不额外调用 glFinish。逐张记录尺寸、handle 和 GL
  error；仅 `LUMINANCE_TEXTURES_READY true` 时进入媒体。它不是对全部 NanoVG 初始化的替代。
- R0W-CPU：`-Variant nvgcpu1`，由产品同一份 NanoVG core 创建 context；render backend 回调
  只返回成功或空操作，不执行 GLES 调用。保留 CPU path cache、默认 state 和 fontstash 创建，
  不创建真实 GPU 纹理、shader 或 buffer，也不执行 NanoVG frame/draw。只有 context 创建成功才播放。
- R0W-SA：`-Variant nvgsmallatlas1`，执行与 R0W-N 相同的 NanoVG GLES2 初始化 flags；仅通过
  NanoVG 既有编译宏把初始 font atlas 设为 1×1，dummy texture 仍为 1×1。它创建真实 shader、
  VBO、两张纹理并执行 backend 的 glFinish，但不绘制。该尺寸不支持真实字体使用，只用于诊断。
- R0W-A256：`-Variant nvgatlas256`，除 NanoVG 初始 font atlas 为 256×256 外与 R0W-SA 相同；
  仍不绘制。它是 1×1 通过和 512×512 失败之间的首个中点，不能由单轮结果直接确定阈值。
- R0W-A128：`-Variant nvgatlas128`，只把 R0W-A256 的初始 atlas 改为 128×128；
  其余初始化和窗口/媒体时序保持一致。该历史二分已停止。
- R0W-A192：`-Variant nvgatlas192`，只把 R0W-A128 的初始 atlas 改为 192×192；
  非 2 次幂尺寸不启用 repeat 或 mipmap，其余初始化保持一致。该历史二分已停止。
- R0W-A224：`-Variant nvgatlas224`，只把 R0W-A192 的初始 atlas 改为 224×224；
  其余初始化和窗口/媒体时序保持一致。该历史二分已停止。
- R0W-A240：`-Variant nvgatlas240`，只把 R0W-A224 的初始 atlas 改为 240×240；
  其余初始化和窗口/媒体时序保持一致。该历史二分已停止。
- R0W-A255：`-Variant nvgatlas255`，只把 R0W-A240 的初始 atlas 改为 255×255；
  不启用 repeat/mipmap。紧邻尺寸的单次观察未建立可重复边界，该历史二分已停止。
- R0W-A257：`-Variant nvgatlas257`，只把 R0W-A255 的初始 atlas 改为 257×257；
  仍不启用 repeat/mipmap。若 257 通过且 256 失败，先按非单调尺寸路径候选处理，之后复测相邻包。
  若同一 256 包不能重复失败，撤回尺寸特异性候选，优先回测默认 512 包和记录会话状态。
  默认 512 原包若也由失败变为通过，不再继续尺寸二分；先核对设备是否重启并回测真实主程序。
- R0W-Y：`-Variant yuvinit1`，保持当前已出画的 R0W-N 默认 512 NanoVG、窗口与媒体时序；
  在 NanoVG 创建前复制真实 `initializeYuvRenderer()` 的 shader 编译/链接、location 查询和六个
  空纹理对象参数设置。程序与纹理保持到进程结束，但不调用 `glTexImage2D`、不上传或绘制 YUV
  帧；只有 YUV 与 NanoVG 两段都成功才开始播放。若本轮仍出画，下一层再加入真实 UI 资源或
  NanoVG frame/draw，不同时改变多个因素。
- R0W-P：`-Variant uifont1`，继承 R0W-Y，只把产品同一份未压缩内置字体资源注册为 `ui`；
  NanoVG 按 `freeData=0` 引用进程期资源，不复制成第二份长期存储。本轮不加载两张 UI PNG、
  不请求字形、不执行 `nvgBeginFrame`/`nvgEndFrame`。只有字体 ID 有效才进入媒体阶段。

后续绘制对照已由六次观察契约取代。空 `nvgBeginFrame`/`nvgEndFrame` 必须在任何非空内容
之前单独检查；当前 backend 在 `ncalls == 0` 时跳过主要 GPU 提交，不等于可省略这项对照。
rectangle/text/image 是未来分解项，本轮不批量实现或逐项发包。

窗口差分日志包含 `activeWindow()` 的实际对象、类名、父窗口、顶层归属、已有 native ID、
几何、窗口属性和 QVideoWidget 子树；使用 `internalWinId()` 读取已有身份，不触发 native 化。
`activeHost=false` 只表示 activeWindow 不等于该对象；不等于主 QGL 抢占，也不单独证明黑屏原因。
不要通过持续强制激活、增加延时或更换输入同时改变多个变量。

## CODA 最小观测

先连续采集稀疏日志复现，只有出现确定的失败分界时才下窄断点。
诊断输出必须同时包括 run-id、session-id、PID、事件递增序号、从进程启动连续计时的 elapsed-ms。
以下是待实现/核对的要求，不能认为现有普通包已经输出全部字段。

| 层 | 字段与真实调用边界 |
|---|---|
| 实验身份 | 提交/配置、模式、样本标识、输入类型、实际后端；不打印签名 URL/Cookie |
| 窗口 | create/show/hide/resize/parent-change、flags、parent、top-level、局部/全局 rect、visible、缓存 native 身份、前台 window group |
| 方向 | 请求与返回、stage、workAreaResized、物理/可用尺寸、panes/fullscreen 状态 |
| 媒体启动 | QMediaPlayer/QVideoWidget 创建，信号连接结果，setVideoOutput/setMedia/play 进入与返回 |
| 异步控制 | pending-media/play，timer 排队/触发/取消，guard 拒绝原因、会话是否已过期、阶段超时 |
| 媒体状态 | state/status/error、audio/video available、duration、每秒至多一次 position、volume/mute |
| 返回故障 | stop/解绑/隐藏/方向恢复顺序；实际退出进程、panic 类别/原因、PC/调用栈及模块信息（能取得时） |

信号必须在实际绑定、加载之前接好；不以延长定时器代替状态机修复。
日志不能为读取窗口身份而新增 `winId()`、show、raise、makeCurrent、reparent 或 processEvents；
只在设计本来需要 native 化的地方调用并缓存身份，避免观测改变实验。

若外层证明媒体已启动仍无画，才核对设备实际 Qt MMF engine 的版本和构建分支，再尝试
Prepare、视频输出切换、窗口更新以及 surface created/parameters/remove 等内部事件。
[Qt Mobility v1.2.0 的 S60VideoPlayerSession 源码](https://raw.githubusercontent.com/qtproject/qt-mobility/v1.2.0/plugins/multimedia/symbian/mmf/mediaplayer/s60videoplayersession.cpp)
包含受宏控制的不同输出路径；上游源码不能单独证明手机 DLL 编入了哪条路径。
只给设备实际存在、符号或调用可核对的入口设断点，不把新版 master 的 EGL 类名当作本机能力。

## 判定与停止条件

| 观察 | 判定 | 后续处理 |
|---|---|---|
| R0 本轮也失败 | 参照尚未校准 | 查样本、实装包、运行库和设备状态，暂停窗口归因 |
| 未到真实 setMedia/play，或时钟不前进 | 启动/媒体问题仍未排除 | 查 pending 请求、错误、音量和状态；MIRROR 按此处理 |
| 有声、时钟前进、videoAvailable，但黑屏 | 优先查视频链，尚无有效解码帧证明 | 对照窗口/绑定；需要时追内部输出 |
| 人工确认连续运动画面且有声 | 本层出画成立 | 固定该配置，按路线图推进 |
| R0 对 B 也黑而对 A 出画 | B 可能有独立媒体兼容问题 | 单列编码/轨道研究，不用它否定 R1 窗口修复 |
| 返回崩溃或主页面丢失 | 生命周期门槛失败 | 单独捕获故障，暂停继续集成 |

`Playing`、`videoAvailable=true`、窗口 ready 或有声音均不是首帧证明；黑色截屏也不能单独
证明 native 视频层没有显示。以用户观察/脱敏屏摄为可见性证据，必要时再取真正的帧输出证据。

R0/R1/R2 的差异仍无法解释时，整理时间线再深入 Qt/MMF；不恢复随机窗口 flag、CDN、清晰度
组合试错。DevVideo memory-output 只保留为后备能力研究，必须另设准入；E7 初始化失败不能
推出所有第三方硬解不可用，603 的成功同样不能外推 E7。

本地与返回研究通过后仍须：普通构建、SIS、菜单独立启动 Release、原有 50 次循环、30 分钟
观察、603 回归和其他初代机型独立验收。研究结果不自动解除 1.2 的不支持公告。

<a id="e7-six-observations"></a>
## 当前六次观察操作契约

**状态：六次预算已执行结束。** R 完成 full/empty/empty/full；P 的两次用于无效启动及 clear
分支校准，P-empty 未执行。逐次结果与构建边界只见设备矩阵，本节保留原操作契约。
六次是包含控制与重复的六个新进程观察。设置、安装、模块静态检查不算播放观察；只要启动并
要求用户判断本次播放，就占一次，超时、启动无效或断线也不免费补跑。无需为六次建立六个包。

### 两个固定包与边界

**R 包：完整 NIKINIKI + 正式播放器。** 在独立诊断 CONFIG 中增加首页绘制 runtime switch，
同一个 EXE 支持 full/empty；普通 CONFIG 不编入开关或诊断输入入口。保持真实 YUV/NanoVG
初始化、字体加载、PNG 上传、启动状态机、正常定时器、主 QGL 映射、播放器对象复用和横屏过程。
使用正式 VideoPlayerWidget/native MMF backend，通过诊断入口只提供已解析的样本 A 本地输入；
播放设置固定为既有强制 MMF 模式，不能改 decoder、header 或编码，不能自动进入 FFmpeg。
该限定只控制当前显示实验，不验证正式自动 preflight 选路。若无法只提供输入而保留正常对象/
窗口顺序，先在主机解决入口，不临场替换成 R1C 或直接调用另一个 CVideoPlayerUtility。

R1C 仅作为真实首页初始化的源码参照；本轮 R **不继承缩半主页、第二 Qt 视频宿主和竖屏视频**。
正式横屏/overlay 条件在 full 与 empty 中完全相同；只减首页实际 UI 绘制，正常播放器控制和
弹幕绘制保持原状。R 与 P 的输出后端、窗口和方向不同，所以跨两包只能比较边界，不能计算
单因素因果效应。R-full 本轮可能成功，这也是有价值的结果，不能预写失败。

**P 包：R0W-P 资源与 Qt 视频路径。** 在同一个 EXE 内支持 clear/empty；保留已有 YUV shader、
六个空纹理、默认 NanoVG context、内置字体、全屏往返/半宽与第二 Qt 宿主。clear 沿用既有
paintGL 的 GL clear；empty 只加空 begin/end，不新增 PNG/CJK、timer、几何、文本或图片。
它是成功参照的带开关新构建，必须花第 5 次重新校准，不能借旧包成功跳过。

两包均使用最低 SDK/GCCE 普通优化基线，诊断 CONFIG 单独列明；全量依赖哈希包括 NanoVG
共用源码、资源、生成 MMP、编译选项和链接依赖。冻结 EXE/SIS/符号后只装一次各自的包。
安装前核对 payload；能读回的实装 EXE 核对完整哈希，不能读回则记 UNKNOWN。配对内不重装、
不重新链接。记录普通包未含诊断宏/入口的静态检查，禁止把两类包都叫“同一个 NIKINIKI”。

### 绘制开关必须保持的语义

- 在第一次允许实际 UI 绘制前读取模式，不能等完整首页已画过再切 empty。继续调用正常
  paintGL、原 clear、resizeGL 和 Qt 自动 swap，不关闭 autoBufferSwap、不跳过整个 paintGL。
- empty 在原 UI 区段按相同尺寸/pixel-ratio 调 `nvgBeginFrame`→无内容→`nvgEndFrame`。
  正常维护 m_hasPainted、帧计数、输入和启动门；资源注册与异步加载不能塞进被跳过的区段。
  “首页没画所以播放器从未启动”属于 INCONCLUSIVE，不是有媒体的黑屏。
- 每种模式记录初始化资源 ID/成功、首次 frame、draw-call/vertex/字形 dirty/upload 计数及累计帧数。
  empty 应为实际 draw=0，首次无字体 atlas 扩张；若不满足，先修开关语义再构建，不能叫空 frame。
  clear 与 empty 都经过 Qt swap；empty 返回不意味着发出 GPU draw。
- 不把 CJK_READY 新增为阻塞启动门，也不人为删 CJK。记录实际 readiness、异步字体阶段与
  媒体入口的先后；full/empty 若因字体阶段或更新频率不同而分流，结果只定位“绘制历史组合”，
  尚不能定位单个 GL 状态。保留原有调度，不用 sleep 把时序差异藏起来。
- 同一模式里只观察一个媒体会话；后续 expose/paint/resize 继续按原策略运行并计数，不能
  把“最后一帧”名称当作之后绝不会再绘制的保证。播放期间的 GL 活动与播放前活动分别统计。

### 顺序、解释和预算

每次从固定竖屏、关闭滑盖的起始条件启动；补丁/运行库/音量/媒体不变。首轮记录是否重启，
随后不计划在 ABBA 中重启或换 CODA 模式；意外重连、系统状态改变或其他 app 活动原样登记。
每轮以相同方式结束，优先正常 quit 并确认 PID 消失；若只能 terminate，记为强制退出并固定
方式。不能把 terminate 当作“析构与 GPU 清理完成”。结束后观察上一声音是否停止。

| 次数 | 模式与唯一主动差分 | 执行检查 |
|---|---|---|
| 1 | R-full，完整首页，正式播放器，无 sanitize | 正式 native backend 已进入播放；不能用 R1C 回执代替 |
| 2 | R-empty，首页 UI 区段为空 begin/end | 首次 UI 帧起生效；资源门通过，empty draw=0，正常播放窗口不变 |
| 3 | R-empty 原 EXE 新进程重复 | 不重装，前 PID 已结束，模式与第2次一致 |
| 4 | R-full 原 EXE 反向 | 恢复 full，仍不重装，记录全部前序与意外事件 |
| 5 | P-clear，带开关新包成功参照校准 | P资源门/窗口过程通过，沿用正常clear与swap，无NVG frame |
| 6 | P-empty，同包只加空 begin/end | 仅在基线有效时执行；第5次失败则改为同模式复验 |

各次结果 A/B 对假说的影响与信息价值只见[总审计第9节](../research/player/E7_BLACK_SCREEN_ROOT_CAUSE_AUDIT_ZH.md#9-top-six-experiments)。

若1有画，2～4改为 R-full 相同条件重复，先界定本轮是否可复现；不趁机继续多参数展开。
若2/3翻转，4恢复 R-full 后，5/6在已占用预算内只复验可疑同一条件，不再引入新模式。
若媒体未启动、只抓到暂停/零时钟、目标 PID/窗口门未确认，记 INCONCLUSIVE 并在剩余次数中
修复校准；不得悄悄在表外补一次。最多六次，可提前停止；启动日志本身不能替代用户观察。

P 的 clear→empty 只有一对，**不是重要因果边界已通过 ABBA**。若 P-empty 黑，本阶段只保存
候选，下一轮先反向重复；不会在六次预算内伪装成四次。若 P 均正常，不能直接宣称所有
NanoVG draw 都无关，因为 rectangle/字形/图片尚未执行。

未来若需要，分层顺序为 R 的 clear→empty→rectangle→text→image→full，以及 P 的
clear→empty→rectangle→text→image→近似首页；只在下一预算内选择首个有信息的边界配对。
这是一张变量拆分图，不是现在要求用户再跑十二项的清单。

### 单次记录卡

```text
run_id / 顺序 / 预定模式 / 实际模式 / 前一个实验及结束方式
commit + dirty-source manifest / CONFIG / SDK / EXE hash / SIS hash / 实装读回状态
设备与运行库引用 / boot或uptime证据(无则UNKNOWN) / CODA connection / PID / 安装与启动方式
起始方向/滑盖/补丁/音量 / 输入样本A哈希 / 实际backend/controller(未取得则UNKNOWN)
资源与启动门 / 首个和最后UI paint序号 / paint返回、swap、之后paint事件
native window代次 / outer与inner窗口 / EGL context、draw/read surface / 实际bind时间与结果
Open/Prepare/Play/轨道/position / frame层证据等级 / 错误或超时
用户：连续运动画面、全黑或局部黑 / 左右是否一致 / 声音 / 判定时间窗
退出PID确认 / 意外重连、其他应用或重启 / 结论与未控变量
```

用 QElapsedTimer 计进程内时间和独立递增序号，QTime/主机 ms 不用于跨重连排序；旧日志保留
原语义，不重新解释为单调时钟。窗口与媒体关键节点记一次，position 最多每秒一次，帧内只累加
计数。固定两个观察采样点并保留人工时间窗；不要循环 glGet/日志改变每帧的负载。

### UI / EGL / native 交接快照

1. 在 paintGL 内记录本次 frame 序号与末尾状态，命名 `UI_PAINT_RETURN`；它**不是 swap 完成**。
   诊断子类在正常 `QGLWidget::paintEvent` 基类返回后记录 `UI_PAINT_EVENT_RETURN`，仅在
   核对这次确实触发 glDraw、autoBufferSwap、doubleBuffer 后作为 swap-return 外层代理。
   `updateGL` 等直达 glDraw 的路径不经过此代理，单列 UNOBSERVED。精确 swap 需匹配 Qt 函数入口。
2. 在正常媒体/window 创建、真实 bind 前后采样已有 EGL current display/context、draw/read
   surface，QGL object/context、cached RWindow/window group、局部/全局 extent、visible、flags、
   parent/top-level、screen rect、inner video child 与 native 代次。查询不到的 clip/background
   surface/ordinal 字段写 UNAVAILABLE；不能用 geometry 推算 WS 有效可见区。
3. 仅当当前 context 正是被测主 QGL，少量快照记录 program、framebuffer、array/element buffer、
   active texture及该单元 binding、viewport、scissor box/test、blend及函数、stencil及mask、
   depth/test/write-mask、cull、color mask。若读取 unit 0 等其他纹理单元，必须原样恢复 active
   unit并记录这项观测性切换；优先不遍历全部 unit。无正确 current 时 GL 字段 UNAVAILABLE，
   不为日志 makeCurrent。`glGetError` 会消费错误队列，固定一次采样并标明副作用。
4. 不以截图黑证明 native video 黑，不为查询调用 winId/native 化、show/raise、processEvents。
   EGL getter也可能有开销；先使用相同稀疏观测的两种模式，无窄证据不加断点/全量同步日志。

### 同一 MMF 会话的视频层观测

R 使用自有 backend，先加仅诊断 CONFIG 的 **实际** AddDisplayWindowL/configureDisplay/Prepare/
Play 进入、返回码与 observer 回调日志，附 session、utility、controller（能取得时）、window
代次。这可证明控制启动和绑定接受两层，不能命名为首视频帧。P 的外层 setVideoOutput 不能
替代 plugin 内真实 Add/SetDisplayWindow；先识别本机已加载插件、导入与可匹配符号。

同会话自然 picture-ready、renderer buffer 入队、surface-update completion 若能从实际
controller/renderer匹配获得，只记最初一个 buffer ID/PTS/尺寸及累计数。surface-created 是
surface 配置证据，不是像素证据；MvroBufferDisplayed 也不是应用可随意注册的通用 listener。
源码比较版的 endpoint/observer 若本机不存在，明确 UNAVAILABLE，不更换输出到
QAbstractVideoSurface 来获得一条不同显示路径的“通过”。

可设计一次 opt-in `GetFrameL` 取样，记录 request→callback、error、尺寸/格式/抽样 CRC 与
取样前后的可见性。SDK 的 `MvpuoFrameReady` 是该请求的回应，不是自然播放首帧。GetFrame 可能
干预解码/同步；必须放在该次正常观察结束以后，前后的结果分开，不用它修饰前半段黑屏。
本轮六次默认不发取帧请求；仅被动两层日志随同六次采集。若后来需要用户再次评价取帧后画面，
那是一个新的观察，必须占用剩余预算或下轮预算。返回不支持只说明取证方法不可用。

### sanitize 诊断设计：暂不占用本轮六次

sanitization 仅为有条件下一轮 Candidate Experiment，完整首页必须已经真实绘制并且原始
黑屏基线可重复。保存当前程序/纹理/缓冲等快照后，在自然保持 current 的 UI 交接点测试如下
总干预：恢复明确的 GL state→flush→按模式选择 finish→doneCurrent→queued event handoff→
已有 video host ready→绑定→play。不得创建临时共享 context、hide/delete 主 QGL 或加 sleep。
如自然点没有正确 current，不偷偷 makeCurrent；记录模式前提未满足。

“标准 state”必须显式列出：默认 framebuffer、program=0、array/element buffer=0、active unit 0、
已使用纹理单元 binding清理、viewport=当前 drawable尺寸、scissor/blend/stencil/depth/cull关闭、
color mask全开、depth/stencil write-mask复位和已使用 vertex attrib禁用。它会改变 UI 后续状态，
恢复前必须核对 NanoVG cache/下一次draw前提；未知默认 FBO 就不强写0。所有调用与错误仅记一次。

总干预若稳定改变结果，下一轮按同包 ABBA **只选择一个有证据的边界**拆解：
state-only / flush-only / finish-only / doneCurrent-only / queued-only / window-ready后单次bind。
先确认观测与纯 queued/no-op 没有同样效果，再决定具体 state 组；不要同时测六项又隐瞒次数。
`glFinish` 可能改变时序，`doneCurrent` 不保证后续不被 paint 重新绑定；记录后续事件，不能
从“总干预有效”推导是 blend/FBO 污染。没有重复和拆解，不向正式包提交 reset ritual。
