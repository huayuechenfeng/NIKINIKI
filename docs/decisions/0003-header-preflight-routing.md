# ADR-0003：使用真实 H.264 header preflight 选路

> 状态：Accepted
> 决定日期：2026-08-29

## 背景

分辨率、Level、CDN、Q6/Q16 和人工 refs/DPB 风险表都不能可靠判断固件是否出画面。控制实验
证明 Broadcom 插件在相同输入契约下接受工作流 header，并明确拒绝故障流 header。

## 决定

每个首选 progressive MP4 在 MMF 打开前，取真实 SPS/PPS 和首个同步 AU，使用 Broadcom
`GetHeaderInformationL()` 做只读 preflight：接受走 MMF，明确拒绝走本机 FFmpeg；Range 或
preflight 自身失败时保守尝试 MMF。

## 后果

- 删除不断膨胀的编码风险规则表；
- 不插入已证明返回同一 Q16 对象的 Q6 请求；
- preflight 不 Configure、Initialize、Start 或显示 DevVideo；
- 每个会话只允许一次单向选路，并记录明确标记。

## 证据

控制实验和样本矩阵见
`docs/research/player/PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md`；旧 1.0 冻结政策保存在
`docs/archive/plans/PLAYER_1.0_DECODING_POLICY_ZH.md`。
