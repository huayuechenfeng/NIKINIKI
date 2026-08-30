# ADR-0002：完整播放器对象图跨会话持久复用

> 状态：Accepted
> 决定日期：2026-08-25

## 背景

旧 one-shot 和 `deleteLater()` 实现能够走到 `DESTROY_READY`，但第二次进入仍在新的
`PLAYER_REBUILD_BEGIN` 之前访问 `0x140` 并 data abort。只持久化 ARGB overlay 也无法修复。

## 决定

player controller、native video host、backend/observer、`CVideoPlayerUtility2`、soft surface
和 ARGB overlay 均存活到应用退出。关闭播放只停止、解绑、清空会话状态并隐藏；再次进入复用
相同原生对象，只关闭和重新打开媒体。

## 后果

- 播放器生命周期按应用而不是单次媒体会话管理；
- 新功能不得在退出播放时销毁仍可能接收 WSERV/MMF 回调的原生窗口；
- Release 50 次循环仍是低概率泄漏和残留窗口的验收门。

## 证据

Nokia 603 已确认 `surfacepersist1` 可以重复退出和进入。完整 A/B/C 过程见
`docs/research/player/PLAYER_0.7_SECOND_ENTRY_CRASH_ANALYSIS_ZH.md`。
