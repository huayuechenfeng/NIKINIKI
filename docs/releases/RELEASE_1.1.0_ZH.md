# NIKINIKI 1.1.0 正式发布说明

> 发布日期：2026-08-31
> 状态：正式 Release SIS 已完成编译、打包、有效签名、Nokia 603 真机验收和公开发布

## 正式安装包

| 项目 | 值 |
|---|---|
| 文件 | `symbian/out/releases/v1.1.0/NIKINIKI_1.1.0_release.sis` |
| GitHub Release | [v1.1.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.1.0)，资产名 `NIKINIKI_1.1.0_release.sis` |
| 重链接材料 | `NIKINIKI_1.1.0_relink_materials.zip`；包含未签名 SIS、产品源码快照、固定 FFmpeg 源码快照、许可证、校验和重建说明 |
| 大小 | 9,842,480 bytes |
| SHA-256 | `48102C45DB23279BB0DC1B09EBEC4224B4F4F4786B994AB813DBD634BF76DA60` |
| 包版本 | 1.1.0 |
| UID | `0xE000B100` |
| 目标 | ARMv5 / GCCE 4.4.1 / `Symbian3Qt474` |
| 能力 | `NetworkServices ReadUserData` |

未签名归档仅用于重签与 LGPL 重链接流程，不作为安装包：
`symbian/out/releases/v1.1.0/NIKINIKI_1.1.0_release_unsigned.sis`，
9,841,776 bytes，SHA-256
`F6B776AA47C2129B161128ABCD473856F8CD03E61752E17D64BEB72EAC3790E7`。

公开安装包统一使用 NIKINIKI 品牌名。`wiliwili_symbian` 只作为升级兼容所需的内部 target、
可执行文件名和历史路径保留，不再用作正式安装包资产名。

## 本次更新

- 修复首次安装后启动时同步加载完整 CJK 字体造成的长时间黑屏。应用会先用内置字体子集显示
  主界面，再分块加载完整字体，不再让字体初始化阻塞首屏；
- 设置页新增“播放方式”，可在独立列表中选择：当前流式播放、`OpenFileL` 边下边播、
  下载后播放；
- 设置页新增“解码方式”，可在独立列表中选择：自动选择、全程硬解、全程软解；
- `OpenFileL` 边下边播改用共享 `RFile`，修复 Nokia 603 上下载几十 MiB 后出现
  `KErrInUse (-14)` 的问题，并把起播预缓冲从 2 MiB 提高到 8 MiB；
- 下载后播放在准备阶段显示文件大小、已下载大小、百分比和进度条，不再以纯黑屏等待；
- 全程软解不再因 MMF controller 拒绝关闭视频轨而提前显示 `SWERR`。

Nokia 603 / Belle 已完成上述播放方式、解码方式、8 MiB 起播预缓冲、下载进度 UI 和设置列表
的真机验收。

## N8 / E7 / X7 / C7 建议

这批初代 Symbian³ 机型建议先升级到 Nokia Belle，再在 NIKINIKI 中选择：

```text
设置 → 播放方式 → OpenFileL 边下边播
```

同一视频能被系统播放器硬解、但 NIKINIKI 默认流式播放只有声音没有画面时，这个模式可绕开
旧固件 MMF streaming/controller 路径，同时继续使用手机本机硬件解码。不同固件和媒体仍可能
存在差异，欢迎反馈设备型号、系统版本、视频 BVID、清晰度和播放方式。

## 安装与回退

所有设备都建议安装 TLS 1.2 补丁；原版 Symbian³ 和 Anna 还需 Qt 4.7.4 与 Qt Mobility 1.2.x。
如果 1.1.0 安装或启动出现问题，请到 QQ 群 `977410275` 反馈，并可先继续使用
[NIKINIKI 1.0.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.0.0)。

## 构建与签名验收

- Qt 4.7.4 qmake 使用 `symbian-sbsv2`，构建目标为 `arm.v5.urel.gcce4_4_1`；
- GCCE Debug/Release 均完成，`sbs errors: 0`，32 条为既有 SDK/GCCE 警告；
- 普通构建包含 H.264-only PPSSPP-FFmpeg ARMv6/VFPv2 软件回退，未启用诊断 CONFIG；
- SIS 包含主 EXE、AppArc 资源、MIF 图标、NIKINIKI CJK 字体和许可证；
- SDK 的 2009–2019 旧自签名已剥离，正式包由 `Qt Development Frameworks` 当前证书重签，
  证书有效期为 2026-08-24 至 2036-08-21；
- 公开仓库、文档和 host JSON 门禁均通过，host JSON 测试为 58/58。

## 已知边界

- Nokia 603 / Belle 是当前完整验证基线；N8 / E7 / X7 / C7 的 Belle + 边下边播建议仍需更多
  用户样本验证；
- 全程硬解不会自动回退，硬件不能解码的视频会黑屏；全程软解受 ARM1176 性能限制，部分码流
  会明显卡顿；普通用户建议保留“自动选择”；
- 原版 Symbian³ 和 Anna 使用同一应用 SIS，但需要补齐运行库并继续收集公测结果；
- 直播、外部播放器兜底和部分网络/API 行为仍属于后续工作。
