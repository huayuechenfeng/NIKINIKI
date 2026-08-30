# wiliwili for Symbian³ 阶段性开发报告与接手指南

> 归档状态：Historical。本文保留 0.x 阶段的追加式开发记录，不再是当前事实来源。
> 当前状态见 `docs/STATUS_ZH.md`；当前计划见 `docs/ROADMAP_ZH.md`；当前架构见 `docs/developer/`。

> 记录日期：2026-08-25
> 当前源码版本：1.0.0（2026-08-29 正式 Release 构建）
> 发布补充（2026-08-29）：用户将本次成功编译的当前主线确定为 1.0 正式版。`Symbian3Qt474` / GCCE 4.4.1 Release 构建以 `sbs errors: 0`、32 条既有警告完成；SDK 旧签名已剥离，并使用有效期 2026-08-24 至 2036-08-21 的当前证书重签。正式产物、SHA-256 和静态验收见 `docs/releases/RELEASE_1.0.0_ZH.md`。原版 Symbian³ / Anna 公测、Release 50 次重入和其他既有设备覆盖边界继续保留为已知后续项，不回写为已完成。
> 当前结论（2026-08-29 更新）：原生横屏 app-shell 已通过 20 轮，主播放器又完成两条 FFmpeg soft、两条 MMF hardware 的四视频真机矩阵，横竖向编码、弹幕/UI、返回和无系统栏均正常。日志 `2746319` 证明旧 90° ARGB pass 已消失，但软解仍把 RGB565 画面绘入全屏透明 ARGB overlay，约耗费 102–179 ms/显示帧。当前源码已建立持久 opaque native RGB565 surface，单一 ARGB 顶层只绘制弹幕/控件；同时将 soft `PositionL()` 改为 500 ms 校准加倍速外推。发布构建已改为最低 `Symbian3Qt474`，移除唯一的 Belle-only CookieManager 后 Debug/Release 均以 32 条既有警告、`sbs errors: 0` 完成，确定采用一个应用 SIS 覆盖 Symbian³ / Anna / Belle。该最低 SDK 包已在 Nokia 603 / Belle 实测正常；先前黑屏/点击闪退报告来自错包，证据作废。原版 Symbian³ / Anna 转入公测收集，不作为当前发布前阻塞。`devvideodirectprobe1` 真机证明全屏 ARGB overlay 下 DSA DrawingRegion 为零，Phase B 未启动，故该显示路线在 1.0 前封存。用户的 SPS ref=7→3 fake 实验补强了“存在 SPS/DPB 准入门”的证据，但不能证明芯片真实 7-ref DPB 能力，也不改变 MMF→FFmpeg/CPU RGB565→外部播放器政策。解码器、queue、catch-up、Range、RGB565 LUT 和 1.0 前编码政策保持冻结。
> 文档定位：这是截至记录日期的开发现状“单一事实来源”。旧的版本说明保留作历史资料；若与本文冲突，以本文和更新的真机日志为准。

## 1. 项目目标

本项目要在 Symbian³ / Nokia Belle 手机上实现一个真正可日常使用的 wiliwili 客户端，尽量继承主项目的产品结构、Bilibili API 行为和交互方式，而不是重新设计一个只有若干列表的演示程序。

目标范围包括：

- 首页推荐、热门、番剧、直播等内容入口；
- 搜索、视频详情、评论与楼中楼；
- 二维码登录、账号资料、动态、消息和设置；
- 普通视频、清晰度切换、播放控制、弹幕；
- 直播播放和后续的直播弹幕；
- 适合 360×640 触屏设备的全屏 UI；
- 完整 SIS 安装、手机图标冷启动、CODA 调试和稳定退出。

项目不追求把现代 wiliwili 的 C++17、Borealis 和 mpv 原封不动编译到 Symbian。GCCE 4.4.1 无法直接承载这套现代依赖。实际策略是复用上游的接口语义、模型、算法和产品结构，在 Qt 4.7.4、Symbian RHTTP、NanoVG/GLES2 与原生 MMF 上做兼容移植。

## 2. 当前状态摘要

### 2.1 状态定义

| 标记 | 含义 |
|---|---|
| 已验证 | 已在 Nokia 603 真机出现过成功结果，并有用户观察或日志佐证 |
| 部分可用 | 主链路跑通过，但覆盖范围、稳定性或交互仍不完整 |
| 已实现待验证 | 代码、编译和打包已完成，尚无对应真机操作结果 |
| 阻塞 | 当前实现已被真机证明不可交付，需要更换技术路径 |

### 2.2 功能总表

| 模块 | 状态 | 当前事实 |
|---|---|---|
| Belle 开发环境 | 已验证 | Debug/Release ARMv5 构建、SIS 打包、签名、WLAN CODA 部署和断点调试均已跑通 |
| Symbian³ / Anna / Belle 统一包 | Belle 实测通过；原版/Anna 公测待收集 | 当前默认使用 `Symbian3Qt474`；移除 CookieManager 后 Debug/Release 均为 `sbs errors: 0`，同一应用 SIS 声明 Qt 4.7.4、Qt Mobility 1.2.0。Nokia 603 / Belle 当前测试无异常；原版/Anna 需离线补运行库并由公测覆盖 |
| 应用启动与主循环 | 部分可用 | Qt Creator/CODA 启动可靠；手机图标冷启动曾成功，但仍出现过转圈、后台残留和不同安装方式行为不一致 |
| 全屏与系统栏隐藏 | 已验证，冷启动回归待完成 | `appshell7` 20 轮及主播放器四视频矩阵均确认横竖屏无系统栏；仍需对最终包的手机图标冷启动回归 |
| NanoVG/GLES2 主 UI | 已验证 | 主页、导航、卡片、触摸、滑动、详情等可显示；这是当前最成熟的 UI 基础 |
| 中文字体 | 已验证 | 完整字体随 SIS 部署，日志有 `PRIMARY_FONT_READY`；用户确认字体和图片曾恢复正常 |
| 缩略图 | 部分可用 | 首页图片可以加载，已缩短轮询与请求尺寸；仍有个别 CDN/网络失败和速度问题 |
| Bilibili HTTPS/API | 已验证 | Symbian 原生 RHTTP 能请求当前 Bilibili API，首页多次返回 `BI:H200` |
| 首页与内容栏目 | 部分可用 | 推荐列表已可用，热门/番剧/直播入口和内容页已搭建；动态、消息、设置仍不完整 |
| 底部导航 | 已验证 | 已由左侧栏改为底栏，为 360 像素宽屏幕释放卡片宽度 |
| 视频详情 | 部分可用 | 标题、描述、分 P/来源和评论入口已实现，部分排版仍需打磨 |
| 评论区 | 部分可用 | 支持最热/最新、点赞数、回复数、无头像、根评论和楼中楼请求；展示质量和分页体验仍未达到正式 Bilibili 客户端水平 |
| 二维码登录 | 部分可用 | Cookie 捕获、持久化和线缆发送已修复；曾出现 `PROFILE_READY`，证明会话可用，但新会话从扫码到账号页仍需完整回归 |
| 账号资料 | 已验证过 | 日志出现 `PROFILE_READY 1547820`、`PROFILE_STATS_READY`；依赖有效持久 Cookie |
| 搜索输入 | 部分可用 | 工具窗口方案能弹出输入法，但焦点仍可能丢失；搜索框只应出现在首页 |
| 搜索结果 | 已实现待验证 | 0.6.12 已从旧接口改为上游 WBI 搜索接口；最后一次测试没有 `SEARCH_SUBMIT`，不能宣称已修好相关性 |
| 渐进式 MP4 选源 | 部分可用 | HTTP/CDN 备用线路、错误分类和本地下载兜底已实现；0.7 真机首次播放已完整出现画面和声音，仍需扩大编码与线路覆盖 |
| 竖屏视频 | 部分可用 | 在未触发旋转的稳定窗口路径中能正常显示并流畅播放；并非所有编码/线路都兼容 |
| 横屏视频 | 已验证 | 0.9 使用 panes + `workAreaResized()` 原生 640×360 状态机；两条 soft、两条 MMF 主线视频均正确显示并返回，旧 90° 路径已删除 |
| 弹幕下载与解析 | 已验证 | 压缩解包、XML 容错和调度可工作，日志出现 `DANMAKU_READY` |
| 弹幕覆盖到视频 | 已验证 | 单一顶层 ARGB 覆盖窗位于 MMF 视频之上；用户确认视频和弹幕同时正常显示 |
| 横屏控制/UI 清晰度 | 已验证 | 控件、弹幕和触摸直接使用原生 640×360 坐标，用户确认主线四视频均正常；无需离屏旋转 |
| 播放器重复进入 | 已验证，压力门槛待完成 | 用户确认 `surfacepersist1` 已可退出并再次播放；确定性的第二次进入崩溃已修复。仍需完成 Release 50 次循环，排除低概率残留窗、后台声音或长期资源泄漏 |
| H.264 编码覆盖 | 部分解决 / 性能优化待验 | BCM 与系统 ARM 均已止损；FFmpeg 已让两条风险流在原生横屏主线出画面，MMF 安全流保持硬解。当前只优化 soft 显示表面和时钟查询，不改编码器；仍不足则按政策转外部播放器 |
| 清晰度切换 | 部分可用 | 选源和重载逻辑已实现，播放器显示架构未稳定前不能视为完整功能 |
| 直播 | 已实现待验证/阻塞 | 直播信息、画质和线路解析已搭建；真机曾遇证书和无音画，直播弹幕尚未完成 |

## 3. 已确认的硬件、系统与工具链基线

### 3.1 真机

- 设备：Nokia 603；
- 固件：`113.010.1506`，Nokia Belle；
- 屏幕：竖屏 360×640，横屏 640×360；
- 调试代理：Public CODA 1.0.6；
- 已验证 Public CODA 的 WLAN 调试链路；具体私网端点不进入公开仓库；
- Breakpoint Mode：KBA；
- 注意：IP 由 DHCP 分配，之后可能改变。

### 3.2 主机与 SDK

- SDK 根目录：`C:\QtSDK`；
- 公开包默认 SDK：`C:\QtSDK\Symbian\SDKs\Symbian3Qt474`；
- Belle 后备/诊断 SDK：`C:\QtSDK\Symbian\SDKs\SymbianSR1Qt474`；
- Qt：Qt 4.7.4 for Symbian；
- Qt Mobility：1.2.1；
- 编译器：GCCE 4.4.1 / ADT Sourcery G++ Lite 4.4-172；
- SBS：2.17.0；
- Qt Creator：2.4.1；
- 目标：ARMv5，Debug 为 `arm.v5.udeb.gcce4_4_1`，Release 为 `arm.v5.urel.gcce4_4_1`。

完整工具链调查见 `docs/reference/TOOLCHAIN_REPORT.md`。

统一包的 ABI 审计、运行库前置和三系统验证矩阵见 `docs/user/COMPATIBILITY_ZH.md`。构建目标下沉不会改变播放器或软硬解策略；原版 Symbian³ / Anna 缺少 Qt 4.7.4 或 Qt Mobility 1.2.x 时，应先离线安装运行库，而不是改用另一套应用二进制。

### 3.3 应用标识和能力

- qmake 项目：`symbian/app/wiliwili_symbian.pro`；
- Target：`wiliwili_symbian`；
- UID3：`0xE000B100`；
- Capabilities：`NetworkServices ReadUserData`；
- 堆上限：`0x04000000`；
- 当前版本：0.9.0。

## 4. 上游复用策略

当前对齐的上游是：

- 仓库：`https://github.com/xfangfang/wiliwili.git`；
- 分支：`yoga`；
- Commit：`88e5876bea9502d06f46a8656e3530684d3aaf7d`；
- 版本主题：`Release version v1.6.0`；
- 许可证：GPL-3.0。

详细基线见 `docs/reference/UPSTREAM_BASELINE.md`。

### 4.1 已直接或等价复用的成果

- Bilibili API 路径、参数、Header 和错误语义；
- WBI mixin key、参数排序与签名逻辑；
- 二维码登录 v2 的轮询状态和 Cookie 规则；
- 首页、搜索、详情、评论、账号等数据结构与页面关系；
- 评论主接口、排序模式、楼中楼接口和兼容回退；
- 播放画质、备用 URL、Referrer、超时和低端设备画质限制思路；
- 主项目的整体信息架构和视觉方向；
- NanoVG、QR-Code-generator 以及少量可由 GCCE 编译的 C 代码。

### 4.2 无法直接复用、已经替换的部分

| 上游能力 | Symbian 端替代 |
|---|---|
| C++17 与现代 STL | GCCE 4.4.1 可接受的 C++/Qt 4 写法 |
| cpr/现代 TLS 客户端 | Symbian 原生 RHTTP + 自行管理 Header/Cookie |
| Borealis 完整框架 | 轻量页面模型 + NanoVG/GLES2 自绘 UI |
| nlohmann/json | Mongoose JSON 小组件和专用解析器 |
| mpv/FFmpeg | 0.7 原生 `CVideoPlayerUtility2` + Symbian MMF；Qt Mobility 路线仅保留为历史 |
| DASH 音视频双轨合流 | 音视频合一的渐进式 MP4 |

PSV/Switch 版的播放器经验可以复用在画质选择、备用线路、Header 和交互上，但不能直接复制 mpv 播放内核。PSV 端是应用内 mpv/FFmpeg 合流 DASH，并带平台硬解；Belle MMF 不会自动把两个 DASH URL 合为一个节目。

## 5. 当前软件结构

### 5.1 主链路

```text
AVKON 应用 / Qt 主窗口
        │
        ├── WiliwiliWidget：QGLWidget + NanoVG/GLES2 主 UI
        │       ├── 首页、栏目、详情、账号、设置
        │       ├── 触摸、滚动、底部导航
        │       └── 图片纹理与字体
        │
        ├── NativeTransport：Symbian RHTTP
        │       ├── API 请求、Cookie、TLS
        │       └── 图片/弹幕/视频选源请求
        │
        └── VideoPlayerWidget
                ├── VideoPlaybackBackend / CVideoPlayerUtility2（正常视频 + 兼容视频音频）
                ├── Mp4AvcProbeReader / HTTP Range（风险 SPS 与逐帧 sample demux）
                ├── FfmpegH264Decoder（风险视频轨本机软件解码）
                ├── MMF 原生 QWidget/CCoeControl/RWindow 视频宿主
                ├── soft opaque native RGB565 QWidget 视频表面
                ├── 单一顶层透明 ARGB 控制与弹幕窗
                ├── 原生 640×360 坐标、直接输入和低频音频时钟校准
                └── 播放控制、画质切换、弹幕调度与应用期原生表面复用
```

### 5.2 关键文件

| 路径 | 职责 |
|---|---|
| `symbian/app/wiliwili_symbian.pro` | 版本、源文件、库、能力、字体部署和打包入口 |
| `symbian/app/main.cpp` | 应用入口、启动日志和窗口显示 |
| `symbian/source/app/wiliwili_widget.cpp` | 主控制器、主 QGLWidget、导航、网络任务、搜索和播放器会话 |
| `symbian/source/ui/video_player_widget.cpp` | 原生横屏状态机、MMF `RWindow`、soft opaque RGB565 surface、上层单 ARGB 控制/弹幕窗和持久会话生命周期 |
| `symbian/source/platform/video_playback_backend.cpp` | `CVideoPlayerUtility2` 打开、显示窗口、rotation/scale、prepare/play 与 MMF 状态映射 |
| `symbian/source/network/native_transport.cpp` | Symbian RHTTP 请求、Cookie 和原生网络错误处理 |
| `symbian/source/network/bilibili_wbi.cpp` | WBI 签名 |
| `symbian/source/network/bilibili_*_parser.cpp` | 首页、详情、登录、播放和内容解析 |
| `symbian/source/model/login_session.cpp` | 登录 Cookie 和会话持久化 |
| `symbian/source/ui/*_screen.cpp` | NanoVG 页面与组件 |
| `symbian/app/resources.qrc` | 图标、占位图、内置精简字体 |
| `resources/font/switch_font.ttf` | SIS 安装的完整动态中文字体 |
| `wiliwili/` | 用于对照的上游源码快照 |
| `docs/research/player/PLAYER_0.7_SECOND_ENTRY_CRASH_ANALYSIS_ZH.md` | 第二次进入 data abort 的最新证据、CODA 判读和下一轮调查边界 |

## 6. 开发历程

### 阶段 M0：环境、构建与真机链路

- 从 Symbian Archive 找到并安装 Qt SDK 1.2.1 与 Belle `SymbianSR1Qt474`；
- 配好 Qt Creator Belle kit、GCCE、SBS、Qt Mobility 和 OpenGL ES 2.0；
- 完成 Qt 最小应用和能力探针；
- 用 Public CODA 通过 WLAN 完成安装、运行和调试；
- 建立 Debug/Release 构建、当前证书签名和完整 SIS 流程。

这是已经完成且不应重做的基础工作。

### 阶段 M1：渲染和应用骨架

- 验证 NanoVG C 源码可在 GCCE 的 GNU99 模式编译；
- 验证 GLES2/EGL 和 QtOpenGL 真机可用；
- 建立 QGLWidget + NanoVG 的主 UI；
- 完成触摸、滑动、列表、卡片和导航原型；
- 逐步从设置列表式原型重构为更接近 wiliwili 的内容界面；
- 修复中文字体缺字、字体部署和图片资源。

### 0.4.x：网络、主页和安装形态

- 接入 Symbian 原生网络请求；
- 首页 API 从离线样例切到真实 Bilibili 响应；
- 建立图片加载、视频详情和基本内容页；
- 加入二维码、搜索等入口；
- 处理手机图标启动、AppArc 资源和完整 SIS 安装差异。

### 0.5.x：账号、评论和第一个播放器

- 增加二维码登录会话和 Cookie 解析；
- 增加账号资料、评论、楼中楼和排序；
- 接入 Qt Mobility `QMediaPlayer/QVideoWidget`；
- 接入弹幕请求、解压、XML 容错和时间调度；
- 部分渐进式 MP4 真机出现画面和声音，证明 Belle 固件硬解和基本 MMF 路径可用。

### 0.6.0—0.6.5：播放线路、画质和账号修复

- 增加清晰度和直播选源；
- 修复二维码响应中多行 Cookie 的解析；
- 改为 raw Cookie Header，避免旧 RHTTP header codec 破坏会话；
- 增加 `Origin`、Referrer 和 Cookie 安全诊断；
- 明确 `Symbian:-34` 是 MMF 建立媒体连接失败，视频线路改为渐进 MP4 的 HTTP/CDN 备用；
- 排除“所有无画面都是 720P”这一错误假设：两个样本容器和编码相近，实际差异更偏向 CDN、文件组织和 MMF 建链；
- 用户确认很多视频开始有画面，也确认部分视频重试后会从无画面变为有画面。

### 0.6.6—0.6.9：横屏、弹幕层和前台所有权实验

- 尝试自动判断视频横竖比例并切换方向；
- 尝试透明顶层弹幕窗口，出现 WSERV 风险，随后移除；
- 搜索输入改为首页上方的短时工具窗口，避免 QGLWidget 内栅格子控件的 `QPainter` 错误；
- 延迟旋转、播放器会话锁和前台恢复保护仍未让第二个播放器顶层窗口稳定显示；
- 日志把 `Symbian:-12015` 收敛为 MMF 到显示设备的 blit 失败，而非下载或音频解码失败。

### 0.6.10：旋转时完整销毁/重建播放表面

- 把方向切换视为 MMF 显示设备销毁边界；
- 旋转前解除视频输出并销毁 `QMediaPlayer`、`QVideoWidget` 和顶层原生窗口；
- 旋转后按窗口、视频子窗口、MMF、媒体源顺序重建；
- 真机在方向改变后访问 `0x54` data abort。原因是仍有定时器或延迟事件访问处于半销毁状态的 `VideoPlayerWidget`。

### 0.6.11：保留播放器根窗口，只重建视频子树

- 不再销毁仍持有事件和定时器的播放器根对象；
- 只释放 MMF 和视频子窗口；
- 旋转与重建能够完成，日志能到达 `PLAYER_NATIVE_READY`、`PLAYER_SOURCE`；
- 但独立顶层播放器从未真正成为屏幕上的可见窗口，手机显示启动前画面/桌面，多任务中也没有可见画面；
- MMF 不依赖 Qt 顶层是否可见，因而后台仍有声音；
- 反复的 Activate/Deactivate 与窗口组争夺最终仍可能触发 data abort；
- 搜索可弹输入法，但旧搜索接口返回内容与关键词无关。

### 0.6.12：单顶层、主 QGL 窗口内嵌播放器

- 取消第二个播放器顶层，把 `VideoPlayerWidget` 变为主 QGL 窗口的子视图；
- `PLAYER_INLINE_ROOT_READY ... false` 已确认它不是顶层窗口；
- 把搜索端点改为上游当前的 `/x/web-interface/wbi/search/type` 并补齐 PC 参数；
- Debug/Release 构建和签名成功；
- 真机横屏仍失败，并连续输出：

```text
QPainter::begin: Paint device returned engine == 0, type: 1
```

这说明至少播放器的普通栅格 QWidget/覆盖层作为 QGLWidget 子树时没有可用的 Qt 栅格绘制引擎。结合手机仍无可见画面，0.6.12 的内嵌合成架构不可用。

更重要的是，0.6.3 曾经尝试过类似的“播放器作为主 QGLWidget 子页面”，0.6.4 的版本记录已经明确因相同 QPainter 问题撤销。0.6.12 实际重复进入了一个已经证伪的方向。后续接手者必须先阅读本报告的“不要重复尝试”部分。

### 0.7.0：原生 MMF 正式播放器、单 ARGB 弹幕层与原生表面复用

- 用 `CVideoPlayerUtility2` 取代 Qt Mobility `QMediaPlayer/QVideoWidget`，直接把 MMF 绑定到 Qt 拥有的原生 `RWindow`；
- 系统、AVKON 和主 QGL surface 始终保持 360×640 竖屏，横屏视频只对 MMF 显示窗口调用 `SetRotationL(EVideoRotationClockwise90)`；
- `AddDisplayWindowL` 延迟到 `MvpuoOpenComplete(KErrNone)` 后执行，解决过早绑定返回 `KErrNotReady (-18)`；
- 不再二次调用 Qt 所有 `RWindow` 的 `Activate()`，解决播放器构造前的 WSERV `SIGTRAP`；
- `KErrMMPartialPlayback (-12017)` 按 MMF 的可恢复语义处理，继续探测音视频轨并进入 `Play()`，而不是把六条 CDN 全部误判为失败；
- 控制栏与全部弹幕共用一个顶层 ARGB 窗口，显式启用 alpha channel，真机确认弹幕位于视频画面之上；
- 横屏 UI 不再把字体直接画进旋转 world transform。文字、控件和弹幕先在复用的 640×360 ARGB 缓冲中正常栅格化，再按设备像素 1:1 旋转 90°，真机确认 UI 清晰；
- 首次完整播放日志确认 `OPEN_COMPLETE 0`、`AddDisplayWindow 0`、rotation/scale 0、`PREPARE_COMPLETE 0`、视频轨和音频轨均可用，随后成功 `Play()`；
- 首次退出后第二次进入在 MMF 重建前触发 `SIGSEGV`。第一轮假设是旧实现从覆盖层输入事件中同步销毁 native sibling window，并在下一次复用原播放器根对象；
- 随后采用 one-shot 播放会话：退出时只停止并隐藏当前表面，主控制器立刻摘除旧会话，再以 `deleteLater()` 在输入事件返回后整体销毁；析构体先解除/销毁 MMF，再由 QObject 销毁 native QWidget；
- 最新完整日志确认 `PLAYER_SESSION_DELETE_LATER`、`DESTROY_BEGIN`、`DESTROY_READY` 全部执行，第二次也创建了新的播放器会话和新的原生覆盖控件。相同的 C++ 会话地址只是分配器复用已释放地址，不能作为旧对象仍存活的证据；
- 尽管会话隔离已生效，第二次仍在 `PLAYBACK_READY` 后、`PLAYER_REBUILD_BEGIN` 前 data abort，手机报告访问 `0x140`，PC 为 `0x7d4d55c2`。该时点第二个 MMF 后端尚未创建，故障汇编是通过无效虚表指针调用虚函数，当前最强假设转为 Qt 顶层 ARGB 覆盖窗的 backing store/paint engine 或延迟窗口事件；
- Qt Creator/CODA 把 Symbian exception/panic 通用映射为 GDB `SIGSEGV`。用户随后确认 Release、Debug 从手机独立启动同样崩溃，CODA 和 UDEB-only 均已排除；packet-size 协商和共享库符号警告不是根因；
- 第一候选把 ARGB 覆盖窗改为主应用拥有的持久实例，每个 one-shot MMF 会话仅 attach/detach owner；但 Nokia 603 真机确认 `overlayreuse1` 第二次播放仍会卡死退出，因此仅覆盖窗重建已排除为充分原因；
- 当前 `surfacepersist1` 让 `VideoPlayerWidget`、native video host `QWidget/CCoeControl/RWindow`、`VideoPlaybackBackend` observer、`CVideoPlayerUtility2` 和 ARGB overlay 全部存活到应用退出。播放退出只停止、解绑和隐藏；下一次在同一 utility 上 `Close` 旧媒体后重新 `OpenUrlL/OpenFileL`；
- 修复后的 Debug/Release 均为 `sbs errors: 0`，对象文件已核对复用标记，`surfacepersist1` SIS 已使用 2026—2036 有效证书重签；用户随后确认可以退出后再次播放。完整历史证据见 `docs/research/player/PLAYER_0.7_SECOND_ENTRY_CRASH_ANALYSIS_ZH.md`。

## 7. 0.6.12 最后失败的根因分析（历史基线）

### 7.1 0.6.12 日志说明了什么

最后一份 0.6.12 日志中的关键顺序是：

```text
WW:PLAYER_SESSION_ACTIVE
WW:PLAYER_ORIENTATION_REQUEST true true
WW:PLAYER_SURFACE_RELEASED_FOR_ORIENTATION
WW:PLAYER_PAGE_PREPARED ... false 3840 2160 true true
WW:PLAYBACK_READY 16 "mp4" 4 3840 2160
WW:DANMAKU_READY 120
WW:PLAYER_ORIENTATION true 0 QSize(640, 360)
WW:PLAYER_REBUILD_BEGIN true QSize(640, 360) false
WW:PLAYER_INLINE_ROOT_READY QSize(640, 360) ... false
WW:PLAYER_SURFACE_REBUILT_AFTER_ORIENTATION ... QRect(0,80 640x218)
WW:PLAYER_NATIVE_READY ...
WW:PLAYER_SOURCE 1 6 false
QPainter::begin: Paint device returned engine == 0, type: 1
```

可以据此排除以下错误方向：

- 不是没有进入播放函数；
- 不是没有获得播放地址；
- 不是没有解析清晰度；
- 不是横屏请求本身失败，AVKON 已返回 640×360；
- 不是 `QVideoWidget` 完全没有创建；
- 不是弹幕没下载，弹幕已解析到 120 条；
- 也不是单纯“再多等 300 ms”就会解决的问题。

失败发生在显示合成层：Qt 的 GL 主窗口、普通 raster QWidget、Symbian 原生视频窗口和 MMF 输出表面之间没有形成一条有效、可见且可绘制的窗口树。

### 7.2 为什么竖屏可以播放，横屏却只有声音

竖屏路径不改变应用的显示设备：

1. AVKON 根窗口保持 360×640；
2. 主 EGL/QGL 表面不重建；
3. `QVideoWidget`/MMF 绑定到一个已经存在且可见的原生窗口句柄；
4. MMF 能把解码帧送到这个稳定目标，所以支持的文件能有画面和声音。

横屏则多出一个破坏性的边界：

1. `SetOrientationL()` 把设备改为 640×360；
2. Qt for Symbian 调整或重建 EGL window surface、CCoeControl/RWindow、子窗口和 Z 顺序；
3. MMF 视频输出可能仍指向旋转前的窗口/屏幕设备，或者新建的视频窗口不在当前可见窗口层级；
4. 音频输出不依赖屏幕窗口，因此继续播放；
5. 视频解码即使成功，也无法 blit 到有效可见目标，表现为无画面、退回桌面/CODA 或 `KErrMMVideoDevice (-12015)`。

因此，“竖屏可播”并不与“横屏窗口架构失败”矛盾，反而是关键证据：基本网络、容器、硬解和 MMF 音频链路已经存在，新增故障集中在旋转后的显示目标生命周期。

仍需区分另一类问题：少数视频在竖屏稳定路径也无画面，才更可能属于 Belle MMF 不支持某个 H.264 profile/level、MP4 组织、码率、分辨率、CDN Range/TLS 或特定线路。不能把这类媒体兼容问题和横屏所有视频的窗口问题混为一谈。

### 7.3 当前置信度

| 结论 | 置信度 | 依据 |
|---|---:|---|
| 横屏主要失败在显示窗口/表面，而非 API | 高 | `PLAYBACK_READY`、`PLAYER_SOURCE` 正常，仍无画面 |
| 音频正常不代表视频窗口正常 | 高 | 多轮真机均出现后台有声音、屏幕无画面 |
| 继续调整延时不能根治 | 高 | 180 ms、620 ms、分阶段延时和会话保护都尝试过 |
| QGLWidget 内普通 raster 覆盖层不可用 | 高 | 0.6.3、搜索子控件和 0.6.12 均出现同类 QPainter engine 0 |
| 直接顶层 QVideoWidget 在旋转后未进入正确可见窗口层级 | 高 | 0.6.11 原生句柄和媒体均就绪，手机仍看不到顶层 |
| 所有剩余无画面视频都是编码问题 | 低 | 同一视频有时重试后可显示，CDN/连接也会影响结果 |

## 8. 已证伪或不应原样重复的方案

### 8.1 不要再用计时器“修”顶层播放器

已尝试过不同稳定等待、`showMaximized()`、`raise()`、`activateWindow()`、AppArc 前台恢复和播放会话锁。它们能改变日志顺序，不能建立可靠的 AVKON/MMF 可见窗口关系。

### 8.2 不要再把普通 QWidget 播放器/输入框直接作为 QGLWidget 子控件

Qt 4.7 Symbian 上此组合多次出现 `Paint device returned engine == 0`。搜索框已经因此改为短时工具窗口；播放器 0.6.12 又重复了相同限制。

### 8.3 不要销毁仍接收事件的播放器根 QWidget

0.6.10 的 `destroy()` 路径导致访问 `0x54`。方向改变时可以释放 MMF 后端和明确拥有的原生视频资源，但不能让活动定时器、网络回调或延迟事件指向半销毁对象。

### 8.4 不要用很多透明顶层窗口覆盖弹幕

该方案会增加 WSERV 窗口数量和窗口组/Z 序风险，并出现过 WSERV 相关异常。它也没有解决播放器窗口本身不可见的问题。

### 8.5 不要先移植 FFmpeg/mpv 来解决当前横屏问题

FFmpeg/mpv 能改善格式与 DASH 兼容，但当前失败首先是窗口和显示表面。即使换解码器，仍要把帧可靠显示到 Belle 屏幕。GCCE/ARMv5 下移植现代 mpv 还是一个独立的大工程。

### 8.6 不要把 Qt Creator 启动成功等同于完整安装成功

CODA 可直接推送/运行 EXE，但手机图标依赖 SIS 中的 AppArc 注册、资源、字体、能力和安装盘路径。每个里程碑都要保留“Qt Creator 调试启动”和“完整 SIS 图标冷启动”两套验收。

## 9. 现存问题与优先级

### P0：必须先解决

1. **媒体编码兼容范围**
   `surfacepersist1` 已解决确定性的第二次进入崩溃。对照实验已经证明 BCM2727 能解析正常流，却在同样 API/Annex-B/MMF 已关闭条件下拒绝 7/4/7/weighted 风险模板，因此旧 `codeccompat1` 链路不能成为正式兼容后端。1.0 前停止扩大硬解边界：ARM `0x102073EF` 只作为本机软件 decoder 候选，失败则实现本机 360P 软件解码；内置软解仍失败时允许用户确认后调用外部播放器。默认源码保留 MMF AAC，并用显式宏隔离旧 BCM 实验。不使用桥接或远端转码。详见 `docs/archive/plans/PLAYER_1.0_DECODING_POLICY_ZH.md`。

2. **播放器压力稳定性**
   用户已确认可以重复播放，但 Release 50 次正式循环仍未完成。继续记录相同对象地址、残留透明窗、后台声音和内存增长，不再把确定性的第二次进入崩溃列为未修复状态。

3. **覆盖层长期稳定性**
   单 ARGB 窗口、弹幕和清晰 UI 已首次真机通过；仍需验证长时间播放、控制栏反复隐藏、弹幕密集场景、前后台切换和内存峰值。

### P1：主功能缺口

1. WBI 搜索结果相关性尚未完整真机验证；
2. 搜索工具窗口仍可能在输入法出现后丢失焦点；
3. 新二维码从生成、扫码、确认到账号页的完整流程需要回归；
4. 评论页需要更清晰的信息层级、分页/加载更多和楼中楼交互；
5. 直播线路仍有证书、容器和无音画问题，直播弹幕未实现；
6. 手机图标冷启动和异常退出后的残留进程需要稳定性测试；
7. 部分渐进 MP4 仍受 CDN、Range、固件解码能力和文件组织影响。

### P2：体验和完善

- 动态、消息、设置页补足真实内容与操作；
- 首页、详情和评论的排版继续贴近 wiliwili；
- 缩略图并发、缓存、占位和失败重试；
- 下拉刷新、加载状态和错误提示；
- 性能、内存、后台/前台、断网恢复和长时间播放；
- 应用内退出、系统返回键和屏幕方向策略统一。

## 10. 0.7 已采用架构：保持系统竖屏并旋转视频

0.6.12 报告提出的首选路线已经由 0.7 实现并得到首次真机成功结果：应用始终保持 360×640 竖屏坐标；遇到横屏视频时，只把视频内容、播放器 UI 和输入坐标旋转 90°，用户把手机横过来观看。以下 10.1 记录正式架构依据；10.2—10.7 保留为历史备选，不应在 0.7 主路线回归完成前启动。

横屏切换导致界面消失/回到系统菜单、以及 soft decoder 的旋转性能影响，详见 `docs/research/player/PLAYER_ORIENTATION_PROBLEM_AND_WORKAROUND_ZH.md`。该专题也记录了真实横屏实验的闪退结论和后续路线 A/B；本节的 portrait+virtual 架构仍是当前主线。

### 10.1 正式路线：固定竖屏窗口，MMF 视频旋转 90°

这条路线有 Belle SDK 的直接接口支持，不只是软件绘图设想：

- `TVideoRotation` 定义了 `EVideoRotationNone`、`Clockwise90/180/270`；
- `CVideoPlayerUtility::SetRotationL(TVideoRotation)` 可设置整个视频输出的旋转；
- `CVideoPlayerUtility2::SetRotationL(const RWindow&, TVideoRotation)` 可只旋转指定显示窗口；
- `CVideoPlayerUtility2` 同时提供 `SetAutoScaleL`、`SetVideoExtentL`、`SetWindowClipRectL`；
- Belle SDK 已提供 `mediaclientvideo`、`mediaclientvideodisplay` 和 MMF controller import library。

Qt Mobility `QVideoWidget` 公共 API 没有 rotation 属性，只暴露全屏、宽高比、亮度、对比度、色调和饱和度，因此 0.7 已经用一个薄的 Symbian 原生播放桥取代正式 `QMediaPlayer/QVideoWidget` 路径，直接使用 `CVideoPlayerUtility2` 和明确持有的 `RWindow`，不依赖 Qt Mobility 未公开的私有后端对象。

0.7 已采用的显示模型：

```text
系统/AVKON/QGL：始终 360×640 portrait，不调用 SetOrientationL

横屏视频：
  视频 RWindow 占据 360×640
  MMF SetRotationL(EVideoRotationClockwise90)
  AutoScale/VideoExtent 让旋转后的 16:9 内容铺满窗口
  播放器逻辑使用虚拟 640×360 坐标

竖屏视频：
  MMF rotation = None
  播放器逻辑使用正常 360×640 坐标
```

播放器 UI、弹幕和触摸统一使用 640×360 的虚拟横屏坐标。绘制阶段以 640×360 原方向栅格化，再做 1:1 的 90° 像素旋转；输入阶段做逆变换：`portrait screen (360×640) ↔ virtual landscape (640×360)`。当前 Nokia 603 使用顺时针方向并已确认观感和清晰度正常。

这条路线的主要优点：

1. 不触发 AVKON 方向切换，不重建 QGL/EGL surface；
2. MMF 视频窗口从创建到关闭一直绑定同一个 portrait `RWindow/CWsScreenDevice`；
3. 直接复用已经成功的竖屏视频显示基础；
4. 不再出现旋转后顶层窗口进错窗口组的问题；
5. 横屏和竖屏视频只改变视频 rotation 与虚拟布局，不改变应用生命周期。

当前验证结果和剩余风险：

- Nokia 603 所选 MMF controller 已确认 `SetRotationL`、auto-scale 和显示窗口配置成功；
- Qt/NanoVG 控制层仍不能作为普通 raster QWidget 塞进 QGL 子树，该限制没有改变；
- MMF 原生视频层覆盖 QGL 的问题已通过一个顶层 ARGB 窗口解决，首次播放确认弹幕和控件位于视频上方；
- 不再为每条弹幕创建单独顶层窗口，全部控制和弹幕都在同一覆盖层绘制；
- 旧包的剩余风险正是 ARGB 顶层窗口的重复创建/绘制生命周期；当前候选已消除跨会话 destroy/create，但尚未由真机证明稳定；
- 播放期间出现的系统对话框仍按 portrait 方向显示，因此播放链路应尽量避免弹出输入法或系统确认框。

稳定验收：正式 Release 在手机图标启动路径连续播放和退出 50 次，无 `APPLICATION_DEACTIVATE` 循环、无残留覆盖窗、无后台声音、无 `Symbian:-12015`、无 data abort。当前尚未达到该门槛。

### 10.2 历史备选：播放器不存在时先旋转主 UI

0.6.12 的实际顺序是：创建播放器子视图、取得播放信息并启动弹幕任务，然后才旋转；日志中的 `PLAYBACK_READY` 和 `DANMAKU_READY` 都早于 `PLAYER_ORIENTATION`。0.6.10/0.6.11 虽然旋转后重建 `QVideoWidget/MMF`，但播放器根窗口在旋转前已经存在。因此，以下流程并未被等价验证：

```text
用户点击播放
→ 只记录待播放的 aid/cid/画质，不创建 VideoPlayerWidget/QMediaPlayer/QVideoWidget
→ 关闭搜索工具窗口，停止首页定时绘制和非必要网络回调
→ 主 AVKON/QGL 窗口仍可见，调用 SetOrientationL(landscape)
→ 等待 resizeEvent 确认 640×360
→ 再等待至少两个成功的 QGL 首帧/交换缓冲周期
→ 暂停或隐藏 NanoVG 页面绘制
→ 此时才在稳定的横屏窗口组中创建原生视频窗口、QVideoWidget 和 MMF
→ 绑定媒体源，开始播放；最后再启动弹幕和控制层
```

退出必须严格逆序：

```text
停止弹幕与控制定时器
→ stop 并解除 MMF video output
→ 销毁视频子窗口/播放器窗口
→ 恢复 NanoVG 主 UI 并确认一帧
→ 最后 SetOrientationL(portrait)
→ 360×640 稳定后恢复首页网络和输入
```

这项实验直接针对“MMF/视频窗口继承旋转前句柄”的高概率原因。验收日志必须证明在 `PLAYER_ORIENTATION ... QSize(640,360)` 和两个 GL 稳定帧之前，没有构造播放器、没有 `PLAYBACK_READY`、没有弹幕定时器，也没有任何视频原生窗口。

但它不能继续采用 0.6.12 的普通 raster QWidget 作为 QGLWidget 子控件，因为该组合已经由 `QPainter engine == 0` 证伪。横屏稳定后应创建一个**此时才出现**、属于当前 AVKON window group 的原生视频窗口/顶层播放视图；或者暂时完全停止 QGL 绘制，让视频窗口独占客户区。第一轮只要求本地已知可播 MP4 的画面、声音和退出恢复，不做控制栏和弹幕。

如果此流程成功，说明无需立刻重写解码器，之后再逐步恢复网络选源、控制栏和覆盖层。如果仍然只听到声音，才进入下面的独立 probe，避免继续在完整应用中猜测。

### 10.3 历史备选：本地文件、无 QGL 的最小播放器

新建独立 `native-video-probe`：

1. 使用普通 Qt QWidget/AVKON 宿主，不创建 QGLWidget；
2. 播放一段已知可由手机系统播放器播放的本地 360p H.264/AAC MP4；
3. 先竖屏，再调用 AVKON 方向切换到横屏；
4. 对比 Qt Mobility `QVideoWidget` 与直接 Symbian MMF API；
5. 记录原生窗口句柄、窗口组、尺寸、可见性和错误；
6. 连续进入/退出、横竖切换至少 50 次，无黑屏、无残留、无 data abort 才通过。

这个实验不请求 Bilibili、不加载弹幕、不绘制 NanoVG。只有这样才能确定是 Qt Mobility 包装层的问题，还是 MMF/AVKON 本身的问题。

### 10.4 已落地实现：原生 Symbian 播放模式

0.7 已把 SDK 中 `CVideoPlayerUtility2`、`CCoeControl`、`RWindow` 和 `CWsScreenDevice` 接成正式后端：

- 主应用保持同一个 AVKON window group 和 360×640 QGL surface；
- 播放期间暂停主 NanoVG 内容绘制，但不销毁 EGL surface；
- 一个 Qt 拥有的原生子 `RWindow` 承载 MMF 显示矩形；
- 媒体 open 成功后才 `AddDisplayWindowL`，然后配置 rotation、scale、extent 和 clip；
- 一个顶层 ARGB Qt 窗口统一绘制控制栏和弹幕，并显式启用 WSERV alpha channel；
- 该 ARGB 窗口由主应用持有并跨播放复用；退出先停止覆盖 timer、解绑受保护 owner、清空事件状态和隐藏；
- controller、native video host、后端 observer 和 `CVideoPlayerUtility2` 同样存活到应用退出，播放间不再销毁任何 Qt/WSERV/MMF 原生对象；
- 完整对象图复用已经消除用户此前稳定复现的第二次进入 data abort；H.264 本机兼容后端已经接入该持久对象图，当前 P0 是真机样本验收，同时保留 50 次 Release 压力门槛。

### 10.5 历史备选：把主壳从 QGLWidget 改为普通 QWidget/AVKON

如果最小普通 QWidget + QVideoWidget 在横屏稳定，可考虑：

- 让应用根窗口成为普通 QWidget/AVKON 宿主；
- 浏览页面中的 NanoVG 作为独立 GLES 子表面或按页面启停；
- 播放页使用普通 raster/native 子树；
- 不让 QVideoWidget 成为 QGLWidget 的子控件。

这是较大的 UI 架构重构，会影响已经稳定的字体、图片和 NanoVG 页面，但有机会让 Qt 栅格控件和 QVideoWidget 回到其支持路径。

### 10.6 历史备选：先交付竖屏播放器 MVP

如果原生横屏短期仍无法稳定，可保留不触发方向切换的竖屏播放器作为阶段性可用版本。视频按比例显示在竖屏上方，控制和详情位于下方。这不能满足最终目标，但比“横屏只有声音”更适合作为可测试基线。

### 10.7 历史备选：调用系统播放器

可研究通过 AppArc/文档处理器把本地或可访问 URL 交给系统视频播放器。优点是系统方向和硬解更成熟；缺点是无法稳定叠加 wiliwili 控制、弹幕和登录 Header，只适合作为兜底。

### 10.8 媒体兼容的后续阶段

播放器窗口通过后，再处理：

1. 用本地样本建立 Belle MMF 支持矩阵：H.264 profile/level、分辨率、码率、moov 大小和位置；
2. 对 Bilibili 渐进 MP4 做 Range、Content-Type、Content-Length 和 CDN 排序；
3. 对失败源尝试下载到临时文件后交给 MMF，区分网络建链与解码；
4. 探测并评估 `CMMFDevVideoPlay` 直连原生 AVC decoder，绕过当前媒体 controller；
5. 原生 decoder 仍不兼容时，独立验证本机 360P OpenH264/裁剪 libavcodec、GLES 上传和 MMF 音频同步。

### 10.9 历史实现：`codeccompat1` BCM 候选（已被真机否决）

- 修复播放 API 格式为 `mp4720` 时旧精确 `mp4` 判断不生效的问题，现在接受 `mp4*`；
- HTTP Range 首段解析 progressive MP4 视频轨，只有 7/4/7/1,2 模板接管，正常 4/3/4/0,0 继续 MMF；
- 根据 `stsz/stsc/stco|co64/stts/stss` 从最近同步帧开始，每约 5 秒请求一批，单次上限 6 MiB；
- 每个 MP4 sample 独立转换为一个 Annex-B access unit；SPS/PPS 在开始和 seek 后注入首帧；
- DevVideo 使用 BCM2727 UID `0x10204C21`、`EDuCodedPicture + EDuElementaryStream`、DTS 和 sequence number，不再按任意 32 KiB 字节块喂流；
- MMF 媒体不再关闭：它继续播放 AAC 并提供 position；只禁用其不可用的视频轨，DevVideo 使用 `CSystemClockSource` 并在漂移超过 150 ms 时校正；
- 内存输出优先采用 YUV420 planar/semi-planar，兼容 RGB565/RGB888 和 YUV422，按覆盖窗所需最大 640×360 转为 QImage；
- 持久 ARGB 窗口先绘制视频，再画弹幕和控制；该条最初采用整帧 1:1 旋转，现已由第 25 节的 native-landscape 直接 640×360 合成取代；
- pause/resume、拖动 seek、持续 Range、`InputEnd`、新媒体、退出和重复进入生命周期均已接入；超出 BCM2727 1280×720 上限或硬解失败时请求 Q16；
- 六轮完整 ARMv5 Debug 与四轮完整 ARMv5 Release 构建均 `sbs errors: 0`，但编译通过不等于固件接受码流。2026-08-26 控制组确认 BCM 对故障 header 稳定返回 -5，因此 `codeccompat1` 和后续 `headercontrol1` 都只能作为历史诊断证据，不能再标记为正式候选。

### 10.10 已完成：`headercontrol1` 对照实验与源码退役

- 正常 High@5.1/4-ref 流：MMF prepare 0，BCM header 返回 0 和 640×360；
- 故障 High@3.0/7-ref/DPB7/weighted 流：MMF -12017，BCM header 稳定 -5；
- ARM `0x102073EF` 对同一故障 header 返回 0，但尺寸是 0×0；
- `headercontrol1` 主动关闭 MMF，所以所有视频数秒后停止属于诊断设计，不是播放器回归结论；
- 专用 BCM/ARM control 函数和参数已经从源码删除；默认构建让正常流 `PROFILE_SKIP`，让风险流保留 MMF AAC。旧 BCM 接管还受 `WILIWILI_ENABLE_UNSUPPORTED_BCM_DEVVIDEO` 显式宏保护。

### 10.11 历史失败实验：`armsoftprobe1`

- MP4 reader 已支持 `ctts` version 0/1；sample/access unit 同时保存 DTS 与 PTS，DevVideo 输入同时设置 decoding/presentation timestamp；
- `CONFIG+=armsoftprobe1` 通过独立宏选择系统 ARM H.264 decoder UID `0x102073EF`；普通构建默认不启用，旧 BCM 实验宏也不会被连带开启；
- 只有 7/4/7/weighted 风险模板进入 ARM 路线，正常模板仍 `PROFILE_SKIP` 并完全由 MMF 播放；
- ARM 路线按约 2 秒批次持续取样，保留 MMF audio-only 会话作为 AAC 播放与主时钟，不调用旧 `closeMedia()`；
- 软件帧继续交给现有 640×360 视频/弹幕/控制合成层，未修改已稳定的横屏旋转和 persistent native surface 架构；
- 实验 Debug 和无实验宏的标准 Debug 均完成 GCCE ARMv5 全量构建，`sbs errors: 0`；标准构建检查确认不含 `WILIWILI_ENABLE_ARM_SOFT_DECODER_PROBE`；
- 当前真机包：`symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_armsoftprobe1_currentcert.sis`，9,241,072 bytes，SHA-256 `17730DC83BAD4FDC66AF35FE721AE6A2C6D56D772EC0B1CEDCF7B370F2E0CF3E`；
- Nokia 603 最终日志确认 `ConfigureDecoderL()=-5`，无法 Initialize 或出帧。该路线已经废止，上述包和代码说明只保留作历史证据。

### 10.12 FFmpeg 真机首帧通过与 `ffmpegsoft2` 性能阶段

- PPSSPP-FFmpeg 源码固定在 commit `b87f7c6d522d1edba77cfc4fac96ce48a236f806`（FFmpeg 3.0.2），用 GCCE 4.4.1 从源码构建 H.264-only `libavcodec/libavutil`，不链接 ABI 不兼容的旧预编译库；
- `ffmpegsoft1` 已在 Nokia 603 让原来只有声音的视频显示软件解码画面，证明 MP4 AU、H.264 解码、DTS/PTS、MMF AAC 主时钟以及视频/弹幕/控制合成出口整体成立；
- 用户观察纯 C 版只有约 2–3 fps，所以“编码兼容”已有实证，“可用性能”仍未通过；
- `ffmpegsoft2` 静态库面向 ARM1176JZF-S，启用 ARMv5TE/ARMv6/VFPv2/inline asm 并禁用 NEON。全库 `-O2` 会令最终 `.rodata` 到达 `0x4A50BB`、撞上 `0x400000` 的固定 `.data` 基址；`-O3` 又会在 `libavutil/tea.c` 触发 GCCE 内部错误，因此最终保留可链接的 `-Os`；
- 后续源码审计纠正了“ARM 汇编已启用”的含义：H.264 qpel、chroma、intra prediction、weighted/biweighted prediction、deblock 和 IDCT 快路径均只在 NEON 上注册，CABAC inline 快路径要求 ARMv6T2；Nokia 603 的 ARM1176 都不具备，现有 ARMv6 对 H.264 的明确主要收益只有 start-code 扫描；
- 常用 640×360 YUV420→RGB565 路径已消除逐像素除法并复用色度运算；落后 MMF 音频超过 500 ms 时跳过非参考帧/全部 deblocking，追回 150 ms 内恢复；
- 新日志 `WW:FFMPEG_SOFT_TIMING` 分别累计 wall/decode/convert 时间，`WW:FFMPEG_SOFT_CATCHUP` 记录追帧状态。当前静态库已重建，按用户要求不再由本会话编译应用或推送手机。
- 首轮 `ffmpegsoft2` Nokia 603 日志已得到 300 帧/57,479 ms（约 5.2 fps）：decode 9,583 ms，convert 46,785 ms。约 81% 墙钟在应用 YUV420→RGB565，证明当前主瓶颈不是 libavcodec H.264；
- 第二轮源码保留 640×360，单独对转换函数启用 GCC 热函数优化，按 2×2 luma block 复用 U/V，并用系数/裁剪表生成 RGB565；其 READY 标识为 `ARMV6_ASM_RGB565_LUT2X2`。
- 第二轮真机验证确认上述优化有效：300 帧从 57,479 ms 降到 25,014 ms（约 12.0 fps），转换从 46,785 降到 12,555 ms（约 42 ms/帧），总速度提升 2.3 倍、转换提升 3.7 倍；600 帧时为 52,550 ms（约 11.4 fps），仍未达到实时门槛；
- 第三轮 late-drop 控制组已经完成：第一段显示 275、跳过 491 次转换、耗时 47,701 ms（约 5.8 可见 fps）；第二段显示 125、跳过 153 次、耗时 17,529 ms（约 7.1 可见 fps）。两段媒体 PTS 都只推进约 0.78× 实时，因此该策略不能交付；普通 `ffmpegsoft2` 已关闭，只有额外 `CONFIG+=ffmpeglatedrop1` 才能重现；
- 用户提供的 UCPlayerEx 报告证明其内置静态 FFmpeg、H.264 ARM qpel 痕迹、解码线程和 direct-bitmap 路径，且报告同类第二视频可接近 30 fps。但定向导入表同时确认 UC 使用完整 `CVideoPlayerUtility` 播放/窗口/Direct Screen Access API；故当前只能判为 MMF + FFmpeg 混合播放器，不能把所有 H.264 性能都归因于纯软解。UC 专有二进制仅作上限/架构参考，不作为代码来源。
- 2026-08-27：`ffmpegsoft2` 已合并到主线。`symbian/app/wiliwili_symbian.pro` 版本为 0.9.0，普通 qmake/Build-App 不再需要额外 `CONFIG+=ffmpegsoft2`，会自动编译 `FfmpegH264Decoder`、CPU RGB565 软输出路径并链接 GCCE 版 `libavcodec.lib`/`libavutil.lib`；旧 GLES-YUV 代码仅保留历史研究用途，`ffmpeglatedrop1` 仍是诊断开关且未进入正式包。正常码流仍走 MMF，已知故障模板才走本机 FFmpeg。
- 2026-08-27 最终输出优化：参考 PotatoStream 可迁移的“无 swscale/关闭 loop filter/低缓冲”思路，但不复制其 3DS Y2RU、hard-float 或 `mpcore` 平台代码。当前默认由 `FfmpegH264Decoder` 在线程内完成紧凑 YUV 平面复制；consumer 先按音频时钟选出最新可显示帧，再复用 `RGB565_LUT2X2` 转换并交给既有 ARGB overlay 的 QPainter 绘制。这样丢弃过期帧时不会再支付 RGB 转换成本，且仍绕过已证实过慢的三平面 GLES texture upload。第 25 节进一步把弹幕/控制层改为原生 640×360 直接合成，旧 GLES 路径仍只作历史研究。
- 软件回退常态使用 `skip_loop_filter=AVDISCARD_NONREF`，落后策略、PTS、Range、queue 与取帧调度保持不变。故障流 READY 应为 `ARM11_GENERIC_H264_RGB565_LUT2X2`；软解不再请求 YUV420 输出。
- 旧 0.9 Debug/Release GLES-YUV 包的 GCCE/SIS 构建记录保留作历史证据；源码切换后的新包尚未在本会话编译，不能把旧包作为当前 renderer 候选。
- 2026-08-27 路由修复：首轮 Debug 只有 `GLES_YUV_READY`，但 Range header 请求最终为 `DEVVIDEO_RANGE_TIMEOUT 1 0 0 20 20`，所以没有 `FFMPEG_SOFT_BEGIN`、GLES surface 或 YUV 首帧；不能把该会话归因于软解卡顿。`VideoPlayerWidget` 现在仅对软解构建的首个 MP4 来源延后 MMF URL 打开，先预取 header，风险模板再预取首批 AU；成功后启动 MMF AAC 并等待 LoadedMedia，安全/所有失败分支都显式退回 MMF。GCCE Debug 已通过；既有签名 0.9 SIS 早于本修复，不能作为此路由修复的验证包。
- 2026-08-28 官方 playurl 重查：Q6 仍是文档定义的 HTML5 progressive-MP4 编号，但对四个已知故障样本、两个正常样本、早期 AV、旧 PGC 样例以及用户指定的四 P `BV15EhG6qEAg` 请求 `qn=6&platform=html5&fnval=1`，当前区域/未登录响应全部实际返回 Q16。Q6/F1 与 Q16 对照为同一 `...-1-16.mp4`、同时长和同字节数。临时加入的 Q16/Q6 能力链已按用户要求完整撤回，因为它只增加网络延迟和状态复杂度。当前恢复单向路由：安全流留在 MMF，已知硬解风险流直接进入本机 FFmpeg。

## 11. 下一会话的第一批任务

接手后建议严格按顺序执行：

1. 先阅读 `docs/research/player/PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md` 和 `docs/research/player/PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md`；不要把 `headercontrol1`、`codeccompat1` 或旧 BCM 诊断包当成正式基线；
2. 用当前源码重新构建 Debug/CODA 或用户包，验证故障样本从 `PLAYER_SOURCE_DEFER_MMF` 进入本机 FFmpeg，日志应出现 `FFMPEG_SOFT_READY ... RGB565_LUT2X2`；不要以启动期 `GLES_YUV_READY` 判断当前路径；
3. 用原故障样本至少播放 30 秒，保存 `FFMPEG_SOFT_READY/TIMING/CATCHUP/PROGRESS`，记录体感帧率、音画同步、pause/seek 和 UI 响应；正常样本应继续 `PROFILE_SKIP`/MMF；
4. 若 CPU RGB565 软解实测仍低于可接受门槛，按既定政策保留兼容实现并转入外部播放器回退设计，不再恢复 BCM/系统 ARM 硬解探索；
5. 完成 Release 50 次循环，检查残留透明窗、后台声音、内存增长和主 UI 恢复；
6. 最后回归 WBI 搜索、二维码登录、评论、冷启动、直播线路和直播弹幕。

## 12. 构建、签名和真机工作流

### 12.1 命令行构建

在仓库根目录的 PowerShell 中：

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-App.ps1 -Configuration debug
.\symbian\Build-App.ps1 -Configuration release
```

构建会写入 Belle SDK 的 `epoc32` 目录。Debug EXE 的典型位置：

```text
C:\QtSDK\Symbian\SDKs\SymbianSR1Qt474\epoc32\release\armv5\udeb\wiliwili_symbian.exe
```

### 12.2 签名

- 工具：`C:\QtSDK\Symbian\SDKs\SymbianSR1Qt474\epoc32\tools\signsis.exe`；
- 当前证书：`symbian/out/current-signing/qt-selfsigned-current.cer`；
- 当前私钥：`symbian/out/current-signing/qt-selfsigned-current.key`；
- 证书有效期：2026-08-24 至 2036-08-21；
- 私钥不可提交或对外发布。

### 12.3 用户约定的部署方式

旧 GLES-YUV 候选的代码、依赖库、Debug/Release 构建、去旧签名、当前证书重签和静态包检查均已完成并归档为历史证据；CPU RGB565 路线切换后的新包尚未在本会话编译，需由用户在 Qt Creator 重新构建后真机验证。Qt Creator 的手工步骤保留为需要 Debug/CODA 日志时的备用流程。

Qt Creator 操作要点：

1. 手机打开 CODA，确认 `Connected`；
2. Qt Creator 选择 `Qt 4.7.4 for Symbian Belle` 的 Debug 或 Release 构建配置，直接重新运行一次 qmake；0.9 主线已默认启用 `ffmpegsoft2`，不要加入 `ffmpeglatedrop1`；
3. 在项目运行设置中确认设备地址和端口；
4. 普通运行用于快速日志测试，调试运行用于崩溃位置；
5. 需要验证图标、字体和 AppArc 时，必须完整安装签名 SIS，再从手机图标冷启动；
6. 每次测试先确认应用管理器中的版本号，避免实际运行旧包；
7. 发现“打不开但后台仍运行”时，先在任务管理器关闭旧进程，再进行下一轮。

不要在日志中发送完整 Cookie、二维码 URL 或带签名的媒体 URL。现有 `COOKIE_WIRE`、Cookie 名称/长度和 URL 哈希日志足够诊断。

## 13. 当前产物

0.7.0 `surfacepersist1` 是用户已确认可重复进入的回归基线。`devvideoprobe1`、`devvideosample1`、`codeccompat1` 和 `headercontrol1` 都只保留为历史路线判定包；`ffmpegsoft1` 已解决“完全无画面”但性能不合格；`ffmpegsoft2` 已合并为 0.9 主线，并已生成当前证书签名的最终验证包：

| 配置 | 文件 | 大小 | SHA-256 |
|---|---|---:|---|
| Debug surfacepersist1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_surfacepersist1_currentcert.sis` | 9,163,400 | `78667C76978D27BCF331AE41C6A11A7B8921DCD4ED00854A2960515F2BB6DB0E` |
| Release surfacepersist1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_surfacepersist1_currentcert.sis` | 9,168,428 | `4BEBC2E8151FD688E4A7252E3DC86C89E3C562D5AB13DD4D8FFF5CF3DDD50190` |
| Debug devvideoprobe1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_devvideoprobe1_currentcert.sis` | 9,164,504 | `27360039D7DD1CD32601F8477FF4FEF0E61CC886260AB40646DB7149F793C753` |
| Release devvideoprobe1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_devvideoprobe1_currentcert.sis` | 9,169,276 | `34D83F9F4D8AE4CAF8AA211A0EE9A4A1FE5886F934A822D77B9D81DC0A1450C0` |
| Debug devvideosample1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_devvideosample1_currentcert.sis` | 9,191,916 | `210B4A5B53F6BA080598561C6552952304911E175C35EAA398CE80001CECA586` |
| Debug codeccompat1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_codeccompat1_currentcert.sis` | 9,205,204 | `61F87FBE6B3A2B9D014CBC569C1F798733655E54C010577EB08EDE667D5EC9FA` |
| Release codeccompat1 | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_codeccompat1_currentcert.sis` | 9,220,876 | `08C9F75DCD4025E2680B41B5A4B908BFF5439E06D46218C88A32047AA048B2D0` |
| Debug ffmpegsoft1（纯 C 性能基线） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_ffmpegsoft1_currentcert.sis` | 9,709,300 | `5FCA1231349D67088C93F0A2D7AEF5D6A90E2DB0E6ECBCD382EC8ED40B7E609A` |
| Debug 0.9 GLES-YUV（历史） | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_debug_full_currentcert.sis` | 9,687,100 | `70ABE6972E52AD533A82A14520B425D6FAF5EB2C344C76AE178C3CE435DC3698` |
| Release 0.9 GLES-YUV（历史） | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_release_full_currentcert.sis` | 9,703,416 | `17CEE045699489B08143793CF3F70A37DF06EEC74BDDAB24CC75E622B60E8F04` |

旧 0.9 GLES 包已完成构建、签名和静态包检查，但不再作为当前 renderer 候选。CPU RGB565 切换后的 0.9 包需重新构建；在用户完成视频覆盖、性能、重复进入和回归门槛前，不标记为公开稳定版。

## 14. 日志判读速查

| 日志 | 含义 |
|---|---|
| `WW:MAIN_ENTER` → `FIRST_FRAME` | 应用、GL 和首帧成功 |
| `WW:PRIMARY_FONT_READY` | 完整字体已加载 |
| `BI:H200` | 首页 HTTP 200 |
| `WW:COOKIE_WIRE a a n` | Cookie 原始长度和线缆长度一致 |
| `WW:PROFILE_READY` | 当前登录 Cookie 被账号接口接受 |
| `WW:WBI_READY 32` | WBI mixin key 已准备 |
| `WW:SEARCH_SUBMIT` | 新搜索输入实际被提交；只记录长度和摘要 |
| `WW:PLAYBACK_READY` | 播放 API 已解析出清晰度和来源，不代表 MMF 已显示 |
| `WW:PLAYER_SOURCE` | MMF 开始尝试某条媒体线路 |
| `WW:NATIVE_MMF_TRACKS partial verr video aerr audio` | MMF prepare 后的部分播放标记、轨道查询错误和音视频可用性 |
| `WW:DEVVIDEO_AVC_DECODERS error count` | 对 `video/avc` 的兼容查询；Nokia 603 的 `-1/0` 是 MIME 不匹配，不能据此断言无 decoder |
| `WW:DEVVIDEO_H264_DECODERS error count` / `H264_UID` | 对固件实际 `video/h264` 注册名的精确查询及 UID |
| `WW:DEVVIDEO_DECODER uid manufacturer identifier accelerated directDisplay maxW maxH maxBitrate infoLen` | 本机 decoder 能力；它只证明候选存在，不证明故障 SPS 已可解 |
| `WW:DEVVIDEO_FORMAT uid index mime optionalDataLen` | decoder 声明的 AVC 输入格式；下一步据此选择 sample feed 形式 |
| `WW:DEVVIDEO_ALL_POSTPROCESSORS error count` / `POSTPROCESSOR` | 后处理器数量及硬件加速、direct display、旋转、缩放能力 |
| `WW:DEVVIDEO_POSTPROCESSOR_FORMAT/YUV/RGB/YUVRGB/SCALE` | 硬解输出可以采用的内存像素格式、颜色转换和缩放能力 |
| `WW:DEVVIDEO_MP4_AVC w h profile compat level nal timescale refs reorder dpb wp wb risk` | Range 预读和 SPS/PPS 分类结果；最后一个 true 才进入 sample feed |
| `WW:DEVVIDEO_PLAYER_RANGE_BEGIN/HANDOFF` | 正式后端请求的 sample 批次与逐 access-unit 接管，不再是任意字节探针 |
| `WW:DEVVIDEO_PLAYER_MMF_VIDEO_DISABLED/INIT` | MMF 不可用视频轨释放结果和 BCM2727 初始化结果，成功均应为 0 |
| `WW:DEVVIDEO_PLAYER_OUTPUT_SELECTED/FIRST_PICTURE` | 内存像素格式和第一张已转换视频帧；`FIRST_PICTURE` 末尾应为 true/非空尺寸 |
| `WW:DEVVIDEO_PLAYER_PROGRESS/CLOCK_RESYNC/SEEK` | 连续播放进度、音频时钟校正和关键帧 seek 重建 |
| `WW:DEVVIDEO_PLAYER_FATAL/FAILED` | BCM2727 或数据链失败，随后应触发安全低清晰度回退 |
| `WW:DANMAKU_READY` | 弹幕已成功解析；0.7 真机已确认单 ARGB 层可覆盖到视频上 |
| `WW:PLAYER_SESSION_DELETE_LATER` → `DESTROY_READY` | 旧播放器 C++/MMF 会话已脱离主控制器并完成析构；不能单独证明 Qt/WSERV 全局窗口状态没有残留 |
| `WW:PLAYER_OVERLAY_PERSISTENT_NEW` | 应用级 ARGB 覆盖窗首次创建；一次应用运行中应只出现一次 |
| `WW:PLAYER_OVERLAY_DETACH` → `ATTACH` → `NATIVE_REUSE` | 旧会话已安全解绑，下一会话复用同一个原生 alpha 窗口 |
| `WW:PLAYER_OVERLAY_FIRST_PAINT` | 当前会话覆盖层已进入第一个有效 QPainter 绘制 |
| `Symbian:-34` | MMF 建立媒体连接失败，常与 HTTPS/CDN/URL 有关 |
| `Symbian:-12017` | `KErrMMPartialPlayback`，至少一路轨道可用；若 `video=false/audio=true`，应进入编码兼容回退，不能仅继续播放声音 |
| `Symbian:-12015` | MMF 视频向显示设备输出失败，重点查窗口/显示表面 |
| `Paint device returned engine == 0` | raster QWidget 在当前 QGL/窗口结构中没有绘制引擎 |
| data abort `0x54` | 0.6.10 半销毁播放器对象/延迟事件路径 |
| data abort `0x140`，且没有第二个 `PLAYER_REBUILD_BEGIN` | 0.7 第二个 ARGB 覆盖窗显示后的 Qt 虚对象/绘制生命周期故障；此时 MMF 尚未重建 |

## 15. 历史文档关系

- `docs/README_ZH.md`：新会话的文档索引和推荐阅读顺序；
- `docs/archive/wiliwiliforsymbian3/DEVELOPMENT_DESIGN_ZH.md`：最初总体设计，部分规划仍适用；
- `docs/archive/plans/NEXT_WORK_PLAN_2026-08-29_ZH.md`：0.7 当前执行顺序和验收门槛；
- `docs/reference/DEVICE_TEST_MATRIX.md`：从早期里程碑到 0.7 的真机/构建矩阵；
- `docs/research/player/PLAYER_0.7_SECOND_ENTRY_CRASH_ANALYSIS_ZH.md`：二次进入证据、失败候选与 `surfacepersist1` 真机修复结论；
- `docs/research/player/PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md`：当前有声无画样本、实流参数、已排除路径和兼容回退设计；
- `docs/archive/plans/PLAYER_1.0_DECODING_POLICY_ZH.md`：1.0 前强制采用的 MMF → 本机软解 → 外部播放器路线；
- `docs/research/player/PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md`：系统 ARM 失败证据、PPSSPP-FFmpeg 当前实现、PTS 基础、性能优化和止损门槛；
- `docs/research/player/PLAYER_ORIENTATION_PROBLEM_AND_WORKAROUND_ZH.md`：原生横屏根因、最终 20 轮证据、主线状态机和旧 90° 路径移除范围；
- `docs/research/player/post-1.0/POST_1.0_BCM2763_HWDEVICE_RESEARCH_ZH.md`：BCM2763 与 “BCM2727” HwDevice 谜题，仅供 1.0 后固件研究；
- `docs/reference/PORTING_AUDIT.md`：移植边界审计；
- `docs/research/future/LIVE_PLAYBACK_ARCHITECTURE_ZH.md`：直播思路，尚未经过稳定播放器验证；
- `docs/RELEASE_*.md`：每次实验当时的预期，不等于事后真机结论；
- 本文：截至 2026-08-25 的事后结论和下一阶段入口。

## 16. 阶段结论

这个项目已经越过“Symbian 能否开发现代联网客户端”的不确定阶段：Belle SDK、ARMv5 构建、SIS、CODA、NanoVG/GLES2、中文字体、Bilibili API、Cookie 登录、内容列表、详情、评论、视频选源，以及 0.7 原生 MMF 横屏视频、声音、清晰控制 UI 和视频上方弹幕都已有真机成功结果。

0.7 证明播放器显示所有权已有可行解：系统方向不变、MMF 只绑定一个 Qt 原生 `RWindow` 并旋转视频，控制和弹幕由一个 ARGB 顶层窗统一承担。横屏字体模糊也已通过“原方向离屏栅格化、1:1 旋转合成”解决。这些成果必须保留，不能因第二次进入崩溃而整体回退到 0.6.x。

用户已确认 `surfacepersist1` 可以退出后再次播放，旧的确定性第二次进入 P0 已功能闭环；Release 50 次压力门槛仍保留。BCM2727 与系统 ARM decoder 均已由真机实验证明不能承担故障码流，1.0 前不再探索 BCM2727/BCM2763 的硬解边界。PPSSPP-FFmpeg 本机软解已经从“候选设计”推进到“真实出画面”：CPU RGB565 从约 5.2 fps 提高到约 11.4–12.0 fps；三平面 GLES-YUV 后续虽完成 ping-pong，但真机证明上传/提交约 216/321 ms/帧，已退出发布主线。late-drop 仅得到约 5.8–7.1 可见 fps，保持退役。PotatoStream 只提供无 swscale、关闭 loop filter、低缓冲和专用 YUV 输出的公开思路，3DS Y2RU/ABI 不移植；UCPlayerEx 只证明历史优化上限和混合架构线索，不复制其专有代码。第 25 节已将验证过的 native-landscape 生命周期与完整对象图复用合并；新的性能和稳定性结论必须来自主线真机包。如果仍不达最低门槛，就按冻结政策进入外部播放器回退。不会引入桥接、远端重封装或远端转码。

## 17. 2026-08-27 GLES 真机结论与当前输出路线

最新 Nokia 603 日志确认 0.9 GLES-YUV/ping-pong 候选已经真正进入软解，但不能承担发布前的实时显示：末段 `uploaded=157`、`uploadMs=33958`，约 216 ms/帧；`paintGLMs` 同样约 216 ms/帧，`presentCallMs` 约 321 ms/次，`drawMs` 仅约 0.06 ms/次。`presented` 与 `uploaded` 一致，`decoded=944`、`outputDrops=780`、`queueDepth=6` 说明帧主要因显示端消费过慢而被淘汰，网络 Range 和 FFmpeg 解码不是本轮主瓶颈。Y/U/V ping-pong 未带来改善。

因此源码已恢复已有的 CPU YUV420P→RGB565 `QImage` 路线作为 soft H.264 默认 renderer：decoder 在线程中只复制紧凑 YUV 平面，consumer 先按音频时钟做 stale/drop 决策，再对选中的帧执行 `RGB565_LUT2X2` 转换；播放器 overlay 继续通过已有 ARGB QPainter 路径显示画面、弹幕和控制。软解不再请求 YUV420 GLES 输出，也不再调用三平面 GLES texture upload。`skip_loop_filter=AVDISCARD_NONREF`、PTS、Range、catch-up 和 queue 策略均保持不变。此前约 11.4–12.0 fps 的 CPU RGB565 测量是当前发布前唯一可引用的性能基线；旧 GLES-YUV 安装包转为历史证据，必须在本次源码切换后重新构建再验收。

## 18. 2026-08-27 CPU/GLES 共同低 consumer-rate 静态审计

新的 RGB565 日志显示 `pictures=255`、`outputDrops=206`、`queueDepth=6`，约只有 43 帧真正被 consumer 取走；在约 14.066 秒媒体 PTS 内仍接近 3 fps。由于 GLES 也出现约 3 fps，renderer-specific 的 RGB 转换或纹理上传不能单独解释共同低速。

源码审计结果如下：

- `VideoOverlayWidget::attachOwner()` 和 CPU 路径均使用 `KOverlayFrameIntervalMilliseconds = 40`，理论上限约 25 fps；没有 200–330 ms 的显式 frame sleep；
- `timerEvent()` 每次只调用一次 `takeAvcHardwareFrame()`，但该调用同步进入 `CVideoPlayerUtility2::PositionL()` 获取 MMF audio clock；随后 overlay 的 `drawDanmaku()` 和 `drawControls()` 又各自读取一次 `position()`，同一 UI 事件周期最多进行三次同步 MMF 位置查询；
- `paintEvent()` 每次清空全屏 ARGB 窗口，横屏还会先绘制完整逻辑 ARGB image、再做一次 90° QPainter image draw。该 UI/WSERV repaint 与同步 `PositionL()` 是两种 renderer 共同的 200–330 ms 固定节流候选，后续应单独测量或缓存单帧 audio position；本轮不改变其调度策略；
- 原实现确实在 `enqueueOutput()` 前完成 RGB565，因此大量最终会被 `takeOutput()` 批量淘汰的帧已经支付了约 43 ms 转换成本。当前实现改为 decoder 只复制紧凑 YUV 平面；`takeOutput()` 先完成 stale/drop 选择并释放旧 `Yuv420Frame`，`takeFrame()` 只转换选中的一帧。

## 19. 2026-08-27 日志 2734939 综合结论

用户提供的可执行文件路径为 `epoc32\\release\\armv5\\udeb\\wiliwili_symbian.exe`。同一份日志混合了第一段 GLES 会话、两段旧的 CPU RGB565 会话，以及末尾一段真正启用延迟转换的 CPU 会话；不能把整份文件归类为单一版本。旧会话打印 `WW:FFMPEG_SOFT_FIRST_FRAME ... RGB565` 且 `repackCopyMs=0`，而末尾新会话打印 `WW:FFMPEG_SOFT_FIRST_FRAME ... RGB565_DEFERRED` 并出现非零 `repackCopyMs`。

旧 CPU 会话本身的数字仍然说明了卡点：`decoded=256`、`pictures=255`、`outputDrops=206`、`queueDepth=6`，反推约 43 帧被 consumer 取走；在 `pts=14066 ms` 内约 3.06 fps。`decodeMs=11230/256≈43.9 ms/帧`，`convertMs=11104/255≈43.5 ms/帧`，约 80% 的 converted picture 最终被淘汰，确实会浪费 ARM11 CPU 并加重 MMF 音频线程压力。

这份日志还保留了第一段 GLES 会话的完整对照：`decoded=944`、`outputDrops=780`、`presented=157`，末段 `uploadMs=33958`、`presentCallMs=50755`，约 3 fps。末尾的延迟转换会话在 `pts=3400 ms` 时为 `decoded=91/pictures=90/outputDrops=68/queueDepth=6`，在 `pts=10366 ms` 时为 `211/210/168/6`；按 `pictures-outputDrops-queueDepth` 反推真正被 consumer 选中的帧约从 16 增至 36，即约 3.5 fps。CPU 路径的 `presented=0/uploaded=0` 是因为这两个计数由 GLES delegate 维护，不能用来判断 RGB565 是否显示；应使用队列账本和 `RGB565_DEFERRED` 标记。

延迟转换本身已在新会话中生效：`convertMs` 从 1125 增至 2380，`repackCopyMs` 从 752 增至 1612；按两个统计点的增量，约 20 个新增 consumer 帧才增加约 1255 ms RGB565 转换，而不是为新增的约 120 个 decoded picture 全部转换。这证明 stale/drop 后再转换的目标已经达到，但 consumer 仍只有约 3–4 fps。两个统计点的 `queueMutexWaitMs=0` 也不支持“输出互斥锁等待”是主卡点。

这段新会话在约 1185 ms lag 时进入 catch-up，随后在日志末尾出现 `NORMAL` 后又重新 `SKIP_NONREF`；catch-up 会放大丢帧，但并不能解释 CPU 与 GLES 都接近相同的低 consumer rate。源码中的 presenter timer 是 40 ms，`takeOutput()` 没有固定等待；共同瓶颈仍优先怀疑 UI/WSERV repaint 和同一 UI 周期内多次同步 `CVideoPlayerUtility2::PositionL()`。

日志随后以“远端主机关闭了这个连接”结束，没有 soft decoder final summary；因此本次只能作约 10 秒的 smoke/performance 片段，不能作完整 30–60 秒稳定性或崩溃结论。

当前发布前结论：延迟 RGB 转换的源码改动已经由本次日志确认生效，但尚未解决共同的 3–4 fps consumer 瓶颈。当前源码已加入低风险时钟采样复用：soft overlay 每个 timer 周期只查询一次 audio `PositionL()`，取帧、弹幕和控制栏复用该值；普通 MMF 保持原行为。下一次真机只需比较 consumer fps、`outputDrops`、AV lag，并继续分段测量 `paintEvent()`/`PositionL()` 耗时；不再优化 RGB565 LUT、decoder、network 或 catch-up 阈值。发布判断应以新包的实际 `RGB565_DEFERRED` 会话为准，而不是旧 RGB565 会话。

本轮新增的低频 pacing telemetry 由 `SOFT_STATS` 追加：`overlayPaintCount/overlayPaintMs`、`overlayPositionQueries/overlayPositionMs`、`overlayTimerEvents/overlayTimerElapsedMs/overlayTimerMaxGapMs`。它只在既有约 5 秒聚合日志中输出，不改变帧调度。

## 21. 2026-08-27 日志 2740509：共同 3 fps 卡点定位到 overlay paint/UI 事件循环

该日志是当前 CPU RGB565 延迟转换路线的单次 640×360 会话，首帧标记为 `RGB565_DEFERRED`，没有 `FFMPEG_SOFT_ERROR`，结束时正常执行 `PLAYER_SESSION_PARKED`。它首次把此前 CPU 与 GLES 都约 3 fps 的共同节流边界直接量化出来：

- `SOFT_STATS pts=3200` 时 `decoded=94/pictures=93/outputDrops=71/queueDepth=6`，按队列账本估算约 16 帧被 consumer 选中；到 `pts=23433` 时为 `447/366/6`，估算约 75 帧被选中，consumer 速率收敛到约 3.2 fps。`outputDrops` 很高，但它是慢 consumer 的结果并被追赶策略放大，不是根因；
- 解码累计 `decodeMs=23303/448≈52 ms/decoded picture`，约 19 fps 的生产能力；`repackCopyMs=3766` 约 8.4 ms/picture；真正选中帧的 `convertMs=5121/75≈68 ms/frame`。这些数字均不足以解释稳定的 3 fps；
- overlay 计时显示，最后两个约 6.8 秒区间内 `overlayPaintCount` 仅增加 20、20 和 19 次，而 `overlayPaintMs` 分别增加约 6.88 s、6.51 s，即约 **333–344 ms/paint**。累计值从 `9103/115` 上升到 `29174/174`，与后段固定约 3 fps 的 consumer 速率相符；
- `overlayTimerMaxGapMs=632`，末段为 `656`，说明 UI timer 存在 0.6 秒级不到达间隙。`overlayPositionQueries=82`、`overlayPositionMs=415`，单次同步 `PositionL()` 约 3.5–5.1 ms，只占 paint 阻塞的很小部分，时钟缓存已经不是第一卡点；
- CPU 路径的 `presented/uploaded=0`、`yUploadMs/uUploadMs/vUploadMs=0`、`drawMs/swapMs/presentCallMs=0` 是预期值；本轮应使用队列账本和 overlay paint/timer 计时。网络 Range 没有 timeout，`queueMutexWaitMs` 仅 1–2 ms，也不支持网络或输出锁为主因。

源码审计与数据相互印证：`VideoOverlayWidget::paintEvent()` 每次都清空全屏 ARGB overlay，并绘制视频、弹幕和控制；横屏还会先完整绘制逻辑方向的 ARGB image，再做一次 90° image 合成。同步 raster/WSERV repaint 正在阻塞 Qt event loop，40 ms timer 只是理论上限，不能抵消 300–340 ms 的 paint 阻塞。下一步只围绕 overlay paint 边界做低风险拆分/缓存或减少全屏 ARGB 合成，并继续保留现有低频 telemetry；不要再修改 decoder、queue、network、catch-up、RGB565 LUT 或 GLES ping-pong。

本次日志只证明播放会话可正常 park，不构成 Release 50 次重入压力门槛，也未改变发布策略：已知兼容流仍走 MMF，故障 H.264 走本机 FFmpeg CPU-RGB565，性能不足时再按既定规则交给外部播放器。

## 22. 2026-08-27 paintEvent 内部测量改动（待单次真机验证）

针对日志 2740509 暴露的 333–344 ms/paint 卡点，源码已加入纯测量性拆分，未改变任何播放行为、定时器、帧队列、decoder、catch-up、network、RGB565 LUT、GLES 或弹幕/控制栏逻辑。`paintEvent()` 仍按原顺序清空 overlay、绘制视频、绘制弹幕/控制，并在横屏路径保留中间 ARGB image 与最终 90° 合成；新增的累计字段为：

`overlayClearMs`、`overlayVideoDrawMs`、`overlayIntermediateMs`、`overlayRotateMs`、`overlayDanmakuMs`、`overlayControlsMs`、`overlayPainterEndMs`、`overlayOtherMs`。

这些字段使用 `User::FastCounter()`，通过一次性本地频率校准换算为毫秒，并继续复用已有低频 `WW:SOFT_STATS`；没有逐帧日志。下一次只编译/安装一次即可判断约 340 ms 到底落在 RGB565→ARGB、横屏中间图/旋转、弹幕、控制栏、`QPainter::end()` 还是其他/WSERV 收尾。当前没有这组新字段的真机结果，因此不应提前据此实施优化。

## 23. 2026-08-27 固定 90° rotation 单点优化（待真机复测）

日志 2738898 已把 `paintEvent()` 后段约 348.5 ms 拆分为：`videoDraw≈72.3 ms`、`danmaku≈49.5 ms`、`rotate≈224.3 ms`，而 `intermediate≈1.1 ms`、`other≈1.3 ms`、`painterEnd≈0`、`clear≈0`。源码静态检查确认，原 `overlayRotateMs` 覆盖的是完整 ARGB 中间图的 `QPainter::translate()` + `rotate(90°)` + `drawImage()`，不是 `QImage::transformed()`；这张图已经包含视频、弹幕和控制栏，因此不能仅旋转视频。

本轮只修改该固定角度路径：新增持久化 `m_rotatedFrame`，尺寸变化时才分配；每帧用直接像素映射 `logical(x,y)→physical(H-1-y,x)` 生成最终 ARGB 图，再以无 transform 的 `drawImage()` 显示。没有改变 decoder、network、queue、catch-up、timer、audio clock、RGB565 LUT、GLES、弹幕内容或输入坐标。`overlayRotateMs` 继续覆盖“固定旋转 copy + 最终 drawImage”，需要下一次单包真机日志确认是否从约 224 ms 降至几十毫秒级。

同一附件中的后续可执行文件 `2740086` 未能完成一次有效软解验证：日志在 `PLAYER_SOURCE_DEFER_MMF` 后只出现 `DEVVIDEO_RANGE_HEADER_BEGIN 1 0 1572864`，没有 `DEVVIDEO_MP4_AVC`、`PLAYER_SOURCE_DEFER_MMF_DONE`、`FFMPEG_SOFT_BEGIN/READY/FIRST_FRAME`，也没有 `DEVVIDEO_RANGE_TIMEOUT` 或进程结束标记。该运行停在首个 MP4 头部 Range 预取期间，MMF 按设计尚未打开；不能据此判断 fixed-rotation 改动造成无画面。附件前一段 `2738898` 已实际触发两次 `FFMPEG_SOFT_FIRST_FRAME`，证明软解路由本身仍可进入。后续需取得包含 Range 完成或超时标记的完整日志，再区分网络预取停滞与播放器问题。

## 20. 2026-08-27 日志 2736142：时钟缓存 A/B 结果

该 UDEB 日志的两段软解会话均打印 `RGB565_DEFERRED`，因此延迟转换路径仍在使用。第一段（640×338）在 `pts=5433/10966/17566/23800/29400 ms` 的选中帧数约为 `20/39/59/79/97`，consumer 速率从约 3.7 fps 降至约 3.3 fps；第二段（640×360）在 `pts=2633/8500/14933/21966/29233/35833 ms` 的选中帧数约为 `13/32/52/72/92/112`，速率从约 4.9 fps 降至约 3.1 fps。与缓存前约 3 fps 的结果相比，没有观察到实质性改善。

延迟转换的账本仍然正确：第二段末尾 `convertMs=7398` 对应约 112 个选中帧（约 66 ms/帧），`repackCopyMs=5260` 对应约 671 个 decoded picture（约 7.8 ms/帧），`queueDepth=6` 且 `queueMutexWaitMs=1`。Range 在 16 秒、32 秒等节点继续 handoff，没有 `RANGE_TIMEOUT`；两次会话都正常 detach/park，重复进入稳定性没有新回归。

本次 A/B 说明重复 `PositionL()` 不是唯一或最大节流来源。catch-up 在第一段累计进入 7 次、第二段进入 5 次，会放大 output drop 和 AV lag，但 consumer 在 NORMAL 与 SKIP_NONREF 间均维持低速。当前最高优先级应转为分段测量 `timerEvent` 到达间隔、`paintEvent()` 总耗时、RGB565→ARGB 绘制耗时及 WSERV repaint；在取得这些边界数据前不再改 catch-up 或 decoder。

## 24. 2026-08-28 soft native-landscape 实验归档

为验证固定 90° 软件旋转是否是 soft 播放的主要代价，曾加入仅作用于 FFmpeg soft handoff 的真实横屏实验。实验代码和对应 `.pro` 配置已完整保存在
`symbian/archive/soft-native-landscape-2026-08-28/`，其中包含当时的源文件和恢复说明。

Nokia 603 真机验证确认：该 soft-only 实验在不稳定窗口状态中请求 native landscape 后会闪退，因此它本身不能作为性能或发布方案。实验分支已撤回并保持归档；后续 app-shell 分层实验从头重建了窗口时序，不能把新的主线实现等同于恢复本归档。

归档分支仍不得重新启用，也不得引用其性能结果。真实横屏的后续结论以第 25 节 app-shell 状态机证据及当前主线为准。

## 25. 2026-08-28 app-shell 原生横屏闭环与主线合并

后续测试从不接 MMF、FFmpeg、QGL 的 disposable raster 顶层窗口开始，逐层加入正式 QGL app shell、动态 RGB565 呈现和 fullscreen/chrome 恢复。最终定位不是 renderer 或 `SetOrientationL()` 必然崩溃，而是 QApplication panes、Qt fullscreen、物理 `screenGeometry` 和顶层窗口显示顺序不一致：

- `AA_S60DontConstructApplicationPanes` 会产生 `availableGeometry=640×360`、`screenGeometry=360×640` 的分裂状态；在这个状态强制显示是旧 app-shell2/3 第一帧后退出的关键条件；
- 预先隐藏主 QGL 会使应用失去前台，后续 timer/commit 停止；主 QGL 必须始终保持 mapped；
- panes 必须在 QApplication 创建时保留。稳定竖屏中先 `showMaximized()` 恢复 pane 工作区，只由 `workAreaResized()` 触发横屏请求；物理屏幕精确 640×360 后才允许独立 raster 顶层 `showFullScreen()`；
- 退出先隐藏横屏窗，请求竖屏并等待物理 360×640，再让主 QGL `showMaximized()`/动态 `showFullScreen()`，直到 available/screen 都精确为 360×640；
- 方向切换等待阶段不在 `resizeEvent()` 中改窗口树。

最终 `appshell7-final-dynamic-fullscreen-rgb565` Nokia 603/CODA 日志完成 20 轮：20 次 `WORKAREA_640X360_READY`、20 次 `FIRST_PAINT_END`、47 次 `RGB565_PRESENT`、20 次 `RESTORED_FULLSCREEN`、0 timeout，最终 `COMPLETE cycles 20`、`EVENT_LOOP_EXIT 0`。用户目视确认色条与扫描线持续正确，横屏和竖屏都没有系统栏。至此正式 QGL 共存、动态 RGB565 raster paint 和双向 fullscreen 生命周期已完成隔离验收。

当前 0.9 主线据此完成以下合并：

- normal build 不再设置 `AA_S60DontConstructApplicationPanes`；主页使用 dynamic fullscreen 隐藏但不销毁 panes；
- `VideoPlayerWidget` 成为独立无边框持久顶层窗口，并继续保留 `surfacepersist1` 的 controller/native video host/MMF/overlay 全对象图复用；
- 进入/退出由 `QDesktopWidget::workAreaResized()` 状态机提交，6 秒超时会安全终止本次播放并恢复主页；
- MMF backend 固定 no rotation；CPU RGB565、弹幕、控制、触摸均直接使用真实 640×360 坐标；
- 删除中间 ARGB frame、固定 90° pixel copy 和最终第二次 `drawImage()`。历史 `overlayRotateMs≈224 ms/frame` 对应的整帧 pass 不再存在；
- 主 QGL 与 player 的 `resizeEvent/resizeGL` 不再互相改几何。

普通 Debug/Release 主线均已用 Qt 4.7.4/GCCE 4.4.1 编译完成，各为 `sbs errors: 0`（34 条既有 SDK/编译器警告）。这仍只是编译门槛；MMF 正常流、FFmpeg 风险流、overlay/input、返回主页、清晰度切换和重复进入必须在当前主线包中验证，之后再进行 Release 50 轮压力测试。旧探针和归档不属于发布构建，普通 qmake 不加入 `CONFIG+=applandscape1`。

## 26. 2026-08-28 日志 2746319：四视频主线通过与 soft native surface

该 UDEB 运行连续测试了四个视频：前两个进入 `FFMPEG_SOFT_READY ... RGB565_LUT2X2`，分别覆盖 640×360 和 360×640 编码画面；后两个输出 `DEVVIDEO_MP4_PROFILE_SKIP` 并留在 MMF。四次会话都保留相同 player/native-host/backend 对象身份，按需完成横屏/竖屏状态机，正常 `PLAYER_SESSION_PARKED`，最终 `EVENT_LOOP_EXIT 0`。用户确认除软解仍有卡顿外，画面方向、比例、硬解流畅度、弹幕/UI 覆盖和返回均正常。

两条 soft 会话的 `overlayIntermediateMs=0`、`overlayRotateMs=0`，证明历史 90° 整帧 pass 已经从正式播放链移除。新的瓶颈进一步收敛到视频仍画在透明 ARGB overlay：640×360 会话后段约 20 次 paint 增加 3.7–4.8 秒，其中 `overlayVideoDrawMs` 约增加 3.1–3.6 秒，即约 155–179 ms/显示帧；带弹幕时 `overlayPaintMs` 约 183–241 ms/帧。360×640 编码会话没有弹幕，后段 video draw 约 102 ms/帧。`PositionL()` 在本轮也表现出约 14–39 ms/次的区间成本，虽不是首要瓶颈，但值得低风险降频。

当前源码据此完成两项、不触碰 decoder 的改动：

- 新增一个随 `VideoPlayerWidget` 全应用期复用的 `SoftVideoSurfaceWidget`，设置 `WA_NativeWindow`、`WA_OpaquePaintEvent` 和 `WA_NoSystemBackground`，直接持有/绘制 `QImage::Format_RGB16`；MMF native video host 与 soft surface 二选一显示；
- 持久透明 ARGB overlay 删除视频帧存储和 `drawImage()`，只绘制弹幕/控件并始终 raise 到视频之上，因此 UI/弹幕仍可见且继续独占输入；
- soft `PositionL()` 每 500 ms 向 MMF 校准，中间用 `QTime` 和 `playbackRate()` 外推；暂停、seek、倍速、attach/detach/source change 都会失效缓存；
- `SOFT_STATS` 现在把 native surface 的真实 presented/last PTS 回传给 decoder telemetry，并新增 `softSurface*` 与 `overlayPositionCacheHits`；旧 CPU 路径的 `presented=0` 歧义因此消失。

这轮没有修改 H.264 解码、loop filter、catch-up 阈值、queue、Range 下载、RGB565 LUT 或选源策略。普通 GCCE Debug/Release 已分别完成 qmake、编译、链接和 SIS 生成，各为 `sbs errors: 0`、34 条既有 SDK 警告。编译只是静态门槛；Nokia 603 必须确认视频可见、UI/弹幕在上、控件可点击、`softSurfacePresented>0`、`overlayVideoDrawMs=0`、`overlayPositionCacheHits>0`，并比较 `softSurfacePaintMs` 与旧 102–179 ms/frame 基线。

## 27. 2026-08-29：正式路由改为 Broadcom 真实 header preflight

- 用户提供的正式播放日志显示一条 `6 refs / reorder 4 / DPB 6 / weighted 1,2` 的视频被旧 `7/4/7` 规则标为 `false`，随后出现 `NATIVE_MMF_TRACKS false 0 true 0 true` 和 `NATIVE_MMF_PLAY`：即只有 AAC、没有像素，却被 UI 误显示为 `PLAY`。这证明启发式 SPS 模板不是可发布的选择器；
- 当前正式路由对首选 progressive MP4 先取 `moov/avcC`，再只取首个 sync sample。reader 生成的 Annex-B 首单元包含 avcC SPS/PPS，且对首单元去重内嵌 SPS/PPS，保持 2026-08-26 control experiment 已验证的 `video/h264 + EDuCodedPicture + EDuElementaryStream` 输入契约；
- 新增 `VideoPlaybackBackend::probeAvcHardwareHeader()`：临时 `CMMFDevVideoPlay` 仅执行 `NewL → SelectDecoderL(0x10204C21) → SetInputFormatL → GetHeaderInformationL → ReturnHeader → delete`。它不调用 `ConfigureDecoderL`、`Initialize`、`Start`、DSA、post-processor、输出格式设置或 `WriteCodedDataL`，因此不是重新开启废止的 BCM 解码/直显实验；
- `GetHeaderInformationL()==0` 记录 `WW:DEVVIDEO_HEADER_PREFLIGHT_ACCEPT` 与 `ROUTE MMF` 并正常打开 MMF。任一 plugin/config/header 拒绝记录 `REJECT` 与 `ROUTE FFMPEG`，随后从同一个 IDR 以既有 16 秒有界 Range batch 进入现有 FFmpeg + MMF AAC 路径。网络/容器 Range 失败仍保守退回 MMF，避免把网络故障误判为编码不兼容；
- 旧 `isFirmwareRiskProfile()` 只留在 reader 作为历史解析 API，已经没有正式调用；`PROFILE_SKIP`、`BCM_REJECTED_KEEP_MMF`、按 1280×720 预先改清晰度的路由均从当前路径移除；
- GCCE ARMv5 Debug、Release 2026-08-29 均完成，`sbs errors: 0`（各 32 条已有 SDK warning）。新日志的必需标记是 `DEVVIDEO_HEADER_PREFLIGHT_BEGIN/STAGE/ACCEPT|REJECT/ROUTE`；
- 签名审计发现工作区 `symbian/out/current-signing/qt-selfsigned-current.cer/.key` 实际生成 2009-10-05 至 2019-10-03 的自签名链，而已有 0.9 `*_currentcert.sis` 是 2026-08-24 至 2036-08-21 的另一张证书。当前目录未找到那张证书的私钥。因此 2026-08-29 新生成的自签名 SIS 只能作本地构建归档，不能替代已安装应用的更新包；真机先通过 Qt Creator/CODA Debug 部署验证，恢复正确的 2026 证书私钥前不得发布新的 SIS。

## 28. 2026-08-29：NIKINIKI 正式名称与公开仓库门槛

- NIKINIKI 现为产品正式名称；`wiliwili for Symbian³` / `wiliwili_symbian`
  继续作为曾用名、内部二进制身份和历史文档名保留；
- 手机菜单/SIS 显示名、Vendor、应用标题、播放器标题、关于页、登录设备名和
  User-Agent 已统一为 NIKINIKI，但 `TARGET`、UID、设置键和资源安装目录保持不变；
- 应用内使用项目提供原图的 256×256 RGBA 缩放版；启动器使用同图形的
  filter-free SVG-T。Belle `mifconv 3.3.3` 与 qmake/SBS 的 MIF 构建均通过；
- GitHub 首页、构建/许可证/发布边界已改为 NIKINIKI；会自动构建/发布上游
  wiliwili 产品的 Actions 已移除，Issue 表单改为 Symbian 专用；
- Debug/Release 全量回归均为 `sbs errors: 0`、32 warnings；生成的 SIS 仍为
  过期 SDK Self Signed，仅作被忽略的本地构建证明，不得发布；
- 剩余门槛：Nokia 603 确认应用列表图标与名称、覆盖安装保留登录/设置、启动页
  新图标正常、MMF/soft 路由无回归；之后仍执行既定 soft-surface 与 50 轮门槛。
