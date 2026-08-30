# NIKINIKI 文档

> 状态：Active
> 适用范围：NIKINIKI 1.0.0 及后续主线
> 本页职责：按读者任务提供入口，不保存开发流水账

NIKINIKI 的文档分为活文档、决策、参考、研究、发布和历史归档。活文档描述现在；
研究和归档解释过去。历史文件即使日期更晚，也不能覆盖活文档。

## 三份接手文档

新维护者通常只需依次阅读：

1. [当前状态](STATUS_ZH.md)：现在已经实现什么、验证到哪里、还缺什么；
2. [路线图](ROADMAP_ZH.md)：当前优先级、验收门槛和明确不做的方向；
3. [总体架构](developer/ARCHITECTURE_ZH.md)：模块边界和主要代码入口。

处理播放器任务时，再阅读[播放器架构](developer/PLAYBACK_ARCHITECTURE_ZH.md)。

## 用户文档

| 任务 | 文档 |
|---|---|
| 下载和安装 | [安装指南](user/INSTALL_ZH.md) |
| 判断系统与运行库兼容性 | [Symbian³ / Anna / Belle 兼容性](user/COMPATIBILITY_ZH.md) |
| 黑屏、网络或播放问题 | [故障排查](user/TROUBLESHOOTING_ZH.md) |
| 项目概览与下载入口 | [仓库 README](../README.md) |

## 开发文档

| 主题 | 权威文档 |
|---|---|
| 当前实现和待验证边界 | [STATUS_ZH.md](STATUS_ZH.md) |
| 当前工作顺序 | [ROADMAP_ZH.md](ROADMAP_ZH.md) |
| 总体模块和依赖方向 | [ARCHITECTURE_ZH.md](developer/ARCHITECTURE_ZH.md) |
| 播放选路、窗口和生命周期 | [PLAYBACK_ARCHITECTURE_ZH.md](developer/PLAYBACK_ARCHITECTURE_ZH.md) |
| Symbian 构建和调试 | [symbian/README.md](../symbian/README.md) |
| 仓库与 GitHub 发布边界 | [REPOSITORY_POLICY_ZH.md](developer/REPOSITORY_POLICY_ZH.md) |
| 文档维护约定 | [DOCUMENTATION_GUIDE_ZH.md](developer/DOCUMENTATION_GUIDE_ZH.md) |

## 决策、参考与证据

- [架构决策记录](decisions/README_ZH.md)：说明当前设计为什么如此，以及改变它需要推翻哪些前提；
- [设备测试矩阵](reference/DEVICE_TEST_MATRIX.md)：真机通过、失败和待测项目的唯一记录；
- [工具链报告](reference/TOOLCHAIN_REPORT.md)：Qt、GCCE、SDK、签名和部署基线；
- [上游固定点](reference/UPSTREAM_BASELINE.md)与[代码边界](reference/CODE_BOUNDARY_ANALYSIS_ZH.md)：来源、复用范围和许可证边界；
- [播放器研究](research/README_ZH.md)：0.x 实验、性能测量和发布后底层研究；
- [版本发布记录](releases/README_ZH.md)：正式包、校验值、签名和重链接材料；
- [历史归档](archive/README_ZH.md)：旧阶段报告、早期设计、会话记录和已被替代的计划。

## 信息归属

| 信息类型 | 唯一归属 |
|---|---|
| 当前能力与限制 | `STATUS_ZH.md` |
| 下一步工作 | `ROADMAP_ZH.md` |
| 当前技术结构 | `developer/` |
| 设计理由 | `decisions/` |
| 真机结果 | `reference/DEVICE_TEST_MATRIX.md` |
| 实验数据与失败路径 | `research/` |
| 安装包、哈希和签名 | `releases/` |
| 会话流水账与旧方案 | `archive/` |

发生冲突时，按上表确定权威来源；不要通过“更新时间更晚”猜测哪份历史文档有效。
