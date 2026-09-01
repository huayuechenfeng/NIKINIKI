# NIKINIKI H.264 ref7 admission 补丁

> 状态：Experimental / manual only
> 已验证设备：Nokia 603 / RM-779 / Belle `113.010.1506`
> 目标文件：`Z:\sys\bin\ivevideodecodehwdevice.dll`
> 补丁不随 NIKINIKI SIS 安装，也不会由应用自动启用

## 作用

`NIKINIKI_REF7_UNIVERSAL_SNR_R1.rmp` 使用 RomPatcher+ `SnR` 通配特征定位 IVE AVC parser
的第一道 reference-count 准入门：

```text
06 28 ?? D8 4B 21 09 03 ?? 42 ?? ?? 06 28
```

只把特征开头的 Thumb 指令从：

```text
06 28    cmp r0, #6
```

改成：

```text
07 28    cmp r0, #7
```

后面的尺寸/reference 联合门、profile、level、buffer 和错误检查均不修改。补丁不包含 Nokia 或
Broadcom 的任何二进制内容。

## 与 NIKINIKI 的关系

补丁启用后，当前主程序仍使用未经修改的真实 SPS/PPS 和 MP4：Broadcom header preflight 接受
合法 ref7 时，现有选路自然进入 MMF 硬件视频。应用不需要识别补丁，也不会修改媒体码流。

未启用补丁时，默认行为完全不变：真实 header 被明确拒绝就使用本机 FFmpeg 软件视频。

另一条已经由 Direct DevVideo 证明成功的 Header/Submit split 路径见
[结题报告](../../../docs/research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)。该方法必须在同一
DevVideo 会话中只用 fake header 完成 Header/Configure，随后提交未经修改的原始 R7 AU；它不是把
fake SPS 持续送入 decoder，也不是当前 MMF 主线已经实现的功能。

## 已验证与未验证

- Nokia 603 SW113 上，先前固定偏移实验已经让 ORIGINAL_R7 全阶段通过并得到 99/99 正确 CRC；
- 本通用 `SnR` 版本随后在同一 Nokia 603 上成功应用，并由正式 NIKINIKI 播放验证；
- 8 套其他 Nokia 固件的重建 DLL 均只出现一次该特征，但尚无对应真机结论；
- [KNOWN_IMAGES.tsv](KNOWN_IMAGES.tsv)中的 `STATIC_ONLY` 只表示离线唯一定位，不表示硬件能够正确解码。

真机事实的唯一记录是[设备测试矩阵](../../../docs/reference/DEVICE_TEST_MATRIX.md)。

## 手动使用

1. 手机必须已经深度破解并安装 RomPatcher+；
2. 禁用旧的 ref7 实验补丁并重启；
3. 把 `NIKINIKI_REF7_UNIVERSAL_SNR_R1.rmp` 复制到存储卡的 `Patches` 目录；
   Nokia 603 的存储卡可能是 `F:\Patches\`，其他设备也可能是 `E:\Patches\`；
4. 关闭相机、图库、系统播放器和其他视频程序；
5. 在 RomPatcher+ 中手动启用；禁止选择 Auto 或 DomainSrv；
6. 如果补丁无法应用，立即停止，不得改地址或强制应用；
7. 测试完成后禁用补丁并重启。

## 风险与结论边界

补丁属于可恢复 ROM shadow 实验，但仍可能导致花屏、媒体服务异常、应用 panic、冻结或重启。
测试前应备份重要数据，不应在日常设备上设置自启。

补丁应用成功只证明特征被找到；只有合法原始 ref7、正确连续画面和 golden CRC/长期播放才能证明
对应设备的硬解能力。不得把本补丁宣传为“所有 ref7、所有 VideoCore 设备或所有固件通用”。

完整证据、两条成功路径和 full-SPS-fake 负控制见
[H.264 ref7 硬解结题报告](../../../docs/research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)。
