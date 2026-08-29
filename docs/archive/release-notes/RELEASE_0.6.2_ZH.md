# wiliwili for Symbian³ 0.6.2 测试版

## 安装包

Debug（建议本轮真机测试使用）：

`symbian/out/releases/v0.6.2/wiliwili_symbian_0.6.2_debug_full_currentcert.sis`

- 大小：9,138,620 字节
- SHA-256：`D66E18B378D96B82AF284B70AA0A23DE7867147D7D573051FF8EF72007D0FEC7`

Release：

`symbian/out/releases/v0.6.2/wiliwili_symbian_0.6.2_release_full_currentcert.sis`

- 大小：9,141,672 字节
- SHA-256：`671E8E11A3A6F18EC484757DFF9D6F3967B0594CBB5C1848C2C0F0626280422B`

两份包的版本头均为 0.6.2，包含 EXE、AppArc 资源、完整中文字体和 OFL
许可证。签名证书有效期为 2026-08-24 至 2036-08-21。

## 本版修复

1. **评论 API -352**
   - 评论列表改用返回结构兼容的传统分页接口，避开 `reply/main` 的浏览器风控。
   - 2026-08-24 桌面只读实测：新端点返回 `code=0` 和评论；旧端点在同一请求头下
     返回 `code=-352`。

2. **搜索输入和闪退**
   - 删除底部原生模态搜索框，改为主窗口顶部的内嵌输入框；软键盘不会遮住内容。
   - 回车提交，返回键取消。
   - 搜索列表暂时使用文字行，不在输入法重建 EGL surface 后上传首张缩略图，消除
     提交约两秒后触发的 OpenGL 崩溃点。

3. **扫码后会话被拒绝**
   - 扫码确认后先访问护照接口返回的跨域完成 URL，再验证 `/nav`。
   - 合并轮询、完成请求和 JSON 中的 Cookie，并按上游 wiliwili 的方式保存稳定的
     `_uuid` 与 32 位 `buvid3`。

4. **0.6.1 普通视频只有声音**
   - 撤销 0.6.1 的原生窗口层级回归：MMF `QVideoWidget` 重新位于透明控制层上方。
   - 控制栏通过让视频矩形避开顶部、底部和清晰度侧栏显示，不再用透明 QWidget
     覆盖视频画面。

5. **直播证书失败**
   - Symbian 上直播 CDN 优先尝试同地址的 HTTP 版本，绕开 MMF 独立 TLS 栈的旧证书
     信任问题；HTTPS 仍保留为备用。

6. **底部系统栏与 Exit 误触**
   - 在构造 `QApplication` 前启用 `AA_S60DontConstructApplicationPanes`，不再创建
     S60 状态栏和软键栏。
   - 不使用曾导致冷启动不稳定的 `showFullScreen()`；主窗口继续使用已验证的
     `showMaximized()`，应用内设置页保留退出入口。

## 建议真机验证顺序

1. 完整安装 Debug SIS，不要只覆盖 EXE；从图标启动并确认底部 Exit 系统栏消失。
2. 打开 0.6 中曾有画面的视频，确认 0.6.2 恢复画面和声音；点击画面检查控制栏。
3. 搜索一个视频，确认输入框在顶部、输入可见，提交后等待至少十秒且不闪退。
4. 打开评论区，确认不再显示 `API -352`。
5. 刷新二维码并重新扫码确认，检查是否显示账号名称和 UID。
6. 打开直播，确认不再弹出不受信证书提示；若仍不能解码，记录右上角状态和
   `WW:PLAYER_SOURCE` / MMF 错误，这将区分容器不支持与网络证书问题。
