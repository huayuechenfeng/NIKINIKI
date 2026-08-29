# wiliwili for Symbian³ 0.6.9 测试版

## 安装包

Debug 完整包（本轮优先测试）：

`symbian/out/releases/v0.6.9/wiliwili_symbian_0.6.9_debug_full_currentcert.sis`

- 大小：9,149,900 字节
- SHA-256：`1C1C528350B7C24A44BFB1AF4528A20D6D53AEC2F37EC9C579E282EE2055CA75`

Release 完整包：

`symbian/out/releases/v0.6.9/wiliwili_symbian_0.6.9_release_full_currentcert.sis`

- 大小：9,152,836 字节
- SHA-256：`F84C876D3777F41CFD1C212DA5316162F7BBD8C7E018D34813E06A56A35D06E5`

Debug/Release 均由 Belle Qt 4.7.4/GCCE 构建，`sbs errors: 0`。包头版本
为 0.6.9，应用能力为 `NetworkServices ReadUserData`，证书有效期为
2026-08-24 至 2036-08-21。

## 修复内容

- 根据 0.6.8 真机日志确认 `Symbian:-12015` 是
  `KErrMMVideoDevice`（视频向显示设备 blit 失败），并非编码或网络失败。
- 新增覆盖播放器打开、隐藏旋转等待、播放和关闭的完整会话状态。不能只依赖
  `isVisible()`，因为播放器在横屏等待期间会主动隐藏。
- 播放会话开始前立即取消主窗口的 AppArc 前台恢复定时器。
- 播放会话期间拦截应用和主窗口的 Activate/Deactivate 恢复操作，不更新主
  QGLWidget，也不允许主窗口调用 `showMaximized()` 抢占播放器。
- `bringApplicationToForeground()` 和 `scheduleForegroundRestore()` 增加第二层
  播放会话保护，避免其他调用路径重新开启主窗口恢复。
- 播放器隐藏、恢复竖屏和主窗口后才释放会话；关闭回调再显式恢复原来的
  图标冷启动保护。

## 预期日志

播放横屏视频时：

```text
WW:PLAYER_SESSION_ACTIVE
WW:PLAYER_ORIENTATION_REQUEST true true
WW:PLAYER_ORIENTATION true 0 QSize(640, 360)
WW:PLAYER_APPLICATION_DEACTIVATE
WW:PLAYER_APPLICATION_ACTIVATE
WW:PLAYER_NATIVE_READY QSize(640, 360) ...
WW:PLAYER_SOURCE 1 ...
```

Activate/Deactivate 在 AVKON 旋转时允许出现，但 `PLAYER_SESSION_ACTIVE` 与
`PLAYER_SESSION_RELEASED` 之间不应再出现主窗口发出的
`WW:AVKON_CHROME_HIDDEN` 或 `WW:FOREGROUND_READY`，也不应出现
`Symbian:-12015`。

退出播放器时应看到：

```text
WW:PLAYER_ORIENTATION_RESTORE 0
WW:PLAYER_SESSION_RELEASED
WW:PLAYER_CLOSED_RESTORE
```

此后主窗口的 `AVKON_CHROME_HIDDEN / FOREGROUND_READY` 可以正常重新出现。

