# wiliwili for Symbian³ 0.5.0 测试版

## 推荐安装包

首轮真机测试推荐 Debug 完整包：

`symbian/out/releases/v0.5.0/wiliwili_symbian_0.5.0_debug_full_currentcert.sis`

- 大小：7,555,988 bytes
- SHA-256：`3F43DD2417485EBB8E0E497CCBF9B7A7185BF14BACDE8F231FD68215A00AF62C`
- 签名有效期：2026-08-24 至 2036-08-21

Release 完整包用于 Debug 通过后的性能与长时间播放测试：

`symbian/out/releases/v0.5.0/wiliwili_symbian_0.5.0_release_full_currentcert.sis`

- 大小：7,558,260 bytes
- SHA-256：`5495C0AFE909E2E412BD0A6327B16FFD7F418B5B100B0D730835D7671A298D4C`
- 签名有效期：2026-08-24 至 2036-08-21

两份包都包含应用 EXE、AppArc 注册资源和完整中文字体，不是智能安装器壳。Debug 和 Release 都由 Belle Qt 4.7.4/GCCE 构建并以 `sbs errors: 0` 完成。

## 0.5.0 主要变化

- 安装字体改为通过 `QFile` 读入内存后交给 NanoVG，修复完整 SIS 从图标启动时整个 UI 无字。
- QR 登录成功后的账号验证首选 `/x/web-interface/nav`，并兼容 `/x/space/myinfo` 两种资料格式。
- 详情页改善标题布局，分离“播放”和“稍后”，加入评论、点赞、投币、收藏与多 P 选择。
- 新增 Qt Mobility `QMediaPlayer + QVideoWidget` 横屏播放器，底层使用 Belle MMF 原生媒体服务。
- 请求 720p 优先的渐进式 H.264/AAC MP4；主 CDN 失败时自动尝试备用及 HTTP 兼容线路。
- 播放器支持播放/暂停、进度拖动、左右键 10 秒跳转、音量、倍速、弹幕开关和返回。
- 新增普通、逆向、顶部、底部弹幕，按播放时间同步；用时间窗口定位控制旧手机 CPU 开销。
- Qt Creator 的 `SilentInstall` 已关闭，部署时必须在手机上确认安装，避免静默失败后仍从图标运行旧包。

## 回来后的安装步骤

1. 先在手机任务管理中结束旧版 wiliwili；不需要重启手机，也不要关闭 CODA 才能正常使用图标。
2. 将 Debug 完整 SIS 复制到手机并手工安装到 C 盘，允许覆盖旧版。若提示更新错误，卸载旧版后再安装。
3. 在应用管理器中确认版本为 `0.5.0`，然后只从手机图标连续启动、退出三次，检查字体与首页。
4. 扫码登录，确认账号页出现昵称/UID；退出重进一次检查会话保存。
5. 进入一个普通视频详情，点“播放”，验证横屏、声音、画面、控制层和返回。
6. 再验证进度拖动、左右键、音量、倍速、弹幕开关、多 P 和十分钟连续播放。

若从 Qt Creator 部署，选择 `Qt 4.7.4 for Symbian Belle` 的 Debug 构建配置，点击左下角绿色运行/调试按钮后，在手机弹出的安装窗口中明确确认覆盖；不要再使用无提示的静默安装。

## 真机判定边界

“硬解”采用 Qt Mobility 到 Symbian MMF/固件解码器的原生路径，不包含软件 FFmpeg。编译、链接、包体和 API 格式已经验证，但具体 Nokia 603 是否调用硬件 H.264 解码器、透明弹幕层能否稳定叠在 MMF 视频窗口上，仍必须以真机为准。

当前目标是渐进式 H.264/AAC MP4。DASH 分离音轨、1080p、高帧率、HEVC/AV1、DRM 与大会员专有流不在 0.5.0 范围；倍速也可能受固件 MMF 插件限制。
