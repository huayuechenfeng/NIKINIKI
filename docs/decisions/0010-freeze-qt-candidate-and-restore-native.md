# ADR-0010：冻结 Qt 候选并恢复 native 产品主线

> 状态：Accepted
> 决定日期：2026-09-10
> 基线：`v1.2.0` / `fc9a0ea`

## 背景

ADR-0009 曾接受把 `QMediaPlayer + QVideoWidget` 作为可选产品后端。该方向已完成
GCCE Debug/Release 构建和完整应用诊断，但 E7 同媒体的完整产品壳尚未出画，
返回、重复使用和 Nokia 603 回归也未通过。同一未提交工作区还混入了旧窗口、
six-observation、绑定、DevVideo 和 NanoVG 统计，不能将主机编译成功视为产品准入。

## 决定

- 完整冻结 Qt 候选、设置持久化、会话选择、`e7fullappqt1 -> e7sixr1` 与必要诊断源码；
- 冻结点保留原产品 target、UID、完整本地样本哈希和包身份对应，但不作为 Release 基线；
- 普通产品恢复为已验证的 native MMF/FFmpeg，不展示未完成兼容验收的 Qt 选项；
- native 播放器不变更真实 header preflight、单向回退、横屏门槛和持久对象图不变量；
- 黑屏研究保留全部脱敏证据，但从 STATUS/ROADMAP 移出逐次运行流水，活文档只留边界和链接；
- 完整本地 MP4 的系统播放器交接是清理后的独立功能，不与本次归档提交混合。

## 后果

- ADR-0009 被本决定取代；ADR-0008 的历史诊断准入仍只作证据边界；
- Qt 候选的编译或 API 成功不是 E7/N8/603 播放通过；
- 普通主线不读取或写入未发布的 `player/backend_mode`；后续外部播放功能需显式处理其迁移；
- 真机结果继续只写入 `docs/reference/DEVICE_TEST_MATRIX.md`，研究过程只写入 `docs/research/`。
