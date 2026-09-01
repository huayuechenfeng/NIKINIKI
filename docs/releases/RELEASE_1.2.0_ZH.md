# NIKINIKI 1.2.0 正式发布说明

> 发布日期：2026-09-02
> 状态：正式 Release SIS 已完成编译、打包、有效签名、Nokia 603 功能验收和公开发布

## 正式安装包

| 项目 | 值 |
|---|---|
| 文件 | `symbian/out/releases/v1.2.0/NIKINIKI_1.2.0_release.sis` |
| GitHub Release | [v1.2.0](https://github.com/huayuechenfeng/NIKINIKI/releases/tag/v1.2.0)，资产名 `NIKINIKI_1.2.0_release.sis` |
| 重链接材料 | `NIKINIKI_1.2.0_relink_materials.zip`；包含未签名 SIS、产品源码快照、固定 FFmpeg 源码快照、许可证、校验和重建说明 |
| 大小 | 待发布构建填写 |
| SHA-256 | 待发布构建填写 |
| 包版本 | 1.2.0 |
| UID | `0xE000B100` |
| 目标 | ARMv5 / GCCE 4.4.1 / `Symbian3Qt474` |
| 能力 | `NetworkServices ReadUserData` |

未签名归档仅用于重签与 LGPL 重链接流程，不作为安装包；其大小和 SHA-256 见
[1.2.0 重链接材料说明](RELEASE_1.2.0_RELINK_MATERIALS_ZH.md)。

## 本次更新

- 首页滑动改为只绘制可见卡片并合并连续触摸刷新，减少卡片滚动时的卡顿；
- 播放器新增协调的右侧竖向音量滑块；主页实体音量键、播放器按键与滑块共享同一持久音量；
- 视频详情页下方新增纵向相关推荐列表，可继续向下滑动并直接打开视频；
- 动态页修复图片/视频缩略图比例与卡片高度，移除会覆盖真实图片的单色占位；图文、文字、
  专栏动态会加载详情正文和评论区；
- 评论区修复昵称与等级图标重叠；
- 普通滚动弹幕从右边缘进入，顶部/底部固定弹幕保持居中；
- 搜索栏右侧采用与主页协调的实体搜索按钮。

Nokia 603 已完成以上功能的用户验收。

## 初代 Symbian³ 设备公告

Nokia N8 / E7 / X7 / C7 等初代 Symbian³ 设备目前存在尚未解决的播放黑屏 bug，1.2 暂时撤下
对这些机型的支持，也不建议安装或将其问题按正常兼容性处理。Nokia 603、700、701、808 等原生
Nokia Belle 设备不受这一限制。

项目近期将集中攻克黑屏问题；最终目标仍是使用同一 ARMv5 SIS 覆盖 Symbian^3 全机型，以及原版
Symbian³、Anna、Belle 全系统版本。

## 已知边界

- 直播仍是实验性能力：Nokia 603 的 MMF 不支持远程或增长文件 FLV；当前本地 FLV 解复用候选的
  AAC 音频接入仍会被 MMF 的裸 ADTS controller 以 `KErrNotSupported (-5)` 拒绝，因此直播不属于
  1.2 的稳定性承诺；
- 软件 H.264 解码受 ARM1176 性能限制，复杂码流仍可能卡顿；
- Bilibili API、登录和媒体 URL 可能随服务端变化。

## 构建与签名验收

- Qt 4.7.4 qmake 使用 `symbian-sbsv2`，构建目标为 `arm.v5.urel.gcce4_4_1`；
- GCCE Debug/Release 均已完成，普通构建未启用诊断 CONFIG；
- SIS 包含主 EXE、AppArc 资源、MIF 图标、NIKINIKI CJK 字体和许可证；
- SDK 的 2009–2019 旧自签名已剥离；正式包使用当前有效的 Qt Development Frameworks 证书重签；
- 公开仓库、文档和 host JSON 门禁均已通过。
