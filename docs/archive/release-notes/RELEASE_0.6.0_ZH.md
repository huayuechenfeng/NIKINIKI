# wiliwili for Symbian³ 0.6.0 测试版

## 推荐安装包

首轮真机测试推荐 Debug 完整包：

`symbian/out/releases/v0.6.0/wiliwili_symbian_0.6.0_debug_full_currentcert.sis`

- 大小：7,571,404 bytes
- SHA-256：`E73A1F46EF5D8F8D36E085A719D1CB605D6D9908C17FA4DD1FBE96BA94B5A930`
- 签名有效期：2026-08-24 至 2036-08-21

Release 完整包用于 Debug 通过后的流畅度和连续播放测试：

`symbian/out/releases/v0.6.0/wiliwili_symbian_0.6.0_release_full_currentcert.sis`

- 大小：7,574,728 bytes
- SHA-256：`5D3BCA6E301246B1885485F0498EA04D11D5CA5C5131FA83FB3B736B117179CB`
- 签名有效期：2026-08-24 至 2036-08-21

两份包都包含应用 EXE、AppArc 注册资源和中文字体，包版本为 0.6.0。
Debug 与 Release 均由 Belle Qt 4.7.4/GCCE 构建并以 `sbs errors: 0` 完成。

## 0.6.0 主要变化

- 普通视频播放接口现在解析所有可选清晰度，在播放器右下角提供触摸画质菜单；
  键盘/实体键也可用 Menu 键打开。
- 切换画质会重新请求对应播放地址，保留当前弹幕，不重复下载弹幕；服务端降级时
  显示实际返回的画质。
- 普通视频与直播分别记忆上次画质，默认值为视频 720P、直播高清。
- 首页直播卡片可直接进入横屏播放器。
- 直播请求复用上游 wiliwili 的 RoomPlayInfo V2 参数和清晰度语义，只接受
  AVC/H.264。
- 直播源按 HLS/TS、HLS/fMP4、FLV 排序，保留全部 CDN；每条 HTTPS 后紧跟
  HTTP 兼容地址，V2 不可用时回退旧 FLV API。
- 未开播、轮播和封禁房间不会误播旧接口返回的陈旧地址。
- 直播模式显示“直播中”，禁用进度拖动、快进退和倍速，保留播放/暂停、音量、
  清晰度、线路回退和返回。

详细设计见 `docs/research/future/LIVE_PLAYBACK_ARCHITECTURE_ZH.md`。

## 回来后的 Qt Creator 操作

1. 在 Qt Creator 左侧选择“项目”，构建配置选
   `Qt 4.7.4 for Symbian Belle (Qt SDK)` 的 Debug。
2. 执行“构建 → 重新构建项目”。本机已有构建产物，但这一步可确保 Qt Creator
   使用相同源码。
3. 点击左下角绿色三角“运行”。手机出现安装确认时选择覆盖安装；不要使用静默安装。
4. 如果 Qt Creator 只部署 EXE 或手机没有弹出覆盖确认，停止运行，改为把上面的
   Debug 完整 SIS 复制到手机后手工安装到 C 盘。
5. 在应用管理器确认版本为 0.6.0，再从手机图标启动。

## 首轮测试顺序

1. 从图标冷启动三次，确认字体、图片和首页正常。
2. 打开普通视频，点右下角清晰度，从 720P 切到 480P，再切回；确认画面恢复、
   弹幕仍在。
3. 首页切换到“直播”，打开正在直播的房间；先测试默认高清，再切“流畅”。
4. 验证直播声音、画面、暂停/继续、音量和返回；返回后再打开普通视频。
5. 若直播失败，等待播放器尝试备用线路，记录右上角 `MMF ... / ERRn` 和
   Qt Creator 输出中的 `WW:LIVE_*`、`WW:PLAYER_SOURCE`、`WW:PLAYER_ERROR`。

不要发送登录 Cookie。若日志里出现完整 URL，发送前删掉 `?` 后的鉴权参数。

## 真机判定边界

“硬解”仍是 Qt Mobility → Symbian MMF/固件解码器的原生路径。GCCE 编译、链接、
安装包结构、直播 API 结构和播放器状态机已经检查，但 Nokia 603 固件对 HLS/TS、
HLS/fMP4 和 FLV 的实际支持必须以真机为准。

0.6.0 尚未实现直播 WebSocket 弹幕；普通视频 XML 弹幕不受影响。大航海付费直播、
DRM、HEVC、AV1、杜比、4K 与直播开播功能不在本版范围。
