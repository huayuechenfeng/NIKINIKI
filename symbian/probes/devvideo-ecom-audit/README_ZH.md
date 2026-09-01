# DevVideo ECom read-only audit probe

> 状态：Archived research probe；ECom/SPI/DLL 定位任务已完成
>
> decoder interface UID：`0x101FB4BE`
>
> target implementation UID：`0x10204C21`
>
> probe UID：`0xE000B11E`

该探针只调用公开 `REComSession::ListImplementationsL()`、BAFL resource reader 和
`RResourceArchive`。它同时枚举 DevVideo decoder interface `0x101FB4BE` 与 MDF Processing Unit
interface `0x10273789`，并按 Symbian^3 开源 ECom server 相同的 v1/v2/v3 字段顺序，只读解析
`Z:\resource\plugins\*.rsc` 的第一个 resource。0.3.0 还使用与 ECom server 相同的 directory/base-name
调用，只读打开 `Z:\private\10009D8F\` 下的 `ecom` SPI archive set。它不创建 DevVideo decoder，
不调用 `CustomInterface()`，不打开测试流，也不修改 ROM、RSC、DLL、ECom registry 或播放器设置。

研究已按 H1 结题；本 target 只为复现 ECom 身份证据保留，不进入普通或发布构建。最终结论见
`docs/research/player/H264_REF7_HARDWARE_DECODE_FINAL_REPORT_ZH.md`。

## 输出

结果优先写入：

```text
F:\Data\NIKINIKI\hwcap\ecom-audit\<时间戳>\inventory.tsv
```

如果 `F:` 不可写，再依次尝试 `E:` 和 `C:`。日志包括：

- interface `0x101FB4BE` 的全部 ECom implementation metadata；
- 目标 UID 是否也注册在 MDF Processing Unit interface `0x10273789`；
- implementation UID、version、display name、drive、ROM flags、VID、data type、opaque data 和 extended UID；
- `Z:\resource\plugins` 每个 RSC 的 resource format、`dll_uid`、interface/implementation 数量和解析状态；
- 结构化命中目标 UID 的 RSC 路径、完整 implementation records、大小和 SHA-256；原始 UID byte search
  仅保留为对照；
- 按 ECom 同 basename 约定推导的 `Z:\sys\bin\*.dll` 候选，以及存在性、可读性、大小和 SHA-256。
- SPI archive 的目录检查、打开状态、archive type、每个嵌入 registration resource 的结构化解析结果；
  命中时记录 archive resource name、`dll_uid` 和由 ECom 本身采用的 DLL basename 映射。

结构化解析从 RSC 取得真实 `dll_uid`。普通 `DLL_CANDIDATE basis=same-basename` 仍只是 loose-RSC
映射；`SPI_DLL_CANDIDATE basis=archive-name` 则复现 ECom `CSpiPlugin` 对 archive resource name 加
`.dll` 的映射。若平台安全阻止读取私有 SPI archive 或 `Z:\sys\bin`，探针只记录错误，不申请
`AllFiles/TCB`，也不尝试绕过。

## 构建

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-Probe.ps1 -Project devvideo-ecom-audit -Configuration release
```

该 target 使用独立 UID，只链接 Qt Core/GUI、公开 ECom/BAFL/file-server client library 和平台
SHA-256 library，不被正式应用工程引用。当前 GCCE ARMv5 Release 构建为 `CAPABILITY None`。
