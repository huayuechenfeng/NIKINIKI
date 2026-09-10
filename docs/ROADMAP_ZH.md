# NIKINIKI 路线图

> 状态：Active
> 基线：1.2.0 正式发布后的维护主线
> 本页职责：维护 Now / Next / Later，不保存已完成实验的过程

## Now：交付完整本地 MP4 的外部播放器交接，并维持 1.2 基线

### 0. 外部播放器交接

- 仅对已完整下载且核验为 MP4 的本地文件提供明确用户选择；不向外部应用透传
  Cookie、签名 URL 或增长中文件；
- 交接前停止并停放内部媒体、解码器、计时器与下载回调，恢复稳定竖屏和前台所有权，
  再以 Symbian 文档交接打开本地文件；
- 区分“交接 API 已接受”、“已离开 NIKINIKI”和“外部播放实际成功”；应用只能观测前两者，
  不得把调用成功写成播放成功；
- 给出无关联处理器、启动失败和返回后文件不可用的明确反馈；从外部应用返回后可重复进入、
  重新下载或选择其他媒体，不复用旧会话回调；
- 迁移旧设置时保留 native 默认；未发布 Qt 候选留下的 `player/backend_mode=1`
  只能显式映射为新的外部播放选择，不得在普通构建中恢复 Qt 后端。

该功能必须作为清理后的独立产品提交，不与本次工作区归档提交混合。主机构建与
静态检查通过后仍标记“设备待验收”，由独立研究会话完成真机结果。

当前源码已实现上述下载、AppArc 交接、旧设置迁移和返回状态重置；普通 Debug/Release
GCCE 构建、开发 SIS 打包、文档、公开边界及主机检查已通过。本阶段剩余门槛只有独立真机
验收，不把主机通过升级为设备兼容通过。

### 1. 播放器稳定性

- 在 1.1 正式 Release 上完成 50 次进入、播放、退出和再次进入；
- MMF 安全流与 FFmpeg 风险流交替测试；
- 分别确认真实 header preflight 的 `ACCEPT → MMF` 和 `REJECT → FFMPEG` 标记；
- 覆盖暂停、seek、倍速、音量、画质、弹幕开关和控制栏；
- 确认无崩溃、黑屏、残留透明窗、后台声音或持续内存增长；
- 做至少 30 分钟的温度、内存和前后台观察。

通过结果统一写入 `reference/DEVICE_TEST_MATRIX.md`。

### 2. 软件视频表面专项验证

- 确认 soft 路径出现 `SOFT_SURFACE_ACTIVE` 和 `SOFT_SURFACE_FIRST_PAINT`；
- 确认 `softSurfacePresented>0`、`overlayVideoDrawMs=0`；
- 确认弹幕和控制始终位于视频表面之上并可交互；
- 确认 `overlayIntermediateMs=0`、`overlayRotateMs=0`；
- 验证 500 ms 音频时钟校准、pause、seek、倍速和会话切换后的 cache invalidation；
- 先取得完整 decode/convert/present/UI 分段数据，再决定下一项性能改动。

### 3. 系统覆盖

- 在原版 Symbian³、Anna 上安装与 Belle 相同的 1.2.0 开发 SIS；
- 核验 Qt 4.7.4、Qt Mobility 1.2.x、TLS、冷启动、首页图片、登录和播放；
- 初代 Symbian³ 的黑屏验证、停止门和取证方法不在活路线图展开；续研只按
  [E7 MMF Prepare 错误来源](research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)和
  [设备矩阵](reference/DEVICE_TEST_MATRIX.md#e7-hx-timed-20260908)执行；
- Nokia 603 已通过共享 `RFile` 增长文件、best-effort 软件视频轨关闭、8 MiB 起播预缓冲、
  完整下载进度 UI 和独立设置列表复测；
- 收集系统版本、设备型号、运行库版本和完整结果，不把“预期兼容”记为通过。

### 4. 首次启动跨设备回归

- Nokia 603 已确认 1.1 最终包的首次启动字体非阻塞方案实际运行正常；
- 在 N8 / E7 / X7 / C7 及原版 Symbian³ / Anna 上继续复测卸载、重启、重装后的冷缓存启动；
- 覆盖内部存储和存储卡安装，并检查完整字体接入后生僻字 fallback 没有缺字或崩溃；
- 真机结果只写入 `reference/DEVICE_TEST_MATRIX.md`，静态检查和 GCCE 编译不能替代冷启动结论。

## Next：改善视频体验

1. 建立 Nokia 603 和 N8 一代的 decode-only、convert-only、present-only 基线；
2. 在测量证明瓶颈后，评估 FFmpeg ARMv6/VFP、loop filter、non-reference frame 和追帧策略；
3. 扩充 360P/480P/720P、profile/level、AAC、横竖向编码和 CDN 媒体矩阵；
4. 回归搜索相关性、输入法焦点、登录态、评论分页和异常恢复。

软件性能目标是先推动 Symbian³ 设备的 360P 软件播放稳定达到 20 fps 以上；
Nokia 603 再向稳定 30 fps 研究。目标不是已取得的结果，任何数字都必须由真机矩阵支持。

## 已关闭的研究边界

H.264 ref7 支线已经按 H1 结题：Nokia 603 的目标合法 R7 graph 可由 Header/Submit split 和精确
admission patch 两条路径正确硬解。该项目不再占用 Now/Next；最终证据、产品边界和归档入口见
[H.264 ref7 硬件解码结题报告](research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)。

E7 黑屏研究已按 [ADR-0010](decisions/0010-freeze-qt-candidate-and-restore-native.md)冻结，
不占用 Now/Next；边界、证据和续研入口只见
[E7 MMF Prepare 错误来源](research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。

## Later：不阻塞播放器主线

- 直播断流恢复、画质切换和直播弹幕；
- 局部布局和其他 UI polish；
- 已封存的底层媒体替代方案只在路线图明确重开研究后再评估。
- 对可选 ref7 补丁做跨设备真机资格测试；静态特征命中不计通过，也不阻塞播放器发布。

阶段性 H.264 研究材料已归档在 `research/player/post-1.0/`；其他研究候选只有建立当前回归基线后，
才考虑提升为正式工作。

## 不重新执行

- 桥接、远端重封装或远端转码；
- Q6/240P 请求阶梯或以切 CDN 代替编码兼容处理；
- 把 full-SPS fake 持续送入 decoder 作为播放兼容方案；成功的 Header/Submit split 只可在同一
  Direct DevVideo 会话中使用，不能伪装成当前 MMF preflight 已经继承该状态；
- 系统 ARM H.264 decoder 产品化；
- Broadcom Direct DevVideo/DSA 全屏显示作为当前播放器后端；
- GLES 三平面 YUV 普通输出；
- 旧竖屏窗口加整帧 90° 软件旋转；
- `AA_S60DontConstructApplicationPanes`、过早隐藏主 QGL 或方向切换期间重建窗口树；
- 只持久化 ARGB overlay，或销毁并重建完整原生播放器对象图；
- 普通包加入 `ffmpeglatedrop1`，或恢复 FFmpeg 全库 `-O2/-O3`。

若新证据需要重新打开上述方向，必须先新增或修订 ADR，写明旧结论被哪项证据推翻。

## 完成定义

一个项目只有同时满足以下条件才能移入 Done：

1. 代码或文档修改已经进入 NIKINIKI 产品主线；
2. 相关静态检查或 GCCE 构建通过；
3. 需要设备行为的项目已经真机验证；
4. 结果写入唯一事实来源；
5. 没有把诊断构建、探针或研究结果表述成正式产品能力。
