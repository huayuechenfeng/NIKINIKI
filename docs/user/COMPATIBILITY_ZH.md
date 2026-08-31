# Symbian³ / Anna / Belle 统一安装包兼容策略

> 状态：Active
> 更新日期：2026-08-31
> 结论：正式版只维护一个以 `Symbian3Qt474` 构建的应用 SIS；该包已在 Nokia 603 / Belle 实测正常。原版 Symbian³ 和 Anna 另行提供离线 Qt/Mobility 运行库说明并在公测收集结果，不为三个系统分别编译应用。

## 1. 发布决定

统一应用包以当前代码的最低目标 SDK 构建：

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-App.ps1 -Configuration release
```

`Detect-SymbianQt.ps1` 现在优先选择 `Symbian3Qt474`；Belle 的 `SymbianSR1Qt474` 保留作后备工具链和设备诊断，不再作为公开应用包的默认编译目标。

发布物计划分为两类，但应用本身只有一个版本：

- `NIKINIKI_版本号_release.sis`：统一的 ARMv5 应用包，面向原版 Symbian³、Anna 和 Belle；
- 运行库前置包/安装说明：只给缺少相应运行库的原版 Symbian³、Anna 用户使用，不能依赖已经停止服务的在线 Smart Installer。

当前 SIS 明确要求 Qt `4.7.4` 和 Qt Mobility `1.2.0` 或更高兼容版本。Belle 真机已有当前项目所需运行环境；原版 Symbian³、Anna 必须先确认或离线补齐这些运行库。缺少运行库时是安装前置条件不满足，不代表需要另一套应用二进制。

本机 Qt SDK 已找到 Anna 离线候选 `Qt-4.7.403-for-Anna.sis` 和 `QtMobility-1.2.1-for-Anna.sis`，但原版 Symbian³ SDK 目录目前只有 `qt_stub.sis`、`qtmobility_stub.sis` 与在线 `smartinstaller.sis`，没有已验证可分发的完整原版 Symbian³ 离线运行库。stub 只用于依赖声明，不能当运行库安装；Smart Installer 也不能作为正式发布依赖。因此，“应用统一包”已经完成，“原版 Symbian³ 离线运行库套件”仍是发布材料阻塞项，不得用 Anna 包未经真机验证直接替代。

## 2. 为兼容旧系统做的唯一源码改动

旧 Belle 构建额外链接了 `cookiemanager.dll`。该库在已安装的原版 Symbian³ SDK 中没有 ARMv5 导入库，是统一包的唯一实际链接阻塞。

当前实现已经：

- 删除 `<cookie.h>`、`CCookie` 和 `-lcookiemanager`；
- 直接通过所有三代系统共有的 Symbian HTTP header API，逐个读取重复 `Set-Cookie` 的 `ECookieName` / `ECookieValue`；
- 保留原始 `Set-Cookie` 文本回退，并继续交给现有 Cookie 合并/持久化逻辑；
- 增加 `WW:COOKIE_PARTS partCount capturedCount rawBytes` 日志，便于真机确认二维码登录没有漏收 Cookie。

播放器、MMF/DevVideo、FFmpeg、GLES、窗口状态机和媒体选择政策均未改动。

## 3. 构建与 ABI 证据

2026-08-29 使用 `C:\QtSDK\Symbian\SDKs\Symbian3Qt474` 对当前 0.9 主线全量构建：

| 配置 | 结果 | 本地构建 SIS | 大小 | SHA-256 |
|---|---|---|---:|---|
| Debug | `sbs errors: 0`，32 条既有 SDK 警告 | `symbian/out/wiliwili-symbian-debug/wiliwili_symbian.sis` | 9,797,988 | `1C9D825BE4B8DBAFDCA5379FB69DE2D85D04557D3A08401DD9A278F66E843C9D` |
| Release | `sbs errors: 0`，32 条既有 SDK 警告 | `symbian/out/wiliwili-symbian-release/wiliwili_symbian.sis` | 9,807,260 | `14C9D9F31681B2FBA3258005948C9BCC6B76BCB02D6CA70DED19914C0DF67741` |

`elftran -dump i` 复核 Debug/Release 导入表后均不再出现 `cookiemanager.dll`。QtCore、QtGui、QtNetwork、QtOpenGL、QtMultimediaKit，以及当前实际使用的 Avkon、WSERV、RHTTP、MMF、DevVideo 和 GLES2 导入均由原版 Symbian³ SDK 成功链接。构建前的双 SDK 导入序号审计也确认当前用到的这些符号在 `Symbian3Qt474` 与 `SymbianSR1Qt474` 中一致。

这证明最低 SDK 编译/链接与静态 ABI 门已通过。随后同一最低 SDK 包已在 Nokia 603 / Belle 实测正常；先前报告的“主菜单后黑屏、点击闪退”来自误装其他包，明确作废。原版 Symbian³、Anna 真机结果留待公测收集，不能把 Belle 结果扩大为全系已验证。

## 4. 功能、性能与稳定性影响

- 功能：应用功能没有按旧系统裁剪；Cookie 捕获实现被等价替换，Belle 发布回归和原版/Anna 公测都应覆盖二维码登录。
- 性能：没有切换编译器、CPU 目标或解码策略；仍为 GCCE 4.4.1 / ARMv5，设备运行时仍调用各自固件 DLL。预计无可测性能损失。
- 稳定性：以最低 SDK 构建可避免引用 Belle 新增导出，降低旧系统启动时 `Missing DLL`/ordinal 错误风险。实际窗口、TLS、MMF/DevVideo 和内存行为仍由固件决定，必须真机验证。
- Belle：同一包不应因“由 Symbian³ SDK 编译”而失去 Belle 固件已有的运行时实现或硬件解码性能；源码没有禁用 Belle 能力。

因此，维护多套应用包不会带来明确收益，反而会扩大测试和发布组合。只有以后出现无法通过运行时检测/兼容代码解决的真实系统 ABI 分叉，才重新评估单独包。

## 5. 真机与公测矩阵

统一包先以 Nokia 603 / Belle 作为发布前回归基线；原版 Symbian³、Anna 使用同一个有效签名 Release SIS 在公测中收集：

| 系统 | 安装前置 | 最小通过项 |
|---|---|---|
| 原版 Symbian³ | Qt 4.7.4 + Qt Mobility 1.2.x | 公测待收集：冷启动、首页 HTTPS、图片、QR 登录/重启保持、MMF 正常流、FFmpeg 风险流、返回/重复进入 |
| Symbian Anna | Qt 4.7.4 + Qt Mobility 1.2.x | 公测待收集：同上 |
| Nokia Belle | 固件现有兼容运行库 | Nokia 603 1.1.0 实测正常；N8 / E7 / X7 / C7 推荐升级 Belle 并使用 `OpenFileL` 边下边播 |

QR 登录是本次兼容改动的专项门：日志须出现非零 `WW:COOKIE_PARTS` 捕获，并最终满足 `WW:LOGIN_COOKIE_SUMMARY true true true`、账号资料成功加载、退出重启后会话仍有效。

仓库 `prerequisites/` 已保留从 Symbian Anna SDK 提取的 Qt 4.7.4 与 Qt Mobility 1.2.1
离线安装包，来源、大小和 SHA-256 见 1.0.0 发布说明。原版 Symbian³ 仍需由公测用户验证这些
运行库在具体固件上的安装与运行结果；这不改变统一应用 SIS 或 Belle 已通过结果。

## 6. 正式发布包

1.1.0 的 `Symbian3Qt474` Release 产物已经剥离 SDK 旧签名，并使用当前有效证书完成重签和公开发布。
安装包身份、大小、SHA-256 与证书信息只在
[1.1.0 发布说明](../releases/RELEASE_1.1.0_ZH.md)维护。SDK 自动生成的过期自签名包、
历史 Belle 包和诊断包均不能替代正式统一包。
