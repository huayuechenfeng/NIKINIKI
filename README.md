<p align="center">
  <img src="symbian/app/icons/nikiniki.png" alt="NIKINIKI icon" width="192" height="192">
</p>

<h1 align="center">NIKINIKI</h1>

<p align="center">
  面向 Symbian³ / Anna / Nokia Belle 的第三方哔哩哔哩客户端
</p>

<p align="center">
  <strong>1.1.0 · 播放策略与首次启动体验更新</strong>
</p>

> NIKINIKI 曾用名为 **wiliwili for Symbian³** / `wiliwili_symbian`。项目源自对
> [xfangfang/wiliwili](https://github.com/xfangfang/wiliwili) 的 Symbian 移植与重构；
> 为保证旧安装包可原位升级并保留设置，内部可执行文件名、UID、部分目录名及历史
> 技术文档仍保留旧名称。
>
> 本项目与哔哩哔哩、Nokia 及上游 wiliwili 均无官方关联。
> QQ交流群：977410275

## 开发说明

本项目的开发过程中使用了 **vibecoding（AI 辅助编程）**。设计取舍、实机验证、
构建与发布均由维护者确认。

## 下载 1.1

- [NIKINIKI 1.1.0 GitHub Release](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.1.0)
- [直接下载正式 SIS](https://github.com/huayuechenfeng/NIKINIKI/releases/download/v1.1.0/NIKINIKI_1.1.0_release.sis)
- [LGPL 重链接材料](https://github.com/huayuechenfeng/NIKINIKI/releases/download/v1.1.0/NIKINIKI_1.1.0_relink_materials.zip)

安装包大小、SHA-256、签名和重链接材料清单见
[1.1.0 发布说明](docs/releases/RELEASE_1.1.0_ZH.md)。若新版本安装或启动异常，请到 QQ 群
`977410275` 反馈，并可先继续使用
[1.0.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.0.0)。

## 安装前置说明

所有设备都建议安装 TLS 1.2 补丁；原版 Symbian³ 和 Anna 还需要 Qt 4.7.4 与
Qt Mobility 1.2.x。完整顺序和运行库下载见[安装指南](docs/user/INSTALL_ZH.md)。
首次启动、TLS 或播放问题见[故障排查](docs/user/TROUBLESHOOTING_ZH.md)。

## 开源与赞助

本项目完全开源并公开发布，也欢迎大家投喂一点可乐钱。赞助完全自愿，不会影响
软件的正常使用、功能获取或源码开放：
[爱发电支持 NIKINIKI](https://afdian.com/a/nankoku)。

## 1.1 更新重点

1.1 修复了首次安装后同步加载完整字体造成的长时间黑屏，并在设置页新增三种播放方式和
三种解码方式。下载后播放现在会显示文件大小、已下载量、百分比和进度条；
`OpenFileL` 边下边播也修复了文件共享冲突并增加到 8 MiB 起播预缓冲。

N8 / E7 / X7 / C7 建议升级到 Nokia Belle，并在“设置 → 播放方式”中选择
“`OpenFileL` 边下边播”，以避开部分旧机型 MMF 网络流式 controller 只有声音没有画面的情况。

## NIKINIKI 是什么

NIKINIKI 的目标很简单：让已经停止官方生态更新的 Symbian 手机，仍能直接访问和
播放现代哔哩哔哩内容。

NIKINIKI 已在 **Nokia 603 / Nokia Belle（640×360）** 上作为主要开发与回归基线，核心
浏览、登录、视频、弹幕和播放器流程已经跑通。项目不依赖 PC 桥接、远程封装或远程
转码；兼容处理尽量在手机本机完成。

播放器同时保留两条路径：

```text
Bilibili progressive MP4
        │
        ├─ Broadcom DevVideo header preflight 接受
        │      └─ Symbian MMF 原生硬件播放
        │
        └─ Broadcom 插件拒绝真实 H.264 header
               └─ 本机 FFmpeg H.264 软件解码
                      + MMF AAC / 音频主时钟
```

这避免了按照 `ref`、DPB、weighted prediction 等参数维护一张不断膨胀的“风险规则表”：
先让设备自己的 Broadcom 插件判断能否接受真实码流，不能硬解时才进入软件路径。

## 已实现

- 首页推荐、分区内容、动态、消息、搜索和视频详情；
- 扫码登录、历史、收藏、稍后再看、关注和互动；
- 普通 H.264/AAC 视频优先使用 Symbian MMF 硬件播放；
- 硬件插件拒绝的部分现代 AVC 视频可在手机本机使用裁剪版 PPSSPP-FFmpeg 软解；
- 播放/暂停、进度拖动、倍速、音量、清晰度和触摸控制；
- 滚动弹幕及播放器透明控制层；
- 原生横屏播放器状态机，可从播放页正常返回并重复进入；
- 播放方式可选网络流式、`OpenFileL` 边下边播和下载后播放；
- 解码方式可选自动选择、全程硬解和全程软解；
- Qt 4.7.4 / GCCE 4.4.1 / Symbian³ SDK 的可复现构建脚本；
- 单一 ARMv5 应用包设计，目标覆盖 Symbian³、Anna 和 Belle。

## 设备与系统

| 系统 / 设备 | 当前状态 |
|---|---|
| **Nokia 603 / Belle** | 主要开发与真机验证基线 |
| Nokia Belle 其他机型 | 预期可运行；N8 / E7 / X7 / C7 推荐 Belle + `OpenFileL` 边下边播，欢迎反馈 |
| Symbian Anna | 使用同一应用 SIS；需要 Qt 4.7.4 + Qt Mobility 1.2.x，真机覆盖仍在收集 |
| 原版 Symbian³ | 使用同一应用 SIS；需要离线 Qt / Mobility 运行库，真机覆盖仍在收集 |

Belle 设备通常已经具备所需运行环境。Anna / 原版 Symbian³ 若缺少运行库，需要先安装
兼容的 **Qt 4.7.4** 与 **Qt Mobility 1.2.x**；项目不依赖已经停止服务的 Smart Installer。

更详细的兼容策略见
[Symbian³ / Anna / Belle 统一安装包说明](docs/user/COMPATIBILITY_ZH.md)。

## 已知限制

- **软件解码目前首先追求“能正确播放”，还不是满帧性能版本。** 对目前需要进入 `SW`
  路径的 360P H.264 视频，可以预期普遍存在不同程度的卡顿；实际帧率会随码流复杂度、
  参考帧结构和设备性能变化，但音频与播放器时钟会优先保持连续；
- 1.0 后的首要性能目标是把 **Symbian³ 全系设备的 360P 软件播放推进到稳定 20fps 以上**。
  其中 Nokia 603 的软硬件性能明显高于 N8 一代，**603 的长期目标是继续向稳定 30fps 攻坚**；
  而 **N8 一代稳定 20fps** 才是下一阶段需要针对性投入和验证的关键目标；
- Nokia 603 上，能够进入系统硬件解码路径的视频目前基本可以稳定达到 **30fps**。N8 一代的
  硬件解码能力低于 603，但对于其硬解兼容范围内的视频，仍预计能够保持流畅播放；
- 直播链路仍属实验性功能，不作为当前正式版的稳定性承诺；
- 原版 Symbian³ / Anna 的离线 Qt/Mobility 运行库仍需要更多真机验证；
- B 站接口、登录流程和媒体 URL 都可能随服务端变化。

## 后续计划

下一阶段优先建立正式版稳定性和软件视频性能基线，再处理外部播放器回退、更多系统覆盖和
直播。当前优先级见[路线图](docs/ROADMAP_ZH.md)，当前技术结构见
[开发文档](docs/README_ZH.md)；历史研究不代表默认实现。

## 构建

需要 Qt SDK 1.2.1、Qt 4.7.4 for Symbian、GCCE 4.4.1，以及同一 SDK 随附的
`qmake`、`make`、`makesis` 和 `signsis`。正式统一包默认使用最低
`Symbian3Qt474`；Belle 的 `SymbianSR1Qt474` 只作为设备调试后备。建议将 SDK
安装在短 ASCII 路径，例如 `C:\QtSDK`。

先构建 H.264-only FFmpeg 依赖：

```powershell
git clone https://github.com/hrydgard/ppsspp-ffmpeg .tmp/ppsspp-ffmpeg-research
git -C .tmp/ppsspp-ffmpeg-research checkout b87f7c6d522d1edba77cfc4fac96ce48a236f806
symbian\third_party\ppsspp_ffmpeg\Build-Gcce-H264.ps1 -EnableArmAssembly -Reconfigure
```

再构建应用：

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-App.ps1 -Configuration debug
.\symbian\Build-App.ps1 -Configuration release
```

构建输出位于被 Git 忽略的 `symbian/out/`。用于公开发行的 SIS 必须使用发布者自己的
**当前有效证书**签名；Qt SDK 自带的旧开发证书可能已经过期。证书、私钥、账号 Cookie、
设备日志与本机构建产物都不得提交到仓库。

更完整的环境说明见 [Symbian 构建文档](symbian/README.md)，FFmpeg 静态链接与 LGPL
重链接材料见 [FFmpeg 说明](symbian/third_party/ppsspp_ffmpeg/README.md)。

## 仓库结构

| 路径 | 内容 |
|---|---|
| `symbian/app/` | Qt/qmake 应用入口、NIKINIKI 图标和资源声明 |
| `symbian/source/`、`symbian/include/` | Symbian C++03 应用、网络、UI 与播放器代码 |
| `symbian/resources/` | 应用字体、卡片背景及对应许可证 |
| `symbian/third_party/` | 固定版本的 NanoVG、QR、FFmpeg 与兼容层 |
| `docs/` | 架构决策、真机证据、实验记录与历史发行文档 |

正式产品代码、构建脚本和公开文档只维护在本仓库；完整上游 wiliwili 与其他研究树只作
行为比较和移植参考，不形成第二份产品主线。

自研代码、上游快照与第三方依赖的精确边界见
[代码边界分析](docs/reference/CODE_BOUNDARY_ANALYSIS_ZH.md)。

## 发布与贡献

提交或发布前建议运行：

```powershell
.\tools\Test-PublicRepository.ps1
```

GitHub 仓库只提交源码、文档、资源和可复现构建材料；Release 资产只应包含经过真机验证
并使用有效证书签名的 Release SIS，以及对应 SHA-256。Debug 包、unsigned SIS、旧诊断包、
证书、私钥和中间文件不公开。

详细边界见
[仓库整理与 GitHub 发布政策](docs/developer/REPOSITORY_POLICY_ZH.md)。

## 许可证与致谢

本仓库整体按 [GPL-3.0](LICENSE) 发布。完整来源清单见 [NOTICE.md](NOTICE.md)。
NIKINIKI 延续并明确标注上游 `xfangfang/wiliwili` 的来源与许可证；PPSSPP-FFmpeg 按
LGPL 2.1 or later 使用，字体及其他第三方组件的许可证随对应目录一并提供。

感谢 wiliwili、Qt、Symbian、FFmpeg/PPSSPP，以及仍在为 Nokia / Symbian 旧设备编写、
维护和测试软件的开发者与用户。
