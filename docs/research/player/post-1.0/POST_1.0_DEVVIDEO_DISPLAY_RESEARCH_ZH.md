# 1.0 后研究档案：DevVideo 后处理与 Direct Screen Access 显示

> 建档日期：2026-08-29  
> 状态：1.0 前封存。不得据此改动正式播放器、重新启用 `devvideodirectprobe1`，或接入 FFmpeg 直显。  
> 产品基线不变：MMF 硬解 → 本机 FFmpeg + CPU RGB565 → 明确外部播放器回退。

## 1. 已完成的第一包与明确结论

`devvideodirectprobe1` 在 Nokia 603 的已验证原生 640×360 窗口状态机上执行了 Phase A：底层 `SoftVideoSurfaceWidget/RWindow` 启动 `CDirectScreenAccess::NewL(..., ETrue)`，顶层正式 ARGB overlay 依次隐藏、显示、raise、持续 UI 重绘和模拟弹幕绘制。

| 检查 | 真机结果 |
|---|---|
| DSA 构造 / `StartL()` | 成功，`dsaError=0` |
| overlay 隐藏采样 | `DrawingRegion`: 0 rect / 0 area |
| overlay 显示、raise、动态重绘后 | 一次 `AbortNow` / `Restart`；Restart 返回 0，但新 `DrawingRegion` 仍为 0 rect / 0 area |
| Phase A | `DIRECT_PHASE_A NO hiddenArea 0 visibleArea 0 minimumArea 184320` |
| Phase B | 未创建 `CMMFDevVideoPlay`，未选 PP，未提交 YUV |
| 清理 | 自动恢复竖屏主页，持久播放器对象保留 |

结论是严格限定的：**全屏透明 ARGB 覆盖窗与底层 DSA 直显在该 Window Server 组合中不能共存。** 这不是“弹幕绘制太慢”或某个 YUV coefficient 的问题。Window Server 按窗口遮挡而非 per-pixel alpha 计算 DSA DrawingRegion；即使 overlay 大部分像素透明，它仍遮住整个底层 DSA 窗口。

不能从这次结果推出“post-processor 无法把 YUV 显示到屏幕”。PP `0x2003162A` 与 `0x10273417` 的 `SelectPostProcessorL` / `SetInputFormatL` / `StartDirectScreenAccessL` 依照硬门完全没有调用。

## 2. 1.0 后唯一仍有价值的显示实验

如果 1.0 稳定后仍要重新评估，第一步不是 FFmpeg，也不是重新显示弹幕，而是单独 UID 的 `DSA + 有界控制条` 静态探针：

```text
640×360 DSA video RWindow
        +
独立、固定、非全屏、无 alpha 的 640×46 底部控制条
        +
视频 RWindow 自己接收其余触摸
```

必须避免以下任何一种东西：全屏透明点击层、全屏 ARGB canvas、浮动弹幕、动画覆盖窗、跨视频区的质量菜单。仅“不画弹幕”而保留全屏 overlay 没有价值，仍会产生同样的零 DrawingRegion。

### D1：UI/DSA 几何门

1. 控制条是几何范围仅限底部 46 像素的独立原生窗口，且为不透明；
2. video surface 自身处理视频区的 tap/drag，不能靠全屏 touch catcher；
3. 记录隐藏控制条、固定控制条、连续按键/触摸下的 `DrawingRegion`、Abort/Restart；
4. 只有 control bar 存在时仍稳定保留大部分视频区（预期至少约 `640×314`）且没有 restart storm，D1 才通过。

D1 失败即永久放弃 Symbian DSA 播放器路线，不再制作 FFmpeg DSA 包。

### D2：静态 PP 显示门

仅 D1 通过才重复当前第一包原定的 Phase B：优先 PP `0x2003162A`，明确失败后才试 `0x10273417`；最多三种 planar YUV420 描述；三固定 345600-byte 槽；黑、白、色条和扫描线持续十秒。仍不接网络、FFmpeg、MMF AAC 或播放器状态机。

必须同时取得 PP callback/telemetry 与人工可见的正确图案。仅有 `WritePictureL()==0` 不能算成功。

### D3：最小媒体接入门

只有 D2 通过才允许一个独立实验把 FFmpeg `AVFrame` 接入三槽池。Y/U/V 必须按 `linesize[]` 逐行复制到紧凑 640×360 槽；任何一帧返回前不得重写槽。该实验仍先不接弹幕、复杂 UI、MMF AAC 同步或正式导航。

## 3. 与现有产品路线的关系

- 当前 CPU RGB565 opaque surface + 单 ARGB UI/弹幕覆盖层是 1.0 正式基线；
- DSA 研究不能以删除弹幕为前提偷偷改变 1.0 产品承诺；
- 即使 D1/D2/D3 全部成功，也需要另行评估“无弹幕、受限 UI”是否值得成为可选播放模式；
- DSA 成功并不自动说明 H.264 `ref=7` 硬解可行。解码器接受范围和显示后处理是两条独立问题。

## 4. 归档证据与安全边界

- 第一包源码：`devvideo_direct_probe.*`，仅由 `CONFIG+=devvideodirectprobe1` 编入；
- 真机关键结果：`WW:DIRECT_PHASE_A NO`、`WW:DIRECT_RESULT phaseA NO phaseB NO`；
- 现有包仅保留为历史诊断，不作为 1.0 安装或回归包；
- 将来实验必须使用独立 UID/包名、独立日志目录和可恢复设备，不能覆盖正式应用资源或用户数据。
