# ADR-0004：媒体兼容只在手机本机完成

> 状态：Accepted
> 决定日期：2026-08-26

## 背景

Belle MMF 对部分现代 AVC 只有音频。Broadcom 拒绝风险流，系统 ARM decoder 又在 Configure
阶段失败；降低到 360P 或切换 CDN 不改变对应码流结构。

## 决定

硬件插件拒绝时，使用 GCCE 4.4.1 重建的 H.264-only PPSSPP-FFmpeg 在手机本机解码视频，
AAC 和主时钟继续由 MMF 提供。不引入桥接、远端重封装或远端转码。内置路径不能处理时，
产品可在用户确认后交给外部播放器。

## 后果

- 用户媒体和登录信息不发送给额外媒体服务；
- ARM1176 软件性能成为明确产品约束；
- 外部播放器交接必须保护 Cookie/签名 URL，并提供可恢复的方向和前台状态；
- 远端媒体处理不能作为性能问题的捷径。

## 证据

FFmpeg 构建、真机出画面和性能测量见
`docs/research/player/PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md`。
