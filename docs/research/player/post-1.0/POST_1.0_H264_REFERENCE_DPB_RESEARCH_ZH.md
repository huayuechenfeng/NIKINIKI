# 1.0 后研究档案：Nokia 603 H.264 reference-frame / DPB 准入

> 建档日期：2026-08-29  
> 来源：用户 2026-08-28 的 Nokia 603 文件级 SPS 伪装真机实验；本项目未在本轮审计其脚本或二进制。  
> 状态：1.0 前封存。不得把 SPS 伪装、码流修改、插件替换或 capability 绕过加入正式播放器。

## 1. 用户实验的可用证据

实验仅改变 `max_num_ref_frames` 的 SPS 声明，不重编码 slice/reference 关系。用户报告检查到一个 `avcC` SPS 和 37 个 in-band SPS，并得到下列结果：

| 样本 | avcC / in-band SPS | Nokia 603 系统播放器结果 | 支持的解释 |
|---|---|---|---|
| 原始 | 7 / 7 | 无法正常打开或被拒绝 | 原始流的 SPS/DPB 要求触发不兼容 |
| 半 fake | 3 / 7 | 提示文件损坏 | 后续 in-band SPS 或流参数一致性仍参与检查/重配置 |
| 完全 fake | 3 / 3 | 可启动、音频和速度正常，但大面积花屏，偶尔短暂恢复 | 视频解码链至少开始运行；真实 slice 仍需要超过宣称 DPB 的参考图像 |

PC FFmpeg 对完全 fake 报告的 “reference frames exceeds max / Missing reference picture / mmco failure” 与该解释一致：若 decoder 按 fake 的小 DPB 回收参考图像，后续实际引用会失效。间歇恢复也与 IDR/GOP 重新建立参考链的推断相符。

因此，这是一条有价值的新证据：**SPS 的 `max_num_ref_frames` 声明会实质影响 Nokia 603 系统播放栈是否愿意启动视频路径；“打不开”不能简单归因为解码算力不足。**

## 2. 与项目既有 Direct DevVideo 证据的合并判断

项目早先的受控 Direct DevVideo header 实验已经在 MMF 完全关闭、相同 `video/h264` / Annex-B / `EDuCodedPicture` 输入下得到：

| 流 | `0x10204C21` 的 `GetHeaderInformationL()` |
|---|---|
| 正常 High / 4 refs / DPB4 / 无 weighted | `0`，并返回正确 640×360 |
| 故障 High / 7 refs / DPB7 / weighted | 稳定 `KErrNotSupported (-5)` |

这两类证据可以共同支持下列较强、但仍有限的结论：真实故障 SPS 在 Nokia 603 暴露的 H.264 decoder plugin 路径中会被拒绝；系统播放器的 fake 行为也表明 SPS/DPB 声明是决定路径是否启动的重要变量。

它们**不能**证明一个简单的 `if (ref > 3)` 硬编码：现有工作流包含 4-ref 成功样本，故至少不能把阈值泛化成 “大于 3 必拒”。也不能仅凭系统播放器的文件级结果确定拒绝精确位于 CVideoPlayerUtility、MMF controller、DevVideo wrapper、decoder firmware parser 或 DPB 内存预算检查中的哪一层。

## 3. 明确不能得出的结论

1. 不能证明物理芯片是 BCM2763，或它一定支持/不支持真实 7-ref DPB；当前工程只确认插件字符串 “Broadcom BCM2727”。物理芯片与固件包装关系仍是研究线索。
2. 不能证明 fake 后的“正常速度”意味着正确的 7-ref 硬解；花屏正说明真实参考管理没有被满足。
3. 不能证明半 fake 的报损只由 in-band SPS 的 ref 值造成；它也可能是 SPS 一致性、reconfiguration 或封装校验造成。 
4. 不能把文件伪装作为播放兼容方案：它会产生未定义/损坏画面，也会被 in-band SPS 打断。
5. 除非保存原始文件、fake 文件、脚本版本和 hash 并作独立解析复核，本档把修改范围视为用户报告，而非项目已复现的二进制审计事实。

## 4. 1.0 后建议的研究顺序

### R1：可复现文件证据归档

在合法本地研究目录保留原始、半 fake、全 fake 样本以及脚本；记录 SHA-256。用独立 H.264 parser 列出每个 avcC/in-band SPS 的 profile、level、`max_num_ref_frames`、VUI 和 NAL 位置，避免把其他无意改动归因于 ref 数。

### R2：直接 decoder 的单变量控制实验

独立 UID harness 复用已验证的 `0x10204C21` Direct DevVideo 输入包装，严格按阶段记录：

```text
original ref=7 / all-SPS fake ref=3
  -> GetHeaderInformationL
  -> ConfigureDecoderL
  -> Initialize
  -> 少量 AU
  -> memory-output picture / return callback
```

目的不是绕过限制并发布，而是定位 fake 码流在 plugin 路径中最早在哪一步失败、是否会实际初始化、是否也出现预期花屏。DSA 显示研究与此实验解耦，优先采用 memory output。

### R3：固件/设备静态比较

遵循 `POST_1.0_BCM2763_HWDEVICE_RESEARCH_ZH.md`，比较 Nokia 603 与可信 Nokia 808 固件中 UID `0x10204C21`、`video/h264`、BCM2727/BCM2763 字符串、ECOM 注册和 capability data。重点寻找 DPB/reference、profile-level 与内存预算配置，而不是替换闭源组件。

### R4：停止条件

若 R2 证明 Direct DevVideo 对 all-SPS fake 也在 header/configure 阶段拒绝，或初始化后只能产生损坏画面且无可配置 DPB 参数，则停止动态绕过研究，保留 firmware/static analysis。不得刷写跨机型二进制、破解签名或在日常设备修改视频插件。

## 5. 对 1.0 的约束

这条档案不改变 1.0 的 MMF → 本机 FFmpeg/CPU RGB565 → 外部播放器政策。它只解释为什么 `ref=7` 兼容性问题值得在发布后从 SPS 准入、decoder plugin 和 firmware capability 三个层面继续研究。
