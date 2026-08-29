<p align="center">
  <img src="symbian/app/icons/nikiniki.png" alt="NIKINIKI icon" width="192" height="192">
</p>

<h1 align="center">NIKINIKI</h1>

<p align="center">
  面向 Symbian³ / Nokia Belle 的第三方哔哩哔哩客户端
</p>

> NIKINIKI 曾用名为 **wiliwili for Symbian³** / `wiliwili_symbian`。项目
> 源自对 [xfangfang/wiliwili](https://github.com/xfangfang/wiliwili) 的
> Symbian 移植与重构；旧可执行文件名、UID、目录名及历史技术文档会继续保留，
> 以保证升级兼容并完整记录项目沿革。

## 当前状态

- 当前版本：**0.9.0 真机验证候选**，尚不代表 1.0 稳定版；
- 主要验证设备：Nokia 603（Symbian Belle、640×360）；
- 普通兼容视频优先使用系统 MMF 硬件播放；
- 被 Broadcom H.264 真实 header preflight 拒绝的码流在手机本机使用 H.264-only PPSSPP-FFmpeg 软件解码；
- 视频、弹幕、控制层与触摸已使用经真机验证的原生横屏状态机；
- 不使用桥接、远程封装或远程转码。

当前源码已通过 GCCE Debug/Release 构建，但新的独立 RGB565 视频表面与本次品牌/
图标变更仍需在 Nokia 603 上完成最后验收。因此，仓库中记录的旧 0.9 SIS 只属于
历史测试证据，不应作为当前公开发行包。完整状态以
[阶段报告](docs/DEVELOPMENT_STAGE_REPORT_2026-08-25_ZH.md)和
[设备验证矩阵](docs/DEVICE_TEST_MATRIX.md)为准。

## 已实现功能

- 首页推荐、分区内容、动态、消息、搜索与视频详情；
- 扫码登录、历史、收藏、稍后再看、关注和互动；
- 普通视频 MMF 播放、部分不兼容 AVC 的本机软件解码；
- 进度、暂停、倍速、音量、清晰度、弹幕和触摸控制；
- 播放器持久化复用，已修复原先稳定复现的第二次进入崩溃；
- Qt 4.7.4 / GCCE 4.4.1 / Symbian Belle 的可复现构建脚本。

## 已知限制

- 软件解码性能受 ARM1176JZF-S 限制，仍在做最终真机性能验收；
- 直播、外部播放器兜底和 50 次 Release 重复进入压力测试尚未完成；
- B 站接口、登录流程和媒体可用性可能随服务端变化；
- 本项目不是哔哩哔哩或上游 wiliwili 的官方产品。

## 构建

需要 Qt SDK 1.2.1、Qt 4.7.4 for Symbian、GCCE 4.4.1，以及同一 SDK随附的
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

构建输出位于被 Git 忽略的 `symbian/out/`。用于公开发行时必须使用发布者自己的
有效证书签名；证书、私钥、账号 Cookie、设备日志与本机构建产物都不得提交。
更完整的环境说明见 [Symbian 构建文档](symbian/README.md)，FFmpeg 静态链接与
LGPL 重链接材料见 [FFmpeg 说明](symbian/third_party/ppsspp_ffmpeg/README.md)。

## 仓库结构

| 路径 | 内容 |
|---|---|
| `symbian/app/` | Qt/qmake 应用入口、NIKINIKI 图标和资源声明 |
| `symbian/source/`、`symbian/include/` | 自研 Symbian C++03 应用、网络、UI 与播放器代码 |
| `symbian/resources/` | 应用字体、卡片背景及对应许可证 |
| `symbian/third_party/` | 经固定版本的 NanoVG、二维码、FFmpeg 与自研兼容层 |
| `docs/` | 当前事实、架构决策、真机证据与历史发行记录 |

本仓库是 NIKINIKI 产品代码、构建脚本和公开文档的唯一主线。正式构建不引用
完整 wiliwili、borealis submodule 或桌面/主机资源；这些内容只保留在并列的
本地研究仓库中，不通过复制或导出形成第二份可编辑产品源码。

自研代码、上游快照与第三方依赖的精确边界见
[代码边界分析](docs/CODE_BOUNDARY_ANALYSIS_ZH.md)。文档阅读顺序见
[Symbian³ 文档索引](docs/README_ZH.md)。

## 发布原则

- 产品代码、资源、构建脚本和公开文档只在本仓库修改；本地研究仓库不得作为
  第二产品主线；
- 提交或发布前运行 `./tools/Test-PublicRepository.ps1`，检查构建闭包、路径边界、
  敏感材料、危险扩展名和文档链接；
- GitHub 仓库只提交源码、文档、资源和可复现构建材料；
- GitHub Release 只发布通过最终真机验收的单一 Release SIS 与 SHA-256；
- Debug、unsigned、旧诊断包、证书、私钥和中间文件不公开；
- 发现问题时请注明设备、系统版本、视频 BVID、复现步骤及不含账号信息的日志片段。

细则见 [仓库整理与 GitHub 发布边界](docs/REPOSITORY_LAYOUT_AND_GITHUB_POLICY_ZH.md)。

## 许可证与致谢

本仓库整体按 [GPL-3.0](LICENSE) 发布。完整来源清单见
[NOTICE.md](NOTICE.md)。NIKINIKI 延续并明确标注上游
`xfangfang/wiliwili` 的来源与许可证；PPSSPP-FFmpeg 按 LGPL 2.1 or later
使用，字体及其他第三方组件的许可证随对应目录一并提供。

感谢 wiliwili、Qt、Symbian、FFmpeg/PPSSPP 以及所有参与 Nokia Belle
生态维护和真机测试的开发者与用户。
