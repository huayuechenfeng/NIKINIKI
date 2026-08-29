# NIKINIKI（Symbian³）文档索引

> 更新日期：2026-08-29  
> 当前开发基线：1.0.0 正式发布版（`ffmpegsoft2`、CPU RGB565 与原生横屏状态机已合并主线）
> 当前状态：`Symbian3Qt474` Release 编译、打包和有效签名已通过；Nokia 603 / Belle 为已验证基线，原版 Symbian³ / Anna 转入公测收集

NIKINIKI 是正式产品名；`wiliwili for Symbian³` / `wiliwili_symbian` 为曾用名、
内部兼容名称和历史文档用名，不做机械删除。

## 新会话阅读顺序

1. `DEVELOPMENT_STAGE_REPORT_2026-08-25_ZH.md`：当前状态的单一事实来源；
2. `SYMBIAN3_ANNA_BELLE_COMPATIBILITY_ZH.md`：一个应用包覆盖三代系统的构建证据、运行库要求、签名限制和真机矩阵；
3. `PLAYER_1.0_DECODING_POLICY_ZH.md`：1.0 前强制执行的 MMF → 本机软解 → 外部播放器路线；
4. `PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md`：系统 ARM 已止损、PPSSPP-FFmpeg/libavcodec 当前实现与真机门槛；
5. `PLAYER_0.9_GLES_YUV_OPTIMIZATION_ZH.md`：已退役的 ARM11/GLES2 三平面 YUV420 实验、PotatoStream 借鉴边界、构建证据与真机结论；
6. `PLAYER_1.0_DEVVIDEO_DIRECT_PROBE_ZH.md`：已封存的 DSA Phase A 硬失败、构建证据和止损边界；
7. `POST_1.0_DEVVIDEO_DISPLAY_RESEARCH_ZH.md`：DSA 受限控制条/静态 PP 的发布后研究门；
8. `POST_1.0_H264_REFERENCE_DPB_RESEARCH_ZH.md`：用户 SPS fake 实验、Direct DevVideo 对照与 DPB 研究边界；
9. `PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md`：当前有声无画样本、BCM/ARM 控制实验与历史证据；
10. `PLAYER_0.7_SECOND_ENTRY_CRASH_ANALYSIS_ZH.md`：历史 A/B/C 结果与 `surfacepersist1` 已验证修复；
11. `NEXT_WORK_PLAN_ZH.md`：软解、外部回退和压力验收顺序；
12. `DEVICE_TEST_MATRIX.md`：所有已验证、失败和待验证项目；
13. `RELEASE_1.0.0_ZH.md`：正式 Release SIS、SHA-256、签名与构建验收；
14. `RELEASE_0.9.0_ZH.md`：历史候选安装包、签名与验证清单；
15. `RELEASE_0.7.0_ZH.md`：历史安装包、SHA-256、架构与已知问题。

如果其他文档与以上文件冲突，以阶段报告、1.0 解码政策和更新日期更晚的真机证据为准。

## 当前成果摘要

- Nokia 603 已确认最终 app-shell/RGB565 原生横屏探针连续 20 轮成功，动态色条/扫描线正确，横竖屏均无系统栏；
- 当前主线保留 Avkon panes 与主 QGL 映射，只在 `workAreaResized()` 确认物理屏幕方向后提交独立播放器顶层窗口；
- 单一持久 ARGB 覆盖层继续把弹幕和控制 UI 放到视频上方；MMF、CPU RGB565、UI 和触摸现在都直接使用 640×360 原生坐标；
- 旧的整帧 90° 中间图、像素旋转和第二次 draw 已删除，历史约 224 ms/frame 的旋转开销等待主线真机性能复测；
- one-shot 会话已经完整到达 `DESTROY_READY`；
- 旧 Release 与 Debug 脱离 CODA 也都会崩溃，CODA 与 UDEB-only 已排除；
- `overlayreuse1` 真机仍会在第二次播放卡死退出，仅持久化 ARGB overlay 已排除为充分修复；
- 当前源码应用期复用 controller、native video host、MMF observer/utility 与 overlay，退出只停止、解绑和隐藏；用户已确认可以重复播放；
- B 站部分最低 360P AVC 使用 7 个参考帧、4 帧重排和 weighted B-frame，Belle MMF 只能播放音频；统一降 360P 和切 CDN 都无效；
- 相同 Direct DevVideo 输入条件下，BCM2727 接受正常 4-ref 流并正确返回 640×360，却对故障 7-ref/DPB7/weighted 流稳定返回 -5；通用输入契约和 MMF 资源占用已经排除；
- ARM H.264 decoder 接受故障 header 但返回 0×0，随后 `ConfigureDecoderL()=-5`；系统 ARM/`armsoftprobe1` 路线已废止；
- PPSSPP-FFmpeg 旧预编译库与 GCCE 4.4.1 因 EABI attribute 44 不兼容；现已用 GCCE 4.4.1 从源码重编 H.264-only `libavcodec`/`libavutil`。`ffmpegsoft1` 已在 Nokia 603 为原故障视频输出画面，但纯 C 版仅约 2–3 fps；当前 `ffmpegsoft2` 恢复 ARMv6/VFP、优化 RGB565 并加入自适应追帧与分段耗时日志；
- MP4 sample 的 `ctts` version 0/1、DTS/PTS 已补齐，并传入 DevVideo decoding/presentation timestamp；
- `headercontrol1` 已退役并从默认源码删除；正常模板继续 MMF，故障模板保留 MMF AAC，旧 BCM 接管需要显式实验宏；
- 1.0 前不再探索 BCM2727/BCM2763 硬解边界：应用内本机软解优先，仍不支持时允许提示用户调用外部播放器；不使用桥接或远端转码；
- `ffmpegsoft2` 已并入 0.9 主线：首选 MP4 的真实 AVC header 会先交给 Broadcom 插件做有界 preflight；接受则继续 MMF，拒绝则在手机本机走 PPSSPP-FFmpeg；晚到丢帧诊断未进入正式包；
- `devvideodirectprobe1` 已在 Nokia 603 通过 Phase A 止损：正式全屏 ARGB UI/弹幕 overlay 下 DSA DrawingRegion 为零，Phase B 没有启动；该显示路线与 1.0 产品 UI 不兼容，封存到发布后研究；
- 用户的 SPS `ref=7 -> 3` 文件级伪装实验显示 SPS/DPB 声明确实影响 Nokia 603 是否启动视频路径，但 fake 码流会因真实参考关系超过声明 DPB 而花屏；它支持存在准入门，不改变 1.0 的 FFmpeg/CPU RGB565 路线；
- GLES-YUV 三平面路径已在 Nokia 603 真机测得约 216 ms/帧上传、约 321 ms/帧提交，ping-pong 无改善，现已退出默认 soft playback；CPU YUV420P→RGB565 路径重新作为默认输出，继续复用原有 ARGB 弹幕/控制覆盖层；
- 约 11.4–12.0 fps 是此前优化 CPU RGB565 输出的实测基线。当前源码已切换回该路线，旧 GLES-YUV 0.9 安装包仅作历史证据，必须重新构建后再做正式版验证。
- 旧 soft-only native-landscape 归档仍是失败历史，不能恢复；后续 app-shell 分层定位证明故障来自 panes/fullscreen/物理几何不一致和过早隐藏 QGL，而不是 native landscape 本身。
- 最终 `appshell7-final-dynamic-fullscreen-rgb565` 在正式 QGL app shell 中通过 20 轮：20 次 640×360 gate、20 次首帧、47 次动态 RGB565 呈现、20 次精确 360×640 fullscreen 恢复、0 timeout；该时序现已合入主播放器。
- 当前普通 Debug/Release 主线均已由 GCCE 编译通过（各 `sbs errors: 0`，34 条既有 SDK 警告）；尚未把“探针通过”扩大为“MMF/FFmpeg 主播放器通过”，下一步只做主线设备验收。
- 发布构建已下沉到原版 `Symbian3Qt474`：移除唯一的 Belle-only `cookiemanager.dll` 后，Debug/Release 均为 `sbs errors: 0`（32 条既有 SDK 警告），导入表不再含 CookieManager。该统一包已在 Nokia 603 / Belle 实测正常；原版 Symbian³ / Anna 另行补齐 Qt 4.7.4、Qt Mobility 1.2.x 后在公测收集，不拆分应用包。先前黑屏/闪退报告来自错包，已作废。

## 专题和基线文档

| 文档 | 定位 |
|---|---|
| `DEVELOPMENT_DESIGN_ZH.md` | 项目早期总体设计；保留原则和范围，具体播放器实现以 0.7 报告为准 |
| `PLAYER_1.0_DECODING_POLICY_ZH.md` | 1.0 前解码选择、软解要求、外部播放器回退和验收门槛 |
| `PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md` | ARM 失败证据、PPSSPP-FFmpeg GCCE 构建/集成、公共 PTS 基础和验收计划 |
| `PLAYER_1.0_DEVVIDEO_DIRECT_PROBE_ZH.md` | 最后一次有界 DevVideo DSA 支线、Phase A/Phase B 硬门和真机操作 |
| `POST_1.0_DEVVIDEO_DISPLAY_RESEARCH_ZH.md` | 已失败 DSA + 全屏 ARGB 的精确边界，以及受限控制条/静态 PP 的发布后前置门 |
| `POST_1.0_H264_REFERENCE_DPB_RESEARCH_ZH.md` | SPS fake 实验与 Direct DevVideo header 对照；H.264 DPB/插件/固件研究顺序 |
| `PLAYER_ORIENTATION_PROBLEM_AND_WORKAROUND_ZH.md` | 原生横屏根因、20 轮最终证据、主线状态机与旧 90° 路径移除范围 |
| `PLAYER_0.9_GLES_YUV_OPTIMIZATION_ZH.md` | 已退役的 GLES2 三平面 YUV420 实验、ARM11 取舍、构建证据和真机结论 |
| `POST_1.0_BCM2763_HWDEVICE_RESEARCH_ZH.md` | 1.0 后再研究 BCM2763 与 “BCM2727” HwDevice 命名/能力差异；当前禁止据此改播放器 |
| `LIVE_PLAYBACK_ARCHITECTURE_ZH.md` | 直播 API、线路和交互设计；普通视频 P0 解决前暂缓实现 |
| `CODE_BOUNDARY_ANALYSIS_ZH.md` | 自研代码与上游 wiliwili 代码的边界：目录归属、跨边界复用清单、依赖方向与许可证 |
| `SESSION_CHANGES_2026-08-28_ZH.md` | 2026-08-28 会话改动报告：四项功能修复、mongoose 兼容层替换、编译期修复与验证状态 |
| `PORTING_AUDIT.md` | 上游组件在 GCCE/Qt 4 下的复用等级和证据 |
| `TOOLCHAIN_REPORT.md` | Belle SDK、GCCE、Qt Creator、签名和部署工具链基线 |
| `SYMBIAN3_ANNA_BELLE_COMPATIBILITY_ZH.md` | 最低 SDK 统一构建、运行库前置、ABI 证据与三系统发布矩阵 |
| `UPSTREAM_BASELINE.md` | 上游 commit 和 submodule 固定点 |

## 历史版本文档

`archive/release-notes/` 保存 0.5 和 0.6 的实验版本说明，只用于追溯。它们不代表 0.7/0.9 当前架构，也不应作为下一会话的执行计划。

## 仓库与发布物边界

`REPOSITORY_LAYOUT_AND_GITHUB_POLICY_ZH.md` 规定源码、文档、可复现构建材料、签名安装包、真机日志和外部研究材料各自的归档位置，以及未来 GitHub 提交与 Release 附件的边界。

## 当前签名安装包

| 配置 | 路径 |
|---|---|
| Release 1.0.0（正式发布包） | `symbian/out/releases/v1.0.0/NIKINIKI_1.0.0_release.sis` |
| Release 0.9 mainline（历史验证包） | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_release_full_currentcert.sis` |
| Debug 0.9 mainline（问题定位包） | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_debug_full_currentcert.sis` |
| Release codeccompat1（历史诊断候选） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_codeccompat1_currentcert.sis` |
| Debug codeccompat1（历史日志包） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_codeccompat1_currentcert.sis` |
| Release surfacepersist1（已验证回归基线） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_surfacepersist1_currentcert.sis` |
| Debug surfacepersist1（已验证回归基线） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_surfacepersist1_currentcert.sis` |
| Debug ffmpegsoft1（历史纯 C 基线：可出画面，约 2–3 fps） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_ffmpegsoft1_currentcert.sis` |
| Debug armsoftprobe1（历史失败候选） | `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_armsoftprobe1_currentcert.sis` |

`surfacepersist1` 是用户已确认可重复播放的 MMF 回归基线。1.0 当前源码将 `ffmpegsoft2` 和 CPU RGB565 输出设为普通构建默认，Qt Creator 不需要再额外加入 CONFIG；不要加入 `ffmpeglatedrop1`。旧 GLES-YUV 包仅作历史性能证据。`armsoftprobe1`、`codeccompat1`、`headercontrol1`、`devvideoprobe1` 和 `devvideosample1` 都只作历史诊断对照，不应再用于判断当前普通构建或 1.0 路线。
