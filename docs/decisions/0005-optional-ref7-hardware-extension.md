# ADR-0005：把 ref7 硬解作为可选扩展，不改变默认回退

> 状态：Accepted
> 日期：2026-09-01

## 背景

Nokia 603 的 IVE AVC HwDevice 在 host parser 中以 `max_num_ref_frames>6` 拒绝合法 R7。受控 Direct
DevVideo Header/Submit split 与单字节 ROM shadow patch 都已经让未经修改的 ORIGINAL_R7 输出
99/99 PC-golden 匹配帧，证明目标 graph 的硬件路径可用。

## 决定

1. 默认产品仍使用真实 header preflight：接受走 MMF，明确拒绝走本机 FFmpeg；
2. 在 `symbian/patches/h264-ref7/` 发布不随 SIS 安装、仅手动启用的实验性 RomPatcher+ 特征补丁；
3. 补丁存在时，主程序只自然利用固件返回的真实 Header 结果，不安装、探测或自动启用补丁；
4. Header/Submit split 作为第二条已验证技术路径保留，但只有建立应用持有的 Direct DevVideo
   decoder、memory output、显示和音频时钟后才能产品化；
5. 禁止把 full-SPS fake 持续送入 decoder 作为播放方案；该控制已证明从第 7 帧开始 corruption；
6. 不分发 Nokia/Broadcom 二进制、ROM 或固件，不把补丁设为自动启动。

## 后果

- 深度破解用户可以选择扩展已验证设备的硬解覆盖，未破解设备继续获得可靠软件回退；
- 当前播放器架构和 SIS 不依赖补丁，补丁失败不会引入新的后端循环；
- 通用特征只证明对已审计 DLL 的静态定位；除 Nokia 603 SW113 外仍须逐设备真机验证；
- 若未来实现应用内 Header/Submit split，必须新增独立架构和设备验收，不能把研究 probe 直接并入
  普通构建；
- 完整证据和术语边界见[结题报告](../research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)。
