# NIKINIKI 与上游 wiliwili 代码边界

> 更新日期：2026-08-29  
> 上游基线：`xfangfang/wiliwili` `88e5876`（v1.6.0，`yoga`）  
> 产品基线：NIKINIKI 1.0.0 与当前主线

## 1. 最终结论

NIKINIKI 是从 wiliwili 的产品行为、API 映射和界面结构出发，为
Qt 4.7.4/GCCE/Symbian³ 大幅重写的 GPL-3.0 项目。开发工作区仍保留完整
wiliwili 树作为历史与对照基线，但 **NIKINIKI 正式构建和独立公开仓库均不再
依赖根目录的 `wiliwili/`、`library/`、`resources/`**。

正式应用的边界是：

- 自研 Qt/C++03 Symbian 应用、网络、UI、播放器与平台代码；
- 由上游 API 映射缩减而来的 12 个端点；
- 许可证完整、固定版本的 NanoVG、QR Code、字体和 PPSSPP-FFmpeg；
- 一张保留自上游的卡片背景资源；
- 行为兼容但没有编译上游 C++17 实现的页面、解析、WBI 与播放器逻辑。

## 2. 两个仓库角色

| 角色 | 内容 | 是否公开为 NIKINIKI 主仓库 |
|---|---|---|
| NIKINIKI 公开仓库 | 自包含产品源码、构建脚本、资源和公开文档 | 是；唯一产品主线 |
| 并列研究仓库 | 完整 wiliwili v1.6.0、submodule、私有日志、历史包和失败实验 | 否；仅作为本地研究/溯源基线 |

并列研究仓库可以继续比较上游行为；公开仓库作为唯一产品主线，避免把约
200 MiB、与 Symbian 无关的桌面/主机代码和 submodule 误表述为 NIKINIKI 的
组成部分，也避免两份可编辑产品源码长期漂移。

## 3. 正式构建的事实来源

边界以以下文件为准：

1. `symbian/app/wiliwili_symbian.pro` 的 `INCLUDEPATH`、`SOURCES`、
   `RESOURCES`、`DEPLOYMENT`；
2. `symbian/app/resources.qrc`；
3. `symbian/reuse-manifest.yml`；
4. 无上游根目录引用的 Debug/Release 实际构建结果。

当前 `.pro` 只引用 `symbian/` 内部路径。正式源文件中不得新增
`../../wiliwili`、`../../library` 或 `../../resources`。

## 4. NIKINIKI 自研部分

| 模块 | 当前路径 | 说明 |
|---|---|---|
| 应用和导航 | `symbian/source/app` | 页面状态机、网络请求编排和持久状态 |
| UI | `symbian/source/ui`、`symbian/include/ui` | Qt 窗口、NanoVG 页面、弹幕和控制层 |
| 播放器/平台 | `symbian/source/platform` | MMF、DevVideo 探针、MP4 AVC 解析、FFmpeg 软解接入 |
| 网络/解析 | `symbian/source/network` | Symbian HTTP、Qt/C++03 JSON 解析、WBI 签名 |
| 模型 | `symbian/include/model`、`symbian/source/model` | Symbian 兼容数据结构与登录状态 |
| 工程/构建 | `symbian/app`、`symbian/env`、`symbian/Build-App.ps1` | qmake、SBS、SIS 和环境检测 |
| JSON 兼容层 | `symbian/third_party/mongoose_compat` | NIKINIKI clean-room 实现；没有复制 Mongoose 源码 |

旧 `namespace wiliwili`、`TARGET=wiliwili_symbian`、UID、QSettings 键和安装路径
属于升级兼容标识，可以继续保留；它们不构成对上游源码目录的构建依赖。

## 5. wiliwili 派生/参考边界

| 上游来源 | NIKINIKI 位置 | 复用方式 |
|---|---|---|
| `api/bilibili/api.h` | `symbian/include/network/bilibili_endpoints.h` | 只保留实际使用的 12 个端点，记录基线和 GPL 来源；不再编译完整头文件 |
| Result Model 字段/语义 | `symbian/include/model`、`symbian/source/network` | Qt/C++03 行为兼容重写 |
| WBI 算法 | `symbian/source/network/bilibili_wbi.cpp` | 使用 Qt 类型和本地 JSON 层重新实现 |
| Presenter/Fragment/Intent 流程 | `wiliwili_widget.cpp` 与各 UI 页面 | 结构/行为参考，不编译上游 C++17 代码 |
| `mpv_core` 页面语义 | Symbian 播放器和平台后端 | 后端完全替换为 MMF/本机 FFmpeg |
| `video-card-bg.png` | `symbian/resources/pictures` | 直接资源快照，GPL-3.0 |

即使移除未使用上游目录，NIKINIKI 仍作为 wiliwili 派生项目整体采用 GPL-3.0，
不会借清理目录淡化上游来源。

## 6. 独立第三方边界

这些组件是通过原 wiliwili 树取得，但版权并不属于 wiliwili：

| 组件 | 本地位置 | 许可证/处理 |
|---|---|---|
| NanoVG + fontstash/stb | `symbian/third_party/nanovg` | zlib/文件内许可；保留原文件和许可证 |
| QR-Code-generator C | `symbian/third_party/qrcodegen` | MIT；保留原文件和许可证 |
| Source Han Sans 与字体子集 | `symbian/resources/font` | OFL-1.1；子集内部名称为 `NIKINIKI CJK` |
| PPSSPP-FFmpeg | `symbian/third_party/ppsspp_ffmpeg` | LGPL-2.1-or-later；提交版本、脚本和许可证，不提交生成库 |

静态链接 FFmpeg 的公开 SIS 还必须附带足以替换库并重新链接的材料；仅公开源码
仓库、不发布二进制时不产生该次二进制分发义务。

## 7. 不属于 NIKINIKI 公开树的内容

- `wiliwili/source` 与除来源参考外的 `wiliwili/include`；
- borealis C++17 核心、XML/View 系统和平台后端；
- cpr、OpenCC、lunasvg、MemoryModule、pystring、libpdr 等未使用 submodule；
- 根 CMake/xmake、Switch/PSV/PS4/WinRT/PC 构建与发布脚本；
- 上游完整主题、翻译和平台资源；
- `symbian/archive` 的失败实验源码快照（结论由文档保留）；
- SIS、EXE、静态库、对象、SDK 生成树、证书、私钥和设备日志。

未使用代码从公开仓库中省略，并继续保存在并列研究仓库；这样既不会污染产品
边界，也不破坏后续上游对照能力。

## 8. 发布与维护规则

1. 新增依赖前更新 `symbian/reuse-manifest.yml` 与 `NOTICE.md`；
2. 正式工程只允许引用公开仓库内文件；
3. 复制第三方文件时保留版权头、许可证、固定 commit 和本地修改记录；
4. 每次提交或发布前运行 `tools/Test-PublicRepository.ps1`；
5. 对外 README 必须继续写明“基于 wiliwili 移植与重构”；
6. 公开 SIS 前另行完成 FFmpeg LGPL 重链接包、签名和 Nokia 603 验收。
