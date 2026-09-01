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

## H.264 ref7 研究结论

- [H.264 ref7 硬件解码结题报告](player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)：
  Nokia 603 的 host admission gate、两条成功解锁路径、通用特征补丁和产品边界；
- [通用 ref7 补丁](../../symbian/patches/h264-ref7/README_ZH.md)：不随 SIS 安装的手动实验成果；
- [阶段研究计划](player/post-1.0/H264_HARDWARE_DECODE_RESEARCH_PLAN_ZH.md)、
  [BCM2763/HwDevice 早期假设](player/post-1.0/POST_1.0_BCM2763_HWDEVICE_RESEARCH_ZH.md)和
  [reference/DPB/fake 控制](player/post-1.0/POST_1.0_H264_REFERENCE_DPB_RESEARCH_ZH.md)
  已结题归档，不再描述当前优先级。

## 其他发布后研究

- [DevVideo display/post-processor](player/post-1.0/POST_1.0_DEVVIDEO_DISPLAY_RESEARCH_ZH.md)；
- [直播架构早期研究](future/LIVE_PLAYBACK_ARCHITECTURE_ZH.md)。

研究候选不会自动成为路线图项目。重新开启前必须有新的测量问题，并检查是否需要 ADR。
