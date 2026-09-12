# NIKINIKI 1.3.0 正式发布说明

> 发布日期：2026-09-12
> 状态：正式 Release SIS 已完成编译、打包、有效签名、用户功能验收和公开发布

## 正式安装包

| 项目 | 值 |
|---|---|
| 文件 | `symbian/out/releases/v1.3.0/NIKINIKI_1.3.0_release.sis` |
| GitHub Release | [v1.3.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.3.0)，资产名 `NIKINIKI_1.3.0_release.sis` |
| 重链接材料 | `NIKINIKI_1.3.0_relink_materials.zip`；包含未签名 SIS、产品源码快照、固定 FFmpeg 源码快照、许可证、校验和重建说明 |
| 大小 | 9,898,032 bytes |
| SHA-256 | `6EB8ECF2D5FC44DE43F71EC3689114F10E5C9845155399E7428906C9251C7854` |
| 包版本 | 1.3.0 |
| UID | `0xE000B100` |
| 目标 | ARMv5 / GCCE 4.4.1 / `Symbian3Qt474` |
| 能力 | `NetworkServices ReadUserData` |

未签名 SIS 仅收录于 LGPL 重链接材料，不作为安装包；其大小和 SHA-256 见材料包内的
`SHA256SUMS.txt`。

## 本次更新

- 播放方式扩展为“内置/系统播放器 × 流式/边下边播/下载后播放”六项；
- 系统“流式”固定请求并核对 360P progressive MP4，直接把 HTTP(S) 地址和 `video/mp4`
  类型交给系统关联播放器；
- 系统“边下边播”和“下载后播放”均先完整下载 360P MP4，核对长度和 `ftyp` 文件头后再
  交接本地文件，不把仍在增长的文件交给外部应用；
- 交接前停止内部媒体并恢复竖屏；启动失败会反馈，从外部播放器返回后可重新选择和播放；
- 保留原有内置 MMF/FFmpeg 默认值与三种内置播放方式，并迁移旧的播放设置；
- 远程交接不附带 Cookie/Referer，也不记录完整签名媒体地址。

用户已完成签名候选包的功能测试并确认可以运行。`StartDocument()` 或系统关联处理器接受请求
只代表交接成功，不能单独证明外部播放器已经正确输出画面和声音；不同固件、网络和播放器仍需
分别验证。

## 初代 Symbian³ 黑屏与系统播放器

Nokia N8 / E7 / X7 / C7 等初代 Symbian³ 机型的内置播放黑屏经过完整应用、窗口、MMF、
QVideoWidget、graphics surface 和 DevVideo 等受控核查后，已定位为系统底层媒体/显示链路限制。
短期内没有可由普通应用层可靠交付的修复。

1.3 可在“设置 → 播放方式”改用系统播放器。系统流式模式直接打开 360P MP4 地址；如果固件的
系统播放器无法流式打开 HTTPS 视频，可尝试安装
[Belle HTTPS Streaming 补丁](https://nnproject.cc/bellehttpsplayer/)，或选择系统播放器的
“下载后播放”。该补丁页面声明支持 Belle Refresh / FP1 / FP2，并依赖 TLS 补丁。

## 可选 H.264 ref7 补丁

[下载 NIKINIKI_REF7_UNIVERSAL_SNR_R1.rmp](https://raw.githubusercontent.com/huayuechenfeng/NIKINIKI/v1.3.0/symbian/patches/h264-ref7/NIKINIKI_REF7_UNIVERSAL_SNR_R1.rmp)。

该 RomPatcher+ 补丁绕过系统 IVE H.264 解码器的 ref=6 准入限制，可提高部分 B 站 ref7 视频
进入硬件解码的机会。它修改的是系统硬解组件的运行时影子，不只影响 NIKINIKI；其他调用同一
硬解组件的播放器也会受到影响。补丁不随 SIS 安装、不由应用探测或启用，必须手动操作且不得
设为自启。目前只有 Nokia 603 / RM-779 / Belle 113.010.1506 真机验证，其他固件不保证可用。
风险、适用边界和恢复方式见[补丁说明](../../symbian/patches/h264-ref7/README_ZH.md)。

## 构建与签名验收

- Qt 4.7.4 qmake 使用 `symbian-sbsv2`，构建目标为 `arm.v5.urel.gcce4_4_1`；
- GCCE Debug/Release 均为 `sbs errors: 0`、33 条既有 SDK/GCCE 警告；SIS 打包、主机 JSON
  58/58、公开仓库边界、文档链接与 diff 检查均已通过；
- 普通构建使用 native MMF/FFmpeg 播放，不包含 Qt 候选后端或 E7 诊断 CONFIG；
- SDK 的 2009–2019 旧自签名已剥离；正式包使用 `NIKINIKI` 当前证书重签，证书有效期为
  2026-09-02 至 2036-08-30；
- Release 只公开正式签名 SIS 和 LGPL 重链接材料，不公开 Debug、证书、私钥或中间包。

## 已知边界

- 初代 Symbian³ 的内置播放黑屏仍存在；系统播放器只是可选兼容路径，不代表所有固件或媒体通过；
- 系统播放器能否直接访问 HTTPS MP4、能否解码具体 H.264 码流由固件和已安装补丁决定；
- 本机 FFmpeg 软件解码受 ARM1176 性能限制，复杂码流仍可能卡顿；
- 直播仍属实验性能力，不属于 1.3 的稳定性承诺；
- Bilibili API、登录和签名媒体 URL 可能随服务端变化。
