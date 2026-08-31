# NIKINIKI 安装指南

> 状态：Active
> 适用版本：1.1.0

## 下载

- [NIKINIKI 1.1.0 GitHub Release](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.1.0)
- 正式资产：`NIKINIKI_1.1.0_release.sis`

安装包校验值、签名证书和重链接材料见
[1.1.0 发布说明](../releases/RELEASE_1.1.0_ZH.md)。公开安装包不会使用已经废止的
`wiliwili_symbian` 品牌名。

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
3. 安装 `NIKINIKI_1.1.0_release.sis`；
4. 首次启动，确认主界面先显示，完整 CJK 字体随后在后台完成加载；
5. 打开首页、图片和二维码登录，确认 HTTPS 正常；
6. 分别测试普通硬解视频和可能进入软件回退的视频。

N8 / E7 / X7 / C7 建议升级到 Nokia Belle，再选择“设置 → 播放方式 → `OpenFileL`
边下边播”。如果安装、TLS 或播放异常，请到 QQ 群 `977410275` 反馈并参考
[故障排查](TROUBLESHOOTING_ZH.md)；安装有问题时可先继续使用
[1.0.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.0.0)。
