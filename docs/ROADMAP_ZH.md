# NIKINIKI 路线图

> 状态：Active
> 基线：1.1.0 正式发布后的产品主线
> 本页职责：维护 Now / Next / Later，不保存已完成实验的过程

## Now：建立 1.1 回归与性能基线

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

- 在原版 Symbian³、Anna 上安装与 Belle 相同的 1.1 SIS；
- 核验 Qt 4.7.4、Qt Mobility 1.2.x、TLS、冷启动、首页图片、登录和播放；
- 在 N8/X7/C7 Belle 对同一 MP4 固定“全程硬解”，依次比较 `OpenUrlL`、
  增长文件 `OpenFileL`、完整下载 `OpenFileL`，禁止同时改变清晰度或解码器；
- Nokia 603 已通过共享 `RFile` 增长文件、best-effort 软件视频轨关闭、8 MiB 起播预缓冲、
  完整下载进度 UI 和独立设置列表复测；
- 若两个 `OpenFileL` 路径有画面而 `OpenUrlL` 黑屏，归因到 MMF streaming/controller；
  若三个应用内路径均黑而系统播放器播放同一本地文件正常，再进入 RWindow/Surface 显示链调查；
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
4. 对内置软件解码不支持、内存不足或性能不可接受的媒体，实现明确的外部播放器确认入口；
5. 回归搜索相关性、输入法焦点、登录态、评论分页和异常恢复。

软件性能目标是先推动 Symbian³ 设备的 360P 软件播放稳定达到 20 fps 以上；
Nokia 603 再向稳定 30 fps 研究。目标不是已取得的结果，任何数字都必须由真机矩阵支持。

## Later：不阻塞播放器主线

- 直播证书、容器、画质切换和直播弹幕；
- 局部布局和其他 UI polish；
- BCM2763/“BCM2727”插件命名、固件和 DPB 边界研究；
- DevVideo post-processor memory-output 等底层替代方案。

发布后研究入口位于 `research/player/post-1.0/`。只有建立当前 1.0 回归基线后，
才考虑把其中某项提升为正式工作。

## 不重新执行

- 桥接、远端重封装或远端转码；
- Q6/240P 请求阶梯或以切 CDN 代替编码兼容处理；
- SPS ref/DPB 伪装；
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
