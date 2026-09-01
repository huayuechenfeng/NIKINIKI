# Direct DevVideo H.264 capability probe

> 状态：Archived research probe；Nokia 603 R/D/WP、SPS-fake A/B、Header/Submit split 与
> original-SPS patch 对照均已完成；真机事实只见 `docs/reference/DEVICE_TEST_MATRIX.md`
>
> 研究 decoder：`0x10204C21`
>
> probe UID：`0xE000B11D`

`cases.ini` 的 `[General]` 按 Qt 4.7 `QSettings` 规则作为根区段读取；不得再通过
`beginGroup("General")` 访问，否则会被解释成 `%General` 并导致元数据为空。

这是 H.264 硬件能力研究支线的独立真机探针。它不链接或启动 NIKINIKI 正式播放器，不使用
Qt Mobility Multimedia、`CVideoPlayerUtility2`、FFmpeg、网络、音频或正式播放路由。

研究已按 H1 结题；本 target 只为复现历史证据保留，不进入普通或发布构建。最终结论见
`docs/research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md`。

## 隔离边界

- 独立目录、target、可执行文件、UID 和 SIS；
- `.pro` 只编译本目录的 `main.cpp`；
- 不被 `symbian/app/wiliwili_symbian.pro` 或 `symbian/Build-App.ps1` 引用；
- 输入依次检查 `F:/Data/NIKINIKI/hwcap/`、`E:/Data/NIKINIKI/hwcap/` 和同路径的 `C:` 版本；
- 结果只写入输入目录下的 `results/<timestamp>/`；
- 不修改测试流、ROM、DLL、ECom registration 或任何播放器设置。

运行 `tools/research/h264_hwcap/Test-ResearchIsolation.ps1` 可静态检查以上代码边界。

## 探针矩阵

首轮固定运行：

```text
REF4 -> REF1 -> REF2 -> REF3 -> REF5 -> REF6 -> REF7 -> REF8
```

REF4 是已知自然样本边界的正控制。每个 case 都重新创建一套 `CMMFDevVideoPlay`，固定 decoder UID，
并逐阶段记录：

```text
file size / SHA-256 / AUD split
decoder info / accelerated flag
SelectDecoderL / SetInputFormatL
GetHeaderInformationL / ConfigureDecoderL
GetOutputFormatListL / SetOutputFormatL
memory destination / Initialize callback
GetBufferL / WriteCodedDataL <= 100 AU / InputEnd
First Picture / raw frame CRC / loss / fatal / StreamEnd / counters
```

前 3 张 memory picture 会原样保存。当前版本把 corruption 写为 `UNKNOWN`；只有把这些数据带回 PC、
按输出格式和 stride 规范化并与 golden frame 对照后，才能改成 MATCH 或 CORRUPT。
`Accelerated()` 与所选 UID 只证明 Broadcom HwDevice 路径，不自动证明压缩数据已经到达物理芯片。

第二轮使用 schema 2 的动态顺序：

```text
R6 -> R7 -> D4 -> D5 -> D6 -> D7 -> D8 ->
WP4_OFF -> WP4_ON -> WP6_OFF -> WP6_ON
```

每条第二轮输入固定为 100 AU，并在最后一个 AU 中包含已纳入文件 hash 的标准 H.264 EOS NAL。探针
会在调用 DevVideo 前重新核对 EOS 数量；不在运行时修改测试流。

Qt 4.7 可能把 INI 中逗号分隔的 `order` 解析为 `QStringList`。schema 2 先按 string-list 读取，再检查
`case0..caseN`，最后只对名称完全匹配的 `H264_BOUNDARY_R2` 使用固定顺序回退。

第三轮 schema 3 是与合法矩阵分开的 original/all-SPS-fake 诊断 A/B：

```text
ORIGINAL_R7 -> FAKE_REF3
```

两条流使用同一套状态机和 decoder UID。`ORIGINAL_R7` 是合法正控制；`FAKE_REF3` 只把每一个 SPS 的
`max_num_ref_frames` 从 7 改为 3，仍保留原始 PPS、slice/reference graph、declared DPB=7 和全部
非 SPS NAL。探针允许该 case 的 `runtime_decode_eligible=false`，仅表示故意不一致的诊断输入可以运行；
绝不能把 Header 通过、出帧或花屏解释为合法 ref=7 硬解。

第四轮 schema 4 根据 RM-779 驱动中已经静态定位的 `max_num_ref_frames > 6` host-side admission gate，
把 `GetHeaderInformationL/ConfigureDecoderL` 使用的 header AU 与 Initialize 后 `WriteCodedDataL` 提交的
完整 AU 序列分开：

```text
R6_NATIVE
R7_NATIVE
FAKE_HEADER_ORIGINAL_R7
FAKE_HEADER_FAKE_REF3
```

关键 case 只在 Header/Configure 阶段使用 `FAKE_REF3`；进入 Initialize 后，100 个 AU 全部来自未经修改、
PC 合法且实际使用第 7 reference slot 的 `ORIGINAL_R7`。探针分别校验 admission file 与 submit file 的
大小和 SHA-256，并在 summary 中同时记录 `header_sha/header_refs/header_source/submit_source`。因此它能
判断原始 R7 是否越过已定位的 Header gate；仍不能仅凭 `WriteCodedDataL()` 成功或损坏画面断言物理芯片
已经收到数据。正式播放器和 ROM 不参与该实验。

## 构建

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-Probe.ps1 -Project devvideo-capability -Configuration release
```

正常输出位于 Git 忽略的：

```text
symbian/out/devvideo-capability-release/nikiniki_devvideo_capability_probe.sis
```

该构建不会编译、链接或打包主应用。

## 准备 Nokia 603 测试包

先用 `Generate-RefMatrix.ps1` 生成矩阵，再运行：

```powershell
.\tools\research\h264_hwcap\Prepare-Nokia603Bundle.ps1
```

默认输出到：

```text
.tmp/h264-hwcap/nokia603-bundle/
```

真机步骤：

1. 完全退出 NIKINIKI、系统播放器及其他可能占用视频 decoder 的程序；
2. 把 bundle 中 `Data` 目录的内容复制到存储卡，使 Nokia 603 上出现
   `F:\Data\NIKINIKI\hwcap\cases.ini`；若设备把存储卡映射为 `E:`，同路径也受支持；
3. 安装 bundle 中 `INSTALL` 目录的独立 probe SIS；
4. 启动 **NIKINIKI H264 HwCap Probe**，点击 **Run capability matrix**，等待当前 bundle 的 case 完成；
5. 把测试数据所在盘的 `Data\NIKINIKI\hwcap\results\` 下最新完整目录复制回 PC。

不要同时启动正式 NIKINIKI。若手机重启、panic 或 probe 被系统关闭，也保留已经写出的
`events.tsv` 和 raw frame；事件日志每一行都会立即 flush。

返回 PC 后使用以下命令核对输入 hash、阶段结果及可直接比较的紧凑 planar YUV420 raw frame：

```powershell
python .\tools\research\h264_hwcap\h264_matrix.py device `
    D:\returned\results\20260830-xxxxxx `
    --matrix .tmp\h264-hwcap\ref-matrix `
    --output D:\returned\device-analysis.json
```

分析器只输出逐 case 事实，`research_outcome` 固定保持 `UNCLASSIFIED`；H1/H2/H3 必须结合阶段日志和
下层证据人工判定。

第二轮改用：

```powershell
.\tools\research\h264_hwcap\Prepare-Nokia603BoundaryBundle.ps1
```

以及 `boundary-r2-matrix` 作为设备结果分析器的 `--matrix` 参数。

第三轮改用：

```powershell
.\tools\research\h264_hwcap\Prepare-Nokia603SpsFakeBundle.ps1
```

以及 `sps-fake-ab-r3` 作为设备结果分析器的 `--matrix` 参数。该 bundle 会醒目标记 fake 为
diagnostic-only，并保留 mutation report、独立 parser/`trace_headers` 与 PC reference-corruption 证据。

第四轮改用：

```powershell
.\tools\research\h264_hwcap\Prepare-Nokia603HeaderSubmitSplitBundle.ps1
```

默认 bundle 为 `.tmp/h264-hwcap/nokia603-header-submit-split-r4-bundle/`。返回结果后以 bundle 内
`PC_REFERENCE` 作为 `h264_matrix.py device --matrix` 参数；R4 不需要 Nokia 808 或 ROM patch。
