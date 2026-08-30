# 0.9 GLES YUV420 软解输出优化

> 文档状态：Historical experiment。当前普通输出为 native RGB565 surface；见
> `docs/developer/PLAYBACK_ARCHITECTURE_ZH.md`。

> 实现日期：2026-08-27  
> 状态：历史实验记录（2026-08-27 真机已验证但不适合发布）。三平面 GLES 上传约 216 ms/帧、QGLWidget 提交约 321 ms/帧，ping-pong 无改善；当前主线已恢复 CPU RGB565，本文件不再是 soft renderer 的执行基线。

## 1. 为什么改输出链路

此前 `RGB565_LUT2X2` 已把 640×360 的软件输出从约 5.2 fps 提高到约 11.4–12.0 fps，但真机日志仍显示每张图约 42 ms 用于应用侧 YUV420→RGB565。libavcodec 本身约 32–38 ms/解码图，CPU 色彩转换与解码串行叠加，成为明确的下一瓶颈。

本轮不修改 MMF、风险模板判定、MP4 Range、PTS、弹幕或持久窗口架构。目标仅是把 FFmpeg 已解码的 YUV420 平面直接交给已有 GLES2 上下文，让 VideoCore IV 在 fragment shader 中完成色彩转换。

## 2. 实现结构

- `FfmpegH264Decoder` 将 `AV_PIX_FMT_YUV420P/YUVJ420P` 按行复制成紧凑 Y/U/V 三平面；不持有 `AVFrame` 生命周期之外的指针；
- 队列仍最多缓存 6 张，并继续按 MMF AAC 时钟选择最新到期 PTS；
- 主应用原有、长生命周期的 `QGLWidget` 创建 3 张 GLES2 `GL_LUMINANCE` 纹理，使用 BT.709 limited/full-range shader 输出 RGB；
- 横屏通过纹理坐标做 90° 旋转，保持 AVKON/QGL 物理竖屏，不改变已经验证的窗口方向策略；
- FFmpeg 软解接管后只隐藏持久的播放器控制器及 MMF 视频子窗，不销毁它们，也不创建第二个 EGL surface；
- 弹幕和控制仍由独立的持久 ARGB 顶层窗口绘制在 GLES 视频之上，因此视频不能遮挡弹幕；
- 播放器控制器在 GLES 模式下被隐藏后，主 `QGLWidget` 会把按键事件转发给活跃播放会话，确保暂停、恢复、返回和菜单不因焦点切换失效；
- GLES shader/纹理在应用初始化阶段失败时，解码器启动前自动选择旧 RGB565 CPU 输出，不会把不兼容的 YUV 队列交给 QPainter。

新增主要日志：

```text
WW:GLES_YUV_READY 3PLANE_LUMINANCE_BT709
WW:FFMPEG_SOFT_BEGIN ... GLES_YUV420
WW:FFMPEG_SOFT_READY ... ARM11_GENERIC_H264_GLES_YUV420
WW:FFMPEG_GLES_SURFACE_ACTIVE ...
WW:GLES_YUV_FIRST_FRAME 640 360 ...
WW:GLES_YUV_PROGRESS ...
```

若出现 `WW:GLES_YUV_UNAVAILABLE`，应看到 READY 回退到 `ARM11_GENERIC_H264_RGB565_LUT2X2`。`WW:GLES_YUV_UPLOAD_ERROR` 表示 shader 已建立但纹理上传失败，需要保留完整错误码和前后日志。

## 3. ARM11/FFmpeg 策略

GCCE 生成配置已核对为：

```text
-mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=softfp
-fomit-frame-pointer -Os
HAVE_ARMV6=yes HAVE_VFP=yes
```

Nokia 603 没有 NEON，也没有 ARMv6T2。PPSSPP-FFmpeg 3.0.2 的 H.264 qpel/chroma/prediction/weighted/deblock/IDCT 主要 ARM 快路径要求 NEON，CABAC inline 快路要求 ARMv6T2；因此不能把“启用 ARM assembly”描述成完整 ARM11 H.264 汇编优化。

本轮采用的安全策略：

- 保持单线程解码和 `AV_CODEC_FLAG2_FAST`；
- 软件回退常态设置 `skip_loop_filter=AVDISCARD_ALL`，牺牲少量去块质量换取 ARM1176 上的稳定预算；
- 落后音频时仍只额外跳过非参考帧，不使用已由真机否决的 180 ms presentation-drop；
- 输出/弹幕计时从 50 ms 调整为 40 ms，理论显示上限从 20 fps 提高到 25 fps；
- 不恢复全库 `-O2/-O3`：前者曾越过 Symbian 固定 `.data=0x400000`，后者曾触发 GCCE 内部错误。

## 4. PotatoStream 与官方 FFmpeg 的借鉴边界

[PotatoStream](https://github.com/PainDe0Mie/PotatoStream) 的可迁移思想是：避免 swscale/CPU 颜色转换、单线程低缓冲、关闭环路滤波、落后时有控制地跳帧，并让专用图形/颜色硬件接管 YUV 输出。它的 Y2RU、`-mfloat-abi=hard`、`-mtune=mpcore` 和 3DS 线程/内存 API 都是平台专用实现，不能移植到 Symbian/GCCE。本工程没有复制其 GPL 源码，只独立实现了相同层次的管线取舍。

FFmpeg 仍使用现有 H.264-only 3.0.2 源码裁剪库，并按照其公开的 `AVFrame::data[]/linesize[]` 平面接口复制解码结果。没有链接社区预编译库，也没有删除 EABI attribute。

## 5. 构建与段检查

Debug、Release 都由 Qt 4.7.4 / GCCE 4.4.1 / SBS 2.17.0 完成，均为 `sbs errors: 0`。

Release 链接布局：

```text
.text      0x00008000 size 0x0011fc70
.rodata    0x00129ef0 size 0x001f0957
.ARM.exidx 0x003291fc size 0x00001d50
.data      0x00400000 size 0x00000150
```

代码/只读段未与固定 `.data` 重叠。

当前真机候选：

| 配置 | 文件 | 大小 | SHA-256 |
|---|---|---:|---|
| Release | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_release_full_currentcert.sis` | 9,703,416 | `17CEE045699489B08143793CF3F70A37DF06EEC74BDDAB24CC75E622B60E8F04` |
| Debug | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_debug_full_currentcert.sis` | 9,687,100 | `70ABE6972E52AD533A82A14520B425D6FAF5EB2C344C76AE178C3CE435DC3698` |

`signsis -o` 已确认当前 Qt Development Frameworks 证书有效期为 2026-08-24 至 2036-08-21；`dumpsis -l` 已确认 EXE capabilities 为 `NetworkServices ReadUserData`。

## 6. 真机验收

1. 启动后确认出现 `WW:GLES_YUV_READY`，且应用首页、搜索和详情页无 GL 回归；
2. 已知正常视频应保持 MMF，并输出 `PROFILE_SKIP`，不能进入 `FFMPEG_GLES_SURFACE_ACTIVE`；
3. 已知 7/4/7/weighted 故障流应依次出现 `GLES_YUV420` READY、surface active 和 first frame；
4. 检查画面方向、上下翻转、BT.709 色彩、比例和黑边；
5. 连续播放至少 30 秒，记录 `FFMPEG_SOFT_TIMING` 的 picture/wall/decode/output 数字、体感帧率、AAC 连续性与音画同步；
6. 打开弹幕并操作暂停、恢复、拖动和清晰度菜单，确认 ARGB 覆盖仍位于视频上方；
7. 退出并重复进入正常流和软解流至少 10 次，再执行既定 50 次 Release 压力门槛。

只有真机日志和观感同时通过，才能把这一优化写成实时性能成果。当前仅能确认实现、编译、链接、打包和静态检查通过。

## 7. 2026-08-27：Range 探测与 MMF 并发的修复

首轮 GLES-YUV Debug 真机日志没有进入软解：虽然启动时出现
`WW:GLES_YUV_READY`，但没有 `FFMPEG_SOFT_BEGIN`、
`FFMPEG_GLES_SURFACE_ACTIVE` 或 `GLES_YUV_FIRST_FRAME`。实际顺序是
`DEVVIDEO_RANGE_HEADER_BEGIN` 后十秒仍为 HTTP status `0`、接收字节
`0`，最终以 `DEVVIDEO_RANGE_TIMEOUT 1 0 0 20 20` 终止；随后 MMF 返回
`KErrMMPartialPlayback (-12017)` 并只保留自身播放路径。

这不是 GLES 上传或 libavcodec 性能数据，根因是风险分类所需的 Qt
Network Range 请求与 MMF 对同一 CDN URL 的打开并发，导致软解路由在
首帧之前失效。

当前源码已改为仅在 Symbian + `WILIWILI_ENABLE_FFMPEG_SOFT_DECODER` 的
首个 MP4 来源上执行预取：

1. 先取 MP4 header；安全模板立即打开 MMF；
2. 风险模板在 MMF 打开前再取首批 Annex-B access units；
3. 首批数据到位后再启动 MMF 以准备 AAC，并沿用原有 LoadedMedia 等待
   和 FFmpeg/GLES 接管流程；
4. header、Range、解析或尺寸检查失败时显式启动 MMF，不能留下空播放页。

新增判读标记：

```text
WW:PLAYER_SOURCE_DEFER_MMF
WW:PLAYER_SOURCE_DEFER_MMF_DONE soft-prefetch-ready
WW:PLAYER_SOURCE soft-prefetch-ready ...
WW:FFMPEG_SOFT_BEGIN ... GLES_YUV420
WW:FFMPEG_GLES_SURFACE_ACTIVE ...
WW:GLES_YUV_FIRST_FRAME ...
```

该修复已通过 2026-08-27 的 GCCE Debug 编译；尚待 Nokia 603 验证。若仍
出现 `probe-range-timeout`，日志现在会明确显示 MMF 回退原因，不能再把
该会话误判为 GLES 或软解性能失败。
