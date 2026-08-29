# wiliwili for Symbian³ 0.6.11 测试版

## 安装包

Debug 完整包（优先用于 Qt Creator/CODA 真机日志测试）：

`symbian/out/releases/v0.6.11/wiliwili_symbian_0.6.11_debug_full_currentcert.sis`

- 大小：9,152,008 字节
- SHA-256：`CAAF1CD238AE4CF28EC0CB7731995894A750DCA3025873236FF73746D29C37D9`

Release 完整包：

`symbian/out/releases/v0.6.11/wiliwili_symbian_0.6.11_release_full_currentcert.sis`

- 大小：9,155,968 字节
- SHA-256：`90EFB7676122742099BAB9C69EB569CE361948C703E9FE200D966D01913F4BE0`

Debug/Release 均由 Belle Qt 4.7.4/GCCE 构建，`sbs errors: 0`。当前
证书有效期为 2026-08-24 至 2036-08-21。

## 本轮修复

- 不再对仍持有活动定时器的 `VideoPlayerWidget` 顶层窗口调用
  `QWidget::destroy()`；该半销毁状态是横屏切换完成后访问 `0x54` 崩溃的
  直接触发条件。
- 横屏前仍完整销毁旧的 `QMediaPlayer/MMF` 后端和 `QVideoWidget` 原生显示
  子窗口，避免旧竖屏 `RWindow/CWsScreenDevice` 导致 `Symbian:-12015`。
- 把“释放视频子窗口 → AVKON 旋转 → 创建新视频子窗口/MMF 后端”拆成更
  安全的三个阶段，并延长窗口服务器处理延迟销毁事件的间隔。
- 增加 `PLAYER_REBUILD_BEGIN` 和 `PLAYER_ROOT_READY` 日志，能区分顶层窗口
  恢复、视频子窗口创建和 MMF 后端创建三个故障点。
- 搜索编辑器现在只属于首页。离开首页、进入详情、播放器或直播时都会
  立即关闭输入法并隐藏编辑器，不能再覆盖视频界面。
- 搜索编辑器作为主窗口的瞬态工具窗口存在，避免脱离应用窗口组；使用
  两阶段延迟激活后再请求 Belle 软件输入法，修复点按后不弹键盘的问题。
- 搜索结果页及其他栏目不会通过延迟事件重新弹出首页搜索框。

## 最短真机验证

1. 在首页点搜索框，确认键盘出现且可以输入。
2. 关闭搜索或完成一次搜索，进入任意视频详情，确认搜索框不在表层。
3. 播放一个横屏视频，等待约 1.2 秒完成安全旋转和原生显示面重建。
4. 确认横屏画面、声音和播放器 UI 均出现，再退出并确认回到竖屏主界面。

搜索输入预期日志：

```text
WW:SEARCH_EDITOR_SHOWN ...
WW:SEARCH_EDITOR_ACTIVATED true
WW:SEARCH_SIP_REQUESTED true
```

横屏进入预期日志：

```text
WW:PLAYER_SESSION_ACTIVE
WW:PLAYER_ORIENTATION_REQUEST true true
WW:PLAYER_SURFACE_RELEASED_FOR_ORIENTATION
WW:PLAYER_ORIENTATION true 0 QSize(640, 360)
WW:PLAYER_REBUILD_BEGIN true QSize(360, 640) false
WW:PLAYER_ROOT_READY QSize(640, 360) ...
WW:PLAYER_SURFACE_REBUILT_AFTER_ORIENTATION QSize(640, 360) ...
WW:PLAYER_NATIVE_READY QSize(640, 360) ...
WW:PLAYER_SOURCE 1 ...
```

`PLAYER_REBUILD_BEGIN` 中的尺寸可能仍是旋转前缓存值；`showMaximized()` 后的
`PLAYER_ROOT_READY` 才应为最终的 640×360。关键验收条件是不再出现访问
`0x54` 的 data abort，也不再出现 `Symbian:-12015`。
