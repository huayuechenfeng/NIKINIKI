# ADR-0001：受工作区事件约束的原生横屏

> 状态：Accepted
> 决定日期：2026-08-28

## 背景

旧路径保持系统竖屏并旋转视频、UI 和输入。它能避免窗口切换崩溃，但软件整帧旋转在真机上
约耗费 224 ms/frame。早期直接切换横屏又因 AVKON panes、Qt fullscreen 和物理几何不同步而退出。

## 决定

保留 AVKON panes 和主 QGL 映射；先退出稳定竖屏 fullscreen，等待 `workAreaResized()` 确认
物理 640×360，再显示持久播放器顶层 fullscreen。返回时先隐藏播放器，确认物理 360×640，
再动态恢复主 QGL fullscreen。

## 后果

- MMF、soft frame、弹幕、控制和输入直接使用 640×360；
- 删除整帧 90° 中间图、像素旋转和逆触摸映射；
- 不使用 `AA_S60DontConstructApplicationPanes`，不在方向切换的 `resizeEvent()` 中重建窗口树；
- 每个状态等待必须有超时和竖屏恢复路径。

## 证据

最终 app-shell 探针连续 20 轮成功；集成主线四视频矩阵也完成进入、播放和返回。
完整日志结论见 `docs/research/player/PLAYER_ORIENTATION_PROBLEM_AND_WORKAROUND_ZH.md`。
