# NIKINIKI 研究与实验索引

> 状态：Historical evidence
> 本目录不描述当前产品实现；当前结构见 `docs/developer/`。

## 播放器历史

| 文档 | 证据范围 |
|---|---|
| [第二次进入崩溃](player/PLAYER_0.7_SECOND_ENTRY_CRASH_ANALYSIS_ZH.md) | one-shot、overlay reuse 与完整对象图持久化对照 |
| [H.264 兼容性](player/PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md) | 样本矩阵、Broadcom/ARM header 控制实验 |
| [本机软件解码](player/PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md) | 系统 ARM 止损、FFmpeg 构建、RGB565 性能 |
| [GLES YUV 实验](player/PLAYER_0.9_GLES_YUV_OPTIMIZATION_ZH.md) | 三平面上传/提交测量与退役结论 |
| [Direct DevVideo 探针](player/PLAYER_1.0_DEVVIDEO_DIRECT_PROBE_ZH.md) | DSA Phase A 边界 |
| [原生横屏定位](player/PLAYER_ORIENTATION_PROBLEM_AND_WORKAROUND_ZH.md) | app-shell 探针、窗口时序和四视频验证 |

## 发布后研究候选

- [BCM2763/HwDevice](player/post-1.0/POST_1.0_BCM2763_HWDEVICE_RESEARCH_ZH.md)；
- [H.264 reference/DPB](player/post-1.0/POST_1.0_H264_REFERENCE_DPB_RESEARCH_ZH.md)；
- [DevVideo display/post-processor](player/post-1.0/POST_1.0_DEVVIDEO_DISPLAY_RESEARCH_ZH.md)；
- [直播架构早期研究](future/LIVE_PLAYBACK_ARCHITECTURE_ZH.md)。

研究候选不会自动成为路线图项目。重新开启前必须有新的测量问题，并检查是否需要 ADR。
