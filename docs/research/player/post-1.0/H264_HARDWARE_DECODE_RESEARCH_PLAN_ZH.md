# NIKINIKI H.264 硬件解码能力研究主线

> 状态：Archived / concluded 2026-09-01
> 形成日期：2026-08-30
> 适用范围：1.0.0 发布后至 2.0 期间的独立研究支线
> 本页职责：归档实验顺序、探针契约、证据门槛和当时的三种终局；不描述当前产品能力

本支线已经按 H1 结题。最终结论、两条成功路径、通用补丁和产品边界统一见
[H.264 ref7 硬件解码结题报告](../H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)。下文保留当时的
执行状态和阶段门，不再作为路线图。

## 1. 研究目标与产品边界

本支线只回答两个问题：

1. Nokia 603 上 Broadcom H.264 decoder implementation UID `0x10204C21` 对
   reference、DPB、weighted prediction、reorder 和 B-pyramid 的真实准入与解码边界是什么；
2. 合法 7-ref 码流是在 Nokia/Broadcom wrapper、可能的 MDF/OMX adapter、firmware，还是
   物理多媒体芯片处失败。

研究可以用三种结果中的任何一种结束：

| 终局 | 必须证明的内容 | 产品含义 |
|---|---|---|
| H1：合法 7-ref 硬解成功 | 原始合法码流未经 SPS 欺骗，进入硬件路径并连续输出与 golden decode 一致的帧 | 评估作为 VideoCore IV 可选硬解扩展进入后续产品化 |
| H2：已经到达芯片但硬件不能正确解码 | wrapper/admission 已通过，且有下层提交证据；多个合法高-ref 控制流仍稳定报错或输出损坏 | 记录真实 firmware/硬件边界，保留 FFmpeg fallback |
| H3：在可审计手段内始终无法把合法 7-ref 送到芯片 | 已完成合法矩阵、框架调用链、ECom 定位、明确接口与跨固件差异审计，仍只在芯片前被拒绝 | 记录可重复的软件/固件准入边界，停止盲目绕过 |

`WriteCodedDataL()` 返回成功、Initialize 成功或 fake SPS 后出现画面，都只能证明相应 API 阶段被
越过。只有 decoder output/counter 加下层 trace，或等价的 firmware/driver 证据，才能写成“已送到
物理芯片”。无法区分 wrapper 与芯片时，状态必须写成 `VENDOR_PATH_REACHED / CHIP_HANDOFF_UNPROVEN`，
不能提前归入 H2。

本支线不改变当前 MMF/FFmpeg 路由，不恢复系统 ARM decoder，不重新尝试 Q6，不接网络，不以
SPS fake 作为播放方案，也不在没有明确符号、UID 或调用契约时调用 CustomInterface 或修改 ROM。

## 2. 本轮只读审计结论

### 2.1 已有证据

以下是实验输入事实，不是新推导；完整日志与边界仍由对应证据文档维护：

- [H.264 兼容性证据](../PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md)记录了同一 Direct DevVideo 输入契约下，
  自然 4-ref/DPB4/weighted-off 流 Header 成功，而自然 7-ref/DPB7/weighted 流 Header 稳定返回
  `KErrNotSupported (-5)`；
- 同一文档记录系统 ARM decoder UID `0x102073EF` 虽返回 Header，但尺寸为 0×0，随后
  `ConfigureDecoderL()` 返回 `-5`，没有 Initialize 或首帧；
- [reference/DPB 档案](POST_1.0_H264_REFERENCE_DPB_RESEARCH_ZH.md)记录 full-SPS fake 可令系统播放器
  进入视频路径，但产生 missing-reference/MMCO 一致的严重花屏；
- [BCM2763/HwDevice 档案](POST_1.0_BCM2763_HWDEVICE_RESEARCH_ZH.md)保存 decoder UID、名称和未来固件
  对照边界。

自然样本本身仍不能解释成“ref=7 单独导致失败”。但后续合法 R/D/WP 矩阵与 schema-3 Direct A/B
已经独立封装了脚本、manifest、输入 hash、字段级 SPS diff、PC decode 和真机阶段/CRC：在固定
640×360、B=0、reorder=0、weighted-off 下，ref=1..6 正确，ref=7/8 只在 Header 返回 `-5`；只把
全部 SPS 的 `max_num_ref_frames` 从 7 改成 3 又会越过 Header。因而当前“ref=6→7 admission cliff”
已经是项目复现事实，而不是由自然样本混合变量推测的结论。

### 2.2 现有代码能复用什么

- 正式 [video_playback_backend.cpp](../../../../symbian/source/platform/video_playback_backend.cpp) 中的
  header preflight 只执行 New、Select、SetInputFormat 和 GetHeader，不 Configure/Initialize/提交帧；
- 同一文件仍保留由编译宏隔离的旧 memory-output 骨架，包含 Configure、输出格式选择、Initialize、
  `GetBufferL/WriteCodedDataL`、`NextPictureL/ReturnPicture` 和 picture/loss counters；
- [mp4_avc_probe_reader.cpp](../../../../symbian/source/platform/mp4_avc_probe_reader.cpp)已经能从 MP4
  解析 profile、level、`max_num_ref_frames`、`num_reorder_frames`、`max_dec_frame_buffering`、
  weighted P/B，并按 sample 生成 Annex-B access unit；
- 现有 `devvideo_direct_probe.*` 研究的是 post-processor/DSA 显示，Phase A 在创建 DevVideo 前已经
  止损，不是本支线所需的 H.264 decoder probe。

新探针应抽取这些已验证的输入和 memory-output 契约，放进独立诊断 target；不得重新打开
`WILIWILI_ENABLE_UNSUPPORTED_BCM_DEVVIDEO`，也不得让探针通过正式播放器入口运行。

## 3. Symbian^3 开源 DevVideo 调用链

本轮核对了 SymbianSource `oss.FCL.sf.os.mm` 的 commit
[`ebaa78373866f90dbf706e8d4eeb59ff65f1e107`](https://github.com/SymbianSource/oss.FCL.sf.os.mm/tree/ebaa78373866f90dbf706e8d4eeb59ff65f1e107)。
该版本实际文件名是 `AVC.h` 和 `DevVideoPluginInterfaceUIDs.hrh`，不是早期资料中的 `AVC0.h` 和
`DevVideoPluginInterfaceUIDs.rh`。

已证实的调用链为：

```text
CMMFDevVideoPlay::GetDecoderListL()
  -> MmPluginUtils::FindImplementationsL(KUidDevVideoDecoderHwDevice)
     interface UID = 0x101FB4BE

CMMFDevVideoPlay::SelectDecoderL(0x10204C21)
  -> CreateDecoderL()
     -> direct CMMFVideoDecodeHwDevice::NewL(implementation UID)
        -> REComSession::CreateImplementationL()
     或：MDF Processing Unit -> generic HwDevice adapter

CMMFDevVideoPlay::GetHeaderInformationL(...)
  -> VideoDecodeHwDevice().GetHeaderInformationL(...)
     -> 0x10204C21 implementation / adapter / lower component
```

关键源码：

- [`devvideoplay.cpp`](https://github.com/SymbianSource/oss.FCL.sf.os.mm/blob/ebaa78373866f90dbf706e8d4eeb59ff65f1e107/mmhais/videohai/devvideo/src/DevVideo/devvideoplay.cpp#L755-L871)
  显示枚举、选择和 Header 的直接分发；
- [`videoplayhwdevice.h`](https://github.com/SymbianSource/oss.FCL.sf.os.mm/blob/ebaa78373866f90dbf706e8d4eeb59ff65f1e107/mmhais/videohai/devvideo/inc/videoplayhwdevice.h#L546-L610)
  把 decoder Header 定义成 `CMMFVideoDecodeHwDevice` 的纯虚方法；
- [`devvideointernal.cpp`](https://github.com/SymbianSource/oss.FCL.sf.os.mm/blob/ebaa78373866f90dbf706e8d4eeb59ff65f1e107/mmhais/videohai/devvideo/src/DevVideo/devvideointernal.cpp)
  负责可选 MDF Processing Unit 列表与查找，不做 Header validation；
- [`DevVideoPluginInterfaceUIDs.hrh`](https://github.com/SymbianSource/oss.FCL.sf.os.mm/blob/ebaa78373866f90dbf706e8d4eeb59ff65f1e107/mmhais/videohai/devvideo/inc/DevVideoPluginInterfaceUIDs.hrh)
  定义 decoder HwDevice ECom interface UID `0x101FB4BE`；
- [`devvideostandardcustominterfaces.h`](https://github.com/SymbianSource/oss.FCL.sf.os.mm/blob/ebaa78373866f90dbf706e8d4eeb59ff65f1e107/mmhais/videohai/devvideo/inc/devvideostandardcustominterfaces.h)
  给出公开 standard custom-interface UID 与 ABI；
- [`devvideoplay.cpp` CustomInterface](https://github.com/SymbianSource/oss.FCL.sf.os.mm/blob/ebaa78373866f90dbf706e8d4eeb59ff65f1e107/mmhais/videohai/devvideo/src/DevVideo/devvideoplay.cpp#L1367-L1370)
  直接调用所选 HwDevice 的 `CustomInterface(TUid)`。

因此，通用 `CMMFDevVideoPlay::GetHeaderInformationL()` 本身没有 SPS/ref/DPB validation，也没有产生
本次 `-5` 的分支。该错误来自所创建的 HwDevice implementation、可能的 MDF adapter，或其调用的
更下层组件。`ConfigureDecoderL()` 同样转发给 HwDevice；但基类为没有 override 的实现提供了默认
`KErrNotSupported`，所以系统 ARM decoder 的 Configure `-5` 不能单独证明已经尝试实际解码。

`AVC.h` 描述 weighted prediction、profile/level、HRD 和 DPB 数据结构；公开的 standard custom
interfaces 中没有发现“提高 decoder reference slots/DPB 上限”的控制接口。`CustomInterface()` 只是
分发机制，不是存在解锁接口的证据。Symbian 的 OpenMAX integration 资料证明 HwDevice 可以包装
OpenMAX IL component，但不证明 Nokia 603 的 `0x10204C21` 采用该实现。

## 4. Capability matrix 测试流

### 4.1 固定素材与编码基线

先生成一份确定性、无版权问题的 10 秒 YUV420P 原始素材，并固定：

- 640×360、30 fps、300 帧、progressive；
- High Profile、Level 3.0；
- closed GOP，`keyint=min-keyint=60`，关闭 scenecut；
- CABAC、8×8 transform、单线程编码、固定 VBV/目标码率；
- 每个 H.264 输出包含 AUD，便于设备端无歧义切分 access unit；
- 同一份 `.h264` 只 remux 成 `.mp4`，禁止再次编码。

基础 x264 命令模板为：

```text
x264 --demuxer raw --input-res 640x360 --fps 30 --frames 300 \
  --profile high --level 3.0 --keyint 60 --min-keyint 60 --scenecut 0 \
  --bitrate 900 --vbv-maxrate 1200 --vbv-bufsize 2400 \
  --threads 1 --aud --ref {N} --bframes 0 --b-pyramid none \
  --weightp 0 --no-weightb -o {case}.h264 base_640x360_30.yuv
```

实际脚本必须把 x264、FFmpeg 版本、完整命令、stderr、输入和输出 SHA-256 写入 manifest。若当前
x264 版本的参数名或实际 SPS 与预期不同，以解析结果为准，不能仅凭文件名认定 case 属性。

### 4.2 第一轮自适应矩阵

| 组 | 变量 | 固定项 | 目的 |
|---|---|---|---|
| R | `ref=1..8` | B=0、weighted off、B-pyramid none | 找 reference/DPB 联合 admission cliff |
| D | `ref=4`，合法上调 VUI `max_dec_frame_buffering=4..8` | slice graph、reorder=0 不变 | 区分 declared DPB budget 与真实 reference graph |
| WP | `weightp=0/2` | ref=4、B=0 | weighted P 单变量 |
| WB | `weightb=off/on` | ref=4、B=3、weightp=0、B-pyramid none | weighted B 单变量 |
| B | `bframes=0..4` | ref=4、weighted off、B-pyramid none | 测 B/reorder 结构，但不冒充语法单变量 |
| BP | `b-pyramid=none/strict/normal` | ref=4、B=3、weighted off | 测 B-pyramid 对 DPB/reorder 的影响 |

R 组中 `max_num_ref_frames` 与 DPB 通常会一起变化，所以 R 组只能定位 joint cliff，不能证明 ref
是唯一原因。B 组改变 B-frame topology 时 reorder 也会随之变化，同样必须记录解析出的实际元组。

只有第一轮发现边界后，才在 `N-1/N/N+1` 上复制 WP/WB/B/BP 组合，避免一开始生成大量不能归因
的笛卡尔积。D 组和以后可能的 declared-reorder 组需要语义级 SPS rewriter；只允许把声明上调到
仍然合法的值，且 slice/reference graph 不变。它们属于合法 capability matrix，不是 fake case。

### 4.3 PC 侧合法性门

每个 case 上机前必须同时通过：

1. x264 编码成功，完整命令和版本已归档；
2. `ffmpeg -v error -err_detect explode -i case -f null -` 无 decode error；
3. FFmpeg `trace_headers` 与独立 parser 对 profile、level、refs、DPB、reorder、weighted P/B 的结果一致；
4. x264 reference usage statistics 表明确实使用了预期的最高 reference index；若没有，case 只能用于
   header admission，不能用于证明高-ref graph 的 runtime decode；
5. `.h264` 与 remux `.mp4` 解码后的逐帧 golden CRC 一致；
6. manifest 记录输入素材、case 文件、parser 输出和 golden frame checksums。

### 4.4 当前实施状态

R 组 PC 资产工厂已经实现于
[tools/research/h264_hwcap](../../../../tools/research/h264_hwcap/README_ZH.md)。当前生成器会产生
REF1–REF8 的 Annex-B/MP4、设备端 `cases.ini`、完整 manifest、编码/解析日志和逐帧 golden CRC，
并在写出 manifest 前强制检查本节六项合法性门。

本机验收已确认 8 个 case 的解析元组依次为 `ref/DPB=1/1..8/8`，其余固定为
`reorder=0, weightp=0, weightb=0, B=0`；x264 summary 在每个 case 都观察到最高声明 reference slot，
因此 8 个 case 都具有 runtime 验证资格。同一记录工具链下独立重复生成的 H.264、MP4 和 decoded-YUV
hash 完全一致。具体版本与每次输出 hash 只由该次 `manifest.json` 保存，生成媒体留在 Git 忽略的
`.tmp/`，不进入公开仓库。

R 组以及第二轮 D/WP Nokia 603 gate 已经完成，逐 case 真机事实只见
[设备测试矩阵](../../../reference/DEVICE_TEST_MATRIX.md)。第二轮证明 ref=4 时 declared DPB=4..8 均可
正确输出，ref=4/6 时 weighted P off/on 也均可正确输出，而 weighted-off 的合法 R7 仍在 Header 返回
`-5`。因此当前 legal cliff 已收敛到 `max_num_ref_frames` 或与它直接耦合的 reference-count 检查；
还没有证明 R7 压缩数据到达物理芯片。original/all-SPS-fake 的 Nokia 603 A/B 已完成：fake 只改变
SPS `max_num_ref_frames` 即越过 Header/Initialize 并提交 100 AU，但输出持续 corruption。这证明
admission 受该 SPS 字段控制，不证明合法 ref=7 可解，也不满足物理芯片 handoff 证据。WB/B/BP 仍待
实施，研究终局继续保持未分类。

## 5. 最小 Direct DevVideo capability probe

### 5.1 隔离方式

已经新增独立的 [symbian/probes/devvideo-capability](../../../../symbian/probes/devvideo-capability/README_ZH.md)
target，使用研究专用 UID `0xE000B11D`、独立包名和固定本地数据目录。它不链接 Qt Mobility
multimedia，不创建 `CVideoPlayerUtility2`，不打开网络，不显示 DSA，也不读 Cookie 或 Bilibili URL。
静态隔离检查同时禁止主应用 `.pro` 和 `Build-App.ps1` 引用该 target、UID 或目录。

第一版输入契约：

- `cases.ini`：case id、文件名、容器类型、预期字段、文件大小和 manifest hash；
- `.h264`：只接受带 AUD 的本项目测试流；
- `.mp4`：留到第二版复用 `Mp4AvcProbeReader` 的 sample table 和 AVCC→Annex-B 逻辑；首个真机包只读
  已由 PC manifest 和 SHA-256 锁定的 Annex-B `.h264`；
- 每个 case 最多提交 100 个完整 `EDuCodedPicture` access unit；
- 每个 case 新建并销毁一套 `CMMFDevVideoPlay`，禁止前一 case 的 decoder state 污染下一 case。

### 5.2 严格状态机

```text
OPEN_LOCAL_FILE
  -> PARSE_AND_VERIFY_EXPECTED_FIELDS
  -> NewL
  -> SelectDecoderL(0x10204C21)
  -> VideoDecoderInfoLC / verify hardwareAccelerated
  -> SetInputFormatL(video/h264, EDuCodedPicture, EDuElementaryStream, ETrue)
  -> GetHeaderInformationL
  -> ConfigureDecoderL
  -> GetOutputFormatListL
  -> SetOutputFormatL(prefer memory YUV420)
  -> SetVideoDestScreenL(EFalse)
  -> SynchronizeDecoding(EFalse)
  -> Initialize / MdvpoInitComplete
  -> Start
  -> GetBufferL + WriteCodedDataL for <=100 AU
  -> InputEnd
  -> MdvpoNewPictures / NextPictureL / ReturnPicture
  -> counters, loss callbacks, CRC, cleanup
```

每一步分别记录 `OK`、同步 leave code、异步 callback error、timeout 或 `NOT_RUN`。Header 成功不得把
后续列自动填成成功；Initialize 是异步阶段；First Picture 必须来自 `NextPictureL()` 的真实 memory
picture。每个 case 设置阶段 watchdog，fatal callback 只投递清理事件，不能在回调栈删除 DevVideo。

### 5.3 日志和 corruption 判定

一行汇总至少包含：

```text
case sha profile level refs dpb reorder weightp weightb bframes bpyramid
select input header configure output_list output_set init
au_written buffers_returned first_picture pictures_decoded pictures_output
picture_loss slice_loss fatal stream_end crc_match elapsed_ms
```

汇总结果使用下列固定列，不用单一的“能播/不能播”覆盖阶段信息：

```text
Case Header Configure Output Initialize AU-written First-Picture
Picture-CRC Raw-CRC Loss Fatal StreamEnd Lower-Evidence Verdict
```

完成后的逐 case 真机填值只写入 `docs/reference/DEVICE_TEST_MATRIX.md`，不在研究计划中维护第二份
结果表。

设备端把可见 crop 按实际 stride 规范化后计算逐帧 Y/U/V CRC，并可选择保存前 3 张原始 memory
picture。PC evaluator 与 FFmpeg golden 按 presentation order 比较。未完成 host 对比时 corruption
必须写 `UNKNOWN`，不能只凭“有 First Picture”填写 `NO`。

探针的最小实现顺序为：

1. 只支持一份带 AUD 的 `.h264` 和 Header/Configure/Initialize；
2. 加入 memory output、100 AU、frame CRC 和 counters；
3. 加入 `cases.ini` 批处理；
4. 最后接入本地 MP4 reader；
5. Nokia 603 上先跑已知 4-ref 正控制、损坏文件负控制，再运行正式矩阵。

任何阶段都不得复用正式播放器窗口、MMF AAC、时钟或路由状态机。

### 5.4 当前实施状态

独立 probe 已完成 GCCE 4.4.1 ARMv5 Release 编译和 SIS 打包，固定按
`REF4 -> REF1 -> REF2 -> REF3 -> REF5 -> REF6 -> REF7 -> REF8` 运行。它在设备端重新核对文件大小、
SHA-256 和 AUD/access-unit 数量；每个 case 重建 `CMMFDevVideoPlay`，最多提交 100 AU，并实时 flush
阶段事件。前 3 张 memory picture 会保存为 raw frame，corruption 在 PC 规范化比较前保持 `UNKNOWN`。

首轮 Nokia 603 R 组和第二轮 D/WP gate 均已完成，真机事实已转入唯一的
[设备测试矩阵](../../../reference/DEVICE_TEST_MATRIX.md)。第二轮的十个 Header-accepted case 共核对
990 条 picture CRC 和 30 张 raw frame，全部匹配 PC golden；合法 R7 仍在 Header `-5`。文件内显式
EOS 加 `InputEnd()` 仍只输出 99/100 picture 且没有 `StreamEnd`，所以这项 drain 行为继续与准入结论
分开记录。original-vs-all-SPS-fake schema 3 A/B 已在 Nokia 603 完成，阶段和 CRC 事实只见设备测试
矩阵；损坏文件负控制继续与合法矩阵和 fake A/B 分开，避免三类证据混为一谈。

## 6. Original 与 all-SPS-fake Direct A/B

该实验与合法矩阵分开统计：

- A：一份 PC 已验证合法、实际使用高 reference index 的原始 ref=7 Annex-B 流；
- B：从 A 复制，重写每一个 SPS RBSP，只把 `max_num_ref_frames` 从 7 改为 3；PPS、slice、
  reference graph、VUI DPB/reorder 和所有其他 SPS 语义保持不变；
- rewriter 必须重新生成 Exp-Golomb、RBSP trailing bits 和 emulation-prevention bytes，并输出字段级 diff；
- B 是故意与真实 reference graph 不一致的诊断流，不进入 capability matrix，也不能用于 H1/H2 的
  “合法 7-ref”验收。

A/B 使用同一个 probe binary、同一个 decoder UID 和同一个输出格式，分别记录 Header、Configure、
Initialize、AU written、First Picture、loss/fatal、CRC。若出现 A Header `-5`，B Header `0` 且 B
能 Initialize/出损坏帧，只能证明 HwDevice/lower parser 的 admission 受 SPS reference 声明影响；
它仍不证明合法 ref=7 可以硬解，也不证明损坏发生在物理芯片。

PC 实施从合法 R7 生成 `ORIGINAL_R7/FAKE_REF3`：两个 SPS occurrence 全部改为 3；独立
parser 与 FFmpeg `trace_headers` 一致；PPS、100 AU、NAL type sequence 和全部非 SPS NAL 逐字节相同；
fake 在 PC 上出现 `Missing reference picture`，decoded CRC 与合法 golden 不同。schema 3 probe 已通过
Qt 4.7.4 / GCCE ARMv5 Release 编译。Nokia 603 上 original Header `-5`，fake 则通过 Header、Configure、
Initialize 和 100 AU submission，输出 99 picture；合法 golden 仅匹配 20 帧，第一处错误为 frame 7，
且无 loss/fatal callback。固定结论是 `ADMISSION_BYPASS_CONFIRMED / CHIP_HANDOFF_UNPROVEN`。

## 7. 后续阶段和硬门

### R0：证据封装

- 为所有合法 matrix 和 fake A/B 建立 manifest、SHA-256、完整 parser 输出；
- 私有或版权样本只保存在并列只读研究仓库，产品树只保存脱敏 manifest 和结论；
- 真机结果写入 `docs/reference/DEVICE_TEST_MATRIX.md`，本页只更新阶段状态和终局判断。

### R1：Nokia 603 capability matrix

- REF1..REF8 首轮 Header 与 Header 成功 case 的 Configure/Initialize/100 AU/CRC 已完成；
- 修正 EOS/flush 收尾并复测边界两侧，不把 watchdog 结束解释为性能或播放结论；
- 若出现 cliff，在边界附近展开 weighted、B/reorder 和 B-pyramid；
- 默认 BufferOptions 与 `iMaxPostDecodeBufferSize=0` 只做一次低成本 A/B。该字段视为 framework
  output/post-decode buffering，不解释成 H.264 DPB。

### R2：ECom implementation 定位

开源框架已经把 `0x10204C21` 定位为 decoder implementation UID，decoder plugin interface UID 为
`0x101FB4BE`。独立只读 `devvideo-ecom-audit` probe 不创建 decoder、不调用 `CustomInterface()`，也不
申请 `AllFiles/TCB`。R1 真机确认目标在 decoder interface 中名为
`IVE Video Decode AVC Hw Device`。R2 又确认 MDF Processing Unit interface `0x10273789` 枚举成功但
数量为 0；按开源 `CMMFDevVideoPlay::CreateDecoderL()` 分支，目标因此直接调用
`CMMFVideoDecodeHwDevice::NewL()`，不是 generic MDF HwDevice adapter `0x102737ED`。

R2 还按开源 ECom server 的 resource-1 v1/v2/v3 顺序结构化读取 33 个
`Z:\resource\plugins` loose resource：27 个是有效 ECom v1/v2 registration，六个是非 ECom resource，
目标结构化/raw 命中均为 0，且无读取错误。开源 `CDiscoverer` 对只读 drive 会先通过
`RResourceArchive::OpenL(..., "ecom")` 读取 `Z:\private\10009D8F\` 的 SPI archive set；只有 archive
未提供 registration 时才扫描 loose RSC。因此，R2 的零命中并非 DLL 不存在，而是把下一检查点精确到
SPI archive。

0.3.0 probe 已按 ECom server 相同的 directory/base-name 方式完成 Nokia 603 SPI audit：archive type
`0x10205C2C` 正确，1,359 个 registration 全部解析，唯一目标 resource 为
`ivevideodecodehwdevice`，DLL UID `0x10204C1E`，并映射到同名 DLL。该 DLL 同时注册 IVE H.263、MPEG-4、
AVC、VC-1 与 VP6 五个 HwDevice implementation。完整大小、SHA-256 和真机字段只见设备矩阵。
接下来需要：

1. 设备型号、product code、firmware/ROFS 版本和原始镜像 SHA-256；
2. SPI open status、archive type `0x10205C2C`，以及含 implementation UID 的 archive resource name、
   interface UID、version、display name、default/opaque data 和 `dll_uid`；
3. archive resource name 映射的 `z:\sys\bin\*.dll` 名称、路径、大小、SHA-256、E32 header、SID/VID/capabilities、
   exports、imports、依赖和字符串；
4. direct-vs-MDF 已完成；继续判断 direct HwDevice DLL 是否调用 OMX、IVE/Broadcom component 或 driver；
5. 对 imports/strings 搜索 OMX、Broadcom、H264/AVC、VideoDecode、ref/DPB/profile/level 和下层 driver。

SPI registration/DLL 定位门已经完成。继续研究不再以 Nokia 808、跨设备实机或 RM-807 固件为
前置条件；这些材料以后只增加对照置信度。任何后续步骤仍不修改正式播放器或手机 ROM。

RM-779 SW113 common-core 的只读 XIP 页重建随后取得了与真机文件 SHA-256 完全一致的 54,564-byte
decoder DLL，故 603 target bytes 已经关闭，不再需要从手机复制。恢复的静态链接只指向 DevVideo、
`rcam` 和 IVE policy client；format-specific RTTI/类名明确包含
`CIveVideoDecodeHwDeviceAVCParser`。对该精确 image 的 Thumb/RTTI/vtable 追踪随后已经定位实际分支：

```text
CMMFDevVideoPlay::GetHeaderInformationL
  -> CIveVideoDecodeHwDevice::GetHeaderInformationL  0x80DBDAC6
  -> CIveVideoDecodeHwDeviceAVCParser virtual method 0x80DC6FB0
  -> parse max_num_ref_frames -> parser+0xC0       0x80DC6D88
  -> if max_num_ref_frames > 6                    0x80DC7220
       User::Leave(KErrNotSupported)               0x80DC7234..0x80DC7238
```

同一分支还包含另一条尺寸联合门：coded luma pixels 大于 `307200` 时，refs=6 也会被拒绝；对
640×360 的本矩阵，实际规则正是 `refs<=6` 通过、`refs>=7` 返回 `-5`。从 Header 公共入口到这个
Leave 点没有命中已恢复的任何 `rcam` 或 IVE policy-client import stub，所以原始合法 R7 在当前
GetHeader 路径中确定尚未到达已识别的下层提交接口。这是 host-side vendor AVCParser admission gate，
不是物理 VideoCore 已经拒绝 ref=7 的证据。

可重复审计脚本为
[`Inspect-IveAvcAdmission.ps1`](../../../../tools/research/h264_hwcap/Inspect-IveAvcAdmission.ps1)：它锁定
RM-779 SW113 extended image 的大小/SHA-256，核对工厂、RTTI、vtable、解析保存点、分支、`-5` Leave
和 gate 前下层 call set；只反汇编，不加载或执行固件代码。

### R3：Nokia 603 Header/Submit split

静态 gate 已经满足“明确检查点”门，下一步先做不改 ROM 的最小绕过判别，而不是等待其他设备：

```text
GetHeaderInformationL + ConfigureDecoderL : FAKE_REF3 的首个 AU
Initialize 后 WriteCodedDataL             : ORIGINAL_R7 的全部 100 AU
```

独立 probe schema 4 固定运行 `R6_NATIVE -> R7_NATIVE -> FAKE_HEADER_ORIGINAL_R7 ->
FAKE_HEADER_FAKE_REF3`，分别校验 admission/submit 文件的大小与 SHA-256，并分别记录来源。关键 case 的
运行时 AU 从第一个 SPS 开始就是未经修改、PC 合法且实际使用第 7 reference slot 的 ORIGINAL_R7；fake
只用于取得 Header/Configure 准入状态，不会混进后续提交序列。

2026-08-31 的 Nokia 603 真机 R4 已得到强阳性结果。四个 case 均重新创建独立 DevVideo 会话，输入与
admission 文件的大小/SHA-256 全部匹配：`R7_NATIVE` 仍在 Header 返回 `-5`；split case 则用 fake-ref3
完成 Header/Configure，Initialize 后接受未经修改的 ORIGINAL_R7 全部 100 AU，并输出 99 张 picture。
99/99 picture CRC 与三张 raw-frame CRC 全部匹配 PC 的 ORIGINAL_R7 golden，无 loss/fatal callback。
R6 与 R7 golden 在前 99 帧中有 93 帧不同；首个差异 index 6 上真机输出 `53B6DB72`，匹配 R7 而不是
R6 的 `66DD8DF6`，故排除前一会话残留。全程 fake control 从 frame 7 开始持续 mismatch，进一步确认
A/B 确实由提交的原始 SPS/reference graph 决定。

这证明 vendor 硬件加速 HwDevice 在 host Header gate 被隔离后能够正确处理该合法 640×360 R7 graph；
它不再支持“该 graph 是 BCM2763 的真实解码上限”这一解释。由于 split 的 Header/Configure 输入仍是
fake-ref3，它还不是 H1 正式解锁；下一步必须让同一未经修改的 ORIGINAL_R7 自身通过 Header/Configure。

### R4：可选跨设备对照

N8/603/808 对照不再阻塞 603 研究终局。有 RM-807 或其他设备时仍用同一矩阵和 ROM-fixed
direct-link 归一化流程比较 UID、DLL、resource/config 与分支；没有 808 实机或固件时，不暂停 R3、
明确接口审计或最小可恢复验证。“808 能 1080p”仍不能替代同流同阶段证据。

### R5：明确接口与 OMX

- 只测试已从公开 header、SDK 或目标 DLL 符号/调用点确认的 CustomInterface UID 和 ABI；
- 当前公开 standard interfaces 没有发现 ref/DPB 解锁项；BufferManagement、WindowControl、
  ResourceManagement 等不因为名字相关就进入随机调用列表；
- 只有目标 DLL imports/strings/dependency 证明存在 OMX，再追踪 `OMX_GetHandle/SetParameter/SetConfig`
  与 component name/parameter index；OpenMAX integration 文档只作为架构可能性证据；
- 不猜 private UID，不猜 vtable slot。

### R6：最小逆向与可恢复验证

只有满足以下至少一项才进入：

- 合法矩阵出现稳定 Header cliff；
- original/fake Direct A/B 只因 reference 声明改变而越过 Header；
- Header/Submit split 仍不能让原始 R7 进入 vendor Write path；
- 可选跨设备 RSC/DLL diff 显示明确的 capability table 或 admission 分支。

当前已经找到 `refs > 6 -> KErrNotSupported`，且 R3/R4 split 已证明 original submit 能正确解码。随后 R5
在深度破解 Nokia 603 上以手动、非自启 ROM shadow 实验只把目标 Thumb `cmp r0,#6` 改为
`cmp r0,#7`；尺寸联合门以及其他 profile、level、buffer 和错误检查均未改变。Header、Configure 与提交
全部使用未经修改的 ORIGINAL_R7：Header/Configure/Initialize 通过、100 AU 被接受，99/99 picture CRC
和三张 raw-frame CRC 与 PC golden 匹配；R6 正控制保持正确，full-fake 负控制保持 corruption。目标以
精确 DLL SHA-256、唯一 12-byte 指令上下文和 expected-before bytes 锁定，离线模型仅一字节变化。

这已经完成原始合法 R7 的短程 Direct 突破。正式 NIKINIKI 在补丁开启时也成功播放了一个此前只能
软件解码的自然样本，但该观察尚缺硬件强制/route telemetry 和长时交互记录。下一步不再扩大逆向，
而是完成 ref=7 weighted/reorder/B-pyramid 合法单变量、至少 10 分钟、pause/resume、seek 和 20 次
创建/销毁。全部通过后才归入最终 H1，并再决定深度破解设备的可选部署形式。Nokia/Broadcom binary、
patch payload 和签名材料不提交、不进入公开包，也不直接接入正式播放器。

## 8. 三种终局的验收门

### H1：硬解成功

必须同时满足：

- 至少一份原始合法 ref=7、实际使用第 7 reference 的流，PC decode 无错误；
- Direct `0x10204C21` 路径 Header、Configure、Initialize 和 AU submission 全通过；
- memory picture 持续输出，规范化 frame CRC 与 golden decode 一致，无持续 loss/corruption；
- 扩展到至少 10 分钟合法流，pause/resume、seek 和 20 次完整创建/销毁可恢复；
- 能说明所用配置/patch 为什么只改变 admission，不破坏低-ref 正控制；
- 真机矩阵明确记录设备、固件、probe build 和全部证据。

### H2：芯片已收到但不能正确解码

必须同时满足：

- 使用原始合法高-ref 流，不以 fake SPS 作为证明；
- 至少通过 wrapper admission 和 Initialize，并有 driver/OMX/firmware trace、硬件 counter 或等价证据
  确认 AU 已提交到下层多媒体处理单元；
- 至少三份合法高-ref 单变量流稳定出现相同的下层 error、buffer failure 或 golden mismatch；
- 同一环境的低-ref 正控制正确；
- 已排除输出格式、AU 边界、时间戳、buffer 生命周期和 probe 自身错误。

只有“Header 通过但花屏”而没有下层提交证据时，结论仍是 `CHIP_HANDOFF_UNPROVEN`。

### H3：无法送到芯片

必须同时满足：

- 合法 capability matrix 和 original/fake A/B 已完成；
- 开源调用链、ECom DLL/RSC、direct-vs-MDF adapter 已定位；
- 已检查明确公开/私有接口和实际存在的 OMX/config 路径；
- Header/Submit split、目标 DLL 中实际存在的接口/config 路径及最小可恢复分支实验均已完成或由明确
  安全理由排除；
- 没有明确、安全、可恢复的配置或最小 probe 能让合法 ref=7 越过已经定位的芯片前 gate。

没有 Nokia 808 不再自动阻止 H3；但若 R3 已进入 `rcam` 路径而缺少足以区分 wrapper/firmware/芯片的
下层证据，结果仍只能写“当前未解决”，不能提前写 H2 或 H3。

## 9. 止损与 2.0 的关系

出现以下任一情况时停止继续扩大逆向范围：

- 在 603 上完成明确 gate 的最小可恢复绕过后，多个合法 R7 控制仍不能越过同一芯片前路径；
- admission 绕过后，多个合法单变量流在已确认的下层硬件路径持续 corruption/panic/buffer failure；
- 603 与 808 使用完全不同的闭源 firmware path，且不存在可审计的接口、resource 或 OMX config；
- 继续推进只剩不可恢复的日常设备刷写、未知 vtable 调用或 VideoCore firmware 盲 patch。

2.0 不以 H1 为发布前提。H1、H2、H3 都是本支线的有效完成结果；无论结果如何，正式播放器仍保留
“硬件接受则 MMF、明确拒绝则本机 FFmpeg”的单向 fallback。软件解码 ARM11 优化与 post-processor
输出研究可以并行，但其测量和验收不混入本 capability matrix。
