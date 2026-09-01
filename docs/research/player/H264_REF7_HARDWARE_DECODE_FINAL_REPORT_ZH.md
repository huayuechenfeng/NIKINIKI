# NIKINIKI H.264 ref7 硬件解码研究结题报告

> 状态：Concluded / H1 achieved on Nokia 603
> 形成日期：2026-09-01
> 设备基线：Nokia 603 / RM-779 / Belle `113.010.1506`
> 本页职责：保存本研究支线的最终结论、证据链、产品边界和成果入口

## 1. 结论

本研究选择三种预设终局中的 **H1：合法 ref7 硬件解码成功**。

对一份 PC 严格验证、实际使用第 7 reference slot 的合法 640×360 H.264 R7 流，Nokia 603 的
Broadcom/IVE 硬件加速 HwDevice 能够连续输出与 PC golden 完全一致的画面。原先的
`KErrNotSupported (-5)` 不是该测试 graph 的 BCM2763 解码上限，而是
`ivevideodecodehwdevice.dll` 内 host-side AVC parser 的保守准入门：

```text
max_num_ref_frames > 6
    -> User::Leave(KErrNotSupported)
```

研究最终证明了两条不同但都成功的解锁路径：

1. **Header/Submit split**：同一 Direct DevVideo 会话用 fake-ref3 只完成 Header/Configure，
   Initialize 后提交 100 个未经修改的 ORIGINAL_R7 AU；输出 99 帧，99/99 picture CRC 与前三张
   raw-frame CRC 全部匹配 PC ORIGINAL_R7 golden；
2. **ROM shadow admission patch**：把 host parser 的 `cmp r0,#6` 精确改为 `cmp r0,#7`，让
   ORIGINAL_R7 自身完成 Header、Configure、Initialize 和提交；同样输出 99/99 正确帧，并使正式
   NIKINIKI 成功播放此前只能走软件解码的自然样本。

这两条路径共同排除了“fake header 只是偶然出图”和“BCM2763 无法处理该 R7 reference graph”两种
解释。

## 2. 首轮能力边界

使用同一份确定性 640×360/30 fps 素材生成合法 H.264 单变量矩阵后，Nokia 603 Direct
`CMMFDevVideoPlay` + decoder UID `0x10204C21` 得到：

- ref=1..6：Header、Configure、Initialize、100 AU 和 memory picture 全部通过；
- ref=7/8：在 `GetHeaderInformationL()` 返回 `-5`，没有进入 Configure 或 AU submission；
- ref 固定为 4 时，合法上调 declared DPB 4..8 均正确；
- ref=4/6 时 weighted P off/on 均正确；
- ref=7 在 B=0、reorder=0、weighted-off 条件下仍被拒绝。

因此自然样本中 refs、DPB、weighted 和 reorder 同时变化的问题被拆开，准入 cliff 收敛到
`max_num_ref_frames` 或与它直接耦合的 reference-count 检查。

## 3. 失败控制：全程 fake SPS

把 ORIGINAL_R7 中每一个 SPS 的 `max_num_ref_frames` 从 7 改为 3，而保持 PPS、slice、reference
graph 和所有非 SPS NAL 不变，会让 Header、Configure 和 Initialize 通过，但 decoder 按较小 reference
声明管理真实 R7 graph：

- 100 AU 被接受并输出 99 picture；
- 只有 20/99 picture CRC 匹配合法 golden；
- corruption 从第 7 帧开始，IDR 后短暂恢复；
- PC FFmpeg 同样报告 missing-reference/MMCO 类错误。

所以必须固定术语：

- **Header/Submit split** 是成功路径；fake 只用于同一会话的 Header/Configure，提交数据全部是
  未修改的 ORIGINAL_R7；
- **full-SPS fake** 是负控制；把 fake SPS 持续送入 decoder 会产生 reference corruption，不能作为
  播放兼容方案。

## 4. Header/Submit split 的强阳性结果

`FAKE_HEADER_ORIGINAL_R7` 的阶段契约为：

```text
GetHeaderInformationL + ConfigureDecoderL : FAKE_REF3
Initialize 后 WriteCodedDataL             : ORIGINAL_R7 的全部 100 AU
```

真机结果：

- fake admission 文件与 original submission 文件分别通过大小和 SHA-256 校验；
- Initialize 后提交序列从第一个 SPS 起就是未经修改的 ORIGINAL_R7；
- 100/100 AU 被接受，输出 99 picture；
- 99/99 picture CRC 匹配 PC ORIGINAL_R7 golden；
- 前三张 345,600-byte raw frame 的 CRC 全部匹配；
- 没有 picture/slice/packet loss 或 fatal callback；
- R6 与 R7 golden 在前 99 帧中有 93 帧不同，首个差异帧匹配 R7，排除了上一会话残留；
- full-fake control 从第 7 帧开始持续 mismatch。

这证明 fake header 可以作为**同一 Direct DevVideo 会话的准入钥匙**，而原始合法 R7 压缩数据随后
由硬件加速路径正确处理。

## 5. 拒绝发生的位置

Symbian^3 开源 DevVideo 调用链显示，通用 framework 只做分发：

```text
CMMFDevVideoPlay::GetHeaderInformationL
  -> CMMFVideoDecodeHwDevice::GetHeaderInformationL
  -> implementation UID 0x10204C21
```

Nokia 603 的 ECom SPI registration 将 `0x10204C21` 映射到：

```text
Z:\sys\bin\ivevideodecodehwdevice.dll
DLL UID3 0x10204C1E
```

MDF Processing Unit interface 枚举为空，因此该实现走 direct HwDevice 创建路径，不是 generic MDF
adapter。RM-779 SW113 common-core 重建出的 DLL 与手机可见文件逐字节同哈希。Thumb/RTTI/vtable
审计最终定位到 `CIveVideoDecodeHwDeviceAVCParser`：解析出的 `max_num_ref_frames` 大于 6 时，在任何
已识别 `rcam`/policy 下层调用之前直接 `Leave(-5)`。

这把故障层从通用 DevVideo、MMF 资源争用、输入封装和物理芯片逐步收敛到 Nokia/Broadcom host
wrapper 的 AVC admission。

## 6. 原始 SPS admission patch

在深度破解 Nokia 603 上，手动 RomPatcher+ ROM shadow 实验只改变一条 Thumb immediate：

```text
cmp r0, #6  ->  cmp r0, #7
```

`bhi`、尺寸联合门和其他 capability/error checks 保持不变。补丁开启时，ORIGINAL_R7 自己完成
Header、Configure、Initialize 和全部提交，得到与 Header/Submit split 相同的 99/99 正确 CRC。

随后把固定偏移实验泛化为通配特征：

```text
06 28 ?? D8 4B 21 09 03 ?? 42 ?? ?? 06 28
```

该特征在已重建的 9 套目标 DLL 中各自恰好命中一次，且都指向第一道 ref gate，不会命中后面的
高分辨率联合门。固定偏移随固件变化，故不能通用；特征搜索可以跨这些已审计版本定位同一语义分支。

主线成果为：

- [通用特征补丁与使用边界](../../../symbian/patches/h264-ref7/README_ZH.md)；
- [已审计 DLL 清单](../../../symbian/patches/h264-ref7/KNOWN_IMAGES.tsv)。

通用 `SnR` R1 已在 Nokia 603 SW113 上通过实际应用测试。其余八套固件当前只有静态唯一匹配，不能
写成真机兼容。

## 7. 主程序的两种扩展方式

### 7.1 外部 admission patch：当前主线可直接利用

补丁启用后，NIKINIKI 现有真实 header preflight 会得到接受结果，随后按既有路径把未经修改的 MP4
交给 MMF。主程序不需要安装、启用、探测或依赖补丁：

```text
真实 SPS/PPS/AU
  -> patched IVE Header accepts
  -> existing MMF hardware path
```

这是当前代码改动最小、已经完成正式播放器 smoke test 的扩展方式。没有深度破解或补丁未启用时，
原有 `Header reject -> on-device FFmpeg` fallback 继续工作。

### 7.2 应用内 Header/Submit split：已证明可行的独立后端方案

主程序未来也可以拥有 Direct DevVideo 会话，在该同一会话内：

```text
fake header -> Header/Configure
original R7 -> Initialize 后全部 AU submission
```

这条路径已经由 99/99 CRC 证明正确，但当前正式 MMF 路径的 preflight 对象会在返回前销毁，MMF 随后
会建立新的 decoder 会话。因此**仅在 preflight 中 fake 一次 header 并不能让现有 MMF controller
继承该状态**。产品化需要应用持有 Direct DevVideo decoder、memory-picture 输出、显示、AAC 主时钟、
seek 和生命周期，不能把它写成当前版本已经实现。

两种方式都不需要修改真实播放 AU。第一种修改 host admission 指令；第二种隔离 admission input 与
submission input。full-SPS fake 不属于这两种成功方案。

## 8. 产品、安全与发布边界

- 正式 NIKINIKI 的默认 MMF/FFmpeg 单向选路保持不变；
- `.rmp` 是不随 SIS 安装的可选纯文本成果，只允许手动启用，禁止 Auto/DomainSrv；
- 仓库不包含或分发 Nokia/Broadcom DLL、ROM、固件、设备转储、私有测试流或签名材料；
- 补丁应用成功不等于所有 ref7 结构都正确；weighted B、复杂 reorder/B-pyramid、其他分辨率和长时
  生命周期仍可能暴露独立限制；
- Nokia 700/701/808、N8/C7/E7/X7 的静态 gate 相似不等于其芯片、显示路径或 firmware 已真机通过；
- 任何其他设备结果必须进入[设备测试矩阵](../../reference/DEVICE_TEST_MATRIX.md)，不能由 DLL 特征
  命中推断。

## 9. 结题与归档

核心研究问题已经回答：Nokia 603 的目标合法 R7 在芯片前被 host AVC parser 的 ref>6 gate 拒绝；
隔离或精确调整该 gate 后，硬件加速路径能够正确解码。因此本支线按 H1 结题，不再继续盲目搜索
CustomInterface、OMX 参数或 firmware patch。

原研究计划曾把“证明解码能力”和“公开补丁的长期产品资格”合并在 H1 验收门中，包括 10 分钟、
pause/seek 和重复生命周期测试。本报告在结题时将两者拆开：99/99 golden CRC、raw-frame 对照、
正负控制和真实播放器 smoke pass 已经回答硬件能力问题；长时稳定性与跨设备覆盖继续作为实验补丁
的资格测试，未完成时补丁保持 `Experimental / manual only`。

未完成的跨设备、更多码流结构和长时间安全性验证属于补丁维护与产品资格测试，不再阻塞播放器主线。
阶段计划、早期 BCM2763 假设、full-SPS-fake 分析、探针和 PC 矩阵工具只作为历史证据归档，不再
描述当前工作优先级。

最终真机事实以[设备测试矩阵](../../reference/DEVICE_TEST_MATRIX.md)为准；长期产品决定见
[ADR-0005](../../decisions/0005-optional-ref7-hardware-extension.md)。
