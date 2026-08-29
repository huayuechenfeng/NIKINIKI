# 播放器原生横屏问题、根因与主线方案

> 更新日期：2026-08-28  
> 适用基线：Symbian³ 0.9 主线；原生横屏主播放器已完成四视频验证，独立 soft RGB565 表面待真机验证

## 1. 历史问题

旧播放器为了绕开 Symbian/Qt 切换方向后窗口消失、黑屏或退出的问题，始终让 OS、AVKON 和主 `QGLWidget` 保持 360×640 竖屏，只模拟横屏：MMF 对视频 `RWindow` 应用顺时针 90° 旋转；软解把视频、弹幕和控制栏先合成到 640×360 ARGB 图，再整体旋转到竖屏窗口；触摸坐标执行反向映射。

这条路线保证了首次播放，但软解真机 profiling 证明整帧旋转约占 224 ms/frame，是约 333–344 ms `paintEvent()` 的最大单项成本。旧的 soft-only native-landscape 实验又因在不稳定窗口状态中直接切换方向而闪退，因此曾被归档。

## 2. 最终定位

问题不是“Belle 原生横屏本身不可靠”，也不是 RGB565、MMF、FFmpeg 或 QPainter 必然导致横屏崩溃。根因是 QApplication/AVKON panes、Qt fullscreen 状态、物理屏幕方向和新顶层窗口建立的顺序不一致。

关键证据如下：

- 设置 `QApplication::AA_S60DontConstructApplicationPanes` 后，方向请求会出现 `availableGeometry=640×360`、`screenGeometry=360×640` 的分裂状态；在该状态强制显示窗口会在第一帧附近退出；
- 过早隐藏主 QGL 窗口会让应用失去前台，定时器和后续提交不再可靠；
- 保留 panes、主 QGL 始终映射、先在稳定竖屏退出 fullscreen、只等待 `workAreaResized()`，能得到物理屏幕精确 640×360；此后新顶层窗口 `showFullScreen()` 可以安全获得完整 640×360；
- 返回时先隐藏横屏窗口，再请求竖屏；物理屏幕与工作区确认恢复后，主 QGL 通过动态 `showFullScreen()` 重新取得精确 360×640，系统栏不会残留；
- 不在方向切换期间的 `resizeEvent()` 中修改播放器窗口树。

最终 `appshell7-final-dynamic-fullscreen-rgb565` 在 Nokia 603/CODA 连续通过 20 轮。日志包含 20 次 `WORKAREA_640X360_READY`、20 次 `FIRST_PAINT_END`、47 次动态 `RGB565_PRESENT`、20 次 `RESTORED_FULLSCREEN`、0 次 timeout，最后为 `COMPLETE cycles 20` 和 `EVENT_LOOP_EXIT 0`。用户同时确认色条、扫描线均正确，横屏和竖屏都没有系统栏。

## 3. 合入主线的窗口状态机

主线现在保留已解决第二次进入崩溃的完整持久对象图：player controller、MMF native video `QWidget/CCoeControl/RWindow`、soft opaque RGB565 `QWidget`、backend/observer、`CVideoPlayerUtility2` 和单一 ARGB overlay 都存活到应用退出。播放器控制窗口改为独立、无边框的持久顶层窗口；播放结束只停止、解绑并隐藏，不销毁媒体对象。

进入横屏的固定顺序：

```text
竖屏主 QGL fullscreen
→ 主 QGL showMaximized()，恢复已构造的 AVKON panes
→ 等 workAreaResized() 确认竖屏 pane 工作区
→ SetOrientationL(Landscape)
→ 等 workAreaResized() 确认 screenGeometry == 640×360
→ 播放器顶层窗口 showFullScreen()
→ 此时才显示 native video host、overlay 并打开媒体
```

退出顺序：

```text
停止/停放 MMF 与软解，隐藏 overlay、video host、播放器顶层窗
→ SetOrientationL(Portrait)
→ 等 workAreaResized() 确认 screenGeometry == 360×640
→ 主 QGL showMaximized() 后动态 showFullScreen()
→ 等 availableGeometry 和 screenGeometry 都为 360×640
→ 解除 player foreground guard，恢复主页
```

每个等待阶段有 6 秒保护超时；失败时终止本次播放并尝试恢复竖屏主页，不再回退到高成本的虚拟旋转。

## 4. 已移除的 90° 路径

原生横屏后，所有播放组成部分都直接使用真实的 640×360 坐标：

- MMF backend 固定使用 `EVideoRotationNone`；
- CPU soft frame 直接以 RGB565/QImage 绘制到独立、原生、不透明的 640×360 子窗口；
- 唯一透明 ARGB 顶层只绘制弹幕和控制栏，并在 soft 或 MMF 视频窗口之上接管输入；
- 已删除逐像素 `(x,y)→(H-1-y,x)` 整帧旋转缓冲和第二次 `drawImage()`；
- 触摸/拖动直接使用物理坐标，不再执行逆 90° 映射；
- 历史 GLES-YUV 路径若被诊断性启用，也不再使用旋转 texture coordinates。

因此历史 `overlayIntermediateMs` 和 `overlayRotateMs` 在新路径应保持为 0。按旧日志分项推算，去掉约 224 ms/frame 的旋转是当前最有价值的软解显示优化；真实收益必须以主线真机日志为准，不能直接把理论差值写成已实现帧率。

## 5. 当前验收边界

最终窗口探针和首轮主播放器四视频矩阵已经闭环；当前独立 soft 视频表面仍需验证：

1. 正常样本走 MMF，画面方向、比例、声音、弹幕、控制栏和触摸正确；
2. 风险样本走 `FFMPEG_SOFT_READY ... RGB565_LUT2X2`，并出现 `SOFT_SURFACE_ACTIVE/FIRST_PAINT`；确认软解画面方向、弹幕/UI 始终在上方，`softSurfacePresented>0`、`overlayVideoDrawMs=0`、`overlayRotateMs=0`，记录实际 fps；
3. 暂停、seek、音量、倍速、画质切换和弹幕开关；
4. 播放返回后主页必须为无系统栏 360×640；再次进入至少 10 轮，播放器原生对象地址保持复用；
5. 最终 Release 做 50 轮压力测试，无退出、残留 overlay、后台声音、黑屏或持续内存增长。

`symbian/probes/landscape-window/`、`CONFIG+=applandscape1` 和 `symbian/archive/soft-native-landscape-2026-08-28/` 只保留为历史定位证据。普通主线构建不要加入 `applandscape1`，也不要恢复 `AA_S60DontConstructApplicationPanes` 或旧虚拟旋转。
