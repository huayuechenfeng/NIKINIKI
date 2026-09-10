# NIKINIKI 架构决策记录

本目录记录已经进入产品主线、且未来修改需要明确推翻前提的长期决定。

| ADR | 状态 | 决定 |
|---|---|---|
| [0001](0001-native-landscape.md) | Accepted | 使用受工作区事件约束的原生横屏状态机 |
| [0002](0002-persistent-player-lifecycle.md) | Accepted | 播放器完整原生对象图跨会话持久复用 |
| [0003](0003-header-preflight-routing.md) | Accepted | 用真实 H.264 header preflight 选择 MMF/FFmpeg |
| [0004](0004-on-device-software-fallback.md) | Accepted | 媒体兼容在手机本机完成，不使用远端转码 |
| [0005](0005-optional-ref7-hardware-extension.md) | Accepted | ref7 补丁与 Header/Submit split 作为可选扩展，默认回退不变 |
| [0008](0008-e7-inprocess-qt-diagnostic.md) | Superseded | 允许 E7 应用内 Qt 视频的受控诊断 |
| [0009](0009-optional-qt-mobility-player.md) | Superseded | 曾接受 Qt Mobility 可选产品后端，未通过完整验收 |
| [0010](0010-freeze-qt-candidate-and-restore-native.md) | Accepted | 冻结 Qt 候选，普通产品恢复 native MMF/FFmpeg |

原始实验数据在 `docs/research/`；ADR 只保存背景、决定和后果。
