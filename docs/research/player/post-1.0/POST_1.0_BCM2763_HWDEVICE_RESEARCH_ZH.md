# 1.0 后研究档案：BCM2763 与 “BCM2727” DevVideo HwDevice

> 建档日期：2026-08-26  
> 优先级：Archived historical evidence；H.264 ref7 支线已结题
> 性质：固件与驱动考古假设，不是当前播放器实现依据

最终结果统一见
[H.264 ref7 硬件解码结题报告](../H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md)；本文只保存早期线索和
固件审计边界。当时的阶段门和 probe 契约保存在已归档的
[研究计划](H264_HARDWARE_DECODE_RESEARCH_PLAN_ZH.md)。

相关的两条后续研究证据分别归档于：

- `POST_1.0_H264_REFERENCE_DPB_RESEARCH_ZH.md`：用户的 SPS ref=7 → ref=3 文件级伪装实验，与既有 Direct DevVideo header 控制组的合并边界；
- `POST_1.0_DEVVIDEO_DISPLAY_RESEARCH_ZH.md`：DSA + 全屏 ARGB overlay Phase A 硬失败，以及受限控制条显示实验的前置门。

## 1. 暂停研究的原因

1.0 前的核心目标是让用户能观看视频，而不是继续消耗周期探索封闭固件的隐藏硬解能力。现阶段已经有明确产品路线：MMF 硬解成功则使用 MMF，失败则本机软解，仍失败则交给外部播放器。

因此，本文件只保存线索、证据等级和未来实验方法。1.0 前不得据此修改 decoder/render architecture，也不得重新启用 `headercontrol1`。

## 2. 已由 Nokia 603 真机日志确认的事实

- DevVideo 枚举到硬件加速 H.264 HwDevice UID `0x10204C21`；
- 厂商/标识字符串为 `Nokia Corporation` / `Broadcom BCM2727`；
- 它登记 MIME `video/h264`，声明最大 1280×720、14 Mbps，并列出 Baseline/Main/High 到名义 Level 3.1；
- decoder 自身报告 `SupportsDirectDisplay=false`；
- 相同 Direct DevVideo 输入条件下，它接受正常 640×360 High@5.1/4-ref 流并返回正确尺寸；
- 它对故障 640×360 High@3.0/7-ref/DPB7/weighted 流稳定返回 `KErrNotSupported (-5)`；
- ARM H.264 decoder UID `0x102073EF` 接受同一故障 header，但尺寸字段为 0×0。

这些事实只能描述固件暴露的插件行为，不能单独证明物理芯片型号或完整硅能力。

## 3. 用户提供、等待固件佐证的线索

- Nokia 603 的实际多媒体平台被认为是 BCM2763 / VideoCore IV，但 DevVideo 仍暴露祖传 “Broadcom BCM2727” HwDevice 名称；
- Nokia 808 也被认为采用 BCM2763，并能进行 1080p 硬件解码；
- 用户计划以后把 Nokia 808（以及可能取得的 Nokia 603）固件材料放入工作区作离线研究。

在取得固件文件、可信硬件资料或二进制关联证据前，上述内容应标记为“研究线索”，不能写成已经由本项目证明的事实。

## 4. 核心未解问题

1. `BCM2727` 是否只是 Nokia/Broadcom 为兼容旧 MMF/DevVideo ABI 保留的插件标识，而底层实际驱动面向 BCM2763？
2. Nokia 603 与 Nokia 808 是否复用了同一插件 UID/接口，却通过产品配置、固件微码、内存带宽或 capability table 暴露不同上限？
3. 808 的 1080p 能力是否走另一个 MMF controller、私有 DevVideo HwDevice 或相机/图库专用播放链，而不是通用 `0x10204C21`？
4. 603 的 720p/14 Mbps 表格是硬件限制、软件许可、固件保守值，还是兼容层刻意裁剪？
5. 7 refs/DPB7/weighted 的拒绝发生在 Nokia wrapper、Broadcom firmware parser、内存预算检查还是更下层的硬件约束？用户的 SPS fake 实验显示 SPS 声明会影响准入，但尚未定位精确层级；
6. 能否通过 808 固件中的同 UID 插件、资源文件或 capability data，在不改微码的情况下解释并扩展 603 的硬解范围？

## 5. 固件到位后的推荐目录

建议只保存用户合法取得的本地副本，不提交到公开仓库：

```text
research/firmware/
    nokia-603/<firmware-version>/
    nokia-808/<firmware-version>/
    manifests/
    extracted/
    notes/
```

每份固件先记录来源、设备型号、产品代码、版本、文件大小和 SHA-256。原始文件设为只读；所有解包结果放入 `extracted/`，不要覆盖原件。

## 6. 未来静态对比步骤

1. 建立 603/808 固件文件清单、SHA-256 和版本差异；
2. 定位 DevVideo decoder、post-processor、MMF controller、ECOM 注册资源、视频驱动和 Broadcom/VideoCore 相关组件；
3. 以 UID `0x10204C21`、`0x102073EF`、字符串 `BCM2727`、`BCM2763`、`video/h264` 为索引，比对 DLL/RSC/注册表；
4. 比较同名插件的 PE/E32Image 头、Secure ID、Vendor ID、capabilities、导出表、版本、依赖、字符串和资源数据；
5. 查找最大分辨率、码率、profile-level、DPB/reference-frame 等 capability table 是否位于插件资源或产品配置中；
6. 判断 808 的 1080p 路径是否使用不同 controller/HwDevice UID，还是相同插件配合不同底层驱动；
7. 只有静态证据形成清晰假设后，才设计独立真机 probe；每次只验证一个变量，不直接修改正式播放器。

## 7. 未来动态实验门槛

- 只在 1.0 稳定版完成后进行；
- 使用独立 UID/包名的诊断程序，避免覆盖正式应用数据；
- 先只读枚举，再 header，再 Initialize，再首帧，不能一步跨越多个阶段；
- 不在日常设备上刷写未知驱动或跨机型二进制；需要固件修改时使用可恢复的备用设备和完整备份；
- 任何 1080p/扩展 profile 结论都必须有连续播放、温度、内存、seek 和重复进入证据；
- 不分发可能受版权限制的 Nokia/Broadcom 固件或私有组件。

## 8. 未来可能产生的价值

如果确认 “BCM2727” 只是 BCM2763 的兼容包装层，未来可能从 808 固件差异中找到更完整的 capability 配置或播放链，为 603/808 扩展本地硬解覆盖。但这只是长期研究方向；在形成可重复、可恢复、合法的证据链之前，不能进入 1.0 产品代码。
