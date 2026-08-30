# NIKINIKI 架构决策记录

本目录记录已经进入产品主线、且未来修改需要明确推翻前提的长期决定。

| ADR | 状态 | 决定 |
|---|---|---|
| [0001](0001-native-landscape.md) | Accepted | 使用受工作区事件约束的原生横屏状态机 |
| [0002](0002-persistent-player-lifecycle.md) | Accepted | 播放器完整原生对象图跨会话持久复用 |
| [0003](0003-header-preflight-routing.md) | Accepted | 用真实 H.264 header preflight 选择 MMF/FFmpeg |
| [0004](0004-on-device-software-fallback.md) | Accepted | 媒体兼容在手机本机完成，不使用远端转码 |

原始实验数据在 `docs/research/`；ADR 只保存背景、决定和后果。
