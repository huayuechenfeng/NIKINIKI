# wiliwili for Symbian³ 0.6.12 测试版

## 安装包

Debug 完整包（本轮真机日志测试优先使用）：

`symbian/out/releases/v0.6.12/wiliwili_symbian_0.6.12_debug_full_currentcert.sis`

- 大小：9,152,464 字节
- SHA-256：`15633D48D8299DBBB17A306DF3D4A7B2C8D213F6C6EF5A96C968F838961C01E6`

Release 完整包：

`symbian/out/releases/v0.6.12/wiliwili_symbian_0.6.12_release_full_currentcert.sis`

- 大小：9,155,712 字节
- SHA-256：`FAEDC205EACD1F64F003D80BB7356FBF183877E075A63B608710852EE4D2B7DC`

Debug/Release 均由 Belle Qt 4.7.4/GCCE 构建，`sbs errors: 0`；SIS
包头版本为 0.6.12。签名证书有效期为 2026-08-24 至 2036-08-21。

## 与 0.6.11 的结构差异

0.6.11 的日志已经证明 AVKON 旋转、播放器顶层句柄、`QVideoWidget` 句柄和
MMF 后端均能创建，但独立的播放器 `Qt::Window` 从未成为手机屏幕上的可见
窗口。MMF 不依赖其可见性，所以仍会在后台播放声音。

0.6.12 不再继续修改延时，而是取消第二个顶层窗口：

- `VideoPlayerWidget` 改为主 QGL 窗口内部的全屏原生子视图。
- 旋转前后始终只有一个 AVKON 顶层窗口和一个应用窗口组。
- 播放器不再调用 `activateWindow()` 或把自己设置成应用活动窗口。
- 主窗口尺寸改变时同步调整播放器子视图为整个客户区。
- 仍在旋转后重新创建 `QVideoWidget/MMF`，避免旧显示设备导致
  `Symbian:-12015`。
- 新日志 `PLAYER_INLINE_ROOT_READY ... false` 中最后的 `false` 表示播放器
  确实不是第二个顶层窗口。

## 搜索修复

- 搜索端点由旧的 `/x/web-interface/search/type` 改为主 wiliwili 当前使用的
  `/x/web-interface/wbi/search/type`。
- 参数集合与主项目的 PC 搜索请求保持一致，包括 `platform=pc`、
  `device=mac`、`highlight=1`、`from_spmid=333.337` 等。
- 搜索仍使用动态 WBI mixin key 签名。
- 增加 `SEARCH_SUBMIT` 日志，仅输出字符数、MD5 前八位和搜索类型，不泄露
  原始关键词；用它可以确认输入框实际提交了不同文本。

## 最短测试顺序

1. 首页搜索一个非常明确的词，确认结果标题与关键词相关。
2. 返回首页，进入一个 16:9 横屏视频。
3. 确认旋转期间没有退回 CODA、桌面或启动前画面。
4. 等待声音开始，确认同一窗口内同时出现画面。
5. 按返回退出播放器，确认恢复竖屏详情页。

预期关键日志：

```text
WW:SEARCH_SUBMIT <长度> <哈希> 0
WW:WBI_READY 32
WW:CONTENT_READY 0 <数量>

WW:PLAYER_SESSION_ACTIVE
WW:PLAYER_ORIENTATION_REQUEST true true
WW:PLAYER_SURFACE_RELEASED_FOR_ORIENTATION
WW:PLAYER_ORIENTATION true 0 QSize(640, 360)
WW:PLAYER_REBUILD_BEGIN ...
WW:PLAYER_INLINE_ROOT_READY QSize(640, 360) ... false
WW:PLAYER_SURFACE_REBUILT_AFTER_ORIENTATION ...
WW:PLAYER_NATIVE_READY ...
WW:PLAYER_SOURCE 1 ...
```

若 `PLAYER_INLINE_ROOT_READY` 最后一项不是 `false`，说明 Qt/Symbian 又把该
子视图提升成了顶层窗口，需要进一步改为直接由主窗口持有 `QVideoWidget`。
