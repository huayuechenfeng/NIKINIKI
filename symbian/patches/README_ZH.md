# NIKINIKI 可选系统补丁

> 状态：Experimental
> 本目录只保存不随 SIS 安装、需要用户明确手动启用的纯文本补丁。

可选补丁不是 NIKINIKI 默认运行条件，也不能替代正式播放器的 MMF/FFmpeg 回退。仓库不分发
Nokia/Broadcom DLL、ROM、固件或签名材料。

| 补丁 | 状态 | 用途 |
|---|---|---|
| [H.264 ref7 admission](h264-ref7/README_ZH.md) | Nokia 603 已验证；其他已知固件仅静态匹配 | 把 IVE AVC HwDevice 的第一道 reference-count 准入上限从 6 调整为 7 |

所有补丁均禁止默认自启。设备、固件、预期字节或测试条件不匹配时必须停止，不能强制应用。
