# NIKINIKI 当前状态

> 状态：Active
> 适用版本：1.2.0 正式发布
> 最近事实核验：2026-09-02
> 本页职责：描述当前实现和验证边界，不记录实验过程或发布校验值

## 发布基线

NIKINIKI 1.2.0 已完成 `Symbian3Qt474` GCCE Release 编译、打包、有效签名和用户完成的
Nokia 603 功能验收并公开发布。
正式安装包、大小、SHA-256、证书和 LGPL 重链接材料只在
[1.2.0 发布说明](releases/RELEASE_1.2.0_ZH.md)维护。

当前统一包：

- 目标为 ARMv5、Qt 4.7.4、GCCE 4.4.1；
- 使用最低 `Symbian3Qt474` 构建，一个应用 SIS 覆盖 Symbian³、Anna 和 Belle；
- Nokia 603 / Nokia Belle 是当前主要真机基线；
- Nokia 603、700、701、808 等原生 Belle 设备不受 1.2 的兼容性限制；
- Nokia N8 / E7 / X7 / C7 等初代 Symbian³ 设备存在播放黑屏 bug，1.2 暂不支持；
  修复后仍以 Symbian³、Anna、Belle 全系统/全机型为目标。

## 已实现能力

- 首页推荐、栏目、动态、消息、搜索和视频详情；
- 二维码登录、登录态持久化、历史、收藏、稍后再看、关注和互动；
- 评论根列表、楼中楼和分页所需的客户端结构；
- Progressive MP4 选源、备用 URL 和 H.264/AAC 播放；
- 播放/暂停、进度、倍速、音量、清晰度、弹幕和触摸控制；
- 主页实体音量键与播放器共用持久音量，播放器使用右侧竖向音量滑块；
- 视频详情的纵向相关推荐列表；动态首图按原始比例显示、卡片随正文和图片自适应高度，
  非视频动态通过详情接口加载图文/专栏正文并继续显示评论列表；
- 原生横屏播放器、无系统栏竖屏恢复和重复进入；
- 系统 MMF 硬件播放与手机本机 FFmpeg H.264 软件回退；
- 深度破解设备可手动使用不随 SIS 安装的实验性 ref7 admission 补丁；Nokia 603 SW113 已通过；
- 设置页可持久选择三种点播传输方式与自动/全硬解/全软解策略；
- Qt 4.7.4 / GCCE 4.4.1 的构建、打包和公开仓库检查脚本。

## 当前播放器

播放器对每个首选 progressive MP4 做一次只读 Broadcom header preflight：

```text
Broadcom 接受真实 H.264 header → MMF 硬件视频
Broadcom 明确拒绝             → 本机 FFmpeg H.264 视频 + MMF AAC
preflight 获取失败            → 保守回到 MMF，并保留失败日志
```

可选 ref7 补丁启用后，主程序仍使用真实 header 和原始 MP4；HwDevice 接受时自然进入上述 MMF
路径。应用不安装、探测或自动启用补丁，未启用时的回退行为不变。

软件画面采用 CPU YUV420P→RGB565 LUT2X2，由持久、不透明的原生视频表面显示；
唯一透明 ARGB 顶层只绘制弹幕和控制。播放器完整原生对象图跨会话复用，横屏下视频、UI、
弹幕和输入都直接使用 640×360 坐标。

实现细节、状态机和日志标记见
[播放器架构](developer/PLAYBACK_ARCHITECTURE_ZH.md)。

## 已验证事实

- Nokia 603 已确认 MMF 首次横屏画面、声音、弹幕和控制 UI 正常；
- `KErrMMPartialPlayback (-12017)` 已作为可恢复轨道探测结果处理；
- `surfacepersist1` 证明完整播放器对象图持久复用可以反复退出和重入，原确定性第二次进入崩溃已功能修复；
- 最终 app-shell 原生横屏探针完成 20 轮，横竖屏均无系统栏，0 timeout；
- 集成主线日志 `2746319` 覆盖两条 FFmpeg soft、两条 MMF 视频，画面方向、弹幕/控制、返回和对象复用均正确；
- Nokia 603 的合法矩阵确认 ref=1..6 正确、ref=7/8 在 Header 返回 `-5`；declared DPB 4..8 与
  weighted P through ref=6 不是独立触发项；
- ECom、ROM 重建和 Thumb/RTTI/vtable 审计把 `0x10204C21` 映射到 direct
  `ivevideodecodehwdevice.dll`，并定位 host AVC parser 的 `max_num_ref_frames>6 -> Leave(-5)`；
- Direct Header/Submit split 已成功：fake-ref3 只供同一会话 Header/Configure，Initialize 后提交
  未修改 ORIGINAL_R7 的全部 100 AU，输出 99/99 PC-golden 匹配 picture 和 3/3 raw-frame CRC；
- 精确 `cmp r0,#6 -> cmp r0,#7` ROM shadow patch 也让 ORIGINAL_R7 全阶段得到同样的 99/99 正确
  结果；通用 `SnR` 版本随后在 Nokia 603 SW113 通过，正式 NIKINIKI 可硬解此前只能软解的自然样本；
- full-SPS fake 持续提交仍从第 7 帧开始 corruption，只是负控制，不属于成功路径。完整证据见
  [H.264 ref7 结题报告](research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)；
- 系统 ARM decoder 虽接受风险 header，但返回 0×0 且 Configure 为 `KErrNotSupported`，不能作为产品后端；
- 本机 FFmpeg 已让原有声无画视频出画面；CPU RGB565 LUT2X2 的历史基线约为 11.4–12.0 fps；
- Nokia 603 已确认共享 `RFile` 的增长文件 `OpenFileL` 可以边下边播，不再出现初版
  `KErrInUse (-14)`；全程软解也不再因 controller 拒绝 `SetVideoEnabledL(false)` 而提前显示
  `SWERR`；
- Nokia 603 已确认 8 MiB 起播预缓冲、完整下载进度 UI、播放/解码独立列表与首次启动字体
  非阻塞方案的 1.1 最终包实际运行正常；
- GLES 三平面 YUV 路线真机成本过高，已经退出普通播放；
- Debug 和 Release 普通构建均已通过，1.0 Release 为 `sbs errors: 0`，32 条为既有 SDK/GCCE 警告。
- 用户已完成 1.2 Nokia 603 功能验收：首页卡片滚动优化、音量控制、相关推荐、动态正文与评论
  排版均确认可用；普通滚动弹幕从右边缘进入，顶部/底部固定弹幕保持居中。

原始测量、样本和否定实验均保存在[播放器研究索引](research/README_ZH.md)，不在本页展开。

## 待验证和未完成

- 1.0 Release 的 50 次播放器进入/退出压力循环；
- 正式 1.0 包中真实 header preflight 的 `ACCEPT → MMF` 与 `REJECT → FFMPEG` 真机标记；
- 独立不透明 soft surface 与 500 ms `PositionL()` 校准方案的专项真机遥测，包括
  `softSurfacePresented>0`、`overlayVideoDrawMs=0` 和 position cache 命中；
- 初代 Symbian³ 设备播放黑屏 bug 的定位和修复；修复后恢复原版 Symbian³、Anna 与
  N8 / E7 / X7 / C7 的统一 SIS 公测；
- 更多机型、H.264 profile/level、分辨率、码率和 CDN 组合的兼容矩阵；
- ref7 通用特征补丁在 700/701/808、N8/C7/E7/X7 等静态候选上的独立真机资格测试；
- 内置软件解码失败或性能不足时的明确外部播放器交接；
- 2026-09-01 第二轮 CODA 已把直播边界定死：首条 FLV 下载到 8,432,361 字节时，
  共享 `RFile` 句柄以 `share-read-write 0` 成功交给 MMF，但本地 `.flv` 的
  `NATIVE_MMF_OPEN_COMPLETE` 仍返回 `KErrNotSupported (-5)`。这证明 Nokia 603 的 MMF 既不能
  直接打开远程 FLV/HLS，也不能打开本地增长 FLV；不再继续调 MIME、CDN 或起播阈值。
  1.2.0 候选代码已改为手机本地增量 FLV 解复用：H.264 转 Annex-B 后交给既有 FFmpeg
  视频路径，AAC 转 ADTS 后写入共享增长文件，仍由 MMF 播放并提供主时钟。解析器有单 tag
  4 MiB、未消费数据 8 MiB 和视频待处理 900 AU 上限，且 CDN 只单向重试。GCCE Debug/Release
  均为 `sbs errors: 0`；共享增长 ADTS 的 MMF 接受与持续读取仍须真机验证，直播弹幕仍未实现；
- 软件播放性能继续提升到可接受的稳定帧率。

这些项目的顺序和验收条件见[路线图](ROADMAP_ZH.md)。

## 已知限制

- ARM1176 上的软件解码不是满帧路径，不同码流会有明显性能差异；
- 当前只把 Nokia 603 / Belle 视为完整设备基线；700 / 701 / 808 保持支持目标，但不能把
  未收集的独立样本写成真机通过；
- Nokia N8 / E7 / X7 / C7 等初代 Symbian³ 设备的播放黑屏 bug 尚未修复，1.2 暂不支持；
- ref7 补丁需要深度破解、只能手动启用；目前只有 Nokia 603 SW113 真机通过，其他固件即使特征
  唯一命中也不构成兼容或安全保证；
- 直播远程 `OpenUrlL()` 和本地增长 FLV `OpenFileL()` 都已由 Nokia 603 CODA 日志确认为
  `KErrNotSupported (-5)`；手机本地解复用路径尚未真机验收，直播不属于当前正式发布稳定性承诺；
- 外部播放器回退尚未形成正式产品闭环；
- Bilibili API、登录和媒体 URL 会受服务端变化影响；
- 1.1.0 已修复首次启动同步加载完整字体导致的长时间黑屏；回退到 1.0.0 时仍需参考
  [故障排查](user/TROUBLESHOOTING_ZH.md)中的旧版处理方法。

## 当前事实的维护位置

| 事实 | 维护位置 |
|---|---|
| 安装包和签名 | `docs/releases/RELEASE_1.2.0_ZH.md` |
| 真机通过/失败 | `docs/reference/DEVICE_TEST_MATRIX.md` |
| 工具链版本 | `docs/reference/TOOLCHAIN_REPORT.md` |
| 当前播放结构 | `docs/developer/PLAYBACK_ARCHITECTURE_ZH.md` |
| ref7 研究结论与可选补丁 | `docs/research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md`、`symbian/patches/h264-ref7/` |
| 工作优先级 | `docs/ROADMAP_ZH.md` |

本页只汇总这些来源，不复制校验值、完整测试表或实验日志。
