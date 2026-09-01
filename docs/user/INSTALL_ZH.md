# NIKINIKI 安装指南

> 状态：Active
> 适用版本：1.2.0

## 下载

- [NIKINIKI 1.2.0 GitHub Release](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.2.0)
- 正式资产：`NIKINIKI_1.2.0_release.sis`

安装包校验值、签名证书和重链接材料见
[1.2.0 发布说明](../releases/RELEASE_1.2.0_ZH.md)。公开安装包不会使用已经废止的
`wiliwili_symbian` 品牌名。

> Nokia N8 / E7 / X7 / C7 等初代 Symbian³ 设备存在播放黑屏 bug，1.2 暂不支持，
> 请继续使用 1.1.0。Nokia 603、700、701、808 等原生 Belle 设备不受影响。

## 系统前置

所有设备都建议先安装 [TLS 1.2 补丁](https://nnproject.cc/tls/)。

Belle 通常已具备 Qt 运行环境。原版 Symbian³ 和 Anna 还需要：

- Qt 4.7.4；
- Qt Mobility 1.2.x。

仓库 `prerequisites/` 提供从 Symbian Anna SDK 保留的对应安装包。三代系统使用同一个
NIKINIKI 应用 SIS，不需要寻找不同系统专用版本。详细依据见
[兼容性说明](COMPATIBILITY_ZH.md)。

## 安装顺序

1. 安装 TLS 1.2 补丁；
2. 原版 Symbian³/Anna 安装 Qt 4.7.4 和 Qt Mobility 1.2.x；
3. 安装 `NIKINIKI_1.2.0_release.sis`；
4. 首次启动，确认主界面先显示，完整 CJK 字体随后在后台完成加载；
5. 打开首页、图片和二维码登录，确认 HTTPS 正常；
6. 分别测试普通硬解视频和可能进入软件回退的视频。

如果安装、TLS 或播放异常，请到 QQ 群 `977410275` 反馈并参考
[故障排查](TROUBLESHOOTING_ZH.md)。初代 Symbian³ 设备请暂时使用
[1.1.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.1.0)。
