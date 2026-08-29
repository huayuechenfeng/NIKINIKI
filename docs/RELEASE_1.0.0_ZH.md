# NIKINIKI 1.0.0 正式发布说明

> 发布日期：2026-08-29
> 状态：正式 Release SIS 已完成编译、打包、有效签名和静态验收

## 正式安装包

| 项目 | 值 |
|---|---|
| 文件 | `symbian/out/releases/v1.0.0/NIKINIKI_1.0.0_release.sis` |
| GitHub Release | [v1.0.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.0.0)，资产名 `NIKINIKI_1.0.0_release.sis` |
| 大小 | 9,804,372 bytes |
| SHA-256 | `49748F5B4061F1F3D776B9FA7275B0092F2BA75F9F46872AE2FF88BB2B641095` |
| 包版本 | 1.0.0 |
| UID | `0xE000B100` |
| 目标 | ARMv5 / GCCE 4.4.1 / `Symbian3Qt474` |
| 能力 | `NetworkServices ReadUserData` |

未签名归档仅用于重签与 LGPL 重链接流程，不作为安装包：
`symbian/out/releases/v1.0.0/NIKINIKI_1.0.0_release_unsigned.sis`，
9,803,656 bytes，SHA-256
`6A820D77649F50986A464FD292BA13940B9641A117F136CC92B2D9BB12D63813`。

## 安装前置包

仓库的 `prerequisites/` 目录保留由已安装 Symbian Anna SDK 提取的官方运行库；
原版 Symbian³、Anna 可使用同一套运行库。下载地址如下：

- [Qt 4.7.403 for Anna](https://raw.githubusercontent.com/huayuechenfeng/NIKINIKI/main/prerequisites/Qt-4.7.403-for-Anna.sis)，7,929,828 bytes，SHA-256 `E270767D363770B7CCFB3D3F1973BC9834F833BCCCD50A5441BDA82E6581B2CE`；
- [Qt Mobility 1.2.1 for Anna](https://raw.githubusercontent.com/huayuechenfeng/NIKINIKI/main/prerequisites/QtMobility-1.2.1-for-Anna.sis)，2,475,460 bytes，SHA-256 `312DE62AA8CA99AE5B460F58F5B2FDB3E45A33935525DACD327C98B44E701065`。

## 构建与签名验收

- Qt 4.7.4 qmake 使用 `symbian-sbsv2`，构建目标为 `arm.v5.urel.gcce4_4_1`；
- GCCE Release 完成，`sbs errors: 0`，32 条为既有 SDK/GCCE 警告；
- 普通 1.0 构建包含 `WILIWILI_ENABLE_FFMPEG_SOFT_DECODER`、`WILIWILI_FFMPEG_ARMV6_ASM`、`libavcodec.lib` 和 `libavutil.lib`；未启用诊断用 `ffmpeglatedrop1`；
- H.264-only PPSSPP-FFmpeg 依赖以 ARM1176JZF-S、ARMv6/VFPv2、`-Os`、NEON disabled 配置重建；
- E32 image 为 ARMV5、Soft VFP、非 Debuggable，模块版本 1.0；code link end 为 `0x00345FDC`，低于固定 data link address `0x00400000`；
- 导入表包含 QtCore、QtGui、QtNetwork、QtOpenGL、QtMultimediaKit、MMF、DevVideo、RHTTP 和 GLES2，不含 Belle-only `cookiemanager.dll`；
- SIS 包含主 EXE、AppArc 资源、MIF 图标、NIKINIKI CJK 字体和许可证；主 EXE 能力与 SIS 包头一致；
- SDK 的 2009–2019 旧自签名已剥离，正式包由 `Qt Development Frameworks` 当前证书重签，证书有效期为 2026-08-24 至 2036-08-21；
- host `mongoose_compat` JSON 测试通过 58/58。

## 1.0 播放策略

兼容 AVC 继续优先走 MMF 硬件播放；Broadcom header preflight 拒绝的风险流在手机本机走 H.264-only PPSSPP-FFmpeg，AAC 仍由 MMF 提供。软件画面使用 CPU YUV420P→RGB565 LUT2X2 和独立不透明原生表面，透明 ARGB 顶层只绘制弹幕和控制。应用不使用桥接、远端重封装或远端转码。

## 已知边界

- Nokia 603 / Belle 是当前已验证设备基线；原版 Symbian³ 和 Anna 使用同一应用 SIS，但仍需公测用户补齐 Qt 4.7.4 / Qt Mobility 1.2.x 并回传结果；
- Release 50 次重复进入压力测试仍未完成；
- ARM1176 软件解码性能受限，既有 CPU RGB565 基线约 11.4–12.0 fps，不代表所有码流都可实时播放；
- 直播、外部播放器兜底和部分网络/API 行为仍属于后续工作；
- 旧 0.9 GLES-YUV 包、0.7 诊断包和 Debug/unsigned 包都不是 1.0 正式发行物。
