# wiliwili for Symbian³ 0.6.10 测试版

## 安装包

Debug 完整包（优先用于本轮真机日志测试）：

`symbian/out/releases/v0.6.10/wiliwili_symbian_0.6.10_debug_full_currentcert.sis`

- 大小：9,150,276 字节
- SHA-256：`A2237E29E6C91585B163029911B515940FDC211900B00F0B53066B344C07865B`

Release 完整包：

`symbian/out/releases/v0.6.10/wiliwili_symbian_0.6.10_release_full_currentcert.sis`

- 大小：9,154,008 字节
- SHA-256：`ABBE511B3B405DB020D91D341C9E13C17322EC8E71D7E0B5EA3DE4035E8A51DF`

Debug/Release 均由 Belle Qt 4.7.4/GCCE 构建，`sbs errors: 0`。SIS
包头版本为 0.6.10，当前证书有效期为 2026-08-24 至 2036-08-21。

## 本轮修复

- 根据真机返回的 `Symbian:-12015`（`KErrMMVideoDevice`）把旋转视为
  MMF 原生显示设备的销毁边界，而不是普通的 QWidget 尺寸变化。
- 进入横屏前解除 `QMediaPlayer` 的视频输出、销毁播放器后端、
  `QVideoWidget` 以及播放器顶层 QWidget 的原生窗口资源。
- AVKON 完成旋转并报告 640×360 后，按“顶层原生窗口、视频子窗口、
  MMF 后端、媒体源”的顺序重新创建播放链路。
- 退出横屏时执行对称的销毁，再恢复竖屏，防止后续竖屏视频继承横屏
  `RWindow/CWsScreenDevice`。
- 补充播放器为空期间的空指针保护，覆盖旋转等待时返回、关闭、轮询、
  重试和窗口 resize 路径。
- 保留 0.6.9 的播放器会话前台所有权保护，主 QGLWidget 在播放期间不会
  抢占播放器。

## 建议测试顺序

1. 先播放一个竖屏视频并退出。
2. 再播放一个 1920×1080 横屏视频，确认画面和声音同时出现。
3. 退出横屏播放器，确认回到竖屏主界面。
4. 再次播放竖屏视频，确认退出横屏后没有污染竖屏显示面。
5. 连续播放两个横屏视频，确认第二次仍能创建画面。

横屏进入时预期出现：

```text
WW:PLAYER_SESSION_ACTIVE
WW:PLAYER_ORIENTATION_REQUEST true true
WW:PLAYER_SURFACE_RELEASED_FOR_ORIENTATION
WW:PLAYER_ORIENTATION true 0 QSize(640, 360)
WW:PLAYER_SURFACE_REBUILT_AFTER_ORIENTATION QSize(640, 360) ...
WW:PLAYER_NATIVE_READY QSize(640, 360) ...
WW:PLAYER_SOURCE 1 ...
```

退出横屏时预期出现：

```text
WW:PLAYER_SURFACE_RELEASED_FOR_ORIENTATION
WW:PLAYER_SURFACE_RELEASED_BEFORE_PORTRAIT
WW:PLAYER_ORIENTATION_RESTORE 0
WW:PLAYER_SESSION_RELEASED
WW:PLAYER_CLOSED_RESTORE
```

关键验收条件是播放源加载后不再出现：

```text
WW:PLAYER_ERROR 5 "Symbian:-12015"
```
