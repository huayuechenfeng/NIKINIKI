# H.264 hardware-capability 测试流工具

> 状态：Archived reproducibility tool
> 适用范围：已结题的 NIKINIKI Direct DevVideo H.264 ref7 能力研究

本目录只生成 PC 侧合法 H.264 能力矩阵，以及与合法矩阵明确隔离的故意不一致诊断流和设备 probe
输入 manifest。它不修改正式播放器、ROM、网络请求或 MMF/FFmpeg 选路。

## 当前实现

`Generate-RefMatrix.ps1` 使用同一个确定性 640×360/30 fps YUV420P 素材，通过 FFmpeg `libx264`
生成 REF1–REF8：

- High Profile、Level 3.0；
- 10 秒/300 帧；
- GOP 60、scenecut off；
- B-frame、B-pyramid、weighted P/B 全部关闭；
- H.264 Annex-B 每个 access unit 带 AUD；
- 同一 H.264 只 remux 为 MP4，不重新编码。

`h264_matrix.py` 不依赖 FFmpeg 的 stream metadata 来认定关键字段。它直接解析 Annex-B SPS/PPS，
记录 refs、DPB、reorder、weighted P/B、尺寸、AUD 和 slice 类型，并调用 FFmpeg 做严格解码，生成
逐帧 Y/U/V CRC32 和整段 decoded-YUV SHA-256。生成器还会把独立解析结果逐字段与 FFmpeg
`trace_headers` 交叉核对。

`Generate-BoundaryMatrix.ps1` 生成第二轮 100 帧边界拆分矩阵：

- `R6/R7`：显式 EOS 下复测首轮 joint cliff；
- `D4..D8`：ref 固定为 4，只合法上调 VUI `max_dec_frame_buffering`；
- `WP4_OFF/ON` 与 `WP6_OFF/ON`：B/reorder 固定为 0，只改变 weighted P；
- 每条流包含一个标准 H.264 `end_of_stream` NAL；
- D 组强制证明所有非 SPS NAL 相同，WP-on 强制 x264 实际使用 weighted P frame。

SPS rewriter 会重新生成 Exp-Golomb、RBSP trailing bits 和 emulation-prevention bytes，只允许上调
DPB 声明。D 组保持实际 slice/reference graph 不变，属于合法能力矩阵，不是 SPS fake。

`Generate-SpsFakeAb.ps1` 从已经通过 PC 合法性门且确实使用第 7 reference slot 的 R7 生成独立诊断 A/B：

- `ORIGINAL_R7` 保持合法原流不变；
- `FAKE_REF3` 重写每一个 SPS 的 `max_num_ref_frames: 7→3`；
- PPS、slice、AU/NAL 顺序和每个非 SPS NAL 必须逐字节相同；
- fake 在 PC 上必须出现 `Missing reference picture`，且 decoded CRC 不得与合法 golden 相同；
- fake 明确标记 `legal_stream=false`、`runtime_decode_eligible=false`，不进入合法 capability matrix。

`Inspect-IveAvcAdmission.ps1` 对 RM-779 SW113 的精确扩展 XIP image 做可重复的只读审计。它锁定输入
大小与 SHA-256，核对 `0x10204C21` 工厂、AVCParser RTTI/vtable、SPS `max_num_ref_frames` 保存点、
refs=6 admission 分支和 `KErrNotSupported` Leave，并确认该 Header 路径在 gate 前没有调用已识别的
`rcam`/IVE policy-client stub。脚本只反汇编数据，不加载或执行固件代码。

## 运行

依赖 PowerShell、Python 3.10+，以及同时包含 `libx264` encoder 和 `trace_headers` bitstream filter 的
FFmpeg/FFprobe。实际版本会写进每次输出的 manifest。

```powershell
.\tools\research\h264_hwcap\Generate-RefMatrix.ps1

.\tools\research\h264_hwcap\Generate-BoundaryMatrix.ps1

.\tools\research\h264_hwcap\Generate-SpsFakeAb.ps1
```

默认输出到已被 Git 忽略的：

```text
.tmp/h264-hwcap/ref-matrix/
.tmp/h264-hwcap/boundary-r2-matrix/
.tmp/h264-hwcap/sps-fake-ab-r3/
```

可用参数：

```powershell
.\tools\research\h264_hwcap\Generate-RefMatrix.ps1 `
    -OutputDirectory D:\nikiniki-hwcap\ref-matrix `
    -DurationSeconds 10 `
    -Ffmpeg C:\path\to\ffmpeg.exe `
    -Ffprobe C:\path\to\ffprobe.exe `
    -Python C:\path\to\python.exe
```

输出目录非空时脚本默认停止；`-Force` 只覆盖工具命名的生成文件，不递归删除目录或其中的未知文件。

## 输出

```text
base_640x360_30.yuv
REF1.h264 ... REF8.h264
REF1.mp4  ... REF8.mp4
cases.ini
manifest.json
logs/
metadata/
```

- `manifest.json`：工具版本、固定参数、完整命令、SHA-256、解析字段和 runtime eligibility；
- `cases.ini`：Qt 4.7.4/Symbian probe 可直接读取的精简输入；
- `metadata/*-inspect.json`：独立 SPS/PPS/NAL 解析结果；
- `metadata/*-golden.json`：逐帧 CRC32 与 decoded-YUV aggregate SHA-256；设备分析器会把
  `events.tsv` 中每一条 `PICTURE` CRC 与对应 golden 帧逐项比较，并另行复核保存的前三张 raw frame；
- `logs/*-encode.log`：包括 libx264 reference usage summary。
- `logs/*-trace-headers.log`：FFmpeg 对 SPS/PPS 关键字段的独立交叉检查。

若 x264 summary 没有证明最高声明 reference slot 被实际使用，该 case 仍可测试 Header admission，
但 `runtime_decode_eligible=false`，不能用于证明高-ref reference graph 已经在硬件中正确运行。

第二轮 Nokia 603 包由以下命令生成：

```powershell
.\tools\research\h264_hwcap\Prepare-Nokia603BoundaryBundle.ps1
```

默认输出到 `.tmp/h264-hwcap/nokia603-boundary-r2-bundle/`。

original/all-SPS-fake R3 诊断包由以下命令生成：

```powershell
.\tools\research\h264_hwcap\Prepare-Nokia603SpsFakeBundle.ps1
```

默认输出到 `.tmp/h264-hwcap/nokia603-sps-fake-ab-r3-bundle/`。该包中的 `FAKE_REF3` 故意违反真实
reference graph 与 SPS 声明的一致性，只用于观察 Direct DevVideo 最早通过/失败阶段。

根据静态 gate 生成 Header/Submit split R4 包：

```powershell
.\tools\research\h264_hwcap\Prepare-Nokia603HeaderSubmitSplitBundle.ps1
```

R4 复用已经锁定的合法 R6、合法 ORIGINAL_R7 与诊断 FAKE_REF3，不重新编码。关键 case 用 fake AU
完成 Header/Configure，但 Initialize 后只提交未经修改的 ORIGINAL_R7 AU；设备端同时核对两个来源的
SHA-256。该实验不修改 ROM，也不进入正式播放路由。

Nokia 603 的只读 ECom inventory 包由以下命令生成：

```powershell
.\tools\research\h264_hwcap\Prepare-Nokia603EcomAuditBundle.ps1
```

默认输出到 `.tmp/h264-hwcap/nokia603-ecom-audit-r3-bundle/`。该包不含媒体，不创建 decoder；设备端
枚举 decoder interface `0x101FB4BE` 和 MDF Processing Unit interface `0x10273789`，按公开 ECom
v1/v2/v3 格式结构化解析 loose ROM plugin RSC，并以 ECom server 相同的 `RResourceArchive` 调用只读
审计 `Z:\private\10009D8F\` 下的 `ecom` SPI archive set。archive 命中时记录 implementation、
`dll_uid` 与 ECom 实际使用的 DLL basename；若平台安全拒绝读取私有 archive 或 `Z:\sys\bin`，结果
保留为拒绝事实，不请求 `AllFiles/TCB`，也不绕过。

## 结题状态

- Header/Submit split R4 已在 Nokia 603 输出 99/99 ORIGINAL_R7 golden 匹配帧；
- original-SPS admission patch 随后取得同样的 99/99 正确结果；
- declared-reorder、weighted B、B-frame/reorder 和 B-pyramid 扩展组没有作为核心结题前提继续生成，
  后续只属于补丁资格测试。

R/D/WP 合法矩阵、original/all-SPS-fake A/B 与 Header/Submit split 的 Nokia 603 真机事实只见
`docs/reference/DEVICE_TEST_MATRIX.md`；独立 probe 和 bundle 使用说明见
[symbian/probes/devvideo-capability](../../../symbian/probes/devvideo-capability/README_ZH.md)。
ECom inventory probe 的字段和安全边界见
[symbian/probes/devvideo-ecom-audit](../../../symbian/probes/devvideo-ecom-audit/README_ZH.md)。

最终结论见
[H.264 ref7 硬件解码结题报告](../../../docs/research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)；
[原研究计划](../../../docs/research/player/post-1.0/H264_HARDWARE_DECODE_RESEARCH_PLAN_ZH.md)只作归档。
