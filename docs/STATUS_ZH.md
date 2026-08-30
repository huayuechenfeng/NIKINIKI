# NIKINIKI 当前状态

> 状态：Active
> 适用版本：1.0.0 与当前主线
> 最近事实核验：2026-08-29
> 本页职责：描述当前实现和验证边界，不记录实验过程或发布校验值

## 发布基线

NIKINIKI 1.0.0 已完成 `Symbian3Qt474` GCCE Release 编译、打包、有效签名和公开发布。
正式安装包、大小、SHA-256、证书和 LGPL 重链接材料只在
[1.0.0 发布说明](releases/RELEASE_1.0.0_ZH.md)维护。

当前统一包：

- 目标为 ARMv5、Qt 4.7.4、GCCE 4.4.1；
- 使用最低 `Symbian3Qt474` 构建，一个应用 SIS 覆盖 Symbian³、Anna 和 Belle；
- Nokia 603 / Nokia Belle 是当前主要真机基线；
- 原版 Symbian³ 和 Anna 需要补齐 Qt 4.7.4、Qt Mobility 1.2.x，并继续收集真机结果。

## 已实现能力

- 首页推荐、栏目、动态、消息、搜索和视频详情；
- 二维码登录、登录态持久化、历史、收藏、稍后再看、关注和互动；
- 评论根列表、楼中楼和分页所需的客户端结构；
- Progressive MP4 选源、备用 URL 和 H.264/AAC 播放；
- 播放/暂停、进度、倍速、音量、清晰度、弹幕和触摸控制；
- 原生横屏播放器、无系统栏竖屏恢复和重复进入；
- 系统 MMF 硬件播放与手机本机 FFmpeg H.264 软件回退；
- Qt 4.7.4 / GCCE 4.4.1 的构建、打包和公开仓库检查脚本。

## 当前播放器

播放器对每个首选 progressive MP4 做一次只读 Broadcom header preflight：

```text
Broadcom 接受真实 H.264 header → MMF 硬件视频
Broadcom 明确拒绝             → 本机 FFmpeg H.264 视频 + MMF AAC
preflight 获取失败            → 保守回到 MMF，并保留失败日志
```

软件画面采用 CPU YUV420P→RGB565 LUT2X2，由持久、不透明的原生视频表面显示；
唯一透明 ARGB 顶层只绘制弹幕和控制。播放器完整原生对象图跨会话复用，横屏下视频、UI、
弹幕和输入都直接使用 640×360 坐标。

实现细节、状态机和日志标记见
[播放器架构](developer/PLAYBACK_ARCHITECTURE_ZH.md)。

## 已验证事实

- Nokia 603 已确认 MMF 首次横屏画面、声音、弹幕和控制 UI 正常；
- `KErrMMPartialPlayback (-12017)` 已作为可恢复轨道探测结果处理；
- `surfacepersist1` 证明完整播放器对象图持久复用可以反复退出和重入，原确定性第二次进入崩溃已功能修复；
- 最终 app-shell 原生横屏探针完成 20 轮，横竖屏均无系统栏，0 timeout；
- 集成主线日志 `2746319` 覆盖两条 FFmpeg soft、两条 MMF 视频，画面方向、弹幕/控制、返回和对象复用均正确；
- Broadcom decoder 在相同输入契约下接受工作流 header，却稳定拒绝 7-ref/DPB7/weighted 风险流；
- 系统 ARM decoder 虽接受风险 header，但返回 0×0 且 Configure 为 `KErrNotSupported`，不能作为产品后端；
- 本机 FFmpeg 已让原有声无画视频出画面；CPU RGB565 LUT2X2 的历史基线约为 11.4–12.0 fps；
- GLES 三平面 YUV 路线真机成本过高，已经退出普通播放；
- Debug 和 Release 普通构建均已通过，1.0 Release 为 `sbs errors: 0`，32 条为既有 SDK/GCCE 警告。

原始测量、样本和否定实验均保存在[播放器研究索引](research/README_ZH.md)，不在本页展开。

## 待验证和未完成

- 1.0 Release 的 50 次播放器进入/退出压力循环；
- 正式 1.0 包中真实 header preflight 的 `ACCEPT → MMF` 与 `REJECT → FFMPEG` 真机标记；
- 独立不透明 soft surface 与 500 ms `PositionL()` 校准方案的专项真机遥测，包括
  `softSurfacePresented>0`、`overlayVideoDrawMs=0` 和 position cache 命中；
- 原版 Symbian³、Anna 的统一 SIS 安装、TLS、登录和媒体回归；
- 当前主线“内置字体子集先出首帧、完整 CJK 字体分块加载”的冷启动修复已通过 `Symbian3Qt474` Debug/Release GCCE 编译，仍需在重装并重启手机后的真实冷缓存条件下验证；
- 更多机型、H.264 profile/level、分辨率、码率和 CDN 组合的兼容矩阵；
- 内置软件解码失败或性能不足时的明确外部播放器交接；
- 直播链路和直播弹幕的稳定实现；
- 软件播放性能继续提升到可接受的稳定帧率。

这些项目的顺序和验收条件见[路线图](ROADMAP_ZH.md)。

## 已知限制

- ARM1176 上的软件解码不是满帧路径，不同码流会有明显性能差异；
- 当前只把 Nokia 603 / Belle 视为完整设备基线，不能把预期兼容写成真机通过；
- 直播不属于 1.0 稳定性承诺；
- 外部播放器回退尚未形成正式产品闭环；
- Bilibili API、登录和媒体 URL 会受服务端变化影响；
- 已发布的 1.0.0 首次启动仍可能因同步读取完整字体而长时间黑屏，处理方式见[故障排查](user/TROUBLESHOOTING_ZH.md)；当前主线修复尚未取得真机结论。

## 当前事实的维护位置

| 事实 | 维护位置 |
|---|---|
| 安装包和签名 | `docs/releases/RELEASE_1.0.0_ZH.md` |
| 真机通过/失败 | `docs/reference/DEVICE_TEST_MATRIX.md` |
| 工具链版本 | `docs/reference/TOOLCHAIN_REPORT.md` |
| 当前播放结构 | `docs/developer/PLAYBACK_ARCHITECTURE_ZH.md` |
| 工作优先级 | `docs/ROADMAP_ZH.md` |

本页只汇总这些来源，不复制校验值、完整测试表或实验日志。
