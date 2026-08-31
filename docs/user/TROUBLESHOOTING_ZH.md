# NIKINIKI 故障排查

> 状态：Active
> 适用版本：1.1.0；同时保留 1.0.0 回退说明

## 首次启动长时间黑屏

1.1.0 已把完整 CJK 字体改为首屏显示后的分块加载，修复首次安装后因同步读字体导致的
长时间黑屏。若 1.1.0 仍长时间黑屏，请记录设备型号、系统版本、安装盘和大致等待时间，
到 QQ 群 `977410275` 反馈。

1.0.0 仍可能出现旧行为：保持应用前台或黑屏状态约 60 秒，然后从后台关闭并重新启动；
若仍然黑屏，重启手机后再试。成功启动一次后通常不会重复发生。

## 首页、图片或登录无法连接

1. 确认设备日期和时间正确；
2. 安装 TLS 1.2 补丁；
3. 原版 Symbian³/Anna 确认 Qt 4.7.4 与 Qt Mobility 1.2.x 已安装；
4. 若仍异常，可尝试 [Qt TLS 补丁](https://nnproject.cc/qtls/)；
5. 区分所有 HTTPS 都失败与单个 Bilibili 接口失败，后者可能是服务端接口变化。

## 视频有声音但没有画面

部分现代 H.264 码流会被手机 Broadcom 插件拒绝。NIKINIKI 应自动进入本机 FFmpeg 软件
视频路径；该路径可能明显卡顿，但不应只停留在音频状态。

如果完全没有软件画面，请记录：

- 设备型号和系统版本；
- 视频 BVID、分 P 和选择的清晰度；
- 是否有声音、弹幕和控制 UI；
- 退出后能否再次进入播放器；
- 日志中的 `DEVVIDEO_HEADER_PREFLIGHT_*`、`FFMPEG_SOFT_READY` 和
  `SOFT_SURFACE_FIRST_PAINT` 标记。

不要公开 Cookie、完整签名媒体 URL、账号令牌或私网 CODA 地址。

N8 / E7 / X7 / C7 建议升级到 Nokia Belle，并先试“设置 → 播放方式 → `OpenFileL`
边下边播”。它可绕开部分旧机型的 MMF 网络流式 controller，同时保留本机硬件解码。
“全程硬解”不会在硬件拒绝时回退，排查结束后建议恢复“自动选择”。

## 播放卡顿

MMF 硬解兼容的视频通常更流畅。进入本机软件 H.264 路径的 360P 视频仍可能低于实时帧率，
这是当前版本的已知限制。切换 CDN 或反复请求更低清晰度不一定改变 H.264 结构。

## 返回后黑屏、残留窗口或后台声音

这属于播放器生命周期回归。请记录发生前的播放次数、所用视频、是否执行 pause/seek/倍速，
以及返回时是否出现 `PLAYER_NATIVE_PORTRAIT_FULLSCREEN_READY`。不要用旧 0.7/0.9 诊断 SIS
与正式包混测。

## 安装包身份

公开用户只应安装 `NIKINIKI_版本号_release.sis`。Debug、unsigned、`surfacepersist1`、`codeccompat1`、
`headercontrol1`、`armsoftprobe1`、GLES-YUV 等包均为历史研究材料。

如果 1.1.0 安装失败或无法启动，请到 QQ 群 `977410275` 反馈，并可先继续使用
[1.0.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.0.0)。
