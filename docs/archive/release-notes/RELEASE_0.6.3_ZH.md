# wiliwili for Symbian³ 0.6.3 测试版

## 安装包

Debug（本轮必须先测试这一份）：

`symbian/out/releases/v0.6.3/wiliwili_symbian_0.6.3_debug_full_currentcert.sis`

- 大小：9,139,672 字节
- SHA-256：`4C18FB6A5412251B047077441F40DDAB558293C4650DED1291248EBFD50338F1`

Release：

`symbian/out/releases/v0.6.3/wiliwili_symbian_0.6.3_release_full_currentcert.sis`

- 大小：9,144,324 字节
- SHA-256：`4BDEF881B0B2593C19055C5D309FBA6B9E2F3EB76D72414117DDAFCFD3E5478B`

两份包均由 Belle Qt 4.7.4/GCCE 构建，Debug、Release 都是
`sbs errors: 0`。包头为 0.6.3；当前签名证书有效期为 2026-08-24 至
2036-08-21。

## 本版修复

1. **播放退到桌面/CODA，只有声音**
   - 日志证明 MMF 已开始加载媒体，但第二个 Qt 顶层播放器窗口在 Belle 横屏切换时
     脱离了应用前台窗口组。
   - 播放器改为主 `QGLWidget` 内部的全窗口子页面，不再创建第二个 `Qt::Window`。
   - 横屏后等待 180 ms，让父窗口完成 resize 并创建原生视频子窗口，再绑定
     `QVideoWidget` 和启动 MMF，消除同一调用栈里的 null receiver 警告。
   - 返回时仅隐藏播放器子页面，主页 EGL surface 始终保留。

2. **扫码成功但 `/nav` 返回 -101**
   - 恢复 wiliwili 上游做法：二维码轮询返回成功后直接保存该响应 Cookie，不再访问
     `data.url` 的跨域地址。
   - 使用 Symbian `CCookie` 逐个解析重复的 `Set-Cookie` 字段，保留明确的
     `name=value`，避免旧 HTTP 栈只暴露第一个 Cookie。
   - 登录只有同时收到 `SESSDATA`、`bili_jct`、`DedeUserID` 才进入账号验证；同时
     补上游要求的 `_uuid` 与独立 `buvid3`。

3. **评论只有少数根评论，没有楼中楼**
   - 根评论页使用旧接口允许的单页上限 20；公开 API 实测 `ps=40` 会返回 `-400`。
   - 直接展开根列表响应中内嵌的首批回复。
   - 点击根评论后调用 `/x/v2/reply/reply`，打开首批 20 条完整回复；返回会重新
     载入根评论。

4. **弹幕 XML 编码失败**
   - 先以 UTF-8 容错解码，再移除 XML 1.0 禁止的控制字符，避免单个非法字节导致
     整份弹幕被 Qt 4.7 丢弃。

## 真机测试顺序

1. 完整安装 Debug SIS，在应用管理器确认版本 0.6.3。
2. 通过 Qt Creator 普通运行进入同一个视频；播放器应在 wiliwili 内横屏，不应暴露
   CODA 或桌面。等待画面和声音，显示/隐藏一次控制栏，再返回详情页。
3. 打开评论，确认根评论的回复以 `↳` 开头显示；点击有“回复 N”的根评论进入完整
   回复页，再返回。
4. 刷新二维码、扫码并在手机确认。日志中的 `WW:LOGIN_COOKIE_SUMMARY` 应为
   `true true true`，随后应出现 `WW:PROFILE_READY`。
5. 若播放仍失败，保留 `WW:PLAYER_PAGE_READY`、`WW:PLAYER_PARENT_GEOMETRY`、
   `WW:PLAYER_NATIVE_READY`、`WW:PLAYER_SOURCE` 和 `WW:PLAYER_ERROR`。

不要发送 Cookie、二维码 URL 或完整媒体 URL。
