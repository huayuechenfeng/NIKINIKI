# 合入主线前最终原生横屏探针 `appshell7-final-dynamic-fullscreen-rgb565`

这是原生横屏根因定位的第二层实验。第一层独立 `combined1` 已在 Nokia 603 图标启动连续通过 20 轮；本层只增加一个变量：正式 wiliwili 的 `WiliwiliWidget/QGLWidget` 主窗口及其原生窗口组在进程中保持存活。

## 构建隔离

Qt Creator 必须打开原来的正式工程：

```text
symbian/app/wiliwili_symbian.pro
```

在 qmake 附加参数中加入 `CONFIG+=applandscape1`。实验不再改变 target 或 UID，目的是让 Qt Creator 2.4/WLAN CODA 复用此前已经验证可启动、可输出日志的正式应用运行配置。

该参数仍生成正式身份：

```text
TARGET = wiliwili_symbian
UID3   = 0xE000B100
VERSION = 0.9.0
```

探针包会覆盖手机上的普通 `wiliwili_symbian`，所以只用于本轮诊断。不带该参数重新运行 qmake 后，0.9 普通构建完全不包含探针源文件和宏。

完整字体与许可证继续部署到正式目录 `/resource/apps/wiliwili_symbian/`，避免不同 UID 争夺同一个文件所触发的安装错误 `131073`。

## 运行边界

- 保留正式应用的 QGL 主窗口、NanoVG 资源和事件过滤器；
- 禁止启动首页网络请求，避免异步回调干扰本轮时序；
- 不创建 `VideoPlayerWidget`，不打开 MMF，不启动 FFmpeg，不提交视频帧；
- 横屏顶层窗口仍是全新、无父对象、opaque 的 raster QWidget，只画黑底文字；
- 探针构建不设置正式应用的 `QApplication::AA_S60DontConstructApplicationPanes`，让 Avkon application panes 像通过 20 轮的 standalone `combined1` 一样正常创建；仍可在 UI ready 后隐藏 chrome，但不再从 QApplication 层永久取消 panes；
- QGL 主窗口在等待横屏期间保持可见，避免没有任何 visible top-level 时应用被 Belle 后台化、Qt timer 停止；
- 启动和每轮竖屏恢复后，正式 QGL 主窗口调用 Qt 公共 `showFullScreen()`，必须确认 `availableGeometry=screenGeometry=360×640`，用于验证主菜单没有系统栏且工作区被完整回收；
- 每轮请求横屏前，QGL 主窗口先调用 `showMaximized()` 退出动态全屏，必须等待带 panes 的竖屏工作区（Nokia 603 实测 `0,26 360×554`）稳定后才调用 `SetOrientationL()`；不使用 SDK 中受保护的 `CAknAppUiBase::SetFullScreenApp()`，也不通过不安全类型转换绕过访问限制；
- 不实现或调用任何探针 `resizeEvent()`；
- 只由 `workAreaResized()` 进入确认；必须看到横向 available geometry 且 `screenGeometry=640×360`，下一事件循环才调用 `showFullScreen()`，与已通过 20 轮的 standalone `combined1` 保持一致；
- 点击/返回后先隐藏并同步删除横屏窗口，再请求竖屏；确认 `screenGeometry=360×640` 且 available geometry 为竖向后恢复 QGL 主窗口。Avkon panes 存在时正常可用区为 `0,26 360×554`，不再错误要求 pane-free 的 `360×640`；
- 横屏 disposable window 预分配 `QImage::Format_RGB16` 的 640×360 RGB565 色条帧，以40 ms timer（25 fps上限）移动扫描线并触发整帧 QPainter blit；这覆盖主线软件帧的像素格式、timer与呈现路径，但仍不加入 MMF、FFmpeg、网络或播放器对象，失败时保持可归因；
- 每轮恢复后等待 700 ms，再自动开始下一轮；横屏页仍需用户点击或按返回键，共 20 轮。

## 关键日志

成功启动：

```text
WW:APP_LANDSCAPE_PROBE_APPLICATION_PANES_CONSTRUCTED
WW:APP_LANDSCAPE_PROBE_BEGIN version appshell7-final-dynamic-fullscreen-rgb565
WW:APP_LANDSCAPE_PROBE_NETWORK_SKIPPED
WW:APP_LANDSCAPE_PROBE_INITIAL_FULLSCREEN_READY ... available "0,0 360x640"
```

每轮：

```text
WW:APP_LANDSCAPE_PROBE_WINDOW_CREATED
WW:APP_LANDSCAPE_PROBE_WORKAREA_640X360_READY
WW:APP_LANDSCAPE_PROBE_SHOW_BEGIN
WW:APP_LANDSCAPE_PROBE_VISIBLE
WW:APP_LANDSCAPE_PROBE_FIRST_PAINT_END
WW:APP_LANDSCAPE_PROBE_RGB565_PRESENT
WW:APP_LANDSCAPE_PROBE_WINDOW_DELETED
WW:APP_LANDSCAPE_PROBE_RESTORED_FULLSCREEN
```

最终成功：

```text
WW:APP_LANDSCAPE_PROBE_COMPLETE cycles 20
```

任何 `TIMEOUT`、`FORCED_PORTRAIT` 或 `ABORTED` 都视为失败。

## 最终结果与退役

用户提供的 Nokia 603/CODA 最终日志已完成全部 20 轮：20 次 `WORKAREA_640X360_READY`、20 次 `FIRST_PAINT_END`、47 次 `RGB565_PRESENT`、20 次 `RESTORED_FULLSCREEN`、0 次 timeout，最后为 `COMPLETE cycles 20` 与 `EVENT_LOOP_EXIT 0`。用户目视确认 RGB565 色条与移动扫描线正确，横屏和竖屏均无系统栏。

因此本探针已完成使命，状态机已经合入普通 0.9 主播放器：保留 panes 和主 QGL 映射、work-area gate、动态 fullscreen、原生 640×360 直接合成，以及精确 360×640 返回。普通主线同时删除了 MMF/ARGB/UI/input 的旧 90° 旋转。探针仍作为历史诊断配置保留，但后续播放器测试不要再加入 `CONFIG+=applandscape1`。

为避免 Symbian 单实例进程阻止下一次 CODA 启动，探针在超时/失败或完成 20 轮后等待 1.5 秒自动退出。安装前先卸载独立 UID 的旧 `wiliwili_app_landscape_probe`；本轮只运行正式身份的 `wiliwili_symbian`。

## Qt Creator

若需复现历史探针，打开 `symbian/app/wiliwili_symbian.pro`，选择 `Qt 4.7.4 for Symbian Belle (Qt SDK)` Debug，在 qmake 附加参数中加入 `CONFIG+=applandscape1`。正常主线编译必须删除该参数并再次运行 qmake；当前发布验证只测试普通 `wiliwili_symbian`。
