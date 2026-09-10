# NIKINIKI 研究与实验索引

> 状态：Historical evidence
> 本目录不描述当前产品实现；当前结构见 `docs/developer/`。

## 播放器历史

| 文档 | 证据范围 |
|---|---|
| [E7 MMF Prepare 错误来源](player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md) | E7/603 的 MediaClientVideo 与 Real controller 有限静态比较、Prepare event 传播边界、E7 运行代码读取脚本及有上限的下一设备契约 |
| [实验性视频兼容模式封存](player/EXPERIMENTAL_VIDEO_COMPATIBILITY_ARCHIVE_ZH.md) | 已移出产品的设置/会话/延后绑定代码契约、完整应用首次失败边界及未执行的同样本入口 |
| [E7 Qt 内部会话观测映射](player/E7_QT_INTERNAL_SESSION_OBSERVATION_ZH.md) | 包身份修正、候选/设备 ROM code 关联、A/完整/完整/A 的 Prepare/apply/Play 实例对照，以及参照翻转后的原始 Prepare event 取证门槛 |
| [E7 六次与 C1/C2 后重新评估](player/E7_BLACK_SCREEN_REASSESSMENT_ZH.md) | 历史假说排序、PP统计归属、C2边界、完整应用同媒体结论及内部会话取证后的唯一方向 |
| [E7 黑屏总审计](player/E7_BLACK_SCREEN_ROOT_CAUSE_AUDIT_ZH.md) | 累计日志归属、被推翻的因果判断、Qt/MMF 与 EGL 调用链、首帧证据缺口；设备事实只见设备矩阵 |
| [E7 DevVideo memory 路线](player/E7_DEVVIDEO_MEMORY_ROUTE_PLAN_ZH.md) | C1/C2/C3/BD 顺序、E7 C1-Solo 与 decoder+post-processor 补测边界；诊断 CONFIG 不属于正式播放器 |
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
