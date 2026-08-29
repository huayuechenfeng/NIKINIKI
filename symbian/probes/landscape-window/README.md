# Symbian 原生横屏窗口生命周期探针

当前探针版本：`combined1`（qmake `VERSION=0.2.0`）。

这是一个与 wiliwili 播放器完全隔离的诊断程序，用来回答一个问题：

> 在不复用主窗口、QGLWidget、MMF、FFmpeg、弹幕或透明 ARGB overlay 的情况下，能否稳定地创建一个新的顶层 QWidget，在系统横屏工作区稳定后全屏显示，再删除它并恢复竖屏？

它不会修改或启用主线中已经归档的 `soft-native-landscape` 实验。

## 固定流程

每轮测试严格执行以下顺序：

1. 在竖屏控制页中创建一个新的、尚未显示的无父对象顶层 QWidget；
2. 调用 Avkon `SetOrientationL(EAppUiOrientationLandscape)`；
3. 只把 `QDesktopWidget::workAreaResized(int)` 当作横屏工作区可用的入口；
4. 信号到达后再跨一个 Qt 事件循环，确认 `screenGeometry()==640×360` 且 `availableGeometry()` 已为横向；
5. 调用新窗口的 `showFullScreen()`，只绘制纯黑背景和静态文字；
6. 再由 `workAreaResized()` 确认全屏可用区成为 `640×360`；
7. 点击屏幕或按返回键后，先隐藏并同步删除这个横屏窗口；
8. 再调用 `SetOrientationL(EAppUiOrientationPortrait)`；
9. 等待竖屏 `workAreaResized(int)` 后才重新显示控制页。

Nokia 603 的实测顺序是：请求横屏后，物理屏幕先成为 `640×360`，但系统栏仍使可用区为 `640×284`；只有 `showFullScreen()` 后可用区才成为 `640×360`。因此不能在调用 `showFullScreen()` 前等待可用区本身到达 `640×360`，否则会形成互相等待。

超时只会记录失败并尝试恢复，不会绕过工作区信号强行进入横屏窗口。

## 设备测试

1. 启动 `wiliwili_landscape_window_probe`；
2. 点击 **Start one landscape cycle**；
3. 看到只有文字的纯黑横屏页后，点击屏幕或按返回键；
4. 回到竖屏控制页后重复，建议先做 20 轮；
5. 如果崩溃、退出或卡住，保留从最后一个 `WW:LANDSCAPE_PROBE_*` 标记开始的日志。

关键日志分界：

- `LANDSCAPE_REQUEST` 之后、`WORKAREA` 之前失败：系统方向切换阶段；
- `SHOW_BEGIN` 之后、`VISIBLE` 之前失败：新顶层窗口全屏建立阶段；
- `VISIBLE` 或 `FIRST_PAINT_BEGIN` 之后、`FIRST_PAINT_END` 之前失败：该独立 QWidget 的首次 raster paint 阶段；
- `FIRST_PAINT_END`：独立窗口已经至少完成一次静态横屏绘制；
- `FULLSCREEN_WORKAREA_READY`：全屏显示后，可用区和物理屏幕均已确认是 `640×360`；
- `WINDOW_DELETED` 之后失败：窗口销毁或竖屏恢复阶段；
- `RESTORED`：一轮完整往返成功。

## Qt Creator 编译

打开 `landscape-window.pro`，选择 Qt 4.7.4 Symbian/GCCE 设备 Kit，先运行 qmake，再构建 Debug。Qt Creator 的 Symbian 部署步骤会生成并安装 SIS；建议优先测试 Debug，以便通过 CODA 收集上述日志。
