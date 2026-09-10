# ADR-0009：把 Qt Mobility 作为可选产品播放器后端

> 状态：Superseded by ADR-0010
> 决定日期：2026-09-07
> 基线：1.2.0 正式发布后的维护主线

## 背景

E7 已有 `QMediaPlayer + QVideoWidget` 真机出画证据，Qt Mobility 的 Symbian backend 也主动
管理 MMF graphics surface、dummy window 和 pending display changes。产品兼容性不再等待
native MMF 黑屏根因调查完成；该调查继续作为独立研究支线。

ADR-0008 只允许应用内 Qt 视频诊断并禁止替换正式后端。新的产品方向需要解除这一限制，但不把
probe 出画提前写成完整播放器通过，也不声称已经解释 Qt 成功的原因。

## 决定

- 设置页新增独立“播放器方案”：默认“native MMF 播放器”，可选“QT兼容播放器”；
- 原“播放方式”改称“传输方式”，继续保存 OpenUrl、增长文件和下载后播放三种点播策略；
- QT兼容播放器通过公开 `QMediaPlayer + QVideoWidget` 使用 Qt Mobility 的 Symbian MMF backend，
  应用不复制、探测或控制其私有 surface/dummy-window 实现；
- 播放器方案在媒体会话开始时锁定。QT失败不自动切换到 native，以免黑屏无错误时形成隐式循环；
- 自动模式中 Broadcom header 接受或 preflight 失败时使用用户选择的硬件后端；明确拒绝仍进入
  既有 FFmpeg 视频 + native MMF AAC/主时钟路径；全软解和直播第一阶段同样保留既有路径；
- Qt 播放对象在主 QGL 已初始化、物理横屏已确认且视频宿主可见后首次按需创建；创建后跨会话
  保留，退出只 stop、清空媒体、解绑输出并隐藏，不销毁对象图；
- 现有横屏状态机、软视频表面、ARGB overlay、弹幕、控制、输入和单向 CDN/清晰度策略不变。

## 后果

- 旧安装继续默认使用 native MMF，不改变已验证的 Nokia 603 行为；
- 下载后播放最接近 E7 已有完整本地 MP4 证据，但 OpenUrl 和增长文件仍须分别真机验证；
- `videoAvailable`、状态和 position 只作为运行标记，不代替人工首帧确认；
- 初代 Symbian³ 支持公告只有在完整产品壳、返回/重入和 603 回归通过后才能调整；
- ADR-0008 被本决定取代，但其中已完成诊断的证据边界继续保留。

## 验收

1. 设置持久化、默认值和下一会话生效；
2. GCCE Debug/Release 编译与 SIS 打包；
3. E7 下载后播放的画面、声音、overlay、暂停、seek、倍速、音量、返回和重复进入；
4. E7 的 OpenUrl、增长文件与自动/硬解/软解组合；
5. Nokia 603 native 默认路径独立回归；
6. 真机结果只写入 `docs/reference/DEVICE_TEST_MATRIX.md`。

2026-09-10 的清理审计确认，完整产品壳 Qt 候选尚未通过 E7 验收，且与大量诊断
入口共享同一未提交工作区。本决定的实现因此不进入普通产品主线；后续边界由
[ADR-0010](0010-freeze-qt-candidate-and-restore-native.md)取代。
