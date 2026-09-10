# E7 MMF Prepare 错误来源：主机映射与设备取证契约

> 状态：E7/603 有限静态对比完成；目标手机 `MediaClientVideo.dll` 已证明 Prepare error 原样
> 转发，冻结 A 的四份有价值 raw 数据也已完成；现已从同一手机的 `HxMmfCtrl.dll` 恢复
> controller 内部 Prepare 汇入、Helix result 汇总和 `-12017` 映射链。当前已把同会话错误追至
> `mdfvidrender.dll` 的 IPC 18 服务端处理：同步 `CreateAndInitL`/TRAP 返回0，异步 DevVideo
> 初始化回调随后以原始`-44`完成同一消息。实际会话选择decoder `0x10204C21`和postprocessor
> `0x10273417`；同机`DevVideo.dll`指令与PID70824回调LR证明`-44`来自decoder初始化分支，
> 不是postprocessor分支。当前又由同机IVE实现及PID70939闭合到
> `CIveVideoDecodeHwDevice::AccessDenied`：策略拒绝回调在状态位未置时于IVE内当场生成`-44`，
> 再沿原链成为Prepare `-12017`。同机policy文件及运行server身份已经确认，但CODA不能断ROM code，
> 普通Logging也未暴露policy业务trace；策略为何拒绝仍UNKNOWN，设备进程已停止
>
> 范围：只追踪冻结最小 Qt A 与现有完整Qt诊断会话中 `KErrMMPartialPlayback (-12017)` 在 Qt observer
> 之前的来源。本文不修改正式播放逻辑，也不把静态候选写成运行事实。
>
> 清理边界：本报告是黑屏研究的详细事实来源；STATUS/ROADMAP 只保留结论和本页链接。
> 设备连接、启动、观察和验证脚本已移到私有忽略归档
> `.tmp/e7-private-tools-20260910/`，公共 `tools/research/e7/` 只保留离线分析工具。

## 已知设备边界

上一阶段在实际加载的 `qtmultimediakit_mmfengine.dll` 上完成了
A → 完整 Qt → 完整 Qt → A 四次同媒体观察。四次均在同一 session 依次命中
Prepare callback、`applyPendingChanges(true)` 返回和内部 MMF Play，输出 window 已由 null
变为非 null，apply 前路径 TRAP error 为0。Prepare error 为
`0/-12017/-12017/-12017`，人工画面为可见/黑/黑/黑，均有声。末次冻结 A 已翻转，故
`-12017` 只是目前最早的结果相关边界，不是已经证明的黑屏原因；旧 native 的无归属 PP 统计
也不能补充本次会话归属。详见
[E7 Qt 内部会话观测映射](E7_QT_INTERNAL_SESSION_OBSERVATION_ZH.md)。

## 可用固件和模块身份限制

用户提供的本地固件材料位于 `<private-firmware-root>`。它不进入产品树，以下只记录主机只读身份：

| 材料 | 大小 | SHA-256 | 能证明的范围 |
|---|---:|---|---|
| `E7-00_Nokia_firmware_RM-626_APAC_CHINA_SW111.040.1511.exe` | 643,920,257 | `FEF879FB9CAEDAD432906167DDF749E24BE5F0E86BD53E8677B11FFAD60CA4AE` | 文件名和归档内容指向 RM-626 / SW 111.040.1511；归档包含多个 product-code 变体 |
| `RM-626_111.040.1511_79u_prd.core.fpsx` | 129,431,499 | `7B66B1D22914DCEB4313BB3CD2732B40D6E314CBE24B212432A4C388F0D1BD00` | 与目标 RM/SW 对应的 core 候选；尚未用设备 product code 或运行 code bytes 证明与当前手机逐字节相同 |

core 中的 `EPOCARM5ROM` 位于文件偏移 `0x716AEA`，ROM header 位于 `0x716BEA`，
ROM base 为 `0x80000000`、ROM size 为 `0x01F00000`。header 记录的 pageable 起点/大小为
`0x00AA4000/0x013ADA50`；compressed-unpaged 起点、压缩大小、展开大小为
`0x00042D88/0x017BC000/0x01E52000`。ROM-FS 目录能够可靠解析出：

| ROM-FS 文件 | `TRomEntry.iAddressLin` | `TRomEntry.iSize` | 属性 | 当前限制 |
|---|---:|---:|---:|---|
| `/Sys/Bin/MediaClientVideo.dll` | `0x809E12C0` | 44,372 | `0x81` | 这是 ROM entry/image 地址，不是 DLL code address；FPSX 中仍为 BytePair 压缩；随后取得同大小的目标手机逻辑文件副本，见下节 |
| `/Sys/Bin/MediaClientVideoDisplay.dll` | `0x809EC090` | 20,236 | `0x81` | 同上；随后取得同大小副本，但仅作导入/调用关系边界检查 |

`MediaClientVideo.dll` 的 `(iAddressLin - romBase)=0x009E12C0`，小于 pageable 起点，故它是
unpaged 逻辑地址；这不代表 FPSX 中对应位置是未压缩文件。单靠 core 当时不能恢复该 ROM DLL 的
header/code；后续手机副本解决了 E7 本模块的 header 和主体代码缺口，但没有反向证明整个 core
候选与当前手机逐字节一致。SDK import stub 的 UID/ABI 仍只作为公开 ordinal/ABI 辅助证据。

把内嵌 ROM 从 `EPOCARM5ROM` 起点送给现有 `readimage.exe` 会得到
`Corrupted BytePair compressed ROM image`；直接输入 FPSX 则为 `Unknown Type`。因此当前不能从
该 core 单独得到可用于断点的 `MediaClientVideo.dll` code hash、E32 header 或函数偏移。
`TRomEntry.iSize` 仍不能当作运行模块 CodeSize。

主机只读解析器为 `tools/research/e7/rom_fs_inspect.py`。它只确认 ROM header 与目录元数据，
不会把 `iAddressLin` 伪装成 code address，也不会猜解压后的函数地址：

```powershell
$python = 'python'
& $python tools\research\e7\rom_fs_inspect.py `
  '<private-input>\RM-626_111.040.1511_79u_prd.core.fpsx' `
  'MediaClientVideo.dll'
```

### 目标手机文件副本

用户从当前参与黑屏试验的 E7 的 `Z:\sys\bin` 复制两份文件，并以只读文件提供。原件保持
`ReadOnly`；分析副本位于被 git 忽略的 `.tmp/e7-mmf-runtime/`，反汇编使用另存的 ELF wrapper，
没有修改或替换手机系统文件。

| 手机来源 | 大小 | SHA-256 | 文件表示与范围 |
|---|---:|---|---|
| `Z:\sys\bin\MediaClientVideo.dll` | 44,372 | `810AB8E280353FAEDABD840CAD66ABC020603D0BD104A6604097406291978809` | 以完整 `TRomImageHeader` 开头的 ROM 逻辑文件，不是普通 E32Image/压缩容器；含44,252字节可映射主体代码 |
| `Z:\sys\bin\MediaClientVideoDisplay.dll` | 20,236 | `901E257D0AD33CB111E935A7C593D33E2121E1A5454A85398045D7F39EBC531C` | 同一表示；只用于有限关系检查，不展开图形栈 |

两份大小分别与 E7 core 的 ROM-FS `TRomEntry.iSize` 完全相同。`MediaClientVideo` header 为：
UID1 `0x10000079`（DLL）、UID2 `0x101F4549`、UID3/SID `0x101F4569`、EABI/EKA2，
`iCodeAddress=0x809E1338`、`iDataAddress=0`、`iCodeSize=iTextSize=0xAD48`、
module version `0x000A0001`、166个导出；`iExportDir=0x809EBDE8`，
`iDllRefTable=0x809EC080`。header code address 与上一设备进程实际模块事件的
`CodeAddress=0x809E1338` 精确相同，且 header 位于 core entry `0x809E12C0` 后 `0x78`，这是
当前手机模块与该 E7 条目的强身份关联；它不证明整套 ROM 或所有运行 code bytes 逐字节相同。

文件长度恰为 `iCodeSize+12`，不是 `sizeof(TRomImageHeader)+iCodeSize`。因此本地文件从
`+0x78` 起只覆盖声明 code range 的44,252字节，即运行映射
`[0x809E1338,0x809EC014)`；声明范围为 `[0x809E1338,0x809EC080)`，末108字节不在文件中。
这与手机文件服务返回的完整 ROM 逻辑文件大小相容，但不能把缺失范围臆造为已转储代码。
export directory 从文件 `+0xAB28` 开始，副本中只有前139/166项；DLL reference table 不在副本
内。主体指令和本次 HandleEvent 所需常量均位于可用范围内。

`MediaClientVideoDisplay` 的 UID2/UID3 为 `0x1000008D/0x10286695`，EABI/EKA2，
`iCodeAddress=0x809EC108`、`iCodeSize=0x4F00`、29个导出；其逻辑副本同样缺少声明范围末108字节。
在 `MediaClientVideo` 全部现有字节上以任意字节对齐扫描，没有一个32位绝对指针落入 display
声明 code range，也没有 display DLL 文件名。仅有 `CMediaClientVideoDisplayBody` RTTI 类名，
说明主 utility 自身包含同名内部类，不等于导入第二个 DLL。由于 DLL reference table 缺失，
“零指针命中”不能排除 ordinal import、动态加载或缺失尾部中的引用；但当前 Prepare 分支没有
调用 display 模块，故按范围约束不继续反汇编它。

可重复校验器为 `tools/research/e7/analyze_rom_module_copy.py`。它按目标 SDK `e32rom.h` 的
`TRomImageHeader` 布局解析，不以 `CodeAddress..DataAddress` 或文件长度代替 code range，并明确
报告缺失尾部与可用导出项：

```powershell
& $python tools\research\e7\analyze_rom_module_copy.py `
  .tmp\e7-mmf-runtime\MediaClientVideo.dll `
  --related-module .tmp\e7-mmf-runtime\MediaClientVideoDisplay.dll `
  --verify-e7-prepare-handler
```

SDK 中只有导入库，不是设备实现：

| SDK 候选 | 大小 | SHA-256 | 身份限制 |
|---|---:|---|---|
| `Symbian3Qt474/.../mediaclientvideo.dso` | 14,260 | `A984B06D367A4E3D8DCA63C3CE0CDF53CA959561BE0EF80812A6A29015D86B07` | ELF ARM EABI import stub，只有公开 ordinal/ABI |
| `Symbian3Qt474/.../mediaclientvideo.lib` | 156,350 | `F1B28D60EA64A1D0F027F52A13996F0EFA51BB2D47D8C0FF01367323CBD762DD` | import library，不含运行实现 |
| `SymbianSR1Qt474/.../mediaclientvideo{000a0000}.dso` | 14,260 | `FDD505F9B72D48C88FD154729584CAC1D88255E28D6B0784170F30EDBD7A1610` | 另一 SDK import stub，不能替代当前设备 |
| `SymbianSR1Qt474/.../mediaclientvideo{000a0001}.dso` | 14,412 | `A04FBDE3C74A8058F2D1F95C44765B9404C53B9C4F7FB77B7381750AC6D6A9C1` | 同上 |

这些文件的 SONAME/component UID `0x101F4569` 可辅助核对公开 ABI，但内部
`CBody::HandleEvent` 不在导出表中。当前产品树、并列研究材料和 Qt SDK 中也没有匹配的
`.map/.sym/.elf` 实现映射。

## 有限 E7 / Nokia 603 静态对比

给定的 `<private-firmware-root>` 中已有 Nokia 603 RM-779 core，但版本是 `111.020.0310`，不是设备矩阵
此前验证的 `113.010.1506`。主机只读搜索另在 `<private-nokia603-root>` 找到
`113.010.1506` 自解压包；仅把它的 common core 提取到本地忽略目录 `.tmp/rm779-sw113-core/`
用于本次静态比较，没有覆盖原件或把 ROM 放入产品树。

| 机型/材料 | 原始材料身份 | core 身份 | 与设备事实的关系 |
|---|---|---|---|
| E7 RM-626 `111.040.1511` | installer 643,920,257 bytes，SHA-256 `FEF879FB9CAEDAD432906167DDF749E24BE5F0E86BD53E8677B11FFAD60CA4AE` | 129,431,499 bytes，SHA-256 `7B66B1D22914DCEB4313BB3CD2732B40D6E314CBE24B212432A4C388F0D1BD00` | 目标 RM/SW core 候选；设备 product-code/运行字节尚未匹配 |
| 603 RM-779 `111.020.0310` | installer 676,824,099 bytes，SHA-256 `81BE79F0510BD878E5A72CB759CD39342865CADAA624DDE50A9C80A094C11AD2` | 122,821,011 bytes，SHA-256 `239C6CB69C42C8DAB8A526C742D2FE120DA22516E1D2894EFC2B069D9D697760` | 只作额外旧固件静态参照，不能代表已验证 603 |
| 603 RM-779 `113.010.1506` | self-extracting installer 266,497,084 bytes，SHA-256 `A48776CE5C7A0F634BF958297683F99CFB8E9934D8185399DC51E1632E37961D` | 138,794,517 bytes，SHA-256 `2733170B188FBB7744AD2EC834991FBCC8DE1BFA57408B58682EB2981C37F630` | 固件版本与设备矩阵的 603 基线一致；尚未用本次 MediaClient 运行字节证明 product-code/core identity |

### MediaClientVideo

| 项目 | E7 `111.040.1511` | 603 `111.020.0310` | 603 `113.010.1506` |
|---|---:|---:|---:|
| ROM-FS `iAddressLin` | `0x809E12C0` | `0x809D6DA0` | `0x8097BCE0` |
| ROM-FS `iSize` | 44,372 | 44,372 | 47,992 |
| 属性 | `0x81` | `0x81` | `0x81` |
| 存储区 | compressed-unpaged | compressed-unpaged | compressed-unpaged |
| 手机逻辑文件/code | 已取得；header及主体代码可读，声明 code 尾108字节不在副本 | 未取得 | 未取得 |
| ROM DLL UID/ABI、imports/exports | UID3 `0x101F4569`、ARM EABI/EKA2；166个导出中前139个可读，DLL ref table缺失 | UNKNOWN | UNKNOWN |
| 可恢复内部函数 | Prepare event handler及转发分支已恢复 | 无 | 无 |
| Prepare event 处理/错误传播 | 对 category `0x101F7F86` 直接转发 event `+4`，不改写 | UNKNOWN | UNKNOWN |

E7 与 603 SW111 的条目尺寸相同，不能据此断言实现相同；603 SW113 的条目大3,620字节，也不能据此
断言 Prepare 逻辑变化。三套内嵌 ROM 都被目标 SDK `readimage.exe` 以
`Corrupted BytePair compressed ROM image` 拒绝。此前 603 ref7 工作成功重建的是 pageable
`ivevideodecodehwdevice.dll` XIP 页，并且曾与设备文件同哈希；本次三个 `MediaClientVideo.dll`
都在 compressed-unpaged 逻辑区，那条重建链不能直接复用。手机副本只解决了 E7 一侧；603 两侧
仍无代码，故仍不能做逐指令 utility 差异比较。当前能证明的是 E7 Prepare 转发不改写 error，
不能把603“未出现问题”解释为 utility 转发不同。

### Controller 注册与实现

固定比较器在三个原始 core 中只检查 `0x101F8514`、`HxMmfCtrl`/`Real Video Player` 及相邻格式
记录。E7 与 603 SW111 的 3,072-byte 局部注册窗口逐字节一致，SHA-256 均为
`3B1D76981EC03776CEBA32E847876F9A5CA3424E75429A0CBFFA98D12F9D3EF2`。603 SW113 仍有同一
implementation UID `0x101F8514`、`Real Video Player` 和 MP4/AVC1 格式记录，但名称大小写变为
`hxmmfctrl`，一个未解释的序列化 `<a>` 值也从 `0x1000000` 变为 `0x4000000`；其局部窗口摘要为
`5DAF9A8FF83874CF9A06BDF451D22ADDD3984289993771DF7645DD2EC3C6BD41`。不解释该字段语义，也不把
记录字节变化当作缺陷。

| 模块/记录 | 两侧相同性 | 实现身份限制 | Prepare 能力结论 |
|---|---|---|---|
| HxMmfCtrl / Real ECom registration | 三套语义上均注册 `0x101F8514`；E7 与603 SW111局部字节相同，603 SW113局部字节不同 | DLL UID3 `0x101F8513` 与 implementation UID 是静态注册证据；实际 Qt utility 选择仍 UNKNOWN | 注册支持 MP4/AVC1，不证明某次 Prepare 走该实现或如何汇总错误 |
| HxMmfCtrl implementation code | 603两套均未恢复；E7随后由同机文件副本恢复 | E7函数偏移和数据流可用，但仍须以未来同次模块加载和code fingerprint关联运行session | controller内部汇总链已恢复；具体下游视频服务创建调用仍UNKNOWN |
| 目标 PP | 未加入比较 | 当前没有同一 utility/controller 会话的证据指向某个 PP | 不用旧 PP 零统计扩展范围 |

本比较的实际收益仅是：603 没有提供一个不同 controller 家族作为解释；SW113 也显示同一
implementation UID 可以伴随不同 utility/注册表示，因而更不能套用 603 地址或成员偏移。它没有
帮助命名 E7 的原始视频初始化失败；E7 代码恢复来自手机文件副本而不是603。双 ROM 比较到此停止。

可重复比较器为 `tools/research/e7/compare_mmf_roms.py`；输出只含容器偏移，不产生运行地址：

```powershell
$python = 'python'
& $python tools\research\e7\compare_mmf_roms.py `
  'E7_SW111=<E7 core.fpsx>' `
  'RM779_SW111=<603 SW111 core.fpsx>' `
  'RM779_SW113=<603 SW113 core.fpsx>'
```

## 同机 HxMmfCtrl 有限静态分析

用户随后从当前参与黑屏试验的同一台E7提供`Z:\sys\bin\HxMmfCtrl.dll`。原件只读，分析副本、
Petran展开文本、重建code和ELF wrapper均保存在git忽略的`.tmp/e7-mmf-runtime/`，没有替换手机或
产品文件。

| 项目 | 结果 | 身份限制 |
|---|---|---|
| 原始文件 | 158,569 bytes；SHA-256 `E01F4F11B360C21DD536D6F23B27167FD41B7AF29D2339E9E52944A66A5B69CD` | 手机文件副本身份；未来运行仍须用同次加载事件和短code fingerprint关联 |
| E32身份 | E32Image V2.04；UID1/UID2/UID3 `0x10000079/0x10009D8D/0x101F8513`；module `10.18`；ARMV5 EABI/EKA2；BytePair `0x102822AA` | UID3、版本和文件名不能单独证明某次session选择了它 |
| 声明code | linked base `0x8000`；code/text size `0x41970`；data size 0；19个DLL import、7个export | `0x8000`是链接地址，不是未来运行地址；运行地址只能用同次`CodeAddress+code offset` |
| Petran展开code | 268,656 bytes；SHA-256 `F2E371650460097CEC108CEACA10B9187B8578D2360BBFF6523688671CB864DE` | 这是从压缩E32文件重建的未重定位code，不与运行整段直接比较哈希 |

`tools/research/e7/analyze_hxmmfctrl.py`会从目标SDK Petran的code dump重建精确小端字节，并输出
header、函数窗口fingerprint及117项`HX_RESULT→TInt`表。它不猜运行基址：

```powershell
& $python tools\research\e7\analyze_hxmmfctrl.py `
  .tmp\e7-mmf-runtime\HxMmfCtrl.dll `
  --petran-code-dump .tmp\e7-mmf-runtime\HxMmfCtrl.petran-code.txt `
  --json
```

### 已恢复的控制器内部链

构造函数code `+0xE64`在`+0xE6C/+0xE70`把linked `0x45560`写入对象首字。旧记录误把同样重置
vtable的析构函数`+0x1108`命名为构造；已按分配调用者和base ctor/dtor复核纠正，vtable数值不变。
该E7 vtable的`+0x88/+0x8C/+0xDC/+0xE0`
依次指向linked `0xBA98/0xC054/0x165D8/0x93CC`，结合函数内嵌名称和调用数据流可复核为
`OnError`、`OnPrepareComplete`、controller `SendEvent`等效实现和`MvpcPrepare`。以下offset均相对
运行加载事件的`CodeAddress`，指令集均为ARM：

| 函数/点位 | code offset | 32-byte SHA-256 | 已证明的数据流 |
|---|---:|---|---|
| `MvpcPrepare` | `+0x13CC` | `3002C431271837C8CD1411A77E5AD113D1C2A50511770176373CAAD0A44B6B7F` | code `+0x16C0`经`[this+0x38]→+0x54→vtable+0x74`调用状态控制器；非零返回以event index 2直接送`SendEvent`，LR为`CodeAddress+0x16F8`；零返回把`[this+0x48]`置2 |
| `OnError` | `+0x3A98` | `8B0A9C9EB366D593D84544FB4245DB7FB0CD98F920CB2FF35713790D658C8777` | 入口`r0=this, r1=severity, r2=HXCode, r3=userCode, [sp]=userString, [sp+4]=moreInfoURL`；控制器会按状态/既存`[this+0x94]`选择最终聚合值并写`[this+0x80]`，不能把该成员无条件等同本次`r2` |
| `OnPrepareComplete` | `+0x4054` | `138B2146A3B4E55CFE6CCD9A54256C0C4852476B991C39B9ACFC00E121D6C261` | 从`[this+0x80]`取聚合HX结果，以event index 2调用`SendEvent`，LR为`CodeAddress+0x40AC`，随后状态置3 |
| HX结果映射器 | `+0xA7A4` | `ECA1A2FB62CF8B5AEE0FFA7D4EA2A64979FB0A61B99D70796DF9D479BCC99BFF` | 线性查找code `+0x3A528`起的117项`{HX_RESULT,TInt}`表；默认输出`KErrGeneral(-2)` |
| `SendEvent`等效实现 | `+0xE5D8` | `E81D25B81CCC3801E497EE75730BFD584EAC2774B470A32376DDACA141FD053C` | 入口`r0=this, r1=内部event index, r2=映射前HX_RESULT, r3=extra`；code `+0xE638`调用上述映射器，之后通过`[this+0x84]`送MMF事件，并在末尾清零`[this+0x80]` |

映射表第38项位于code `+0x3A658`：`{0x00040024, -12017}`；整表SHA-256为
`8716FD5624516A85E49155F7C5B289FC8046EDB2D5F5F19021AB58AC09D3156A`。因此已经证明：若MMF
Prepare收到`-12017`，该E7 controller边上的直接前值是Helix `HX_RESULT 0x00040024`；仍未证明
`0x00040024`的符号名、产生它的具体视频服务/decoder调用或更底层OS错误。

文件中没有`HLX_MDF_VIDEO_SERV`字符串，也没有一个可据此命名的视频服务DLL import；存在
`DT_Plugins`/`DT_RCAPlugins`等动态插件配置及`HXMMFPlayCtrl::InitIHXPlayerL`。进一步的数据流给出
一个具体下游边界：code `+0xD48C`的`InitResourcesL`等效函数调用`+0xD238`，后者经单例进入
code `+0xEE00`的loader；loader明确加载字符串`hxmedpltfm.dll`，并解析
`HXMediaPlatformOpen`、`HXCreateMediaPlatform`、`HXMediaPlatformClose`、`CreateEngine`、
`CloseEngine`和`SetDLLAccessPath`六个入口。`+0xD238`随后先调用Open，再调用Create并把结果对象写入
controller base的`+0xD8`；loader/符号缺失或Create失败会从该函数返回负HX结果。三个函数的
32-byte fingerprint依次为`5C31B2B1016D906B6329D524B4524170E2754587F5D4761F6CBE3A505362ECED`、
`53324E914A1FDE3F7EE1257A198F69F7D98D6E59C52547BAC78A1BA9B3643497`、
`097C808CCFA0AB20B5C74C791D6E16B49B5C0432EBC1A9C5D70946EDB8687810`。

这证明`hxmedpltfm.dll`是Hx控制器创建Helix media platform的实际动态边界，但仍不证明
`HLX_MDF_VIDEO_SERV`线程一定由这六个入口直接创建，也不证明观察到的Prepare `0x00040024`来自
这段早期platform初始化：现有失败会话已经完成controller加载并到达Prepare，更可能的失败点仍在
该platform或其插件的后续视频初始化。因而本模块恢复了**platform交接和失败汇总链**，没有恢复
视频服务线程的具体创建实现。先前失败会话缺少该线程仍只是相关边界，不能写成“创建调用失败”。
`MediaClientVideoDisplay.dll`不在本模块的导入表和本次Prepare链中，故不扩展图形栈分析。

### 同机 hxmedpltfm 有限静态分析

用户从同一台E7手工复制了`Z:\sys\bin\hxmedpltfm.dll`。这也修正了此前记录中的拼写差错：
HxMmfCtrl展开code内的实际ASCII字符串和用户文件名均为`hxmedpltfm.dll`，不是
`hxmedpltfrm.dll`。原件只读，副本、Petran文本和展开code只保存在忽略目录
`.tmp/e7-mmf-runtime/`：

| 项目 | 结果 | 身份限制 |
|---|---|---|
| 原始文件 | 143,580 bytes；SHA-256 `4B89693DE030667BBA0F3EA1E5134893CB0F0D5FF23C0E7D737526A54297923A` | 用户声明来自同一手机；既有运行没有为本模块单独保存code fingerprint，故仍须未来同次加载事件关联 |
| E32身份 | E32Image V2.04；UID1/UID2/UID3 `0x10000079/0/0`；module `10.18`；ARMV5 EABI/EKA2；BytePair `0x102822AA`；8个DLL import、13个export | UID2/UID3为0使文件名和版本更不足以单独关联运行模块 |
| 声明code | linked base `0x8000`；code/text size `0x3D794`；data size 0 | linked地址不是运行地址；未来只用同次`CodeAddress+code offset` |
| Petran展开code | 251,796 bytes；SHA-256 `3F9C0198425BF3AA6447E37CFBE2579DB902BE1B61146D0C20FC298E4535C8C9` | 未重定位code，不能与压缩文件或未来整段运行映像直接比哈希 |

模块自带的八项name→ordinal表位于code `+0x3752C`，与13项E32 export目录交叉验证后得到：

| 名称 | ordinal | export code offset | 实际主体 | 32-byte主体SHA-256 |
|---|---:|---:|---:|---|
| `GetSymbolOrdinal` | 1 | `+0x4A8C` | 同export | `B870B5434F08F4D0BD3C7D4C6E04F35C429FF8C411FF1761BA351BC9D6FEDEC5` |
| `FreeGlobal` | 2 | `+0x4AE4` | 同export | `DA6558588472AE479D19AB97B26B9F1B8767548DE013024BEF4546583709BAD8` |
| `HXMediaPlatformOpen` | 3 | `+0x46DC` | `+0x43F8` | `70420B21D9AD34E5060EA8D1C52CADF565AB05C488521E06104125CEA9439E8E` |
| `HXCreateMediaPlatform` | 4 | `+0x46E0` | `+0x4418` | `738F6B1DD21EE96578F9DB53CC9E82BEAC91123A7A32A6C883B2A2960F9BBA55` |
| `HXMediaPlatformClose` | 5 | `+0x46E4` | `+0x44F0` | `80BDEEA3E3795E278FBB85D01E09CACF5A252D887C5FAA0721FFD783B97339D3` |
| `CreateEngine` | 6 | `+0x46F8` | 同export | `F6B936EC3887C870D8E7E92B561175509E493D2DEC1B8795A8131C775D793ED6` |
| `CloseEngine` | 7 | `+0x48C4` | 同export | `C488DD3E7A5577E10557507B10215D9C7795621188A799DBAEC05776A7EF75B7` |
| `SetDLLAccessPath` | 8 | `+0x43E0` | 同export | `D31624E516E68A5E2E5280DF01AD803220156CC616870DF7C63BCACCCE864781` |

`tools/research/e7/analyze_hxmedpltfm.py`可从Petran dump重建code、验证上述表和export，并生成稳定
fingerprint；它明确不把linked base当运行base。静态数据流显示：

- `HXMediaPlatformOpen`直接返回0；`HXCreateMediaPlatform`可返回未Open的`0x80040009`、分配失败
  `0x8007000E`，或原样返回内部QueryInterface调用结果；
- `CreateEngine`成功返回0，分配失败为`0x8007000E`，其本地初始化失败汇总为`0x80004005`；
- 全展开code中没有literal `0x00040024`，也没有`HLX_MDF_VIDEO_SERV`字符串；有`DT_Codecs`、
  `DT_RCAPlugins`、`DT_Plugins`和`IHXMediaPlatform*`结构字符串，但没有静态下游codec DLL名。

因此这份DLL验证了Hx控制器所调用的platform/engine边界及其早期返回条件，却没有把本次Prepare
聚合值定位到这些导出：`0x00040024`更可能由创建后的engine/动态插件路径产生或转发。literal缺失
不排除计算值或任意下游返回；同样不能因线程字符串缺失便断言本模块不参与创建。当前没有证据支持
在`HXMediaPlatformOpen/CreateEngine`上增加设备断点：它们发生在Prepare前的公共初始化，不能回答
失败分支的原始视频错误，额外暂停反而增加时序干预。

## E7 controller 候选与 Qt 关联限制

E7 core 中 `HxMmfCtrl.dll` 宽字符串/邻近 DLL 记录约在容器偏移 `0x1EF9DF7`；implementation UID
与 `Real Video Player` 的序列化 ECom 记录约在 `0x7B6135F`。`0x53E6A0D` 附近只有经过 BytePair
替换的零散路径字符，不能可靠恢复成完整 `hxmmfctrlimpl.cpp` 来源字符串，故不再把它列为独立
身份依据。以上容器位置都不是函数或运行 code offset。

Qt Mobility 候选源码也定义 `KHelixUID={0x101F8514}`，并在候选 `doLoadL` 中调用
`OpenFileL(path, KHelixUID)`。但上一阶段只对实际模块中的 Prepare/apply/Play 三段建立了
fingerprint identity gate，未对 `doLoadL` 建立运行匹配。因此，**HxMmfCtrl/Real 是强静态候选，
不是当前 Qt utility 已实际选择该 controller 的证明**。同名、相同 UID 或旧 native 会话的 Real
记录都不能补齐这项关联。

后续八次Qt内部运行全部出现`MMFControllerProxyServer-*`线程；五个Prepare 0/可见会话另出现
`HLX_MDF_VIDEO_SERV*`，三个`-12017`/黑屏会话没有。这是实际Helix视频服务路径及其失败边界的
运行证据，但仍没有把ECom implementation UID `0x101F8514`从当前utility/controller对象成员中读出，
故精确controller实现UID继续标为UNKNOWN。

## Prepare 错误传播图

```text
Qt 候选 doLoadL
  OpenFileL(path, 0x101F8514)             [候选源码；本次运行未证明]
             │
             ▼
实际 MMF controller / implementation UID  [精确UID仍UNKNOWN]
  MMFControllerProxyServer-*                [8/8运行线程事件]
  HLX_MDF_VIDEO_SERV*                       [5个0/可见有；3个-12017/黑无]
             │                              [线程缺失不等于创建失败]
             ▼
同机 HxMmfCtrl Prepare汇入                 [E7同机文件静态证明；运行关联待做]
  MvpcPrepare同步非零返回 ───────────────┐
  OnError(r2=HXCode,r3=userCode)          │
    → 按状态聚合到[this+0x80]             │
    → OnPrepareComplete读取聚合值 ────────┤
                                         ▼
  SendEvent(event index=2, raw HX_RESULT)
    mapper表: 0x00040024 → -12017          [E7表项静态证明]
             │
             ▼
  Prepare 完成后生成 TMMFEvent            [SDK 接口契约]
  category = 0x101F7F86
  error = 0                                 [本轮4次直接读取raw event]
        = -12017                            [旧3次保存调用现场+E7数据流]
             │
             ▼
MediaClientVideo event-monitor thunk       [E7代码已恢复]
  r0 = CBody+8, r1 = &TMMFEvent
  thunk: r0 -= 8 → HandleEvent等效实现
             │
             ▼
MediaClientVideo HandleEvent等效实现       [E7代码已恢复]
  category匹配0x101F7F86后：
  observer = [CBody+0x518]
  error = [event+4]
  observer.vtable[1](observer, error)       [原样转发，无转换/汇总]
             │
             ▼
MVideoPlayerUtilityObserver::
  MvpuoPrepareComplete(TInt)               [公开 observer 契约]
             │
             ▼
Qt S60VideoPlayerSession::
  MvpuoPrepareComplete(TInt)               [设备已证明：0 或 -12017]
```

传播边的证据强度不同：四次在Qt入口直接读取`r5`所指event并校验UID/error/LR；旧三个
`-12017`会话没有保存event内存，但保存的LR和`r5`证明来自同一E7转发调用现场，而目标指令明确从
`[r5+4]`装载传入`r1`。因此“utility原样转发”已经得到静态和动态联合证明；controller内部为何没有
启动Helix视频服务、以及哪个调用产生`0x00040024`仍未证明。Hx控制器内部链目前是同机文件静态
证据；在未来同一运行中通过模块fingerprint和对象vtable命中前，不把它写成该session的动态事实。

常量 `KMMFEventCategoryVideoPrepareComplete={0x101F7F86}` 来自目标 SDK 的
`mmf/common/mmfvideo.h`。`TMMFEvent` 的目标 ABI 布局由
`mmfcontrollerframeworkbase.h` 给出：`iEventType` 在 `+0`，`iErrorCode` 在 `+4`，
`iReserved1` 在 `+8`，共12字节。`RMMFVideoPlayControllerCustomCommands::Prepare()` 的接口注释
规定 controller 通过该 category 的事件报告完成。

目标 E7 Thumb 代码把入口 `r1` 保存为 event 指针；接口 thunk 的四字节为
`subs r0,#8; b HandleEvent`，证明入口 `r0=CBody+8`。主实现以相邻 open/prepare/play-complete
UID 表为联合证据定位 Prepare case：literal 指向文件 `+0x99F4` 的 open UID，`+4` 恰为文件
`+0x99F8` 的 `0x101F7F86`。匹配后代码执行
`ldr r0,[CBody+0x518]`、`ldr r1,[event+4]`、读取 observer vtable `+4`，再 `blx`；两者之间没有
错误码判断、替换或汇总。二进制内也没有 `-12017` 字面值。后者单独不能排除计算生成错误，
但该 Prepare 分支的直接数据流已经排除了 utility 在此边上改写输入值。

公开 ordinal 由目标 SDK `mediaclientvideo.dso` 辅助命名，并由 E7 本文件 export table 前139项
定位。ordinal 135～139 的 `CVideoPlayerUtility2` wrapper 都先读取 `[utility+8]` 再调用内部
实现，证明公开 utility 的 body 指针偏移为 `+8`。因此未来同一暂停可验证
`[QtSession+0x78]+8` 指向的 CBody 是否等于 thunk `r0-8`。HandleEvent 的 observer 字段为
`[CBody+0x518]`；上一阶段 E7 实际 Qt callback thunk 已证明 observer subobject 为
`QtSession+0x50`。这给出不主动调用播放器方法的同一对象关联门：

```text
QtSession +0x78 -> CVideoPlayerUtility2
utility +0x08   -> CBody
CBody +0x08     == raw-event thunk r0
CBody +0x518    == QtSession +0x50 (observer subobject)
```

controller session/implementation UID 尚未从 CBody 成员中可靠恢复；该 UNKNOWN 不由旧 native
会话的 Real 身份或静态 ECom 注册补齐。

## 已完成的 MediaClientVideo 观测映射

最初最靠近原始失败的可靠E7映射是`MediaClientVideo` event-monitor接口thunk：文件
`+0x76B2`、code `+0x763A`、Thumb。CODA不能读取或在该ROM地址下断点，最终改在已可验证的Qt回调
入口观察。E7 handler把原始event指针保存在callee-saved `r5`，共享observer调用的Thumb LR为
`CodeAddress+0x762B`；实际四次LR均匹配，且`r5`内event可读。该替代点已经回答进入utility时的
raw error，并保持未匹配运行MediaClient fingerprint的限制。

| 拟观测点 | code offset / 指令集 | 身份依据 | 参数与读取内容 | 工具可行性 / UNKNOWN |
|---|---|---|---|---|
| event-monitor接口 thunk | `+0x763A` / Thumb（文件 `+0x76B2`） | 4-byte thunk `08 38 10 E7` 将 `r0` 减8并跳到下项；SHA-256 `9EFB250D43DBAE2B0049A6B345F999A1CE6F0D25223EB3717FFF5463F7D7C821` | 入口 `r0=CBody+8`、`r1=&event` | ROM code读取和断点均被CODA拒绝；未动态命中 |
| HandleEvent等效主实现 | `+0x7460` / Thumb（文件 `+0x74D8`） | 完整474-byte函数 SHA-256 `711FD616ECD6384247AF85AB2AE6444F1FDD3CF103BE6B513737571D0232B2C3`；Prepare窗口 `+0x74B6`、24 bytes SHA-256 `713F0E328713C20F26225E0ADE8E646C9D119A69CDB902FAE563F5FCDADA8A59` | `r0=CBody`、`r1=&event`；静态证明 Prepare case 把 `[event+4]` 原样传给 `[CBody+0x518]` 的第二虚函数 | 不需要作为首选断点；用于验证 thunk身份、成员偏移和转发边 |
| `S60VideoPlayerSession::MvpuoPrepareComplete(TInt)` | code `+0x74D2`，Thumb；Qt运行fingerprint已匹配 | 回调入口LR=`MediaClient+0x762B`；`r5`保留event；observer=`session+0x50` | 本轮直接读UID/error，旧日志复核传入参数 | 已完成；证明0与-12017均由utility上游带入 |
| Hx `SendEvent`等效实现 | `+0xE5D8` / ARM | 同机E32展开code、vtable `+0xDC`及调用数据流；32-byte SHA-256 `E81D25B81CCC3801E497EE75730BFD584EAC2774B470A32376DDACA141FD053C` | `r0=controller`、`r1=event index`、`r2=映射前HX_RESULT`、`r3=extra`、LR；同时读取`[r0]`和`[r0+0x80]` | **新的首选点**；`r1=2`是Prepare。LR `base+0x16F8/+0x40AC`区分同步返回/异步汇总 |
| Hx `OnError` | `+0x3A98` / ARM | vtable `+0x88`、内嵌trace字段及数据流；32-byte SHA-256 `8B0A9C9EB366D593D84544FB4245DB7FB0CD98F920CB2FF35713790D658C8777` | `r2=本次HXCode`、`r3=userCode`、栈上两个说明字符串；`r0=this` | 只在首选点证明走异步`OnPrepareComplete`后作为条件性第二点；不先堆断点 |
| controller proxy / Helix视频服务创建 | **函数/偏移UNKNOWN** | 8次均有proxy线程；5个0/可见有`HLX_MDF_VIDEO_SERV*`，3个-12017/黑无；Hx文件没有该线程名 | 目标是把OnError输入或同步Prepare返回继续关联到实际创建/初始化调用 | 缺少拥有线程创建/下游失败实现的同机模块；不得用线程缺失代替返回码 |

`Breakpoints.getCapabilities/getStatus` 未返回可枚举槽信息；实际Qt code读取、寄存器、堆内存、
断点和自动继续可用，但MediaClient高地址ROM header/code读取及断点均返回NotSupported。不得再把
Qt区域能力推广到ROM。获准替代门保留同次路径/base、Qt fingerprint和回调现场自校验，并明确
MediaClient运行fingerprint未匹配；后续controller点也必须先在E7自身模块建立可执行的身份与地址
换算，不能复用本段ROM地址或扩大扫描。

## 首进程运行代码读取脚本

`.tmp/e7-private-tools-20260910/coda_dump_module_code.py` 已按冻结 A 固定为
`nikiniki_e7_qt_window_probe.exe` / UID `0xE000B153` 和 `MediaClientVideo.dll`。默认只打印 dry-run
契约，不连接 CODA；实际执行还必须同时给出 `--execute`、确认词和私有串口参数。

脚本的范围门如下：

- 先拒绝已有冻结 A 进程，随后启动一个新进程并立即记录为已计次；
- 只自动继续非目标 Shared Library stop；其他停止原因立即中止；
- 目标模块名按 File/Name 的完整 basename、大小写不敏感匹配，不接受
  `MediaClientVideoDisplay.dll`；
- `CodeAddress` 只确定代码基址；`DataAddress` 可能是分离映射的数据地址，只记录而不作为代码
  结束边界。当前既往 CODA Shared Library 事件没有报告 `CodeSize`；若本次事件明确出现该字段，
  则优先使用其值；
- 事件没有 `CodeSize` 时，只在固定候选地址 `0x809E12C0` 读取32字节
  `TRomImageHeader` 前缀。该地址来自 E7 core 候选的 `TRomEntry.iAddressLin`，本身不是 code
  address；目标 SDK `e32rom.h` 证明 `iCodeAddress`/`iDataAddress`/`iCodeSize` 分别位于
  `+0x14/+0x18/+0x1C`；
- 只有运行时 header 的 UID1 为 `KDynamicLibraryUidValue (0x10000079)`、UID3 为
  `MediaClientVideo` component UID `0x101F4569`，且 header `iCodeAddress` 与同次模块事件
  `CodeAddress` 完全相等，才接受 `iCodeSize`。`iCodeSize` 必须为正，且
  `CodeAddress+iCodeSize` 不得溢出32位地址空间。地址不可读、短读、UID/基址不符、size 非正或
  地址相加溢出时
  立即停止，不改猜其他 header 地址、不扫描邻域，也不退回 `CodeAddress..DataAddress`；
- ROM entry 的44,372 bytes 不作为解压后 code 大小或上限。64 KiB 仅是读取量硬上限：若已验证的
  `iCodeSize` 大于64 KiB，只保存从 code base 起的64 KiB，并在 manifest 标成**部分转储**；不会
  因达到64 KiB而声称代码范围完整；
- 以1,024-byte小块读取，每块验证返回长度并保存地址、大小和 SHA-256；任一短读/拒绝即停止，
  不向前后扩大扫描，也不把部分数据标为完整；
- 原始运行 header 前缀、分块、聚合的 `runtime-code-region.bin`、manifest 和 CODA 日志只能写入仓库
  `.tmp` 下的新空目录。manifest 明确标记它是**已加载/已重定位运行代码**，不能与 FPSX 压缩表示
  或重建 E32 整文件直接比较摘要；同时保存运行基址、被接受的原始 CodeSize、实际读取长度、
  完整/部分标记及范围来源；
- UID 与同次加载基址匹配只接受这个运行时 header 作为本模块的范围来源，不证明整套设备 ROM 与
  离线 E7 core 候选逐字节一致；
- 不设置断点、不调用播放器方法、不写目标内存。成功或失败后都在模块暂停中 terminate，并再次
  查询 PID；未确认退出即报错。第一进程到此结束，不进入播放，也不在现场临时猜函数断点。

审核命令只做 dry-run：

```powershell
& $python tools\research\e7\coda_dump_module_code.py
```

用户另行明确开始后，实际命令模板为：

```powershell
& $python tools\research\e7\coda_dump_module_code.py `
  --execute `
  --confirm-counted-run COUNTED-E7-CODE-DUMP-1 `
  --serial <CODA串口> `
  --python-path .tmp\e7-qt-internal-20260907\pydeps `
  --output-dir .tmp\e7-mediaclient-runtime\run01
```

私有串口值、raw code 和日志不写入本文或公开树。

### 首进程执行结果

用户明确授权后只执行了代码获取进程 #1；没有设置断点、没有进入 Play，也没有继续 #2～#4。

| 字段 | 设备事实 | 证明限制 |
|---|---|---|
| 目标 | 冻结 A，PID `34779` | 启动前查询无同名进程；本次只到 Shared Library stop |
| 实际模块事件 | `Z:\sys\bin\MediaClientVideo.dll`，`CodeAddress=0x809E1338`，`DataAddress=0x00400000`，`RequireResume=false` | 事件没有 `CodeSize`；`DataAddress` 不参与范围计算 |
| 离线 header 候选 | E7 core 的 `TRomEntry.iAddressLin=0x809E12C0`；运行 CodeAddress 恰为其后 `0x78` bytes | `0x78` 与目标 ABI 的完整 `TRomImageHeader` 大小一致，强支持 entry/header 与运行模块的结构关联；仍不证明整套 ROM 与候选逐字节相同，也不给出 `iCodeSize` |
| header 读取 | 在同一模块事件上下文请求 `0x809E12C0` 起32 bytes，CODA `Memory.get` 返回 `Code=1 / AltCode=0xFFFFFFFB (-5) / functionality is not supported` | 没有返回任何 header bytes，故 UID、header `iCodeAddress` 和 `iCodeSize` 均未能动态校验；不能退回地址差或 ROM entry size |
| 输出/退出 | 未生成 code/header 二进制；只保留本地忽略日志 `coda.jsonl`，4,729 bytes，SHA-256 `3EA25875F1EAB5CC3FA8B941A0F6753A5B5DAA783731623627FA2FD5B1113509`；terminate 后查询 PID 列表为空 | 本次是失败的代码获取启动，不是播放结果；其余三次预算保持未使用 |

因此当前动态阻塞已精确化为：CODA Shared Library 事件只给出 code base，不给 code size；Memory
service 又拒绝读取高地址 ROM image header。仅知道 `CodeAddress=entry+0x78` 不能恢复 header 中的
`iCodeSize`。当前日志没有区分“该 CODA Memory 实现普遍不支持高地址 ROM header”与“该上下文/
区域不支持读取”，故不把错误扩大解释为设备安全或 ROM 不可读。

## 设备取证预算与停止状态

用户在参数失败后明确把目标改为 **4 次产生核心数据的冻结最小 Qt A 新进程**；核心问题之外的
工具/参数失败只记录、不计入这四份数据。成功/失败组合不能预定，也不为凑
`0/可见` 与 `-12017/黑屏` 无限重复。用户随后明确授权使用同机文件派生、但在命中现场自校验的
替代身份门；四份有价值数据现已全部取得，预算止于4/4。

| 次数 | 目的 | 固定动作与停止点 |
|---|---|---|
| #1（已停止） | 模块身份/代码校准 | PID 34779 捕获实际模块和 code base；模块事件无 CodeSize，运行 header 的 Memory.get 返回 NotSupported；未读 code、未进入 Play、未设断点，已确认退出 |
| setup-1（已停止，不计有价值四次） | 首次 raw 脚本启动 | PID 35466；脚本遗漏冻结包必填 `--e7-backend=A`，应用记录 `mode=UNKNOWN / modeValid=0`，没有创建媒体、没有加载 Qt MMF 模块、没有设置断点；等待第二模块超时后 terminate并确认退出 |
| setup-2（已停止，不计有价值四次） | 修正参数后的能力门 | PID 35479，`mode=A / modeValid=1`；实际 MediaClient/Qt 模块基址为 `0x809E1338/0x7B320000`。首个 MediaClient code fingerprint 的 `Memory.get` 返回 NotSupported，未取得字节、未设置断点、未到 Prepare；terminate并确认退出 |
| setup-3（已停止，不计有价值四次） | 首次替代身份门 | PID 35491；模块路径/base及Qt两个fingerprint通过，但CODA拒绝在 ROM `0x809E8972` 设置 breakpoint，返回 NotSupported/-5；未到Prepare，terminate并确认退出 |
| V1 | Qt入口读取raw event | PID 35507；raw UID `0x101F7F86` / error 0；LR `0x809E8963`；session `0x02A32DA0`、utility `0x00803B28`；observer关系与同session Play通过；暂停15/62 ms；人工确认画面、声音、背景正常，无播放器UI |
| V2 | 同上 | PID 35527；raw UID/error、LR、session/utility与V1相同；关系与Play通过；暂停16/63 ms；人工结果同V1 |
| V3 | 同上 | PID 35547；raw UID `0x101F7F86` / error 0；LR `0x809E8963`；session `0x02A32D50`、utility `0x00803B28`；关系与Play通过；暂停16/16 ms；人工结果同V1 |
| V4 | 同上 | PID 35568；raw UID `0x101F7F86` / error 0；LR `0x809E8963`；session `0x02A32BC8`、utility `0x00803AF8`；关系与Play通过；暂停31/31 ms；人工结果同V1 |

所有次数使用冻结 A、`E:/test/test.mp4`、同一安装包，不重装、不换媒体、不运行其他媒体应用。
每次均为新进程；#2～#4 在 Play 后用 CODA terminate 统一退出，相邻启动至少15秒。断点切换必须在
目标线程暂停期间完成并确认成功；观测点暂停均少于100 ms。不开新
decoder/controller，不主动调用 `PositionL`、`GetFrameL` 或任何播放器方法。

`.tmp/e7-private-tools-20260910/coda_observe_mediaclient_prepare.py` 已修正参数并保持默认 dry-run。它明确
传递 `--e7-backend=A`，只监听
`MediaClientVideo.dll` 与既有 Qt MMF 模块。ROM断点被CODA拒绝后，获准模式不再声称匹配运行
MediaClient code fingerprint：它要求同机复制文件的header code base与同次模块路径/base匹配，
先匹配Qt Prepare/Play code fingerprint，再只在可下断点的Qt回调入口读取保留的`r5` event指针。
只有event UID为`0x101F7F86`、raw error等于Qt参数、LR等于E7转发调用返回地址
`MediaClientVideo+0x762B`，且`[utility+8]+0x518=session+0x50`、同session Play全部成立，才接受
为有价值数据。每次Play后固定观察60秒、terminate并确认PID退出。该门是调用现场自校验，**不是**
运行MediaClient代码逐字节匹配。

主机审核命令不会连接设备：

```powershell
& $python tools\research\e7\coda_observe_mediaclient_prepare.py
```

未来只有用户明确开始后才使用执行门；私有串口与输出仍只进忽略目录：

```powershell
& $python tools\research\e7\coda_observe_mediaclient_prepare.py `
  --execute `
  --confirm-counted-run COUNTED-E7-RAW-PREPARE-3 `
  --allow-file-derived-identity `
  --confirm-file-derived-identity AUTHORIZED-SAME-DEVICE-FILE-GATE `
  --observe-raw-event-at-qt-entry `
  --serial <CODA串口> `
  --python-path .tmp\e7-qt-internal-20260907\pydeps `
  --output-dir .tmp\e7-mediaclient-runtime\<value-run>
```

最初 code-size 路线已按门停止；手机逻辑文件现已完成离线映射，因此后续不再要求整段
CodeSize 转储。setup-1 的参数失败是工具错误，不是播放或MMF结果；日志位于忽略目录
`.tmp/e7-mediaclient-runtime/run02/coda.jsonl`，41,420 bytes，SHA-256
`7E07F3B43C4C6D459E9B43805BA2A71FE50772DC8ED74530E2F046464AA77492`。setup-2 日志位于
`.tmp/e7-mediaclient-runtime/valuable01/coda.jsonl`，41,244 bytes，SHA-256
`3F39077D5157D0CF03643D178AC5BF137DF39A425995B682F0DAFB8857B3620E`。setup-3另记录ROM断点能力
同样被拒绝。四份有效日志仍仅位于忽略目录，大小/摘要依次为
`127352 / 26AC375E...`、`127349 / F2E763AD...`、`125995 / 2C6CF9CE...`、
`126818 / 8083EFDA...` bytes/SHA-256。四次实际MediaClient/Qt基址均为
`0x809E1338/0x7B320000`，Qt fingerprint均匹配，运行MediaClient fingerprint仍未读取；四次均在
约60秒末保持Playing/Buffered、音视频available、position前进、公开error 0，这些字段不替代人工
首帧，但人工结果已经独立确认。四个新进程均在Prepare前出现`MMFControllerProxyServer-*`及
`HLX_MDF_VIDEO_SERV*`线程，之后进入raw event回调；这证明运行时使用了Helix视频服务路径，但尚未
把具体ECom implementation UID和当前utility/controller对象做成员级绑定。

对上一轮A/完整/完整/A保存的回调寄存器做离线复核，四次Qt Prepare入口的LR同样都为
`0x809E8963`；`r5`分别为`0x00435C70/0x004614C8/0x004614C8/0x00435C70`，与E7 handler保留原始
event指针的调用约定一致。对应Qt参数为`0/-12017/-12017/-12017`。目标E7代码已证明Prepare分支在
这个调用点执行`r1=[r5+4]`后直接`blx` observer，因此旧三次`-12017`在进入MediaClientVideo时已经
存在，utility没有把别的错误转换为`-12017`。这是保存调用现场与同机E7静态数据流的联合证明；旧
日志没有读取`[r5]`的event UID，不能虚构为当时直接读过12字节event。

同时，线程事件给出新的最早运行差异：旧唯一成功/Prepare 0会话出现
`HLX_MDF_VIDEO_SERV*`，旧三个`-12017`/黑屏会话只出现`MMFControllerProxyServer-*`而没有该
Helix视频服务线程；本轮四个0/可见会话也全部出现该线程。该5/5对3/3相关性把下一边界推进到
controller proxy创建/初始化Helix视频服务之前或该步骤本身，但线程名不等于原始返回码，也不单独
证明implementation UID。

## Hx 最小动态取证结果

`.tmp/e7-private-tools-20260910/coda_observe_hx_prepare_error.py`用于第一阶段；它不使用linked base `0x8000`
或任何旧运行地址，每次只接受同次Shared Library事件给出的
`HxMmfCtrl.dll`和Qt MMF `CodeAddress`。在设置断点前分别短读并匹配Hx的`OnError`、
`OnPrepareComplete`、`SendEvent`和Qt Prepare窗口；拒绝、短读或摘要不符立即停止，不扫描邻域。

首选且唯一预置的Hx断点为`same-run Hx CodeAddress+0xE5D8`，ARM、size 4。命中后只接受
`r1=2`的Prepare事件，记录`r0 controller`、映射前`r2 HX_RESULT`、`r3 extra`和LR；另读84字节
controller对象前缀，要求`[r0] == Hx CodeAddress+0x3D560`的E7 vtable，并记录聚合成员
`[r0+0x80]`。LR等于`base+0x16F8`表示`MvpcPrepare`同步返回路径，等于`base+0x40AC`表示
`OnPrepareComplete`异步汇总路径；其他LR只记UNKNOWN，不猜调用者。目标暂停时删除Hx断点、确认
Qt Prepare Thumb断点成功后才恢复，随后读取最终Qt error和`r5`保留的12-byte MMF event。脚本不
调用播放器方法、不读帧、不写目标内存，并在完成或失败后terminate及确认PID退出。

取得同机`hxmedpltfm.dll`后，后续`coda_observe_hx_onerror.py`准备稿把该模块加入精确library名单。
若同次load事件出现，只短读`CodeAddress+0x46F8`的32-byte `CreateEngine` fingerprint并立即继续；不在
Open/Create公共路径设置断点。没有load事件只记录UNKNOWN，不能解释为模块未加载。这样可把手工
副本与运行会话关联，同时仍把唯一机制断点留在Hx两处OnError和SendEvent，避免额外改变Prepare
时序。

```powershell
# 主机审核；不会连接或启动设备
& $python tools\research\e7\coda_observe_hx_prepare_error.py --run-id=hx-dry

# 已执行契约的复核模板；再次执行仍须单独授权
& $python tools\research\e7\coda_observe_hx_prepare_error.py `
  --execute `
  --confirm-counted-run COUNTED-E7-HX-PREPARE-BOUNDARY-1 `
  --serial <CODA串口> `
  --python-path .tmp\e7-qt-internal-20260907\pydeps `
  --output-dir .tmp\e7-hx-prepare-runtime\<run>
```

2026-09-08用户授权取得四份有价值数据；主机依赖失败和到达核心边界前的异常退出按用户口径只
记录、不计入四份。两次主机依赖失败均发生在创建CODA连接前：工作区Python缺少pyserial，以及
错误的同名`serial`包遮蔽缓存pyserial；都没有设备进程。另一次PID `35730`已启动并命中event 1，
但约6.2秒自行退出，未到Prepare；CODA随后等待超时，按停止门记录为无效启动，不能解释为Helix
创建失败。

四份有效内部数据均来自冻结`e7bindtiming1` A和同一样本A。脚本在首个Prepare `SendEvent`处停止，
所以本组没有人工首帧判断，也不以画面结果补齐内部链：

| 次数 | PID | Hx Prepare原始值 / 来源 | OnError与Helix线程 | 暂停与退出 |
|---|---:|---|---|---|
| H1 | 35694 | `0x00040024`；LR=`Hx+0x40AC`，即`OnPrepareComplete` | 未预置OnError；未出现`HLX_MDF_VIDEO_SERV*` | Hx非Prepare/Prepare及Qt各15/16/16 ms；退出确认 |
| H2 | 35712 | `0`；LR=`Hx+0x40AC` | 主入口`+0x3A98`零命中；出现Helix视频服务线程 | 31/16 ms；退出确认 |
| H3 | 35743 | `0x00040024`；LR=`Hx+0x40AC` | 主入口`+0x3A98`零命中；未出现Helix视频服务线程 | 16/16 ms；退出确认 |
| H4 | 35761 | `0`；LR=`Hx+0x40AC` | 主入口`+0x3A98`和调整接口入口`+0x3E10`均零命中；出现Helix视频服务线程 | 15/16 ms；退出确认 |

四次实际Hx base均为`0x7AF00000`，所有已用Hx code fingerprint匹配同机文件；H1还取得Qt base
`0x7B320000`及Qt fingerprint匹配。H1的`controller=0x1E300678`、vtable
`Hx+0x3D560`匹配，Prepare时`[controller+0x80]=0x00040024`；同次Qt保存event的UID为
`0x101F7F86`，event.error和回调参数均为`-12017`。因此已动态证明这次失败的实际controller路径是
Hx：`OnPrepareComplete`把聚合HX结果`0x00040024`交给`SendEvent`，随后同一事件以`-12017`到达Qt。
这不是仅凭静态注册或模块名字回填controller，也不是utility转换。

H2/H4的`0`与H1/H3的`0x00040024`再次和Helix视频服务线程出现/缺失形成2/2对2/2相关，但仍不能
把缺线程直接命名为线程创建调用失败。H3未观察到主入口，但其零命中不能单独证明路径未执行：
安装接受不等于断点实际命中，须保留断点能力/覆盖范围限制。
离线复核显示另有独立函数体`OnError adjusted interface +0x3E10`：入口`r0`先减`0x1C`恢复controller，
并在code `+0x3F48`把候选错误写入`[controller+0x80]`；它不是跳入`+0x3A98`的简单thunk。H4已证明
该附加断点和fingerprint可用，但H4是成功分支，因此尚未观察它在失败分支是否命中。

### 调整接口的六次有界复捕获与写入点复核

用户随后两次授权继续观察，且明确离开后不能提供画面状态。每组都预先硬限制为三个新进程，避免
用无限重复启动凑失败；六次都在首个Hx Prepare事件处停止并terminate，没有使用人工画面作内部
归因：

| 次数 | PID | Hx Prepare原始值 / 来源 | OnError与Helix线程 | 暂停与退出 |
|---|---:|---|---|---|
| F1 | 35823 | `0`；LR=`Hx+0x40AC` | `+0x3A98/+0x3E10`均零命中；出现Helix视频服务线程 | event 1/Prepare各16/16 ms；退出确认 |
| F2 | 35842 | `0`；LR=`Hx+0x40AC` | `+0x3A98/+0x3E10`均零命中；出现Helix视频服务线程 | 16/16 ms；退出确认 |
| F3 | 35860 | `0`；LR=`Hx+0x40AC` | `+0x3A98/+0x3E10`均零命中；出现Helix视频服务线程 | 16/0 ms；退出确认 |
| F4 | 35925 | `0`；LR=`Hx+0x40AC` | 两处OnError零命中；`hxmedpltfm`运行fingerprint匹配；出现Helix视频服务线程 | 15/15 ms；退出确认 |
| F5 | 35944 | `0`；LR=`Hx+0x40AC` | 同F4 | 15/15 ms；退出确认 |
| F6 | 35963 | `0`；LR=`Hx+0x40AC` | 同F4 | 15/0 ms；退出确认 |

六次Hx base均为`0x7AF00000`，四个运行fingerprint均匹配同机文件；Prepare controller均为
`0x1E300678`，其聚合成员为0。地址在不同进程中碰巧复用不替代每次独立identity gate。六份忽略
日志大小/SHA-256为：F1
`69667 / 18F244CD2820A9A50D2C033026E215FFC075C81CF4B0975050DEC050CFA79B2C`、F2
`69654 / 31BBC74F05F352C5F2EE220C9D8654EE6941E115E5EBF5081E00EC3F563169C4`、F3
`69653 / 761C4DCE0B51B871D6CFCC95CB2B6FE92F687B3F702367FA232E0A1C5F5950D0`；F4
`71087 / FC86B937C60300F0C6F51246948D57D925C1C21EB33C0446128529D034C8559B`、F5
`71075 / 3D02AB220449D648DE89E60EBAAE879E9CDA98332890146005DF9E8AED25DE2D`、F6
`71077 / DC4081B21D8826AAD47DF5A5B3839585F09F8A3317050CB8DC35D2E38915F358`。F4～F6还
把手工副本与运行模块闭环：同次`hxmedpltfm base=0x7CAC0000`，`CreateEngine+0x46F8`的32-byte
fingerprint全部匹配。它们仍只增加Prepare成功内部样本，没有回答调整接口在`0x00040024`失败分支
是否命中。

F4前有两次脚本执行在连接前发生主机失败，随后只读诊断确认：宿主Python把受ACL限制而无法枚举的
pyserial目录解释为空`serial` namespace；这些动作均未打开COM4、未创建CODA会话或设备进程。
修正仅位于研究工具：当已缓存同名模块缺少
`Serial`时清除错误缓存，并从本地忽略的只读pyserial ZIP加载；ZIP在同一宿主权限下验证后才执行
F4。这些是工具准备记录，不是MMF或播放器结果。

### 完整应用定向捕获的预MMF阻塞

用户当时授权改用此前重复失败的完整应用`e7fullappqt1`，目标是在最多三个**到达Prepare**的新进程
中捕获同一OnError→SendEvent链；画面和声音因用户不在现场统一为UNKNOWN。主机先复核并固定
`wiliwili_symbian.exe` / UID `0xE000B100`及唯一有效参数`--e7-mode=full`，脚本还要求运行日志同时
匹配`FULL-QT/e7fullappqt1/modeValid=1/complete-local/sample A/qt-qvideowidget/preflight disabled`。

两次启动均在媒体样本身份已经匹配、正式播放器对象已创建后，停在产品既有横屏状态机：work area
从`360x554`进入`640x284`，但日志名为physical的Qt `screenGeometry()`快照仍报告`360x640`；
这不是直接硬件读数。`SetOrientationL`请求返回0，约6秒后
`PLAYER_NATIVE_ORIENTATION_TIMEOUT stage 2`使会话park并恢复竖屏。两次均未加载`HxMmfCtrl.dll`、
未安装任何Hx断点，也未进入MMF Prepare：

| 记录 | PID | 参数/样本 | 最早停止边界 | 退出与日志 |
|---|---:|---|---|---|
| FULL-setup-1 | 35984 | runtime identity与样本A匹配 | stage 2：available `640x284`、physical `360x640`；无Hx load | module等待超时后terminate，PID消失；`32206 / D5121C4327B044D4EA0E5F2DBFEE1939BE0778BF62F119CB14C0AD2D72DBF630` bytes/SHA-256 |
| FULL-setup-2 | 35995 | 同上 | 同上 | terminate并确认退出；`32192 / BB66207EBFC25DDE232D14228975C7A6299006101D0592405F8576E84291D164` |

按用户当时口径两次都是核心问题外的无效启动，不计最多三个有效进程；当时有效预算为0/3。因为相同外部
前置条件已连续复现，且用户不在现场，当时没有尝试第三个setup。
这两次只证明当前无人值守状态下完整诊断未到MMF，不是Prepare 0、不是播放成功/失败，也不改变
OnError假说。上述三次预算随后已被香港时间2026-09-08 10:00前的限时授权取代，不再是当前计数。
恢复取证需要既有横屏流程真正进入MMF；这不否认NIKINIKI此前能够进入横屏。仍使用相同包、参数、
样本和断点，不修改播放器或增加延时。

限时续查取得一次完整应用前置复核，结果唯一记于
[设备矩阵限时记录](../../reference/DEVICE_TEST_MATRIX.md#e7-hx-timed-20260908)。新的观察终点为
首个Prepare SendEvent，或产品自己明确放弃横屏准备；不等待旧60秒窗口，也不作播放验收。
只读`getHalInfo([])`可以查询屏幕状态，不用于修改HAL。Qt Creator对应CODA客户端没有提供已验证的
前台恢复接口，本轮不猜协议、不注入调用、不解锁/关闭系统安全功能。焦点窗口组的所有者仍UNKNOWN。
用户随后明确因无人解锁暂停所有新增设备进程，10:00前仅做下述离线分析；不再查询CODA或重复横屏
阻塞启动。既有目标已确认退出，旧三次计数和此前限时启动许可都不构成当前重启授权。

### 调整接口分发表及零命中证据门槛

对同机Hx展开code的有限复核新增了可重复验证的接口关联，而非新增插件分析：

| E7 code证据 | 可确定的映射 | 动态校验方式 |
|---|---|---|
| 构造`+0xE6C..+0xE80`：加载literal `+0x10D0=linked 0x45560`，写对象首字后依次加`0x184/0x20`；析构`+0x1110..+0x1124`也重置此表 | `[controller]=base+0x3D560`；`[controller+0x1C]=base+0x3D704` | 同次基址加偏移；读取当前controller前缀，不能用上一PID的对象地址 |
| 数据槽`code+0x3D720=linked 0xBE10` | 调整接口vtable槽`+0x1C`指向`code+0x3E10` | 先核实两个vtable，再只读已知模块槽4字节，要求值为同次`base+0x3E10`；不追踪任意指针 |
| `+0x3E10`保存32-byte栈，`+0x3E14 sub r4,r0,#0x1C`；`+0x3E18/1C/34`保存r1/r2/r3 | 入口`controller=r0-0x1C`，severity/HXCode/userCode为r1/r2/r3；原始`[sp]/[sp+4]`为两个说明指针 | 仍在入口读寄存器和有界字符串，不主动调用方法 |
| `+0x3EDC..+0x3F04`读`+0x90/+0x48/+0x94`，可能改选既存`+0x94`；`+0x3F48`写`+0x80` | 写入的聚合结果未必等于本次HXCode | 在已有对象前缀中同时保留两个选择状态及`+0x80/+0x94`；状态数值暂不命名成未验证枚举 |

`tools/research/e7/analyze_hxmmfctrl.py`逐字校验构造/析构、接口和下述字段生命周期的关键指令/数据，
实际同机文件通过；并提供纯主机错误选择模型，模型测试不是设备路径验证。
这些是静态映射；新增的运行接口槽校验尚未在到达Hx的新会话执行，不能回填旧运行。
研究observer保留原三处断点，新增同PID身份门、SendEvent的`+0x94`及接口槽校验、截止时间和明确
pre-MMF停止记录。异常清理先terminate并确认目标退出，再清理其断点，避免在未确认暂停时切换断点。
暂停值是主机处理暂停事件后的服务耗时，可能漏计传输/队列等待，不能当作设备暂停总时长上界。

旧“调整接口已验证断点能力”的表述收紧为：短code fingerprint匹配且add/enable获接受；
`Breakpoints.getStatus`不可用，调整入口尚无实际命中。SendEvent可命中只证明该点可观测，不能替代
调整入口的有效性。目标错误再现且OnError零命中时，先核对加载暂停→安装/enable→resume顺序、
进程/线程范围、接口槽及清理记录；仍有观测能力缺口则保留UNKNOWN，不能直接排除OnError。

### 保存错误字段的有限生命周期恢复

以下全部为同机Hx展开code的ARM指令，偏移相对code起点，反汇编linked地址须减`0x8000`。
没有使用603/SDK地址；同机代码已能回答本段问题，未扩大到用户许可的E7 ROM或插件树。

| 字段/阶段 | E7指令及对象依据 | 能证明与不能证明 |
|---|---|---|
| 真正构造 | 分配入口`+0xF04`请求`0x158`字节，`+0xF10`调用`+0xE64`，后者`+0xE68`调用base ctor `+0xA4E4`；旧`+0x1108`由deleting dtor `+0x11C4`调用并尾调base dtor `+0xA634` | 纠正阶段命名；不是凭vtable store猜构造 |
| `+0x48/+0x80/+0x90/+0x94`初始化 | `+0xA550`写`[C+0x48]=7`；`+0xA554`传`C+0x50`给`+0x7BC4`，该helper返回原指针；`+0xA57C/+0xA58C/+0xA590`分别`str r5,[r0,#0x30/#0x40/#0x44]`且`r5=0` | 明确写`C+0x80/90/94=0`，并非推测分配器清零。辅助构造`+0xFF18/+0x7BC4/+0x21E50`的返回指针和callee-saved寄存器已核对；不能把初始值回填到Prepare时刻 |
| `+0x90`赋值 | `+0x3204`入口`r0=C`，`+0x320C`保存为r5；`+0x3260/+0x3268`取descriptor中的word，`+0x326C str r0,[r5,#0x90]`；同函数trace为`GetDownloadID ... downloadID=%u` | CustomCommand case 4在`+0x3618`调用它；读取失败分支仍汇入store，不能单凭字段假定操作成功。当前Qt是否执行该command及其值UNKNOWN |
| `+0x94`保存/替换 | 主入口`+0x3B64..+0x3B8C`、调整入口`+0x3EDC..+0x3F04`相同选择逻辑；条件写在`+0x3B78/+0x3EF0` | 仅`int32(C+0x90)>0`且`C+0x48==7`时保存本次HXCode（含0）；其他state时，非零保存值可替换本次HXCode |
| `+0x80`提交/清零 | `+0x3BD0/+0x3F48 str r5,[r4,#0x80]`；severity 4的条件返回在store之后。SendEvent `+0xE6FC/+0xE700`只显式把`+0x80`置0 | severity 4仍可留下聚合错误，供之后Prepare取用；本次是否如此UNKNOWN。SendEvent清聚合值不等于清保存值 |
| `+0x48`写入时序 | StopL `+0x1370/+0x1374`和OnPresentationClosed `+0x43A4/+0x43A8`写7；MvpcPrepare `+0x16CC..+0x16D4`在既有Prepare调用返回0后写2；OnPrepareComplete在SendEvent返回后`+0x40AC/+0x40B0`写3 | 不命名未经验证的状态枚举，不预设OnError/Prepare回调发生在这些写入之后 |

有界枚举找到五处立即数word store `[...,#0x94]`：两处上述OnError，`+0x5BF0/+0x738C`属于另一
state对象，`+0xC708`为栈字段。真正base ctor清零使用别名地址，故立即数枚举并不完整。
已检查的ResetL主体`+0xA81C..+0xA87C`没有显式清`C+0x90/94`；其嵌套Reset/其他被调函数、
回调、批量或间接写入未被穷尽。结论仅为**未恢复出专门的post-event保存值清零路径**，不是证明其不存在。

错误选择伪代码（C为同controller；数值不擅自命名成HX错误符号）：

```text
// OnError入口：severity=r1, HXCode=r2, userCode=r3；调整接口C=r0-0x1C。
// 两个特殊HXCode 0x80040027/0x00040025以外，会先调用ResumeScheduler(+0xA8E0)。
// 下列字段是该调用之后选择时刻的值，不能直接拿入口快照替代。
selected = HXCode
if int32(C[0x90]) > 0:                // CMP + BLE，不是普通nonzero判断
    if C[0x48] == 7:
        C[0x94] = selected
    else if C[0x94] != 0:
        selected = C[0x94]
if selected == 0x000406A2:
    tailcall existing_notification(C[0x84], 0, userString)
    // 绕过本函数直接写+0x80；被调函数的副作用未在本模型中分析。
else:
    C[0x80] = selected
    if severity == 4: return          // 已经写入，不是忽略该error
    continue existing_event_dispatch

// 随后另一已定位路径，不保证与上面紧邻或同一回调栈：
OnPrepareComplete: SendEvent(C, 2, C[0x80], extra)
SendEvent: map(0x00040024) -> -12017; existing_forwarding; C[0x80] = 0
```

`ResumeScheduler`为同二进制内嵌trace命名；它可能改变状态/调度顺序，动态重入仍UNKNOWN。
选用旧值分支还经过既有日志调用，不能把“静态预测选中值”伪装成已观察到的store或无重入保证。
保存错误是**同对象内**的候选保留路径；它不证明错误跨进程继承，也未证明产生了此前`0x00040024`。

旧日志是否保存这些字段已用`audit_hx_saved_fields.py`离线复查十份有效Hx记录；结果与缺口唯一记于
[设备矩阵字段审查](../../reference/DEVICE_TEST_MATRIX.md#e7-hx-old-fields-audit)。原始寄存器不等于对象内存，
`quiet`读取未落盘的字段无法追补。现在研究observer保存**既已读取**的`0x98`字节prefix及摘要、四字段；
没有额外Memory读取。该保存增强只经主机测试，尚无新运行数据。
本次主机验证：83项研究工具单元测试、121份Markdown检查和`git diff --check`通过；没有GCCE编译、
SIS打包或新增设备验证。原始DLL、展开code和审查报告均留在本地忽略目录，未修改正式播放器。

### 下一次失败捕获的最小方案（仅准备，等待重新安排）

1. 仍用既有完整Qt诊断、完整样本A和已验证参数；先具备正常进入MMF的前台条件。本轮不解锁、不试启动。
   同次Hx加载暂停时核对模块、原三个点code fingerprint及分发表，确认add/enable在恢复前完成；
   运行地址只取该次base+code offset，匹配短块不宣称整个模块相同。沿用CODA已验证的寄存器/参数格式。
2. 初始只设主OnError `+0x3A98`、调整OnError `+0x3E10`和SendEvent `+0xE5D8`。同PID/thread/实例
   记录event 1基线、OnError原始severity/HXCode/userCode、入口SP/LR、有界两个说明和
   `C+0x48/80/90/94`；event 2记录同四字段、r2及调用者。不存在的命中或字段记UNKNOWN。
3. 仅OnError命中后，为区分入口与真正选择时刻，允许在目标仍暂停时核验并安装**一个**对应选择前点：
   调整`+0x3EDC`（主入口对应`+0x3B64`），ARM，32字节SHA-256均为
   `5BCCCB44C3A6901429A72489822077DCA9CE885C3E96952ADD8E0EC2CCA180FD`。
   此时`r4=C, r5=当前HXCode, r7=原HXCode, r8=severity, sp=入口sp-32`；读同四字段。
   以PID/thread/C/SP关联原调用帧，保留原入口点以识别嵌套；不能拿不同帧拼链。断点确认成功后才恢复；
   选点命中时仍暂停才移除。此动态切换现已实现并完成下述离线验证，尚未设备执行。
4. 不另设构造/Reset/DownloadID断点，不主动调用任何播放器方法。记录实际命中顺序和每次主机暂停服务
   耗时（不是设备暂停总时长上界）；若SendEvent先于关联选择点发生，不能强排为线性顺序。
   目标Prepare及必要关联后续事件完整后terminate并确认退出，不作60秒播放验收。建议新授权先只取
   **一个新进程**，从Hx恢复请求起设45秒主机截止，最多8次业务断点命中（含嵌套）；到限不再resume，
   不补跑。截止在同步命令之间检查，传输超时可能使实际terminate晚于45秒，必须保留实际耗时，不能
   声称硬实时上限。这些仅为待确认上限，不是延时或重试策略。新字段或选点的代码/内存能力失败、
   异常、重入无法关联或恢复顺序不明即停。

唯一要区分的是：失败Prepare的`0x00040024`来自本次OnError输入，还是在上述条件下选取先前保存值；
只抓到后者仍需追其先前写入，不称原始初始化故障已定位。OnError零命中时仍先排除观测遗漏，不能因
旧日志缺字段或本轮只做静态分析就否定该路径。后续预算及解锁条件由用户重新安排，本轮不消耗进程。

#### 选择点切换实现与冻结

按用户追加的**仅主机实现**授权，`coda_observe_hx_onerror.py`接入独立研究状态模块
`hx_selection_state.py`；没有修改播放器，也没有连接CODA。冻结标识`e7-hx-selection-switch-v1`。

| 状态/记录 | 已实现行为 | 仍保留的限制 |
|---|---|---|
| 入口→选择 | 每个入口生成新frame ID，保存原始输入、SP/LR、线程、controller和前缀；选择点要求同PID/thread/C、`SP=entrySP-32`、r5/r7/HXCode及r8/severity一致 | 选择已读取不等于函数已返回；`return_observed=false`，不猜测完整调用栈寿命 |
| 嵌套与单点 | 同入口嵌套复用一个gate；不同入口先确认移除旧gate，再安装/enable内层gate；内层选择命中后恢复尚待选择的外层gate；原入口/SendEvent点不移除 | 同时最多一个动态gate。pending帧期间出现另一线程停止，或SP重用/展开无法解释，即停；不假设CODA为全线程暂停 |
| Prepare关联 | 保存全部SendEvent，出现OnError后只由首个OnError controller的Prepare结束该链；其他controller不能替代。Prepare先于选择时保留顺序，在原界限内等后续事件 | 同controller和先后顺序不等于已证明因果writer；没有OnError时Prepare记录仍可结束，但不能排除OnError或判断播放成功 |
| 读值与推算 | `selection_snapshot_unassociated`保存实际读数；核对后`selection_frame_matched`保存匹配frame与推算；最终`selection_trace_final`包含全部入口、选择、事件和未完成frame | 推算专属`prediction`，标为`static_inference_from_preselection_snapshot_not_observed_write`；始终`actual_store_observed=false`，不把预测提交值命名为已观察写入 |
| 切换失败/退出 | add前登记待清理ID；add/enable/remove或fingerprint失败不resume、不继续装第二个gate。异常也保存部分trace；先terminate并核对PID不存在，再清理包括结果不确定的ID | 退出或枚举格式不明确不移除断点；清理失败显式报告，不假装成功。保存summary异常仍进入terminate/close；不碰其他身份不明的进程 |

离线测试使用合成寄存器/对象和替代的Coda/传输，不加载实际客户端、不打开端口。
新增31项状态/集成测试包含同入口与跨入口嵌套、不同controller、Prepare提前、顺序SP重用、字段/寄存器
不匹配、跨线程停止、安装/enable/移除/恢复/fingerprint失败、fault、summary异常及退出确认失败；
也覆盖旧日志中的`rR0..rR12/rSP/rLR/rPC`命名。全部研究工具共83项通过；这是主机验证，**不是设备
断点切换能力通过**。

本地不可原位改写的冻结清单为`.tmp/e7-hx-prepare-runtime/selection-switch-v1.freeze.json`，记录10项
脚本、测试、私有客户端和既有同机分析输入的大小/SHA-256；没有复制或更改原始DLL。
主脚本SHA-256 `137E95B8CEEF9617BB122410DAFFF18936A17BBAF229531E55BB1250F56E96ED`，状态模块
`C8B6F9BED19C335CED8299F8DB1D1F63BB2B5D49AF77EECB778148C9DE7F7AEB`。
冻结只约束所列文件字节，不代表Python/OS/pyserial环境或整个设备运行库已冻结。后续执行前复核清单，
任何源文件变化都须新版本验证，不静默覆盖本版本。

可运行的主机预览命令（不连接设备）：

```text
python .tmp/e7-private-tools-20260910/coda_observe_hx_onerror.py --target FULL --run-id selectionfreeze1
```

当前停止点是**脚本冻结、等待设备条件及用户重新安排**；旧10:00限时许可不自动启用此次脚本，
不因离线测试通过而消耗设备进程，也不扩大ROM或插件分析。

#### 选择点真机执行与原始错误输入闭环

用户确认设备已亮屏并授权执行后，先按冻结清单复核所有输入。第一次 setup（PID 36088，
`selectiondevice01`）在Hx加载暂停时已匹配目标和模块，但研究脚本在成功
`Breakpoints.add(Enabled=true)`后又发送了独立`Breakpoints.enable`；该CODA实现返回
`functionality is not supported`。此时没有进入Play、OnError或Prepare，进程随即terminate并确认消失。
它是工具预置差错，依用户既定规则只记录、不计有价值数据。原始忽略日志为
`.tmp/e7-hx-prepare-runtime/selectiondevice01/coda.jsonl`，52398 bytes，SHA-256
`6EF2FAD06035F53C334E9791DD5168CB664EE3784B8EDEEA6F46BFB1F9F2CA1C`。

修正只移除了多余的独立enable；断点add仍显式携带`Enabled=true`，并把“add获接受”和“实际命中证明
有效”分开记录。84项测试通过后另存不可原位改写的
`.tmp/e7-hx-prepare-runtime/selection-switch-v2.freeze.json`，没有覆盖V1。V2状态模块SHA-256为
`4188692CCA6FC0753EB479C99C466690B4200F8D1998678E6EBE33B03AE6571C`，主脚本仍为上节摘要；
CODA不支持`getStatus`，所以断点有效性只由后续实际命中证明。

随后唯一有效目标进程（PID 36102，`selectiondevice02`）取得一条完整失败链并按契约提前停止；没有
继续采集同类进程。完整应用身份、完整本地样本A、Hx和`hxmedpltfm`同机fingerprint均匹配，Hx同次
运行基址为`0x7AF00000`。所有业务命中均在线程`p36102.t36107`和controller `0x22900678`：

| 顺序 / 主机ms | 观测点 | 实际读取 |
|---|---|---|
| 1 / 4984 | `SendEvent +0xE5D8` event 1 | `+0x48=7, +0x80=0, +0x90=0, +0x94=0` |
| 2 / 9781 | 调整接口`OnError +0x3E10` | `r0-0x1C=0x22900678`；severity 4；`r2=0x00040024`；userCode 0；两个说明指针均null；入口`+0x48=2, +0x80=0, +0x90=0, +0x94=0`；LR=`Hx+0x684C` |
| 3 / 9875 | 动态选择前点`+0x3EDC` | 同PID/thread/C/SP帧关联成立；`r5=r7=0x00040024, r8=4`，四字段未变；静态推算选择当前HXCode，仍明确`actual_store_observed=false` |
| 4 / 10062 | `SendEvent +0xE5D8` event 2 | 同controller，LR=`OnPrepareComplete+0x58`；`r2=0x00040024, +0x80=0x00040024, +0x90=0, +0x94=0` |

这动态证明：本次Prepare的`0x00040024`先作为调整接口OnError的**原始输入**出现；`+0x90=0`且
保存值`+0x94=0`，所以本次不是DownloadID/状态条件下改选旧保存错误。选择点没有观察实际store，
但稍后同controller Prepare现场已经实际读到`+0x80=0x00040024`；先后和同实例仍不单独命名具体
writer。目标在约10秒终点terminate，PID消失及剩余初始断点移除均确认。主机服务暂停下限依次为
模块装点16 ms、platform身份16 ms、event 1 78 ms、OnError 172 ms、选择78 ms、Prepare 110 ms；
它们不是设备总暂停上界。画面、声音、背景按无人现场契约均为UNKNOWN，不作播放验收。

原始忽略日志`.tmp/e7-hx-prepare-runtime/selectiondevice02/coda.jsonl`为93854 bytes，SHA-256
`A542BEAA1F0EC800436780C1C74D3138CAE4F97135716BB95D2191995D079513`。V2清单保持原来的
host-tested状态、不事后改写；实际执行事实由本节和设备矩阵记录。

`LR=Hx+0x684C`把边界再前移一层。同机code显示`+0x6798`是`HXMMFStateCtrl`调整接口的OnError
等效体：GCC vtable组`code+0x3D99C`的offset-to-top为`-0x14`，共同typeinfo为linked
`0x43DC0`，vptr `code+0x3D9A4`的槽`+0x0C`指向linked `0xE798`，即code `+0x6798`。
入口先令`state=r0-0x14`，原样保存severity/HXCode/userCode；先调用`[state+0x54]`的虚槽
`+0x58`，其非零返回再经`[state+0x94]`虚槽`+0x1C`把原参数转给controller，调用在`+0x6848`、
返回点正是动态LR `+0x684C`。这里的**state对象`+0x94`是observer接口指针**，不是
HXMMFCtrlImpl controller的保存错误字段。原始寄存器还保留调用方`r4=0x2290A8F0`，与该调整体把
state保存在r4的数据流一致。

独立有限校验器`tools/research/e7/analyze_hx_statectrl_onerror.py`逐字验证上述ARM指令、主/调整
vtable和共同typeinfo；脚本/测试SHA-256分别为
`5DBC833F2C0D0789F6E6D64EFD25EF517902BA303D627E24F584A1A9C7CC99D0`和
`4E0CC255169C9E610BE5B024624DBCA589ADF4911E2B49108CD3728E22A58EF9`，全研究工具现为87项测试通过。忽略报告
`.tmp/e7-hx-prepare-runtime/selectiondevice02/statectrl-onerror-boundary.json`为1694 bytes，SHA-256
`3E4A013149F4A93395ED5400DC93AF7FA9AC35CDFD29DD88BE7028858D4B3072`。这只证明
`HXMMFStateCtrl`转发边界，不证明哪个更下游初始化调用产生`0x00040024`。

#### StateCtrl上游入口真机闭环与调用者身份缺口

用户授权后，以不可原位改写的V3清单
`.tmp/e7-hx-prepare-runtime/statectrl-onerror-v3.freeze.json`复核12项输入，零不匹配；
冻结清单本身SHA-256为
`B1E84399E117FB80AC7B483CBA70D8A1AD8DB844297C01E89B06A6F97E36DBF2`。它保留已验证的
controller OnError/selection/Prepare闭环，只新增`HXMMFStateCtrl +0x6798`，并继续只用
`Breakpoints.add(Enabled=true)`，没有恢复独立enable。91项离线研究测试先于设备执行通过。

唯一有效进程PID36141（`statectrl01`）使用已有完整Qt诊断和完整本地样本A；
Hx同次base为`0x7AF00000`，`hxmedpltfm`同次base为`0x7CAC0000`，所有预置点与
platform短块fingerprint均匹配。五个业务命中全在线程`p36141.t36146`：

| 顺序 / 主机ms | 观测点 | 实际读取与关联 |
|---|---|---|
| 1 / 5219 | `SendEvent +0xE5D8` event 1 | controller `0x22900678`，raw 0，`+0x80/+0x94=0/0` |
| 2 / 9860 | `HXMMFStateCtrl +0x6798` | `interface=0x2290A904`，`state=r0-0x14=0x2290A8F0`；severity 4，`r2=0x00040024`，userCode 0，两说明指针为null；`[state+0x54]=0x2290C3B0`，`[state+0x94]=0x22900694` |
| 3 / 9969 | controller调整OnError `+0x3E10` | `interface=0x22900694=state.observer`，`controller=interface-0x1C=0x22900678`；同thread、severity/HXCode/userCode和两栈参数均一致 |
| 4 / 10125 | 动态选择前 `+0x3EDC` | 同调用帧，`r5=r7=0x00040024, r8=4`；选值仍是静态推算，`actual_store_observed=false` |
| 5 / 10219 | `SendEvent +0xE5D8` Prepare event 2 | 同controller，`r2=+0x80=0x00040024`，LR=`Hx+0x40AC`；目标链闭合 |

这次将最早已证边界前移到StateCtrl的**入口参数**：`0x00040024`在进入
`HXMMFStateCtrl::OnError`等效体时已经存在，StateCtrl后续只按原参数转发给同controller。
因此StateCtrl与controller都是已证转发/汇总者，不是已定位的错误生成者。

入口LR为`0x7CA3C2B0`，ARM；同次运行短读得到
`0x7CA3C2A8: 04 10 A0 E1 3C FF 2F E1`，故实际调用指令为
`0x7CA3C2AC: 0xE12FFF3C`，即`BLX r12`，当时`r12=0x7AF06798`正是已验证的
StateCtrl调整OnError入口。这证明调用边，但不能把间接调用者自动命名为错误生成者。

调用者的**同次模块归属仍未证实**：该LR不在本次显式订阅并验证的
`HxMmfCtrl [0x7AF00000,0x7AF41970)`或
`hxmedpltfm [0x7CAC0000,0x7CAFD794)`范围。脚本为了避免扩展依赖树，没有订阅其他
library load事件；目标进程在闭环后已按合同terminate，无法事后从同PID补取模块表。
E7 core的有限字符串检索显示`hxmedplyeng.dll`与`hxnetwksvc.dll`候选名；
`0x7CA3xxxx`形式及其与`hxmedpltfm`base的排列使`hxmedplyeng.dll`成为**静态候选**，
但没有同次load base、该DLL自身code size和指令fingerprint，故禁止写成运行归属事实。

进程在约10秒的新终点terminate，PID消失、动态gate及四个初始断点移除均确认；
各停顿主机服务下限为装点15 ms、platform身份0 ms、event 1 78 ms、StateCtrl 94 ms、
controller OnError 125 ms、selection 109 ms、Prepare 79 ms，不是设备总暂停上界。
画面、声音和背景均为UNKNOWN，不作播放验收。原始忽略日志
`.tmp/e7-hx-prepare-runtime/statectrl01/coda.jsonl`为113452 bytes，SHA-256
`2E0948F0D2468990D089E08FF1BFEF4DF62FA7C0FD84AC4EC563C9D97D5FA144`。

对同机展开code作有限ARM立即数store枚举，共找到九处形如`str Rx,[Ry,#0x80]`的显式写入：

- `+0x3BD0/+0x3F48`分别属于主/调整接口`OnError`，可写动态HX结果；
- `+0x5BDC/+0x6938/+0x7558`位于具有独立vtable和`HXMMFStateCtrl`内嵌名称的另一对象区域，不能
  仅因成员偏移同为`0x80`就归给Prepare controller；
- `+0xBB38/+0xBBD8`位于`HXMMFBaseCtrl::TimerFunc`路径，写入literal `0x8007000E`，不是本次
  `0x00040024`；
- `+0xE4C0`写的是`[controller+0x38]`所指嵌套对象的`+0x80`；`+0xE700`是`SendEvent`末尾把
  controller聚合成员清零的已知路径。

这项枚举排除了若干“同偏移即同字段”的假候选，但不覆盖先计算地址再store、批量复制或下游调用
间接写入，不能声称所有writer都已恢复。当前CODA Breakpoints协议只暴露software/hardware/auto
**指令断点**；本地对应适配层明确标注watchpoint尚未实现，因此不能在event 1取得同实例controller
后直接对`[controller+0x80]`设置数据写断点。

在不启动播放器的前提下，还尝试用CODA FileSystem只读打开静态调用边界
`Z:\sys\bin\hxmedpltfm.dll`；设备返回`Code -21`/access denied，未取得任何字节，也未扩大系统盘
扫描。记录为`.tmp/e7-mmf-runtime/fetch-hxmedpltfm-20260908.jsonl`。这次读取拒绝不消耗播放进程
预算，也不说明模块不存在或加载失败；随后用户手工提供的同机副本已解除文件缺口，分析结论见上。

忽略目录原始日志大小/SHA-256分别为：H1
`70607 / A0D0B131CB89388F868520A01E1F88356B0BBCF7BF6BEB4F0B3E457CFC901FA4`、H2
`68781 / 9A2F096B0453BA9AE8E65AE28A8C9FCBC412DDE4C379F6B3769F75608685577A`、H3
`69256 / 1B02BB54018D4EAD1A7D4F7792BD299E204976CFAD5F67CC31E1C14582D7F0C0`、H4
`69715 / 908F22F2E7C149D3F42EAA6420F009862491821D77D6580A3A1AA76CE8FB67FC`。
无效PID 35730日志为
`67272 / D0A600DEEC68B0E7B56081961728FEA5438E2740DEB47620B8C6824E7C7501AC`。

#### 同机 `hxmedplyeng` 的partial-playback生成点

用户从当前E7复制的`hxmedplyeng.dll`原件与只读工作副本均为299725 bytes，SHA-256
`7925716164592CE4EB3C65A2A363B83358B6FF349CA2C61AD597C9D44D131144`。文件是E32Image
V2.04、ARMV5 EABI/EKA2、BytePair压缩、VFPv2、paged code/data、非debug，声明展开code/text
`0x799D0` bytes、data 0、12项import、9项export；UID1为`0x10000079`，UID2/UID3均为0。
原件保持只读，Petran展开code另存于忽略目录，498128 bytes，SHA-256
`1894666DA65D1B23209540577D63CDC798DBDCC6B2068DDEAFB64566A0314736`。

旧运行窗口`04 10 A0 E1 3C FF 2F E1`在展开code中出现六次；逐一从旧调用地址
`0x7CA3C2AC`反推base，只有窗口`+0x3C2A8`/调用`+0x3C2AC`得到64 KiB对齐的
`0x7CA00000`，且声明code范围终点`0x7CA799D0`不越过旧同次
`hxmedpltfm base=0x7CAC0000`。该点逐字为`mov r1,r4; blx r12`，返回偏移`+0x3C2B0`；
其函数`+0x3C238..+0x3C2DC`按每个sink条目的严重度上下界筛选，再从虚槽`+0x0C`原样转交
severity/HXCode/userCode和两条说明。这个静态候选完整解释了旧LR，但“唯一满足地址排列的候选”
仍不等于同次library load身份；未来运行必须重新取得base并匹配代码短块。

沿同一模块向前的有限xref恢复了比“转发”更早的静态边界：

1. player等效`OnError +0xEDAC`从`player+0xCC`取error-control，再调用
   `+0x3C720`；后者最终进入上述sink dispatcher。两层均保留r2，不生成目标码。
2. `0x00040024`作为32-bit literal在整个展开code中只出现一次，位于`+0x1206C`。
   `+0x11F40`把它装入r2，`+0x11F44`设severity 4，`+0x11F3C/+0x11F4C/+0x11F50`
   把userCode及两条说明置0，`+0x11F54`的ARM `BL +0xEDAC`调用player OnError；返回偏移为
   `+0x11F58`。因此，若未来同次运行caller LR命中该偏移，`0x00040024`是在
   `hxmedplyeng`中**明确合成的partial-playback通知**，不是某个插件原样返回的HXCode。
3. 生成函数`+0x11EBC`遍历`player+0x144`集合；当前条目`+0x44` bit2未置位时调用
   `+0x2D0C4(entry,&flag)`。第一次非零返回会保存在r5并跳过本次通知；否则，当
   `[entry+0xAC] != 0`时进入上述literal调用。条目、集合的正式类型名仍UNKNOWN。
4. `+0x2D0C4`与内嵌配置名`DisablePartialPlayback`、`MinValidRendererReqd`、
   `MaxNumOfVideoStreamsAllowed`、`MaxNumOfAudioStreamsAllowed`及renderer MIME/plugin字符串
   位于同一函数区。其尾部把两个HX_RESULT候选与`0x8007000E`比较，并受流计数、
   `MinValidRendererReqd`阈值和partial开关控制；条件满足时，`+0x2DAE8`令输出flag为1、
   `+0x2DAEC`把将返回的r6清零、`+0x2DAF0`实际写`[entry+0xAC]=1`。随后调用者看到
   “helper返回0且partial marker为1”，再合成`0x00040024`。r5/r6各自对应哪个具体
   renderer初始化调用仍未恢复，禁止提前命名decoder、renderer或插件。

有指令依据的有界伪代码为：

```text
for entry in player.collection_at_0x144:
    if entry.flags_0x44 & 4: continue
    current = helper_2D0C4(entry, &flag)
    if current != 0 and first_result == 0:
        first_result = current
        continue
    if entry.partial_marker_0xAC != 0:
        player_on_error(severity=4, hx=0x00040024, user=0, null, null)

helper_2D0C4(...):
    ... obtain renderer/setup HX_RESULT candidates in r5/r6 ...
    if partial_policy_conditions and r5/r6 are not 0x8007000E:
        *flag = 1
        r6 = 0
        entry.partial_marker_0xAC = 1
    return r6
```

可复核报告`.tmp/e7-mmf-runtime/hxmedplyeng/hxmedplyeng-caller-analysis-v3.json`为9115 bytes，
SHA-256 `DEF1198C09D3E767C1DF82FEB48E1368DED938F90CB7D8C1883508DC1441EF8D`；对应分析器加入了
独立ARM branch目标、唯一literal及证据指令校验。全研究工具100项离线测试通过。

动态V6合同只新增`hxmedplyeng +0xEDAC`，并保留已验证的StateCtrl/controller/selection/Prepare链。
它只在同次load base的`+0xEDAC`和`+0x3C2A8`两个32-byte fingerprint均匹配后装点；完整闭环还
要求player caller return=`+0x11F58`、StateCtrl caller return=`+0x3C2B0`、相同参数/线程及同
controller Prepare=`0x00040024`。在精确generator命中时额外保存调用者r4-r8，并通过r6读取当前
entry的`+0x44/+0xAC`；这里能证明实际partial marker，但不能恢复helper内部已被清零前的r6。
V6不可原位改写清单为`.tmp/e7-hx-prepare-runtime/player-onerror-v6.freeze.json`，SHA-256
`855C652FBBD8FDF440A1D4925E70DF377939B917B4C61706757E172D77F5425C`；它以V5完整输入清单为基底，
显式替换最终observer脚本摘要，组合校验无不匹配。

用户离场前允许无需触屏的CODA工作。一次实际尝试PID36262正确匹配FULL身份与样本，但横屏只到
stage 2：available `640x284`、physical `360x640`，约5.9秒后触发已有orientation timeout。
它未加载Hx、未装任何断点、未进入Play/Prepare；脚本terminate并确认PID消失。画面/声音/背景均
UNKNOWN，不是播放对照或错误链样本。忽略日志25294 bytes，SHA-256
`E04BEE3706EE24BF11BD5E1F3297B9E1B916E7BE47410FCD8DA9DAD6DD15F09E`。为避免把锁屏前置状态
混入机制，不再无人值守重复启动。

设备恢复解锁后，冻结V6在PID36354、run `playeronerror03`中一次取得完整目标链。同次运行
`hxmedplyeng.dll`加载base为`0x7CA00000`，两个fingerprint匹配；player OnError的LR为
`0x7CA11F58`，即精确generator return `+0x11F58`。该入口实际收到
`severity=4 / HXCode=0x00040024 / userCode=0`，r4与player `0x2290FF98`一致，r6 entry为
`0x22919808`，实际`[entry+0xAC]=1`。随后同线程依次关联到StateCtrl `0x2290A8F0`、observer
`0x22900694`、controller `0x22900678`和Prepare event 2 `0x00040024`；controller在OnError前
`+0x80=0`，Prepare时为`0x00040024`。这把此前的静态生成点提升为同次运行事实，但仍未保存
helper清零/写marker之前的r5/r6。目标约10秒闭合后terminate，PID消失且所有断点移除确认；画面、
声音和背景均UNKNOWN。日志`.tmp/e7-hx-prepare-runtime/playeronerror03/coda.jsonl`为131048 bytes，
SHA-256 `DC60656BFE17A42FC3C0778835EF5C17ED579DC754177176D30D95FDC962F1EB`。

满足V6门槛后，主机仅加入一个更早的`hxmedplyeng +0x2DAD8`点。该点在
`ldr 0x8007000E`、r5/r6比较以及`+0x2DAE8/+0x2DAF0`写output/marker之前；当前sp由函数序言
`push {r4-r11,lr}; sub sp,#0x7C`验证，原caller LR位于当前`sp+0x9C`。V7读取r4 entry、原始
r5/r6、r7及其写前值、r8-r11、stack `+0x54/+0x58`和entry `+0x44/+0xAC`，并要求保存caller
return=`+0x11F1C`及随后同entry的完整V6链。点位32-byte fingerprint SHA-256为
`9D78AFEBD86FE3250949B3C11D0FA2CF90B0F0958F3E5FED6EB536824F483EA2`；102项研究测试通过。
冻结清单`.tmp/e7-hx-prepare-runtime/partial-helper-v7.freeze.json`为2420 bytes，SHA-256
`E4B2BC4C9FF1044CB51C2D1592EC3B1F14CCED72C1B88A020C205FC9759794C2`。

两次V7设备尝试均在Hx加载前停止：PID36377/`partialhelper01`和PID36388/`partialhelper02`都只到
available `640x284`、physical `360x640`，随后orientation stage 2 timeout；未安装任何V7断点、
未到Play/Prepare，均terminate并确认PID消失。日志分别为
`25292 / B36BB07B7FBCE8D4A68008B4E68096728F356133A6541D63A6B21DCEDC9A53AF`和
`25278 / EE684919D007B7EDD4DD6170B0AC90BD39D7CE73F67C349F07532149F4E81B10`。连续同边界失败后已停止
重试；两次均不是错误链数据，画面/声音/背景UNKNOWN。

现场再次解锁后，V7 `partialhelper03`（PID36405）取得完整目标链。同次helper入口entry为
`0x22919808`；在OOM比较及`+0x2DAE8/+0x2DAF0`写入之前，实际读取
`r5=0 / r6=0x80004005`、output和`[entry+0xAC]`均为0。保存caller return为精确
`hxmedplyeng+0x11F1C`，随后同entry marker变为1，并闭合到player、StateCtrl、controller和
Prepare `0x00040024`。这证明本次partial不是由OOM值`0x8007000E`触发；静态数据流又证明
`r6=0x80004005`来自较早的`moveq r6,r5`，而最近的r5候选是`+0x2BD6C`返回值。日志为
139679 bytes，SHA-256
`45351D990CD5C80FC9C94560B9FECB4D41D20438B45545366553C1BA20E35AD8`。

V8只在`+0x2BD6C`的两个相关虚调用返回点增加动态切换。`rendererreturn01`（PID36430）证明同一
entry、同一对象`0x2291CA94`、vtable `0x7C9AA9A4`的vslot `+0x10`目标
`0x7C9812E8`返回0，而紧接的vslot `+0x18`目标`0x7C981C18`返回`0x80004005`；该值与随后
helper的r6一致。由于第二返回点没有及时移除，后来命中其他对象并在Prepare前耗尽业务命中上限，
故V8只作强候选链，不单独称完整闭环。PID已退出并清点断点；日志115661 bytes，SHA-256
`7C52616E3ED056464EFAAE0F7FEF5371589B32C8ABD25468803EA9E054AE48DF`。

V9把第二返回点改为首次非零结果时在目标暂停期间立即移除。`rendererreturn02`（PID36450）再次
取得同一entry/object/vtable：vslot `+0x10`返回0，vslot `+0x18`返回`0x80004005`；随后依次
观察helper同entry `r6=0x80004005`、player精确generator、StateCtrl、controller OnError、选择点
及同controller Prepare event 2 `0x00040024`。因此已证最早动态边界是这个实际虚调用的返回值，
不是Qt窗口、MediaClientVideo转换、StateCtrl或controller生成。各业务暂停的主机服务下限依次为
78/32/47/16/15/16/16/15 ms，不是设备暂停总时长上界。目标约10秒闭合后terminate，PID消失，
包括动态切换点在内的所有断点移除均确认；画面、声音、背景均UNKNOWN。日志152261 bytes，
SHA-256 `8A2C3250949DEA1A3920E425B352602D60692DCE0CD275DA4E7A255B93ADFE68`。V9冻结清单
`.tmp/e7-hx-prepare-runtime/renderer-return-v9.freeze.json`为2128 bytes，SHA-256
`1FE211A26EA30A2E434192F13488692849A0CA37AB2EF4897F2E8D165F5D5A93`；对应observer脚本为
80068 bytes / `50C449F82CA72052B16CA10253D96DB1B26437B8178EF4394A83C107280BB68F`，103项研究测试通过。

上述对象vtable和两个方法地址都不在V9已验证的`HxMmfCtrl`、`hxmedplyeng`或`hxmedpltfm`代码
范围内；当时只能把E7 core注册材料中的`mdfvidrender.dll`列为静态候选。后续同机文件与同次
运行身份已经补齐，结果以下一节取代当时的候选状态。

### 同机 mdfvidrender 身份与失败方法

用户从当前目标E7提供的`Z:\sys\bin\mdfvidrender.dll`副本保持只读，分析副本位于
`.tmp/e7-mmf-runtime/mdfvidrender/`。原文件为103,785 bytes，SHA-256
`C40B803E7D116395294F62BE7F542C00812C00E405325184F95E38606D9CF056`；它是E32Image V2 BytePair
输入，UID1/2/3为`0x10000079/0/0`，ARMv5 EABI/EKA2，链接code base `0x8000`，
`iCodeSize=iTextSize=0x2C5C8`、data size 0、11个imports、9个exports，module version
`0x000A0012`。Petran展开得到181,704字节code，SHA-256
`26E831F91EF11704E15EF31471097CAA1352743932474CAF5FD17B23AA0DC4B7`。文件偏移、链接地址和
运行地址分别保留；展开成功不等于整套ROM逐字节匹配。

离线文件中唯一对齐vtable位于code `+0x2A9A4`，slot `+0x10/+0x18`分别指向链接地址
`0x92E8/0x9C18`，即code `+0x12E8/+0x1C18`。三者套用PID36450保存的
vtable/method地址后独立导出同一候选base `0x7C980000`。随后`mdffailure02`（PID36606）的
同次load事件实际给出`mdfvidrender.dll CodeAddress=0x7C980000`；跳板`+0x1C18`、函数体
`+0x16F0`及两个返回闸门的四个不可重定位ARM指令窗口均逐块匹配，运行vtable的9个重定位
指针也逐项匹配。因此V9的`0x7C981C18`现已证明属于本机`mdfvidrender.dll`，不再是文件名推测。
事件没有CodeSize；`[0x7C980000,0x7C9AC5C8)`仍是同机文件声明范围加同次短块验证所得范围，
不能改写成调试器直接报告的完整运行extent。

`+0x1C18`本身只是：

```text
sub r0, r0, #4
b   +0x16F0
```

`+0x16F0`函数体的有界数据流如下；函数名没有map符号，嵌入的
`CMdfVideoRenderer::IsStreaming()`/`OnHeader`字符串只支持其语义位置：

```text
second_method(interface_this, arg1):
    this = interface_this - 4
    if arg1 == NULL:
        return 0x80004005                 # 本地literal；PID36450/36606均已排除
    raw1 = arg1->vslot_0x30(...)
    if raw1 < 0:
        return raw1
    ...                                  # MIME/header状态准备
    raw2 = init_like_91C8(this+0x34, this+0x48, optional(this+0x44),
                          this+0x50, this+0x2C, helper_13D8(this), flag)
    call this+0x50 vslot_0x10(...)        # 不覆盖保存raw2的r5
    if raw2 < 0:
        return raw2
    ...
```

PID36606在同线程`p36606.t36611`、同adjusted renderer `0x2291CA90`（外层接口
`0x2291CA94`）实际观察到：`arg1=0x2291B258`，`+0x1774`的raw1为0；随后`+0x1930`的raw2
已经是`0x80004005`；外层`hxmedplyeng +0x2BE58`对同一接口、同一方法`0x7C981C18`立即收到
同值。因此失败方法没有把另一错误转换成E_FAIL；本次最早已观察边界是同模块`+0x91C8`返回
的原始`0x80004005`。

`+0x91C8`附近的`Init`/`Error in Init`字符串支持把它暂称`init_like_91C8`，不是已恢复导出名。
其静态路径只有本地`0x80004001`和`0x8007000E`负值，不含本地EFAIL生成；故本次EFAIL只能来自
某个嵌套调用原样返回或异常到HX_RESULT的映射。有限恢复最初得到这些候选门：`+0x6500`、早期
`+0x12A30`映射、`+0x90B0`、`[this+0x60] vslot+0x0C`、`+0x8D10`、`+0x808C`、
`+0x754C`或晚期`+0x12A30`映射。以下定点记录已经把实际路径收窄到`+0x754C`内部的晚期
`+0x12A30`异常映射；其他分支不再是本次运行路径候选。

首次`mdffailure01`（PID36591）已取得同次模块load base，但错误地把含运行重定位的跳板后半
纳入16字节静态哈希，故在设mdf内部断点前按规则停止；PID消失且唯一外层断点已移除。该次日志
52,062 bytes / `BAE533BB1D1224E6E38F5C2E59235EA6ADB8EAD799959AE21FD9B52B53770D91`。
修正后的PID36606约10秒取得三点目标链即terminate，PID消失、三个断点全部移除；画面、声音、
背景均UNKNOWN。日志68,540 bytes / `B9B9379BEF4F901CD999883F8C40D9B65DD5A01A5238F86DB21C96C3B7493464`。
可复核离线报告`.tmp/e7-mmf-runtime/mdfvidrender/mdfvidrender-analysis-v5.json`为9,568 bytes /
`A9B1E04F631ED7B6E22587A3AAF29304D77E699AEB16959C3274CF6464F8A285`；旧V3/V10只保留历史追溯，
不再代表最早失败边界。

### `init_like_91C8`内部的实际失败选择

`mdfinitselect01`（PID36659）在同次模块/vtable门全部通过后实际命中`mdf+0x9764`。同线程现场
`r1=mdf+0x984C, r2=0, r3=r5=0x80004005, r6=0`与`+0x754C`返回后的选择块精确匹配，
adapter为`0x2291E260`，保存renderer为`0x2291CA90`、外层接口为`0x2291CA94`。这次脚本把
ARM调用后已被覆盖的LR错误地当作必需分类条件，因而写了`public point miss`；原始寄存器和代码
现场证明公共汇入点确实命中，该字段只代表旧分类器失败，不能写成断点未命中。日志66,152 bytes /
`9F04CD2ED0F32C81A07CA122DA7EBA4A15162D8D3273D39C85357BCE53CD9FEE`。

修正后`mdfcall754c01`（PID36684）把单点移到调用`+0x754C`之后、`mov r5,r0`之前的
`mdf+0x96D0`。同线程、同adapter/renderer关联下实际读到`r0=0x80004005`，随后外层同方法也收到
同值。这证明EFAIL由`+0x754C`原样交给`init_like_91C8`，不是公共汇入块生成。日志69,370 bytes /
`0E2819B899177EB5C73B5F3D42C83A1874C668B83C4AE80113F696159BAA06F2`。

`+0x754C`的相关失败选择可按本机指令缩写为：

```text
raw = Connect-like(this+0x144)                 # +0x774C calls +0x10374
if raw != 0:
    goto select_error
...
raw = CreateAndInit-like(this+0x144, work)     # +0x7D1C calls +0x1060C
if raw != 0:
    goto select_error
...
select_error:                                  # +0x7E04
    if work.word0 != -1:
        selected = work.error_0x40
    else:
        selected = exception_to_hx(raw)         # +0x7E14 calls +0x12A30
    cleanup_without_replacing(selected)
    return selected
```

这个伪代码只给出已恢复的选择关系；`Connect-like`/`CreateAndInit-like`是嵌入字符串支持的语义名，
不是导出符号。`+0x7DFC`位于其他路径的公共服务/报告调用中，LR落在其后只能证明经过该清理路径，
不能把清理函数命名为错误生成者。

`mdfcall754cexit01`（PID36727）在`mdf+0x7E1C`暂停。旧脚本误把已于`+0x79A8`覆盖为1的r8
当成入口对象并拒绝生成业务记录，但保存的原始寄存器仍可复核：`PC=mdf+0x7E1C`、
`LR=mdf+0x7E18`、`r6=0x80004005`、`r0=0x80004005`、work=`r5=0x2291EB28`。相邻指令
`+0x7E14 bl +0x12A30; +0x7E18 mov r6,r0`证明映射函数这一次实际返回EFAIL并被选中。由于脚本
在对象关联前拒绝，本PID只证明内部返回路径，不单独声称闭合外层对象链。日志58,695 bytes /
`1B1B14E39E353E7518223934F332FF78A714B729416A7913625F4CA6E0E0FB33`。

`+0x12A30`本身是16项`TInt -> HX_RESULT`表映射：入口r0是原始Symbian错误；匹配时返回对应值，
未匹配时返回默认`0x80004005`。68字节不可重定位指令fingerprint为
`9693651F1E9E21EE942E80980C3D588AD60FA59856128EDCB5B929C803BC998F`；`+0x12A74`是运行重定位
表指针，明确排除在该fingerprint外。128字节表位于展开code `+0x28C30`，SHA-256
`B99D2EF310EF74ABD70F94F8C7500EAE28C6D5091680C2D090903F7ED49BCD20`。表明确包含
`-2 -> EFAIL`，但不包含`-44`。

前两次mapper尝试（PID36762/36773）均在Hx加载前停于横屏stage 2，不是错误链样本；第三次
PID37026因首版mapper fingerprint误含`+0x12A74`重定位literal而在身份门停止，也未设mapper点。
解锁后`mdfexceptionmap04`（PID37041）以同次load base、既有四个代码窗、重定位vtable和修正后的
68字节fingerprint建立身份，在`+0x12A30`第一条指令执行前实际捕获：

- 同线程`p37041.t37046`，调用者LR恰为`mdf+0x7E18`；
- 原始`r0=0xFFFFFFD4`，即有符号`-44`；adapter/renderer/外层接口关联全部匹配；
- `work.word0=0xFFFFFFFF`，所以指令路径选择mapper，而非`work+0x40`保存值；
- `-44`不在16项表中，因此入口处只能**预测**将走默认EFAIL；这不是已经观察到的写入或返回；
- 随后同线程、同renderer的外层方法实际返回EFAIL；mapper实际返回EFAIL另由PID36727证明。

目标链约10秒闭合后terminate；PID37041消失、两个断点移除和session end均确认；画面、声音、背景
全部UNKNOWN。日志72,894 bytes /
`40764B2D4808BC5ABEF0CFE6523D2333BCC8F4AA5398FCDE04BAB52578FB0D46`。目标SDK
`Symbian3Qt474/epoc32/include/e32err.h`把`-44`定义为`KErrHardwareNotAvailable`，说明这是环境或
硬件可用性错误；它本身尚不能区分资源已占用、服务连接失败、设备初始化失败或其他硬件不可用条件。
本阶段输入、脚本、全部有效/无效记录及限制冻结于
`.tmp/e7-mmf-runtime/mdfvidrender/mdf-failure-v11.freeze.json`（3,159 bytes /
`6D3E4C9C3624252095E9161B04C83F32C19090B29631C52EFA94ACB5FFC6FE39`）；14项清单逐项复算通过，
全套123项E7研究测试通过。私有DLL和设备日志继续保持本地忽略。

因此目前最早的实际失败链为：

```text
mdfvidrender +0x754C内部原始Symbian错误 -44 (KErrHardwareNotAvailable)
  -> +0x12A30表未命中，默认映射为HX EFAIL 0x80004005
  -> +0x754C / init_like_91C8 / renderer方法原样返回EFAIL
  -> hxmedplyeng partial marker及0x00040024
  -> StateCtrl -> Hx controller -> MMF Prepare event 0x00040024
  -> MediaClientVideo原样转发为Qt PrepareComplete(-12017)
```

截至PID37041，链中从`-44`到EFAIL的动态/静态组合已经闭合，但`-44`究竟由`+0x10374`的服务连接，
还是`+0x1060C`内的TRAP/CreateAndInit返回仍UNKNOWN。后续先完成生命周期对照；其无差分后，才按
同一既有范围恢复这两个调用后的直接返回点，不展开插件或图形栈。

## 结果分流与唯一方向

- raw event 的 `iErrorCode` 已是 `-12017`：旧失败会话已经满足；utility原样转发边关闭，不再研究
  Qt窗口。
- raw event 是其他值而 Qt 收到 `-12017`：这与当前 E7 Prepare 分支的数据流冲突；先停止并复核
  模块 fingerprint、事件 UID、实例关联和断点干预，不另猜 utility 转换分支。
- 已定位某个视频初始化调用的原始失败：只针对该条件提出一项可逆因果验证或最小修复，不补丁系统。
- `-12017` 同时出现在可见和黑屏，或 error/画面不再对应：撤销错误因果假说，下一点后移到同会话
  帧交付/renderer 证据；API success 仍不等于有效帧。

随后完成了可逆的**视频硬件资源生命周期对照**，没有改Qt窗口、权限或系统DLL。保持同包、同素材、
同输出合约和单次初始化；A前序用CODA terminate，B前序由用户先返回播放器、再从首页正常退出应用。
成功侧可能完全不进入错误映射器`+0x12A30`，所以统一判据为同一对象的`+0x754C`实际返回和Qt
Prepare结果，而不是要求mapper入口从-44变0。

主机退出代码复核进一步区分了三层行为：播放器Back的`releasePlaybackSurfaceForOrientation()`只调
`QMediaPlayer::stop()`并隐藏/保留对象图；`PLAYER_SESSION_PARKED`只证明返回页面，不证明关闭媒体。
返回首页后再次Back才进入`shutdownAndQuit()`；event loop退出后栈上`WiliwiliWidget`析构并删除
`VideoPlayerWidget`，后者依次stop/clearMedia/delete backend。Qt backend的clear路径实际执行
`stop -> setMedia(empty) -> setVideoOutput(null) -> hide`，随后析构删除`QMediaPlayer/QVideoWidget`。
因此B前序必须按顺序观察`PLAYER_SESSION_PARKED`、`APPLICATION_NORMAL_QUIT_REQUEST`、
`APPLICATION_CLOSE_EVENT`、`EVENT_LOOP_EXIT 0`、`PLAYER_SESSION_DESTROY_BEGIN`、
`QT_PLAYER_PARKED`、`PLAYER_SESSION_DESTROY_READY`以及同PID `processExited`；最后一个PID消失不能替代
前述析构证据。这些标记证明应用侧Qt对象析构路径返回，仍不是MDF服务端已释放资源的直接确认。

主机新增`coda_mdf_lifecycle_contrast.py`，所有前序和目标进程均使用相同的
`mdf+0x96D0`（`+0x754C`返回后、写r5前）与Qt `+0x74D2` Prepare点；保持同包、完整样本A、
`--e7-mode=full`和既有身份门。A/B/A/B四个目标各自独立建立前序，不复用上一目标状态。首次A1前序
执行尝试在CODA连接前因私有`pyserial`路径遗漏退出；没有`Processes.start`或设备PID。修正并经用户
放宽次数授权后，实际结果如下：

| 配对 | 前序PID／退出 | 目标PID | 前序结束至目标启动 | 目标`+0x754C`／Prepare |
|---|---|---:|---:|---|
| A1 | 69389／CODA terminate确认 | 69413 | 约18.5秒 | `0x80004005`／`-12017` |
| B1 | 69437／完整正常析构门及PID退出 | 69459 | 约15.5秒 | `0x80004005`／`-12017` |
| A2 | 69483／CODA terminate确认 | 69507 | 约11.8秒 | `0x80004005`／`-12017` |
| B2 | 69531／完整正常析构门及PID退出 | 69553 | 约13.9秒 | `0x80004005`／`-12017` |

两次B均按序取得全部应用侧标记；两次B前序自身也同样为`0x80004005/-12017`，用户观察为无画面、
声音正常、UI正常，随后按约定正常退出。四个目标在约10秒完成内部取证后由脚本主动terminate；用户
看到它们很快退出，故画面、声音和背景均记UNKNOWN，不能称为应用闪退或播放失败验收。A/B四个目标
的内部初始化结果没有差分，因此否定的有限命题是：**当前有证据的应用正常析构方式不足以稳定消除
下一进程的初始化失败**。这不证明不存在其他资源因素，也不证明服务端释放完成或资源泄漏。

生命周期对照无差分后，唯一取证点收窄为`+0x754C`内两个直接返回点：`+0x7754`在
`BL +0x10374`之后、CMP之前；`+0x7D20`在`BL +0x1060C`之后、CMP之前。两者直接读取r0，并用
`+0x754C`嵌套栈保存的调用者`+0x96D0`、外层renderer及adapter反向链接关联同一实例；不依赖LR。
静态指令证明两个非零分支都汇入`+0x7E04`，再由已观察的sentinel路径把原值交给mapper。

首版`mdfrawproducer01`（PID69720）曾只在公共汇入点`+0x7E04`取数：三段运行fingerprint及模块身份
匹配，r0再次实际为`-44`，但LR为下层遗留值而不是`+0x7754/+0x7D20`。AAPCS允许callee以
`pop pc`返回而不恢复LR，所以该方法不能区分生产者；这次只复证raw -44，不作来源结论。异常路径又
暴露PID枚举同时存在`pNNN`格式，原脚本未能在当次异常清理中证明PID退出。手机完整重启、重新插拔
USB后，只读枚举确认没有NIKINIKI进程，因此PID69720现已证明不再运行；该确认不是原脚本退出成功
证据。修正版`coda_observe_mdfvidrender_raw_producer.py`只保留`+0x7754`、`+0x7D20`和Qt Prepare，
并明确不把LR当证据。

重启后的`mdfrawproducer02`（PID69811）取得了完整目标链。实际加载的`mdfvidrender.dll`基址仍为
`0x7C980000`；三段调用路径fingerprint、既有代码窗、9项运行vtable、Qt Prepare fingerprint均匹配。
同一`p69811.t69816`、同一adapter `0x2291E260`、work `0x2291EB28`、renderer `0x2291CA90`及嵌套/
外层保存帧关联下：

- `mdf+0x7754`是`BL +0x10374`后的第一条指令，执行前r0实际为0；因此本次Connect-like调用成功，
  不是本次`-44`生产者；
- `mdf+0x7D20`是`BL +0x1060C`后的第一条指令，执行前r0实际为`0xFFFFFFD4`，即`-44`；因此本次
  `CreateAndInit-like +0x1060C`调用直接返回`KErrHardwareNotAvailable`；
- 随后同进程Qt session `0x04D7EF40`实际收到Prepare `-12017`。这与已证明的mapper、partial和
  controller链闭合，但仍不把API失败等同于人工黑屏；本次画面、声音、背景均UNKNOWN。

捕获约10.2秒完成后脚本terminate PID69811；`processExited`、PID空枚举、两个mdf点及Qt点移除均
确认。原始日志`.tmp/e7-mmf-runtime/mdfvidrender/mdfrawproducer02/coda.jsonl`为67,064 bytes /
`8C16068DBAA418F6DF273448D9678E72830DCF2A712AF81A4992B4E049B7E7D4`。本次证明的边界是
`+0x1060C`整体返回`-44`；该函数内部究竟是TRAP捕获的leave，还是后续内部创建/初始化调用直接返回
`-44`仍UNKNOWN。输入、首版无效分类、重启后空枚举、修正版脚本及本次完整链冻结在
`.tmp/e7-mmf-runtime/mdfvidrender/mdf-raw-producer-v14.freeze.json`（SHA-256
`BF1EC5581C886E1F0A76E1F876B730ABAC21AAC7EE5A1C7F986D973E6E5CE732`）；清单内7项文件的大小和
SHA-256逐项复算通过，E7研究测试136项及文档检查121项通过。

### `+0x1060C`两个内部错误出口

同机展开代码进一步把两个互斥来源恢复为：

```text
captured = 0                              # +0x10658 -> [sp+0x18]
TRAP(protected_1055C(client), captured)   # +0x10668；leave由handler写入[sp+0x18]
if captured != 0:                         # +0x10694..+0x1069C
    return captured

params.work = work                        # +0x106AC
params.tail = 0                           # +0x106B0
raw = wrapper_10494(client, 18, params)   # +0x106B4
return raw                                # +0x106B8写入后，+0x106E0原样重载
```

这不是源码函数名恢复。`+0x10698`位于`ldr r0,[sp,#0x18]`之后、`cmp`之前，可同时读取r0和栈槽；
若值非零，只能证明受保护的`+0x1055C`内部某处leave，不能把TRAP本身写成原因。只有该值为0才会执行
`+0x106B4`；`+0x106B8`是其后的第一条指令，r0尚未写回或比较。`+0x10494`把client加4后调用
E32 import trampoline `+0x5B0`，并原样返回r0；literal为`0x60B`，但没有匹配import映射，故不补造
提供DLL或公开符号。函数224字节fingerprint为
`974A35EBAB7F76BF5EBC74121F9205D60447F3572B79F0D9CF0A8E589111D93E`，wrapper 16字节为
`7216D51537F126752A1F2BE70A0DB2E9A85502D3098F18225D5DE08B2CB252A3`。

`coda_observe_mdfvidrender_create_init_source.py`同时核对`+0x1060C`保存的client/work、返回到
`+0x7D20`的LR、外层`+0x754C`保存的adapter、`+0x96D0`调用帧、renderer反向链接和`+0x1930`
外层帧；两个值互斥选择后再关联同进程Qt Prepare。首个执行`mdfcreateinitsource01`（PID69840）
在模块加载前停于既有横屏门：available已为640×284，但physical仍为360×640，6秒后
`orientation_timeout`。没有mdf模块加载、业务断点或Prepare记录；脚本terminate、PID空枚举和session
结束均确认，因此这是前置阻塞，不是错误链或播放结果。日志24,989 bytes /
`8F99A7C17E6006BADD2137630DFFE8BE4AFF69212446C072397D53F15FCB0E35`。

静态报告`.tmp/e7-mmf-runtime/mdfvidrender/mdfvidrender-analysis-v6.json`为11,092 bytes /
`544221B0DE548D45131D229DB149B3A10938D2F11AB9736020A626F73AA90F50`；点位、脚本、测试和无效启动
冻结于`.tmp/e7-mmf-runtime/mdfvidrender/mdf-create-init-source-v15.freeze.json`。142项E7研究测试
通过。设备恢复物理640×360前不机械重跑；恢复后唯一动态目标仍是
`具体内部调用 -> -44 -> +0x1060C返回 -> 同进程Prepare -12017`。

解锁后的`mdfcreateinitsource02`（PID69853）完成了该目标。实际`mdfvidrender.dll`仍加载于
`0x7C980000`，224字节`+0x1060C`、16字节wrapper、外层caller、既有代码窗和9项运行vtable
fingerprint全部匹配。同一`p69853.t69859`和同一client/work/adapter/renderer链中：

1. `+0x10698`实际r0与`[sp+0x18]`均为0，证明本次受保护`+0x1055C`没有leave；
2. 之后实际执行`+0x106B4 BL +0x10494`；在紧接着的`+0x106B8`，尚未写回或比较的r0实际为
   `0xFFFFFFD4`=`-44`，当时client=`0x2291EAB8`、work=`0x2291EB28`、request code静态为18，
   参数块=`0x2160BA74`且保留同一work；
3. `+0x10494`只把client调整到内嵌session handle，调用import trampoline `+0x5B0`并原样返回r0；
   因而`-44`是该同步IPC的直接返回，不是TRAP、wrapper或`+0x1060C`汇总产生；
4. 同进程Qt session `0x04D7EF80`随后实际收到Prepare `-12017`。

手机提取DLL的Petran import表把十进制ordinal 1547（thunk literal `0x60B`）唯一列在
`euser{000a0000}[100039e5].dll`；`Symbian3Qt474`目标的`euser.dso`将ordinal 1547命名为
`RSessionBase::DoSendReceive(int, TIpcArgs const*) const`。因此可复核的最早边界现为：

```text
CMDFDevVideoClient session
  -> RSessionBase::DoSendReceive(function=18, args containing same work)
  <- MDF DevVideo server response -44 (KErrHardwareNotAvailable)
  -> +0x1060C returns -44
  -> exception mapper default EFAIL
  -> partial / 0x00040024 / MMF Prepare -12017
```

SDK DSO只用于同UID/version ABI的ordinal命名，不证明手机`euser.dll`与SDK逐字节一致。请求18在目标
服务端对应的dispatch实现及其返回`-44`前的具体条件仍UNKNOWN；现有证据不能在资源占用、权限、
HwDevice创建或其他硬件不可用条件之间选择。捕获约10.1秒闭合后terminate PID69853，三个断点移除、
`processExited`及空PID枚举均确认；画面、声音、背景全部UNKNOWN。日志69,201 bytes /
`E1A31E2024765F4579DF658CBDCF9B74DC33156983AA677145E006DB4C1F4C99`，证据冻结于
`.tmp/e7-mmf-runtime/mdfvidrender/mdf-create-init-source-v16.freeze.json`（SHA-256
`35902853F9E3E854B95E07F012C02D7CEA8BA1BA36B08E9DAB97686618634395`）；清单内9项文件逐项复算通过。

上述IPC客户端边界已由下节继续推进；不再把“寻找服务端dispatch”保留为当前建议。

## IPC 18 服务端与异步 DevVideo 初始化边界

### 同机服务端映射

手机副本`mdfvidrender.dll`的代码和RTTI/字符串把客户端、服务端与初始化回调收在同一二进制中。
Connect路径构造的18字UTF-16描述符为`HLX_MDF_VIDEO_SERV`，调用的是目标SDK ABI中
`RSessionBase::CreateSession(...)`对应的import。实际PID70527还出现了同进程worker
`p70527.t70534`，线程名为`HLX_MDF_VIDEO_SERVd8ade8b`；因此本次服务不是凭线程名猜出的独立EXE，
而是由同机`mdfvidrender.dll`代码在应用进程内处理。线程名只作对象链的交叉核对，函数身份仍由
同次base、代码fingerprint、请求号和对象字段联合证明。

`CMDFDevVideoServerSession::ServiceL`的等效实现位于同机代码`+0xE494`。它从`RMessage2+4`
读取function；`function=18`经`cmd-10`跳表进入`+0xE814`。该分支把完整0x28字节消息保存到
server `+0x60`，随后在TRAP内调用`+0xE35C`。同机代码、import和公开类接口辅助得到的最小伪代码为：

```text
ServiceL(message):                         # mdf +0xE494
    if message.Function() == 18:           # branch mdf +0xE814
        server.savedMessage = message      # exact 0x28-byte copy to +0x60
        trapped = TRAP(CreateAndInitL(arg0))
        if trapped != 0:
            savedMessage.Complete(trapped)
        else:
            return                         # message remains pending

CreateAndInitL(arg0):                      # mdf +0xE35C
    ValidateInitParamL(arg0)               # +0xE2A8
    CreateDevVideoL(...)                   # +0xDB94
    ConfigureDecoderL(...)                 # +0xDAEC
    GetHeaderInfo/related setup(...)       # +0xD9A0
    SetClientThreadId(...)                 # +0xD61C
    devVideo.Initialize()                  # import call at +0xE420

MdvpoInitComplete(error):                  # mdf +0xF66C
    if error == 0:
        perform success-only interface work
    savedMessage.Complete(error)            # call at +0xF7BC
```

函数名是由本机RTTI/字符串、目标ABI import和指令数据流共同辅助命名；偏移与错误流来自本机代码。
公开DevVideo源码不是地址证据。PID70527的同次运行在`+0xE884`实际读取TRAP结果0，server为
`0x2291EB80`、保存消息为`0x2291EA64`、arg0/work为`0x2291EB28`；server `+0x50`的DevVideo对象
为`0x2291EDB0`，decoder/postprocessor HwDevice id分别为1/2。之后同一worker在`+0xF66C`
以`error=-44`回调同一server，并在`+0xF7BC`以`-44`完成同一保存消息；客户端同一work的
`+0x106B8`随即返回`-44`，Qt session `0x04D7EF40`再收到Prepare `-12017`。因此本次已排除：

- `ServiceL`的function 18同步参数验证/创建/配置/TRAP直接产生`-44`；
- IPC框架在同步返回路径凭空替换为`-44`；
- 客户端wrapper、异常mapper或Qt utility首次生成`-44`。

仍不能排除异步HwDevice初始化内部的业务失败、leave捕获或下层请求完成；`MdvpoInitComplete`只是
本次最靠近错误源的已证明转发点。`mdfserverinit01`（PID70505）在旧的`+0xE880`点读取了尚未
装入TRAP槽的陈旧r0，故该值无效；它仅保留为工具差错记录。有效PID70527日志为71,318 bytes /
`2DD387E6973C3DEE6DCC09E583F0DFDAD460E0838B5495AF550141C35C6AE358`，捕获完成后目标PID、owned
断点和调试session均清理；画面、声音、背景UNKNOWN。

### DevVideo选择结果与当前身份阻塞

`.tmp/symbian-source-mm`固定在提交`ebaa78373866f90dbf706e8d4eeb59ff65f1e107`。其中
`mmhais/videohai/devvideo/src/DevVideo/devvideoplay.cpp`说明该版本接口逻辑：`Initialize()`先初始化
decoder；decoder成功后如有postprocessor再初始化postprocessor；`MdvppInitializeComplete(device,
error)`按**精确device指针**选择分支并把非零error原样交给observer。该源码只能确定候选语义和需要
读取的参数，不能证明E7 ROM实现逐字节相同，也不能提供设备断点地址。

PID70569在真正调用`CMMFDevVideoPlay::Initialize()`之前完成了同次选择取证：

| 字段 | 实际值 |
|---|---|
| server / work / DevVideo | `0x2291EB80` / `0x2291EB28` / `0x2291EDB0` |
| decoder implementation / id / object | `0x10204C21` / `1` / `0x2291EE90` |
| postprocessor implementation / id / object | `0x10273417` / `2` / `0x2291F508` |
| primary / proxy vptr | `0x806091D0` / `0x80609214` |
| observer / initial state | `0x2291EBB8`=`server+0x38` / `1`（not initialized） |

以上布局与目标GCCE ARM EABI类声明一致：primary vptr `+0`、proxy vptr `+4`、observer `+8`、
decoder `+0xC`、postprocessor `+0x10`、state `+0x14`。但该进程**未执行Initialize、未取得callback、
未取得Prepare结果**：CODA没有报告`DevVideo.dll`的Shared Library load，候选ROM entry
`0x80602920`的header读取返回`functionality is not supported / AltCode -5`，脚本因此在`+0xE420`
前停止并terminate。PID70552则因脚本等待不存在的第三个module event而在ServiceL命中时停止，亦无
业务结果。这两次都不是成功/失败播放样本，画面、声音、背景UNKNOWN。

E7 RM-626/SW111.040.1511 core候选只提供`/Sys/Bin/DevVideo.dll`的ROM entry
`iAddressLin=0x80602920`、entry size 31,232、attribute `0x81`；entry size不能当code size，core仍为
compressed-unpaged表示。CODA只读文件获取`Z:\sys\bin\DevVideo.dll`又返回`-21 access denied`，且
未启动进程。故目前没有目标手机`DevVideo.dll`的逻辑文件、可靠code range或callback fingerprint；
UID、vptr和候选ROM地址只能建立强候选，不能满足动态断点身份门。

服务端脚本、DevVideo选择脚本、只读获取工具、两份有效/无效运行日志与同机mdf代码共同冻结于
`.tmp/e7-mmf-runtime/mdfvidrender/mdf-devvideo-async-boundary-v17.freeze.json`（4,077 bytes /
SHA-256 `8FF6BC7D60D585DA76CF64155EFB396C59B0B9AD5BB0E87BA0BF747645DC00A6`）；清单13项文件已逐项
复算一致。脚本语法检查和E7研究测试155项通过。

**该阶段的下一动作**是取得当前E7的只读`Z:\sys\bin\DevVideo.dll`副本。先离线核实ROM/E32表示、
UID、code address/size，并从proxy vtable恢复`MdvppInitializeComplete`等效入口及稳定fingerprint；
只有这些成立，才执行一次有界运行，在同一DevVideo对象上读取`device`指针与原始`error`。若device等于
`0x2291EE90`对应的新进程decoder对象，下一层只进入UID `0x10204C21`的实际HwDevice；若等于
postprocessor对象，下一层只进入UID `0x10273417`的实际HwDevice。没有该文件时停止猜地址，不追加
播放器或生命周期矩阵。

该动作及对应动态分流已由下节完成，不能再把“取得DevVideo副本”列为当前建议。

当时返回总审计摘要：**已证明**MDF IPC 18在同进程`mdfvidrender.dll`服务线程中处理；同步
`CreateAndInitL`的TRAP结果为0，随后DevVideo异步初始化回调以原始`-44`完成同一消息，再沿既有链到
Qt Prepare `-12017`。本次会话实际选择decoder `0x10204C21`和postprocessor `0x10273417`。
**推测**公开同版本DevVideo逻辑意味着`-44`应来自这两个HwDevice之一的初始化回调，但目标ROM实现
尚未动态验证。**未知**具体是decoder还是postprocessor，以及其内部哪项初始化条件失败。当时唯一下一方向
是先取得目标手机`DevVideo.dll`，可靠恢复回调点，再做同对象一次定点取证；不做窗口、延时、权限猜测、
系统补丁或DLL替换。

## 同机 DevVideo 路由与 decoder 失败归属

用户从当前参与试验的E7复制出的`Z:\sys\bin\DevVideo.dll`原件只读保存在本地忽略目录。文件为
31,232 bytes，SHA-256为
`6F0CBB47A4F136609F23AAFFA4BDB03E0B5F5D2ABACF64114950E84C62FB0BC2`。它是
`TRomImageHeader + mapped ROM code`表示，不是普通E32Image；header给出UID3/SID `0x101F9ED6`、
EABI/EKA2、`iCodeAddress=0x80602998`、`iCodeSize=iTextSize=0x7A00`。文件在0x78字节header后实际提供
31,112字节，覆盖`[0x80602998,0x8060A320)`；声明末尾120字节缺失，未补造，也未把文件尾当作完整
export表。分析记录`.tmp/e7-mmf-runtime/devvideo/devvideo-analysis-v2.json`为6,135 bytes /
`CCFE1DDE34852DA0C634BCB68B12BE8BDE7EB94F24C17B0399E1BD671A7E5943`。

PID70569已经实际读到的primary/proxy vptr `0x806091D0/0x80609214`均落在该同机文件可用代码范围；
proxy vtable的`MdvppInitializeComplete`槽（`+0x28`）为Thumb指针`0x80605AB9`。其thunk
`0x80605AB8: subs r0,#4; b 0x80605A70`把secondary proxy this还原为primary对象。`0x80605A70`
保存`device/error`，以`[this+0x0C]`和`[this+0x10]`精确区分decoder/postprocessor，然后分别进入：

```text
postprocessor callback handler 0x8060573E
    if error != 0: state=8; observer->MdvpoInitComplete(error)
                  BLX at 0x80605778, Thumb LR=0x8060577B

decoder callback handler       0x8060578A
    if error == 0 and postprocessor exists: postprocessor->Initialize()
    elif error == 0: state=4; observer->MdvpoInitComplete(0)
    else: state=8; observer->MdvpoInitComplete(error)
          shared BLX at 0x806057C4, Thumb LR=0x806057C7
```

两条失败路径都把保存的回调error原样传给observer；这里没有`-44`映射或汇总。首个运行
`devvideoinitsource03`（PID70803）验证同次live对象和两个vptr后，CODA读取ROM字节及在
`0x80605AB8`设置ROM断点均明确返回`functionality is not supported / AltCode -5`。脚本依约在
Initialize之前terminate并确认PID为空、owned断点清理；没有Prepare或播放结果。日志72,541 bytes /
`FD32CE4551CD191735207A21AC6CE926AF16BDEBE240185F57290442D8E8F5CE`。这只证明工具能力限制，
不能解释为callback未执行。

因此第二个运行不再尝试ROM断点，而在已验证可用的RAM `mdfvidrender +0xF66C` observer入口读取LR。
`devvideoinitsource04`（PID70824）通过相同包/样本身份、同次mdf fingerprint和同一
server/work/DevVideo对象关联，实际选择decoder UID `0x10204C21`（id 1）及postprocessor UID
`0x10273417`（id 2）。Initialize后同一服务线程在observer入口得到：

- `error=-44`，DevVideo状态为8；
- `LR=0x806057C7`，精确等于同机decoder handler中observer BLX后的Thumb返回地址，而不是
  postprocessor返回地址`0x8060577B`；
- 同进程Qt session `0x04D7EF60`随后Prepare `-12017`。

由此当前最早已证明链为：

```text
decoder implementation UID 0x10204C21 Initialize
  -> exact E7 DevVideo decoder callback handler receives error -44
  -> unchanged observer call (LR 0x806057C7), DevVideo state 8
  -> same MDF saved message completes -44
  -> client IPC 18 returns -44
  -> EFAIL / partial / 0x00040024
  -> same-process Qt Prepare -12017
```

捕获约10.4秒即terminate；PID为空和断点清理确认，画面、声音、背景均UNKNOWN。日志78,342 bytes /
`9D23F31666A94BE53C3118906C8DC058A5E214508521E5FE0512AB7CAF8FC53C`。这已否定“本次`-44`
来自postprocessor初始化”，但仍未看到decoder内部最先失败的调用。

E7 RM-626/SW111.040.1511 ROM审计只把`0x10204C21`与
`/Sys/Bin/ivevideodecodehwdevice.dll` basename/显示名作静态候选关联；该旧审计明确未可靠恢复pageable
代码和完整ECom字段。Nokia 603另有同UID到同名DLL的真机ECom证明，但属于另一设备/固件，不能补作
本次运行模块身份。两者只说明下一份材料的优先候选，不提供E7动态地址或失败条件。

**当时的唯一下一动作**是取得当前E7只读`Z:\sys\bin\ivevideodecodehwdevice.dll`副本，先按实际表示恢复
decoder Initialize与其下层异步回调/错误完成路径。只有同机指令能证明最先返回或转发`-44`的具体调用；
603副本只在对应接口已经由E7代码指向后用于辅助命名和有限差异解释。鉴于CODA不支持ROM代码断点，
下一动态点优先放在该decoder直接调用的可断RAM接收者/服务端回调并用同机LR回溯；若不存在可靠的
RAM边界，则明确停在同机decoder文件缺口，不再用启动次数猜测。

主机当时通过CODA FileSystem对`Z:\sys\bin\ivevideodecodehwdevice.dll`作了一次不启动进程的只读获取；
`FileSystem.open`返回`-21 access denied`，本地目标文件不存在。记录1,087 bytes /
`4349F593D2B2C7D9404C0A5D4A41704306C1553828887853C0168208132FF5CB`。该缺口随后由用户手动复制的
同机文件补齐；没有用ROM entry大小或603同名DLL替代。

当时返回总审计摘要：**已证明**同机`DevVideo.dll`把本次原始`-44`从decoder初始化回调原样转发，
PID70824以精确LR闭合到同进程Prepare `-12017`，postprocessor不是本次错误来源。**推测**E7 ROM
候选将UID `0x10204C21`映射到`ivevideodecodehwdevice.dll`，但尚缺本次运行的同机实现文件/代码身份。
**未知**decoder内部哪个初始化调用、资源或服务状态产生`KErrHardwareNotAvailable`。当时唯一方向是分析
同机IVE decoder并捕获其最早原始失败，不改播放器、权限、时序或系统二进制。

## 同机 IVE AccessDenied 与 `-44` 生成点

用户从当前参与试验的E7复制出的`Z:\sys\bin\ivevideodecodehwdevice.dll`只读原件为54,100 bytes，
SHA-256 `77E24AA6DD3382D07F2802159415B9F3B70D96790E8C70F637E75342DF6B3A87`。它同样是
`TRomImageHeader + mapped ROM code`，UID1/UID2/UID3为
`0x10000079/0x10009D8D/0x10204C1E`，EABI/EKA2，code address/size为
`0x80D25578/0xD354`。文件在header后提供53,980字节，覆盖
`[0x80D25578,0x80D32854)`；声明尾部120字节缺失，未补造。该哈希与此前E7 core候选重建出的
`Nokia_E7_00.dll`完全相同，因此**只对这一个文件**补足了候选ROM与手机副本的表示关联；不推广为
整套ROM逐字节一致。静态分析报告`.tmp/e7-mmf-runtime/ive/ive-analysis-v3.json`（7,302 bytes /
SHA-256 `E502A92558D020E7559DA2EED54A0E2DAC7C5ECFEA3D375DFEC13475DCDD53AF`）记录身份、代码窗和
下层stub；原件和报告均留在本地忽略目录。

实际AVC implementation UID仍为`0x10204C21`。同机文件的AVC primary vtable address point为
`0x80D317B4`，DevVideo公开ABI中的Initialize槽`+0x44`指向Thumb `0x80D26323`。Initialize及后续
policy callback代码内确有通向`ivepolicyserverclient.dll`候选范围的import veneer，但PID70939本次
实际失败没有从某个下层返回值原样带入`-44`。真正执行的错误出口为：

```text
CIveVideoDecodeHwDevice::AccessDenied(this)       # 0x80D273D2, Thumb
    flags = *(this + 0xBC)                         # 0x80D2743C
    if (flags & 0x20) == 0:                        # LSLS #26 / BMI at 0x80D2743E..40
        observer->MdvppInitializeComplete(this,
                                           -44)    # MVNS #43; BLX at 0x80D2744E
                                                    # return LR 0x80D27451
```

函数名并非按地址猜测：同一代码的trace literal低16位为`0x86`，入口/出口分别加2/3得到
`0x88/0x89`，component UID为`0x10204C1E`；目标SDK
`SymbianSR1Qt474/epoc32/include/platform/symbiantraces/autogen/ivevideodecodehwdevice_0x10204c1e_TraceDefinitions.h`
把这两个ID命名为`CIveVideoDecodeHwDevice::AccessDenied` entry/exit。名称证据来自目标SDK字典，地址、分支、对象偏移和
错误构造来自同机指令。`+0xBC` bit 5的内部字段名尚未恢复；动态执行AccessDenied且到达该BLX已足以
证明当时该位未置，后续运行又直接读取了该字段。

设备推进分为三条记录：

- `devvideoinitsource05`（PID70899）在MMF加载前被横屏准备超时挡住；没有目标模块断点命中或Prepare，
  随即terminate并确认PID为空。日志26,245 bytes / SHA-256
  `CED717A2E052B9A2F5BCC89E47E23A91BBDE9C6859F977B7BBD943C28806664C`，仅记前置阻塞；
- `devvideoinitsource06`（PID70914）进入同一decoder失败链，在RAM MDF observer处取到嵌套IVE LR
  `0x80D27451`。首版分类器尚未收录该第三出口，依身份门停止并清理，未等待Qt Prepare。日志73,789
  bytes / SHA-256 `6B085AED7A300A69BD67E0AB7F71705FE22B3CEE1BBDCB2A9033E5F8DD9FCAAC`；
- 修正并离线验证后，`devvideoinitsource07`（PID70939）在同一server/work/DevVideo/decoder链中再次
  取得DevVideo decoder LR `0x806057C7`、嵌套IVE LR `0x80D27451`、decoder this `0x2291EE90`。
  在observer调用尚未返回时，实际读取`decoder+0xBC`（`0x2291EF4C`）为`0x00000000`，bit 5确实未置；
  IVE构造并传出`-44`，同进程Qt session `0x04D7EF70`随后收到Prepare `-12017`。画面、声音、背景
  全部UNKNOWN，不作播放验收。日志81,372 bytes / SHA-256
  `22C32C4599AE6E9F7B129C1A3C8C03939B363B38DC9EF71508E083687E2FD50F`。约11秒后脚本terminate，
  `processExited`、空PID枚举及owned断点清理均确认。

当前最早有指令和运行证据的错误生成点不再是泛称“decoder初始化内部”，而是**IVE policy明确返回
AccessDenied后，decoder的拒绝处理分支因`+0xBC & 0x20 == 0`生成
`KErrHardwareNotAvailable(-44)`**。这份DLL已到厂商HwDevice层，但不是物理最底层：请求决策仍由
`ivepolicyserverclient`/`ivepolicyserver`完成，之后才可能涉及`rcam`/logical channel。此次`-44`
不是摄像头驱动或固件直接返回，不能继续沿驱动树追同一个错误码。

**该阶段留下的下一取证方向**是只追IVE policy的AccessDenied决定：取得同机
`ivepolicyserverclient.dll`与`ivepolicyserver.exe`身份，恢复`RequestIveAccess`请求字段及
`CIveContext::ExecuteDecisionOnCurrentClientL`的Granted/Denied选择条件；动态侧优先利用已有trace或
可断RAM消息边界，关联同一PID、surface/focus、priority与token。证据门槛是同一请求的实际字段及
Denied选择分支，不能用`-44`、线程名或SDK函数名代替。达到该门槛后才选择一项可逆对照；目前不增加
权限、不改priority、不补丁policy，也不恢复窗口或生命周期矩阵。

返回总审计摘要：**已证明**当前E7会话实际选中IVE AVC decoder；policy的AccessDenied回调实际执行，
IVE对象`+0xBC`为0，随后同机指令当场生成`-44`，并沿DevVideo/MDF/Helix闭合到同进程Prepare
`-12017`。**推测**拒绝可能与policy请求中的客户端资格、活动客户端或资源分配有关，尚不
选择其中任一项。**未知**policy服务端本次选择Denied的具体字段和条件。下一节继续收窄该边界。

## 同机 policy 身份、静态决策链与动态可观测性边界

本轮先从RM-626/SW111.040.1511 core只读重建两个policy模块；来源core为129,431,499 bytes，
SHA-256 `7B66B1D22914DCEB4313BB3CD2732B40D6E314CBE24B212432A4C388F0D1BD00`。用户随后从当前参与试验
E7的`Z:\sys\bin`复制出同名原件并保存至本地忽略目录
`.tmp/e7-mmf-runtime/ive-policy/phone/`。两份手机原件与固件重建候选分别**逐字节一致**，因此下表
静态code offset已从“同固件候选”升级为“同机文件映射”；运行时仍须用同次module-load事件确认基址，
不能只靠离线文件地址认定本次加载。

| 同机文件 | bytes / SHA-256 | UID / ABI | code范围与限制 |
|---|---|---|---|
| `ivepolicyserverclient.dll` | 4,972 / `CAB0774617B89E21A9327FC45A7A77AEAB6C9289933859A137753A87CCED5F73` | UID3/SID `0x10204C26`，EABI/EKA2，version `0x000A0001` | code `0x80D20F18+0x136C`；现有表示少声明尾120 bytes |
| `ivepolicyserver.exe` | 12,784 / `2DFFC361DDDB0BCC8B4DF339E9FD4FC6731726698CF4594B3B30AF6526839DC1` | UID3/SID `0x10204C27`，EABI/EKA2，version `0x000A0001` | code `0x80D22308+0x31B8`；现有表示少code尾64 bytes，data不在该逻辑副本中 |

原件只读，解析和派生报告另存；没有把原件、ROM或日志加入产品树。分析器
`tools/research/e7/analyze_ive_policy.py`冻结了本节使用的全部关键代码窗。以
`participating-phone-copy`身份重新生成的私有报告
`.tmp/e7-mmf-runtime/ive-policy/ive-policy-phone-analysis-v1.json`为12,964 bytes，SHA-256
`2DBD009ADC341C7BCCAC2BC357FA7F6847E977C3A5B9A5F4E14800BF348659A6`；public入口、封包、RunL、
ExecuteDecision、共用rule返回、Resource rule、allocator返回和Denied调用前窗口均通过。

目标SDK DSO把IVE decoder所调用的client导出确认为
`CIvePolicy::RequestIveAccessL(unsigned, TIveMode, int, int, RCam::TCamera,
const TCamResourceLevel*)`。当前手机IVE原件在`+0xD38`调用该导出，调用现场静态固定为：

```text
r0 = decoder+0x18 policy对象
r1 = aOriginalClientPid（具体值运行未知）
r2 = aUsageMode = 1
r3 = aCameraHandle = 0
[sp+0] = aWaiting = 0
[sp+4] = RCam::TCamera = 0
[sp+8] = decoder+0x188，指向0x24-byte TCamResourceLevel（内容运行未知）
```

目标SDK trace把`r1`明确命名为`aOriginalClientPid`，并另有“无法以指定PID打开client process”的
client记录；候选server的`CIveRuleClientProcess`也实际以current `+4`字段尝试建立进程信息。因此动态
取证必须把请求`r1`、server current `+4`和本次NIKINIKI PID三者比较，不能只关联当前执行线程。
这仍不证明既有失败就是PID错误；若三者一致且首条规则返回0，该候选才被本次运行排除。

候选client的public入口code offset `+0x28E`把这些字段传给impl；impl在IPC function 1中封包
`pid/mode/cameraHandle/camera/resource-present/control/resource-level`。`CIvePolicyImpl::RunL`
位于候选code offset `+0x7AC`：active-object `iStatus==0`时，response selector 0调用
`AccessGranted(token)`，selector 1调用`AccessDenied()`；因此本次Denied是policy业务响应，不是client
transport status错误。未读取到的resource-level内容仍为UNKNOWN。

服务端`CIveContext::ExecuteDecisionOnCurrentClientL`候选code offset为`+0x8CE`。身份由同一函数的
指令、trace component `0x10204C27`以及目标SDK字典中的entry/exit `0x17/0x18`和
Granted/Denied flow `0x09/0x0A`共同确认。其mode switch与七个RTTI/vtable证明：对于本次固定的
`TIveMode=1`，只按以下顺序执行：

```text
CIveRuleClientProcess
  -> CIveRuleNumOfActiveClients
  -> CIveRuleResource
```

本模式静态排除了`CIveRuleSenderCapability`、`CIveRuleNumOfCamera`、`CIveRuleMatchCamera`和
`CIveRulePreEmption`。这否定了“mode 1直接由SenderCapability规则拒绝”的静态候选，但不证明应用权限
与所有下层行为无关。`ClientProcess`先尝试按pending client `+4`中的PID建立进程信息；
`NumOfActiveClients`可设置`+0x90`短路字段，精确语义名尚未恢复；`Resource`把mode 1映射为资源码5。

`CIveRuleResource`的对象关系已经由构造代码和vtable闭合：`CIveResourceManager` primary对象的
`+4`是`MIveResourceAllocator` secondary interface；DecisionEngine保存该`+4`指针，Resource rule再
保存同一接口。rule的vslot 0落到`+0x131A` thunk，先`this-=4`，再进入ResourceManager allocator
`+0x1240`。allocator把pending client的token、PID、camera、资源码5及可选0x24-byte resource-level
传给RCam。

同一固件候选`rcam.dll`为3,952 bytes，SHA-256
`641537952D4E1EC95C0A0B3F3D6A5961C71BF62CAA8DDB4E9CAD1E5B5F74962B`，code
`0x80BC8FD8+0xF70`；其表示少声明尾120 bytes，仍只是固件候选。server import veneer目标与可用export
表及目标SDK DSO共同确认allocator直接调用ordinal 15/16，即带或不带resource-level的
`RCam::SetClientInfo(...)`；释放走ordinal 23 `RCam::RemoveClientInfo(...)`。active-client查询目标也
落在同一RCam代码范围，SDK ABI辅助命名为两个`GetClientsWithResourceLevel`重载，但对应export项在
缺失尾部，故保持ABI辅助映射而不写成完整映像证明。

最小错误选择伪代码为：

```text
current.decision(+0x44) = 0
if mapUsageMode(current) == 0:             # mode 1 -> current+0x54 = 5
    rc = resourceAllocator.allocate(current)
        # direct lower boundary: RCam::SetClientInfo(...)
    if rc == 0:
        current.decision(+0x44) = 1
        current.resourceGranted(+0x50) = 1
    else:
        current.decision(+0x44) = 0
ExecuteDecisionOnCurrentClientL(current)
    -> decision!=0: AccessGranted(token)
    -> decision==0: AccessDenied()
```

这里的`rc`是**下一次必须观察的值**；静态分析没有证明本次是RCam返回非零，也没有证明具体返回码、
资源持有者或拒绝条件。RM-779/SW111.020.0310（不是已知良好SW113实机）的对应client public/packaging、
server decision/policy-selection、ResourceManager构造、allocator和Resource rule代码窗均与E7候选逐字节
相同，只说明603材料有助于确认结构，不能解释两机行为差异或替代E7运行事实。

本机server trace字典还把以下事件明确分开：带token的AccessGranted、AccessDenied、从context移除
active client、从camera driver移除client info/reference、释放不配合client，以及忽略预期的remove
错误。它证明policy记录与RCam登记是不同层次，但不证明本次出现残留、执行过哪条清理或其先后顺序。
因此后续记录current `+0x48` token用于同请求关联；除非规则返回或自然trace实际指向清理差异，不增加
生命周期实验或清理断点。

### 同机动态尝试与停止边界

`ivepolicy01`启动现有完整应用Qt诊断（PID71154）后，同次创建policy server PID71162；load事件确认
`Z:\sys\bin\ivepolicyserver.exe`、code base `0x80D22308`与同机文件header一致。Qt RAM Prepare点已经
安装，但给server共用rule返回`+0xD98`增加显式`Hardware` breakpoint时，CODA立即返回
`functionality is not supported / AltCode -5`。脚本没有进入policy业务观察、没有取得Prepare，也没有把
这次写成错误或播放样本；应用PID被terminate并确认消失。脚本未终止共享server：原始枚举明确仍为
`[["p71162"]]`，最初汇总因旧PID解析器未接受`p`前缀而误写空数组，现已修正；server随后自然退出。
日志51,951 bytes / SHA-256
`1D7FA2BBE961151FA4D9767702B835054E3A8A2A3E0EDD61E55DC7393FD99E53`。

为区分“ROM breakpoint受限”与“没有失败”，`ivepolicytrace01`改为不设任何policy ROM断点，只保留
Qt Prepare RAM点及CODA `ProgramOutputConsoleLogger`（应用PID71176）。同次policy server PID71184再次
从`0x80D22308`加载，约10.4秒时同一Qt session `0x04D7EFD8`实际收到Prepare `-12017`，故目标错误已在
无ROM断点干预下复现；画面、声音、背景全部UNKNOWN。该日志只包含应用Qt输出和既有、无实例归属的PP
枚举统计，没有SDK字典中的RequestIveAccess、AccessGranted/Denied、original PID、active-client、
resource allocator或清理业务记录。因此`Logging`不是当前固件OST/TraceCore业务trace的替代品。
应用随后terminate并确认消失；脚本明确未终止当时仍在枚举中的共享server PID71184，后续只读inspect
确认其自然退出。日志55,653 bytes / SHA-256
`2852F2B204A96D785E22C0644557BFE30C92D24B0B144E1673E0AC0E7A6B197B`。

这两次把边界精确限定为：同机文件和同次server身份可靠，失败可复现；但当前CODA不能在目标ROM code
设置hardware断点，普通Logging也不暴露policy业务trace。`RunControl`报告的单步/范围单步能力不能从
模块入口直接设置一个“进入远处异步handler时停止”的目标，盲目跨活动调度器单步无法可靠关联请求，
故不以它绕过身份与调用帧门槛，也不继续机械启动相同脚本。已冻结的server观测offset仍保留为映射材料，
不是当前工具已可执行的断点契约。

主机SDK把这批字典对应的底层载体进一步限定为Nokia Autogen BTrace：
`SymbianTraceMacros.h`固定primary category `0xA0`，`RBTrace`公开了buffer/filter/get-data接口，Belle SDK
也有`btracec.dso`导出；但当前CODA TCF Hello只公布Logging等服务，没有Trace/TraceCore服务。本机未找到
TraceViewer/OST接收器。直接用`RBTrace`自制程序还会修改设备全局filter/mode并消费共享trace buffer，且
原始Autogen记录未先证明具备足以区分component和本次请求的上下文，所以它不是无需验证的安全替代。
CODA只读打开手机`Z:\sys\bin\btracec.dll`又被设备以`-21`拒绝；这只说明该路径不可读，不说明文件不存在。
E7 core静态目录可见`ostbuffer.dll`和`ostbufferwriter.ldd`，仍不足以证明主机接收链已经可用。本轮未安装
采集器、未改全局trace状态。

Belle SDK另有`RULogger`用户态会话，可查询已安装output plugin、现行filter和plugin配置，再显式开始/
停止采集；它比直接消费`RBTrace`更适合作为能力门。经单独授权，现已建立独立、可卸载的查询探针：
`e7_ulogger_probe.exe`由SymbianSR1Qt474 / GCCE 4.4.1以`arm.v5.udeb.gcce4_4_1`构建，E32头确认
UID/SID `0xE000B15B`、EKA2、capability位全零；9,319 bytes / SHA-256
`4782A144D1859BFEC3FCAD63EE77E03396DEC7DA42355F6E58E4F3912183EB4E`。它只编译了`Version`、
`Connect`、installed/active output、active plugin config、primary/secondary filter、secondary开关及
buffer/notification/mode的getter，没有`Start/Stop/Restart`或任何setter。自签名SIS为10,456 bytes /
SHA-256 `D945A6AD71F1EE136711F50A273C36502AD6C3F056EE9DD30D490E71251BF615`。

设备`ulogger-capability01`在RM-626 / 111.040.1511上安装返回0，`getPackageInfo`随后确认独立包
`E7 ULogger Capability Probe` 1.0.0；它没有覆盖NIKINIKI，保留为可按名称及UID卸载的`TYPE=SA`包。
安装前探针和`uloggerserver.exe`均无运行PID。第一次按文件名、第二次按完整安装路径
`E:\sys\bin\e7_ulogger_probe.exe`创建进程均返回`KErrNotFound(-1)`，没有取得PID，也没有进入
`E32Main`。因此不能把`-1`记成`RULogger::Connect`或任一getter的返回；所有ULogger接口结果都是
NOT OBSERVED。CODA随后只读打开已安装EXE及`Z:\sys\bin\uloggerclient.dll`均被受保护路径策略以
`-21`拒绝，这个相同拒绝不能证明文件存在或不存在，也不能区分EXE不可见、静态导入缺失或其他装载依赖。
本轮没有启动trace或媒体，没有改filter/config，没有重启或终止共享服务，也没有构建更高能力版本。
完整私有运行日志为5,839 bytes / SHA-256
`85E3A89E3934278913CAABA35704B02CA68CED08575CB07B47DD7BEFD22EC92F`。
最终只读复查确认包仍已安装，探针与`uloggerserver.exe`进程枚举仍为空；复查日志1,681 bytes /
SHA-256 `88FE9A0C2135074817B5808476382954BEB68D7DFF15411FB4B10889840622A6`。

用户随后用X-plore人工确认`E:\sys\bin\e7_ulogger_probe.exe`实际存在，排除了“安装记录存在但payload
未落盘”。探针E32 import table只有五个直接依赖：

| 直接依赖 | import证据 | 设备／固件证据 | 结论 |
|---|---|---|---|
| `drtaeabi{000a0000}.dll` | 21个ordinal | 同RM-626/SW111 core的`/Sys/Bin/drtaeabi.dll`存在；既有可运行程序也使用该ABI | 已确认不是本探针独有缺口 |
| `efsrv{000a0000}[100039E4].dll` | 5个ordinal | core存在；可运行NIKINIKI也直接导入 | 已确认可用基线 |
| `euser{000a0000}[100039E5].dll` | 55个ordinal | core存在；可运行NIKINIKI也直接导入 | 已确认可用基线 |
| `scppnwdl{000a0000}.dll` | ordinal 3 | core明确存在`/Sys/Bin/scppnwdl.dll` | 固件存在；未单独做运行ordinal测试，但不是缺文件候选 |
| `uloggerclient{000a0000}[1028304C].dll` | 13个ordinal | X-plore已检查位置未发现ULogger文件；同core ROM-FS也无匹配项 | **当前唯一缺失的直接装载依赖** |

`uloggerserver.exe`不是静态导入；只有`RULogger::Connect()`执行后才可能按名字启动。因此server缺失不能
解释当前“未进入`E32Main`”的装载失败。output plugin也是Connect之后由server/ECom枚举的运行期组件，
从未执行到该阶段；其具体文件和依赖仍未完整检查。`uloggerclient.dll`自身的传递依赖也因目标文件缺失而
无法列出，但它们不早于当前已缺失的直接模块。由“payload存在 + 四项其他直接依赖存在 + 唯一ULogger
直接依赖缺失 + loader `KErrNotFound`”可将本次失败边界确定为**缺少`uloggerclient.dll`运行组件**；
不需要、也不允许通过安装SDK DLL验证。ULogger支线至此关闭。

| 查询接口 | 本次实际返回 | 权限结论 |
|---|---|---|
| `Version()` | NOT OBSERVED | 未进入程序，无法验证 |
| `Connect()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetInstalledOutputPlugins()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetActiveOutputPlugin()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetPluginConfigurations()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetPrimaryFiltersEnabled()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetSecondaryFiltersEnabled()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetSecondaryFilteringEnabled()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetBufferSize()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetNotificationSize()` | NOT OBSERVED | 未进入程序，无法验证 |
| `GetBufferMode()` | NOT OBSERVED | 未进入程序，无法验证 |

探针自身声明`CAPABILITY NONE`且安装成功，只能证明签名／安装不需要为该EXE增加平台能力；由于装载失败
发生在API调用前，不能宣称这些查询在目标固件上“零能力可用”，也没有任何证据要求升级能力。

### ULogger关闭后的直接观测选项

下面只保留能观察**本次policy请求或服务端实际决策**的方案；另开一个`CIvePolicy`客户端会制造不同
session，不能替代当前decoder请求，故不列为有效取证。

| 方案 | 可直接回答 | 最小能力验证成本 | 剩余限制／扰动 |
|---|---|---|---|
| Nokia OST/TraceViewer兼容主机接收器 | 现有Autogen BTrace可直接给出`aOriginalClientPid/mode/waiting`、token、Granted/Denied及部分清理事件 | 先纯主机取得兼容接收器和字典；一次不启动媒体的USB握手验证。握手通过后一次失败播放捕获；不需要新应用capability | 当前主机尚无接收器，零售固件是否开放OST USB transport/trace activation未知；现有字典没有明确的allocator原始返回记录 |
| 直接`RBTrace`只读／采集器 | 读取同一category `0xA0`缓冲可看到上述已编译业务trace | 第一步需一个不改filter/mode的`CAPABILITY NONE`探针，只验证`btracec.dll`装载、`RBTrace::Open/Mode/Filter`；SDK头未声明所需capability，必须以返回值验证。若可用，正式采集另需一次明确授权 | E7 core无`btracec.dll`条目，实际Z盘存在性仍未证明；采集会消费全局buffer，若需启用filter还会改变系统trace状态 |
| 支持ROM断点的硬件／厂商调试器 | 可在已恢复的rule返回、Resource allocator和Denied选择指令处直接读current对象、返回值及分支 | 不依赖Symbian应用capability；成本是取得JTAG／厂商TRK类工具及第一次ROM断点能力验证，之后一次失败会话可覆盖核心点 | 当前CODA明确不支持ROM hardware breakpoint；设备和工具准备成本最高，但对allocator失败条件最直接 |
| 可逆ROM trampoline／policy定点插桩 | 可把rule／allocator输入输出写入独立RAM日志 | 需先证明精确指令覆盖、回跳、并发和异常清理，再取得内核级patch机制或厂商签名；至少一次安装验证和一次失败捕获 | 会修改共享系统代码，能力通常超出自签名应用；风险最高，不作为当前建议 |

该表记录的是ROM-shadow授权前的方案筛选。用户随后明确授权可逆ROM shadow定点诊断；实际能力结果见
下节。OST主机接收器未取得，未执行USB握手；这条旧建议不再与下节并列为“唯一下一步”。

返回总审计摘要：**已证明**手机policy副本与RM-626/SW111候选逐字节一致；本次mode 1静态只执行
ClientProcess/ActiveClients/Resource，Resource经allocator调用RCam SetClientInfo；同次server身份已
确认，且无ROM断点运行再次出现Prepare `-12017`。独立零能力ULogger探针已安装，X-plore确认payload
存在；静态导入和同固件模块清单把装载失败确定到缺失的直接依赖`uloggerclient.dll`，ULogger支线关闭。
**推测**Denied可能由客户端资格、活动客户端或RCam资源登记之一造成，尚未选择。**未知**实际请求
PID/resource-level、逐规则返回、allocator原始值和拒绝条件。该摘要已由下节ROM-shadow能力结果取代。

### ROM shadow v1：写入／恢复成立，动态断点能力不成立

用户授权后，主机建立了独立、无自动启动的固定白名单控制器。其E32头经目标SDK Petran核实为
ARMv5 EABI/EKA2、UID/SID `0xE000B15D`、capability全零，直接依赖只有`drtaeabi/efsrv/euser`。
EXE为8,598 bytes / SHA-256
`B8ED2B8B445C304D6935457DA689BA960C047BFB43A98FEEE99D8E23A5DD684A`；未签名SIS为9,020 bytes /
SHA-256 `814E1FEC0DDFE994374D433B7BCA167344F5C4FAD817051EBD79FB4C89094241`。控制器不接受任意地址或
payload，只允许audit/selftest/prime/restore四个一次性命令；固定halfword如下：

| 名称 | 地址 | 原字节 | 运行语义 |
|---|---:|---:|---|
| client request入口 | `0x80D211A6` | `FF B5` | Thumb `push {r0-r7,lr}`；入口参数尚未动态取得 |
| server Denied调用前 | `0x80D22D52` | `20 6C` | Thumb `ldr r0,[r4,#0x40]` |
| 共用rule返回 | `0x80D230A0` | `00 28` | Thumb `cmp r0,#0`；r0/r4/r5分别为rule结果/index/current |
| Resource allocator返回 | `0x80D24392` | `00 28` | Thumb `cmp r0,#0`；r0/r4分别为allocator结果/current |

无目标进程的PID71540自检实际执行
`0x80D230A0: 00 28 -> 00 BE -> 00 28`；写入、`User::IMB_Range`和每次readback均返回0，随后四点
全部回读原值、driver卸载返回0。prime阶段再把四个原halfword写回自身，实际建立byte-identical shadow页，
各点仍逐字节一致。这证明同机`patcherS3.ldd`的control 105写入、缓存同步调用及显式恢复链可用，
不证明含插桩代码的执行安全性。

动态能力检查使用冻结`e7bindtiming1` A和同一样本A。完整应用PID71547先因既有横屏门停在
physical 360x640而未进入MMF；没有安装policy点。冻结A PID71665随后因主机脚本错误把Hx ARM点声明
为Thumb而code-abort；该次全部作废，已修正为4-byte ARM并添加回归测试。修正后的PID71784和71904
均以同机Hx fingerprint证明ARM点可正常命中，并同次观察到server从`Z:\sys\bin\ivepolicyserver.exe`、
base `0x80D22308`加载。然而在server仍暂停时，CODA对已shadow的`server+0xD98`分别请求`Auto`和
显式`Software` breakpoint，均立即返回`functionality is not supported / AltCode -5`；早先原ROM
`Hardware`类型也同样被拒绝。拒绝发生在业务执行前，故没有取得request、rule、allocator或Denied字段。

每次停止都先移除已接受的应用RAM断点并确认实验应用PID消失；脚本没有terminate共享server，待其自然
退出后才调用restore。最后一次报告中四点的restore_before/restore_after/audit均为上述原字节，
`operation_result=0`、`driver_unload=0`、`free_page_called=0`。不调用FreePage避免误撤同页其他shadow，
因此含原字节的shadow页可能保留至设备重启；这不是物理ROM改写，也不能表述为shadow页已释放。

当前精确阻塞不是缺少policy地址，而是现有驱动只提供shadow分配／写入，没有**执行时透明跳板**和
**驱动拥有的并发安全记录存储**；当前CODA对该ROM线性地址的Hardware/Auto/Software断点又全部拒绝。
手工留下BKPT再靠异常改寄存器或猜测代码padding，会破坏原指令/CPSR或并发语义，不能作为观察方案。

**唯一下一取证方向**：仅设计一个专用LDD固定点instrumentation v2。最低风险候选不再寻找代码洞，
而是用固定Thumb undefined sentinel、`DKernelEventHandler::EEventHwExc`和驱动拥有的有界环形buffer；
handler只处理精确PC/进程/fingerprint，记录后模拟原指令并返回`EExcHandled`。必须逐点证明Thumb异常PC、
覆盖指令的等效执行、CPSR、栈8-byte对齐、用户内存XTRAP、并发和溢出；
记录以原始client PID/current指针关联，溢出只丢记录而不改变Granted/Denied。安装和移除只在policy
server未运行时完成，部分失败恢复全部原字节并逐点回读；未证明stub映射与缓存一致性前不生成patch。
证据门槛是：先以不启动媒体的无害固定函数验证“异常命中—记录—原语义模拟—移除—重启后原字节”全链；
通过后才对上述四点运行一次失败请求。正式播放器、IVE错误码和资源规则均不得修改。

当前设备仍安装独立包`E7 Shadow Patch Controller` 1.0.0，可按UID `0xE000B15D`卸载；上传SIS和
每次只读报告仍在E盘，命令文件已一次性删除。控制器和实验应用PID均不存在，policy server已自然退出，
patcher driver已卸载。详细私有材料位于`.tmp/e7-policy-shadow-v1/`。

返回总审计摘要：**已证明**同机policy四个目标halfword可由固定控制器shadow、IMB、恢复并精确回读；
最终字节全为原值。CODA在shadow页上仍拒绝Auto与Software断点，原ROM Hardware也已知拒绝。

### ROM shadow v2：CMP能力门通过并取得Resource allocator返回

早期PID72046/PID780分别证明undefined接管与PC偏移，并暴露`ThreadRawWrite(-38)`、错误选择local
CodeModifier及handler Close count误判；失败后驻留均由重启清除。随后`E7 CMP Exception Gate` 1.0.3
实际把独立宿主`cmp r0,#0`从`2800`临时改为`DE00`：原始、异常模拟和恢复后三阶段对输入`0/1/-1`
均得到`6/2/10`。PC偏移0，四次handler均成功，`simfail=0`、overflow=0，模拟NZCV与原CMP一致；
`RestoreCode=0`、回读`2800`、Close count=1、driver unload=0，最终PASS。这首次证明CPU实际执行了临时
指令且原CMP语义被透明模拟。报告SHA-256为
`4A7BE6623810ED1FA673F817632A29FB85DA449978F0FCA20EE7F47693378367`。

随后只安装同机已复核的policy公共rule返回`0x80D230A0`和Resource allocator返回`0x80D24392`，原
halfword均为`0028`。无媒体smoke完整通过安装、恢复、handler关闭和driver卸载。real1中observer正确
安装，但CODA启动第二进程清除了全局CodeModifier登记并自动恢复代码；该次0 hit、显式Restore为`-1`，
属于工具干预无效启动，不解释为规则未执行。只读probe随后确认两个完整代码窗均为原字节。

observer 1.0.1改由自身创建唯一完整应用Qt诊断进程。controller PID1127创建媒体PID1132；新policy
PID1140/TID1141中取得同一`current=0x00720890`的有序记录：

| seq | 点位 | 关键寄存器 | 运行事实 |
|---|---|---|---|
| 1 | rule公共返回 | `r4=0, r0=0` | 第一条rule返回0 |
| 2 | rule公共返回 | `r4=1, r0=0` | 第二条rule返回0 |
| 3 | Resource allocator返回 | `r0=0xFFFFFFFE` | allocator实际返回`-2` |
| 4 | rule公共返回 | `r4=2, r0=0` | Resource rule包装层随后返回0 |

四条记录`handled=4`、`simfail=0`、overflow=0，且CPSR before/after逐条相同。由此已证明本次mode-1
请求没有在前两条rule处终止，确实进入Resource；Resource下层allocator返回`-2`，rule包装层本身仍
返回0。`-2`是Symbian标准`KErrGeneral`数值，但当前证据不解释RCam为何返回它。画面、声音和背景均
UNKNOWN；本次不是播放验收，也没有同次Qt Prepare记录。observer取得记录后卡在媒体退出等待，用户
随后重启；只读postflight确认observer、媒体、policy及旧控制进程均不存在，`target_write=false`。
有效live报告SHA-256为`E400EE48795FC8A36D620DC708AEA30E0FBE7F1FA9CDFA0914A125BF3712C1EF`；
postflight SHA-256为`60B94054A3B643BBAC65631458917B6E6096B433C19F13CDEEA6AF1B2D5553EE`。

同机policy代码把直接边界限定到`0x80D23548..0x80D23620`。两路在
`0x80D235E8: 0006 / lsls r6,r0,#0`（反汇编别名`movs r6,r0`）汇合，最终于
`0x80D2361C: movs r0,r6`原样返回。最小ABI已由调用前数据流、policy import veneer、RCam候选函数体
和SDK DSO符号联合恢复：

目标符号映射使用`SymbianSR1Qt474/epoc32/release/armv5/lib/rcam.dso`（5,316 bytes，SHA-256
`493BE38BA50E94B9749405C8468BA7A1583A789335C1668DFFC80A1F8ABBA062`）；`readelf -sW`的dynamic
symbol index 15/16与`nm -D -C`的值`0x38/0x3C`共同给出下表两条签名。它只命名目标SDK ABI，运行时
身份仍必须由policy veneer和两个live RCam代码窗确认。

| 项目 | ordinal 15：带resource level | ordinal 16：无level | 来源与边界 |
|---|---|---|---|
| C++符号 | `RCam::SetClientInfo(TCamArg, TCamArg, TCamArg, TCamResourceLevel&, unsigned int)` | `RCam::SetClientInfo(TCamArg, TCamArg, TCamArg, unsigned int)` | `SymbianSR1Qt474` `rcam.dso` ordinal 15/16；最小`Symbian3Qt474`安装没有该DSO |
| policy调用／veneer | `0x80D235D6 → 0x80D224E0 → 0x80BC9779` | `0x80D235E4 → 0x80D224E8 → 0x80BC9753` | veneer 16-byte SHA-256 `93E4812B57B1299753558D012A213FE8B83FFC6AF65C074AC18C433C04477E92` |
| 分支条件 | `[current+0x14] != 0` | `[current+0x14] == 0` | 这是level-present分支事实，不从trace字段名猜测 |
| policy `BLX` link LR | `0x80D235DB`，返回到`0x80D235DA`后跳公共点 | `0x80D235E9`，直接返回公共点 | 这是callee保存并用于返回的incoming LR，**不是**公共点live LR |
| 各重载调用公共函数时的link LR | `0x80BC97A1` | `0x80BC9775` | 两值都被各自epilogue作为返回PC消费，**不是**policy公共点live LR，不能分类重载 |
| RCam候选窗 | `0x80BC9778+0x2C`，SHA-256 `ADE98E894886356169C125E5E2DDFAC8CD852D1C2ED503879B048B5A97379DBC` | `0x80BC9752+0x26`，SHA-256 `E19B8BA4A93949D23805228D1D23CB1571FA47C9C8E6B23E919C006506C11DE4` | `rcam.dll`仍只是同固件候选；observer安装前须从live ROM逐字节复核 |

两个重载的EABI参数来源如下；表中`sp`是allocator在`sub sp,#0x5C`后的固定栈基址：

| EABI位置 | 实际参数 | 调用包装副本 | allocator源栈副本 | current源字段 |
|---|---|---|---|---|
| `r0` | 隐式`RCam* this` | `[sp+0x4C]` | 不适用 | allocator primary `this+8`的嵌入RCam对象 |
| `r1` | 第一个按值`TCamArg`的地址 | `sp+0x10`，内容为PID | `[sp+0x54]` | `[current+0x04]`原始client PID |
| `r2` | 第二个按值`TCamArg`的地址 | `sp+0x0C`，内容为token | `[sp+0x58]` | `[current+0x48]`policy token |
| `r3` | 第三个按值`TCamArg`的地址 | `sp+0x08`，内容为resource code | `[sp+0x50]` | `[current+0x54]`；mode 1映射为5。精确枚举语义仍不命名 |
| ordinal 15入栈`+0` | `TCamResourceLevel&` | `[sp+0]=sp+0x18` | `[sp+0x18..+0x3B]`，0x24-byte/9-word | `[current+0x18..+0x3B]`；各word语义仍UNKNOWN |
| ordinal 15入栈`+4` | 最后`unsigned int` | `[sp+0x04]` | `[sp+0x3C]` | `[current+0x10]`camera handle |
| ordinal 16入栈`+0` | 最后`unsigned int` | `[sp+0x00]` | `[sp+0x3C]` | 同一`[current+0x10]`camera handle；`sp+4`不是该重载参数，不解释 |

RCam返回后`r1-r3`都是易失寄存器，不能在公共点把它们当成调用入参。observer保存固定0x5C-byte栈、
0x58-byte current、两份0x24-byte level、RCam this、attempt、原始`r0`、live LR、policy身份和
`r5=current`。重载用两个互相独立的事实分类：已验证policy代码的`current+0x14`分支，以及调用后仍在
allocator栈中的互斥outgoing-stack形状；ordinal 15为`stack0=sp+0x18, stack1=camera`，ordinal 16为
`stack0=camera`。重试循环的每次返回仍按同一current和顺序记录，最多32条。

#### MOVS门、SetClientInfo动态结果与同机RCam/EUser映射（已完成）

`E7 MOVS Exception Gate` 1.0.0初版因`RunL()`在0x4000进程栈上分配16 KiB descriptor而在报告前
溢出；修为heap buffer后，链接`RunL`栈帧为1,740 bytes。真机随后完整通过原始→undefined异常模拟→
恢复后三阶段的`0/1/-1 × 4种初始C/V`12组：`r6/NZCV`全部一致，PC bias 0，13 hit/handled，
simfail/overflow为0，恢复回读`0006`，handler Close count 1，driver unload 0。报告SHA-256为
`FA127D891F69617FA4B670CF473062E98DB19A37C1C8CB5E55BB320291795C13`；该能力门不再重跑。

1.0.0单点observer在唯一自有媒体PID916引发的policy PID924/TID925中，于同一
`current=0x00720890`记录一条原始返回：

| 字段 | 真机值 |
|---|---|
| 返回／重载 | `r0=0xFFFFFFFE (-2)`；`current+0x14=1`；`stack0=sp+0x18`；`stack1=camera=0`，故为**ordinal 15带resource-level重载** |
| 标量输入 | original client PID `916`；token `2`；resource code `5`；camera handle `0`；包装／源栈／current三份全部一致，`match=0x3F` |
| 对象／次序 | RCam this `0x00720470`；current `0x00720890`；attempt `0`；这是字段原值，不是记录序号 |
| resource level | `34363248, 02800168, 00000000, 02800168, 72707768, 01900000, 00720984, 00000020, 00000001`；栈/current两份逐word一致，语义仍UNKNOWN |
| MOVS透明性 | `r6_after=0xFFFFFFFE`；CPSR `08000030→88000030`，与独立MOVS门一致；无snapshot/simulation/association/overflow失败 |
| LR | `0x80484F75`；旧`lr_ord=0`只说明原候选LR分类假设错误，不否定这条原始记录 |

该次宿主未走到final清理报告，不能写成PASS；用户重启后只读postflight已确认全部相关进程为空。
捕获报告和postflight SHA-256分别为
`2AAD3E131DE8EC1729F4C76041C7091E3F8F8F12D9FB0A36ADE0FE406F65F96D`和
`10A653F6FB2199453A02E4D1E522DC489485ECDBF99DE3697516E7369590DA6D`。

随后1.0.1加入完全不安装sentinel、不启动媒体、不初始化CodeModifier的mode 0同机映射门。实机结果：

- RCam ROM header及allocator/policy veneer、ordinal 15/16、公共控制函数`0x80BC9302`、控制veneer
  `0x80BC9088`共七个有界窗口全部一致；
- `0x80BC9088`同机目标为`0x80484F6D`；同机EUser header为
  `10000079/1000008D/100039E5`，code `0x80478808+0x497D4`，export count 2564，ordinal 491也
  实际指向`0x80484F6D`；目标SDK `euser.dso`将ordinal 491命名为
  `RBusLogicalChannel::DoControl(int, void*, void*)`；
- 目标16 bytes为`10 B5 00 68 F6 F7 62 E8 10 BD 30 B5 06 29 89 B0`；入口先取channel handle，
  `0x80484F70`的BLX把返回LR置为`0x80484F75`，故该LR是两重载共享的下层残值；
- mode 0报告PASS、driver unload 0，随后只读postflight全空。报告／postflight SHA-256为
  `C7B7F6F643B894324066B9A6E06295BE45961E8EA1199D159FA3AD22F85C28C1`／
  `FDD65B84C1BD3CC37DCEE8706450E0DBAD1560588D6BD410436D9DF4CF73DC56`。

因此不再重跑媒体捕获。RCam两个重载都把请求送到同一公共调用；ordinal 15当次等价的下层边界是
`RBusLogicalChannel::DoControl(function=0, a1=(void*)7, a2=bounded SetClientInfo request)`，其`-2`
被RCam和policy allocator原样返回。这不是服务端IPC，而是camera logical channel控制调用；下一步不能
继续泛称“RCam服务端”。

1.0.1已改用`Kill`前后落盘检查点及5秒`ExitType()`轮询，仍保留45秒捕获、10秒policy退出和恢复失败
驻留规则，不导入`Logon/WaitForRequest`。host UID3/SID `E000B165`、NONE；LDD UID2 `100000AF`、
UID3 `E000B166`、ALL。signed SIS为20,116 bytes，SHA-256
`120C0DB0377CA48AEED30CE6B371E2004FB6C869F30D85E280F1032BAA8820D5`，证书有效至2036-08-21。

离线分析v6、MOVS离线门、observer 1.0.1离线门、交叉设备验收JSON的SHA-256依次为
`FC8C9129804C1B8D12DF5AA96B2CAD2E981D4C23534D6336CBDF5D7A76338F65`、
`4811B160A77D065EE6C9985EBA23E7128E9AF4DD9E0CB5424609133015AE639D`、
`4B48428CC562CCBF79495A8659C8CE1395AD1508A1380039F4E285C580557607`、
`329968D43BA6000144E159405C236CF9FB5F24780E4B625A48226040BC4C1DDA`。

**截至该次真机捕获已证明**：ordinal 15带resource-level的`SetClientInfo`返回原始`-2`，上述有界输入
成立；`-2`来自camera logical-channel control 0/subcommand 7并被上层原样传播。当时仍未知camldd控制
处理器中最早生成`-2`的条件，以及本次画面、声音、背景和同进程Qt Prepare结果。随后执行的唯一离线
恢复见下节；没有重跑既有真机点，也没有向其他依赖树铺点。

#### `camldd` control 0 / subcommand 7离线恢复与有限分支（2026-09-10）

离线恢复使用RM-626/111.040.1511固件候选中的完整ROM表示；`camldd.ldd`、`dcam_policy.dll`、
`dcamdecoder.dll`、`dcam_use_case.dll`的SHA-256依次为
`BEAC868B8188DDF2459D3847F92BC7FC8D3ED8111FC99612CC96A34979CDE41B`、
`648AAD2EA8BEA19A29641415CC49F76E4BB41956FC4C31EC12216B4434B6C5AD`、
`F38CD9ED8ABBFC37CB7C226EF71E4783BBDBDC4B5A31E2A22D87327D5315243F`、
`19AF225E3B856A27AAEEEE3EE2CA969EB3A83FA939EAEAF483E7DC9068A6957F`。这些模块尚未像RCam/EUser
一样取得同次live代码窗，故下列地址仍是同固件ROM候选静态地址，不是旧PID运行状态。

`camldd`入口`0x803FFB98`在function 0复制`a2`的20 bytes，以`a1=7`验证channel状态，再从
`0x803FF8C0`分派到`0x803FF70C`。验证器只在channel状态1接受subcommand 7，拒绝时本地产生`-5`而
不是`-2`。handler在`0x803FF768`调用`DCamPolicy::SetClientInfo` ordinal 8目标`0x8040F8E4`，
随后不改写`r0`直接返回。因此，模块身份成立时，本次外层`-2`已经排除camldd验证拒绝；camldd
subcommand 7本体没有本地`-2`生成点。

| 20-byte请求位置 | camldd读取／传递 | 本次有界值 |
|---:|---|---:|
| `+0x00` | `TCamArg`包装地址置于`r1`；静态本身不命名内容 | `916`；上层三副本已证明等于original client PID |
| `+0x04` | 第二个`TCamArg`包装地址置于`r2` | `2` |
| `+0x08` | `TCamChannelUseCase`值置于`r3` | `5`；不扩写未证实枚举名 |
| `+0x0C` | `TCamera`值置于stack `+0` | `0` |
| `+0x10` | 非零时复制恰好36 bytes并以stack `+4`传副本地址，零时传空指针 | 本次非零 |
| channel `+0x2C` | 作为`DCamPolicy* this`置于`r0`；不是请求字段 | 运行值未捕获 |
| channel `+0x30` | 以stack `+8`传`DThread*`；不是请求字段 | 运行值未捕获 |

usecase值5进入`DCamDecoder::Create` ordinal 14。九个level word在其initializer中的实际读取如下；
名称继续保持UNKNOWN，不能由数值外观命名：

| word | 原值 | 本次路径中的用途／分支 |
|---:|---:|---|
| 0 | `34363248` | 复制并参与32-byte临时描述 |
| 1 | `02800168` | 复制，另分别读取低／高16位 |
| 2 | `00000000` | 该initializer未读取 |
| 3 | `02800168` | 非零，走直接值分支 |
| 4 | `72707768` | 非零，走列表查找而非空值后备分支 |
| 5 | `01900000` | 传入转换调用并复制 |
| 6 | `00720984` | word 7临时分配成功时作为32-byte源地址 |
| 7 | `00000020` | 长度32，不触发`>0x200`的本地`-4` |
| 8 | `00000001` | 命中允许分支，不触发本地`-6` |

当前输入可达的`-2`候选已经收敛为下表。这里的“原样返回”只描述本层行为，不宣称目标内部原因：

| 候选 | 可达条件与本层行为 | 一项候选所需区分证据 |
|---|---|---|
| policy一次性初始化 | `[policy+0x108]==0`时，三项初始化调用和随后的queue初始化错误可在helper中原样返回 | 同次`[policy+0x108]`及SetClientInfo `0x8040F9C8`处helper原始返回；当前UNKNOWN |
| `DCamUseCase::BaseCreate` | policy helper成功后，`DCamDecoder::Create`于`0x80417E14`调用；target `0x804049C8`又把`0x8009E768`结果原样返回 | BaseCreate返回为`-2`；返回0才进入initializer |
| D1 | `0x80417058→0x80407ED8`（SDK ordinal 70）错误原样返回 | initializer公共返回`r0=-2, LR=0x8041705C` |
| D2 | `0x80417100→0x80407E30`（ordinal 71）错误原样返回 | `r0=-2, LR=0x80417104` |
| D3 | `0x80417134→0x8040798C`（ordinal 120）错误原样返回 | `r0=-2, LR=0x80417138` |
| D4 | `0x804171A0→0x804087C0`（ordinal 53）错误原样返回 | `r0=-2, LR=0x804171A4` |
| L1，本地`-2` | `0x8041728C→0x8033FAA4`后，写入local `sp+0x14`的out-status非零；`0x8041742C: mvn r0,#1`规范化为`-2` | `r0=-2, LR=0x80417290`并保存out-status；不命名其业务语义 |
| D5 | word 7非零且临时分配成功后，`0x804172F0→0x80097C8C`错误经释放后原样返回 | `r0=-2, LR=0x80417304` |
| D6 | `0x8041735C→0x80405630`（ordinal 127）错误经释放后原样返回 | `r0=-2, LR=0x8041736C` |
| D7 | `0x804173B0→0x8040819C`（ordinal 72）错误原样返回 | `r0=-2, LR=0x804173B4` |
| D8 | `0x804173F0→0x8040819C`（同一ordinal 72）错误原样返回 | `r0=-2, LR=0x804173F4` |
| L2，本地`-2` | `0x80417418→0x8033FCCC`返回0；同一`0x8041742C`规范化为`-2` | `r0=-2, LR=0x8041741C`且原始调用返回0；不命名其业务语义 |

policy对usecase 5的前置配额检查只本地产生`-14`或`-5`；decoder对象分配失败规范化为`-4`；word 4
列表未命中为`-5`。这些都不是本次`-2`候选。initializer公共返回`0x804172C0`前的`r0/LR`能唯一
区分D1–D8、L1和L2，但它不能覆盖在此前返回的policy一次性初始化或BaseCreate，因此未命中不是可判定
结果；所有行仍只是静态候选，不能把“最早静态候选”写成“本次实际原因”。完整地址与身份表见
`.tmp/e7-camldd-control7/README_ZH.md`，SHA-256
`A1694C65342CD55D2E7A0C003783689BDFFD94C2B778E761D16E6555E348B55A`。

本轮不进入设备：上述模块是ARM代码，而现有handler只验证过Thumb CMP/MOVS；公共点首条是
`add sp,sp,#0x4C`，尚无独立ARM/SP/CPSR透明门。更重要的是，当前至少需要跨policy helper、BaseCreate
和initializer多层铺点才能完全分类，不满足“一次定点观察能区分候选”的准入条件。没有新observer构建、
签名、安装或CODA连接。集中研究转为长期小投入；**唯一下一方向**是纯离线消除两个上层旁路，或证明
一个覆盖它们及initializer分支的单点ARM分类契约。做不到就不恢复真机执行，也不追完整驱动树。
