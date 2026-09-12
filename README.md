<p align="center">
  <img src="symbian/app/icons/nikiniki.png" alt="NIKINIKI icon" width="192" height="192">
</p>

<h1 align="center">NIKINIKI</h1>

<p align="center">
  面向 Symbian³ / Anna / Nokia Belle 的第三方哔哩哔哩客户端
</p>

<p align="center">
  <strong>1.3.0 · 系统播放器兼容播放</strong>
</p>

> **初代 Symbian³ 播放说明**：Nokia N8 / E7 / X7 / C7 等机型的内置播放器黑屏问题经受控
> 核查位于系统底层媒体/显示链路，短期内无法由普通应用代码可靠修复。NIKINIKI 1.3 可以在
> “设置 → 播放方式”中改用系统播放器。系统流式播放会直接交接 360P MP4 链接；如果系统播放器
> 不能流式打开 HTTPS 视频，可尝试安装
> [Belle HTTPS Streaming 补丁](https://nnproject.cc/bellehttpsplayer/)，或选择系统播放器的
> “下载后播放”。调用被系统接受不等于实际播放成功，兼容性仍取决于固件与系统播放器。

> **H.264 ref7 破解补丁下载**：
> [下载 NIKINIKI_REF7_UNIVERSAL_SNR_R1.rmp](https://raw.githubusercontent.com/huayuechenfeng/NIKINIKI/v1.3.0/symbian/patches/h264-ref7/NIKINIKI_REF7_UNIVERSAL_SNR_R1.rmp)。
> 它绕过系统 IVE H.264 解码器的 ref=6 准入限制，使部分 ref7 B 站视频有机会继续使用硬件解码。
> 这是系统级补丁，因此不仅影响 NIKINIKI，也会影响其他调用同一系统硬解组件的播放器。
> 补丁需要深度破解和 RomPatcher+，必须手动启用且不得设为自启；目前只在 Nokia 603 SW113
> 真机验证，其他机型或固件不保证可用。完整说明见
> [ref7 补丁文档](symbian/patches/h264-ref7/README_ZH.md)。

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

## 下载 1.3

- [NIKINIKI 1.3.0 GitHub Release](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.3.0)
- [直接下载正式 SIS](https://github.com/huayuechenfeng/NIKINIKI/releases/download/v1.3.0/NIKINIKI_1.3.0_release.sis)
- [LGPL 重链接材料](https://github.com/huayuechenfeng/NIKINIKI/releases/download/v1.3.0/NIKINIKI_1.3.0_relink_materials.zip)

安装包大小、SHA-256、签名和重链接材料清单见
[1.3.0 发布说明](docs/releases/RELEASE_1.3.0_ZH.md)。若新版本安装、启动或系统播放器交接异常，
请到 QQ 群 `977410275` 反馈。

## 安装前置说明

所有设备都建议安装 TLS 1.2 补丁；原版 Symbian³ 和 Anna 还需要 Qt 4.7.4 与
Qt Mobility 1.2.x。完整顺序和运行库下载见[安装指南](docs/user/INSTALL_ZH.md)。
首次启动、TLS 或播放问题见[故障排查](docs/user/TROUBLESHOOTING_ZH.md)。

## 开源与赞助

本项目完全开源并公开发布，也欢迎大家投喂一点可乐钱。赞助完全自愿，不会影响
软件的正常使用、功能获取或源码开放：
[爱发电支持 NIKINIKI](https://afdian.com/a/nankoku)。

## 1.3 更新重点

播放方式现在提供“内置播放器 / 系统播放器 × 流式 / 边下边播 / 下载后播放”六种选择。
内置播放器仍是默认值，继续使用 NIKINIKI 的 MMF/FFmpeg 路径；系统播放器模式固定选择并
核对 360P progressive MP4，再交给手机已关联的播放器。

- 系统“流式”直接打开 MP4 链接，不先下载完整文件；
- 系统“边下边播”和“下载后播放”目前都会完整下载并核验 MP4，再交接本地文件；
- 交接前停止内部媒体并恢复竖屏，失败时给出反馈；从系统播放器返回后可以再次使用；
- 旧设置会迁移，原有内置播放默认值和三种内置模式保持不变。

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

需要绕开内置播放时，1.3 还可以把经过核对的 360P MP4 URL 或完整本地文件交给系统播放器。
NIKINIKI 只能确认系统接受了交接请求，无法确认外部播放器最终是否真的出画面和声音。

这避免了按照 `ref`、DPB、weighted prediction 等参数维护一张不断膨胀的“风险规则表”：
先让设备自己的 Broadcom 插件判断能否接受真实码流，不能硬解时才进入软件路径。

深度破解设备可以选择使用[实验性 ref7 admission 补丁](symbian/patches/h264-ref7/README_ZH.md)。
它已在 Nokia 603 SW113 上验证，不随 SIS 安装、不允许自动启用；未破解或未启用补丁时，上述默认
MMF/FFmpeg 路由完全不变。

## 已实现

- 首页推荐、分区内容、动态、消息、搜索和视频详情；
- 扫码登录、历史、收藏、稍后再看、关注和互动；
- 普通 H.264/AAC 视频优先使用 Symbian MMF 硬件播放；
- 硬件插件拒绝的部分现代 AVC 视频可在手机本机使用裁剪版 PPSSPP-FFmpeg 软解；
- 播放/暂停、进度拖动、倍速、音量、清晰度和触摸控制；播放器右侧音量滑块与主页实体音量键；
- 滚动弹幕及播放器透明控制层；
- 首页可见卡片渲染与合并刷新；视频详情页纵向相关推荐；
- 自适应动态图文/专栏详情、评论区，以及避免昵称和等级图标重叠的评论排版；
- 原生横屏播放器状态机，可从播放页正常返回并重复进入；
- 播放方式可选内置/系统播放器各自的流式、边下边播和下载后播放；
- 解码方式可选自动选择、全程硬解和全程软解；
- Qt 4.7.4 / GCCE 4.4.1 / Symbian³ SDK 的可复现构建脚本；
- 单一 ARMv5 应用包设计，目标覆盖 Symbian³、Anna 和 Belle。

## 设备与系统

| 系统 / 设备 | 当前状态 |
|---|---|
| **Nokia 603 / Belle** | 主要开发与真机验证基线 |
| Nokia 700 / 701 / 808 等原生 Belle | 受支持；欢迎补充独立真机反馈 |
| Nokia N8 / E7 / X7 / C7 等初代 Symbian³ | 内置播放受系统底层黑屏问题限制；1.3 可改用系统播放器 |
| Symbian Anna / 原版 Symbian³ | 使用同一 SIS；需补齐运行库，内置播放仍受具体固件能力限制 |

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
- 初代 Symbian³ 设备的内置播放器黑屏属于已确认的系统底层限制，短期内没有应用层修复；
  可改用系统播放器，但远程 HTTPS 流式能力仍取决于固件或可选补丁；
- B 站接口、登录流程和媒体 URL 都可能随服务端变化。

## 后续计划

下一阶段优先建立正式版稳定性和软件视频性能基线，再处理更多系统覆盖和直播。
当前优先级见[路线图](docs/ROADMAP_ZH.md)，当前技术结构见
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
| `symbian/patches/` | 不随 SIS 安装的可选纯文本系统补丁及适用边界 |
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
