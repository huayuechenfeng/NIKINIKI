# NIKINIKI（曾用名 wiliwili for Symbian³）0.9.0 最终真机验证包

> 更新日期：2026-08-29  
> 状态：当前源码已合入最终原生横屏状态机、独立不透明 soft RGB565 视频表面和低频 `PositionL()` 缓存，并已用最低 `Symbian3Qt474` 通过 Debug/Release 构建；下面列出的 current-certificate 签名包仍是历史 Belle 构建，不能作为这次改动或三系统统一包的正式候选。

> 真机更正：最低 SDK 统一包已在 Nokia 603 / Belle 实测正常；先前“主菜单后黑屏、点击闪退”是误测其他包，结论作废。原版 Symbian³ / Anna 兼容性留待公测用户验证。

## 1. 本版本内容

0.9 将此前的 `ffmpegsoft2` 合并到主线。普通构建同时包含两条本机播放路径：

- 兼容的 H.264/AAC 继续优先使用原生 MMF 硬解；
- Broadcom H.264 插件拒绝真实首个 sync-sample header 的码流转入 GCCE 4.4.1 构建的 PPSSPP-FFmpeg H.264 软解，AAC 仍由 MMF 提供；
- FFmpeg 软解现在默认使用已有的 CPU YUV420P→RGB565 `QImage` 路线，视频由持久、不透明的原生子窗口呈现；单一透明 ARGB 顶层只绘制弹幕和控制并始终位于视频上方；三平面 GLES upload 已退出 soft playback 默认路径；
- 旧 GLES shader/纹理代码仅保留为隔离的后续研究代码；不会创建第二个 EGL surface，也不会销毁已验证可重复进入的播放器/MMF 对象图；
- GLES 播放时隐藏的播放器不再丢失键盘焦点；主 `QGLWidget` 会把按键转发给活跃播放会话，保留暂停、恢复、返回和菜单操作；
- preflight 接受的正常码流输出 `DEVVIDEO_HEADER_PREFLIGHT_ACCEPT` / `ROUTE MMF` 并留在 MMF，不会被软解强行接管；
- `ffmpeglatedrop1` 仅为诊断配置；当前主线保留 Avkon panes，按 `workAreaResized()` 提交 native landscape，并让 MMF、CPU RGB565、弹幕、控制和输入直接使用 640×360 坐标；旧 90° 整帧旋转已删除。

软解已经在 Nokia 603 为原有声无画视频输出真实画面。CPU RGB565 输出此前实测约 11.4–12.0 fps；切换前的 GLES-YUV 候选实测约 216 ms/帧上传、约 321 ms/帧提交，已确认不适合发布。日志 `2746319` 随后证明原生横屏主线能连续播放两条 soft 与两条 MMF 视频，但旧实现的 RGB565→ARGB 全屏画面绘制仍约占 102–179 ms/显示帧。当前源码据此拆分原生视频表面，Debug/Release 已重新构建，仍需真机验证；本文列出的旧 GLES 包不代表当前 renderer。

用户提供的 `2734939` UDEB 日志混合了旧会话和末尾的新会话：旧 CPU 会话首帧为 `RGB565`、`repackCopyMs=0`，而末尾 CPU 会话已经出现 `RGB565_DEFERRED` 和非零 `repackCopyMs`。新会话仍只有约 3–4 fps 的 consumer rate，但 `convertMs` 只随实际选中的帧增长，说明延迟转换已经生效。验收必须按会话区分，并按实际取出的帧而非 decoded 总数解释 `convertMs`。

该日志最后以远端调试连接关闭结束，未形成 final summary；因此它只能证明延迟转换 smoke 运行成功，不能替代完整的 30–60 秒性能、同步和稳定性验收。

## 2. 安装包

正式发布决定是一个 `Symbian3Qt474` 应用 SIS 同时覆盖原版 Symbian³、Anna 和 Belle。原版/Anna 若缺少 Qt 4.7.4、Qt Mobility 1.2.x，应另行离线安装运行库，不单独编译应用。完整证据和设备矩阵见 `SYMBIAN3_ANNA_BELLE_COMPATIBILITY_ZH.md`。

2026-08-29 新生成的统一 Debug/Release SIS 位于 `symbian/out/wiliwili-symbian-{debug,release}`，最低 SDK 构建均为 `sbs errors: 0`；但它们只使用 SDK 的 2009–2019 过期自签名证书，不可作为正式分发包。恢复当前有效证书私钥之前，下面的历史包也不得重命名为“统一版”。

| 配置 | 当前证书签名包 | 大小 | SHA-256 |
|---|---|---:|---|
| Release | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_release_full_currentcert.sis` | 9,703,416 bytes | `17CEE045699489B08143793CF3F70A37DF06EEC74BDDAB24CC75E622B60E8F04` |
| Debug | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_debug_full_currentcert.sis` | 9,687,100 bytes | `70ABE6972E52AD533A82A14520B425D6FAF5EB2C344C76AE178C3CE435DC3698` |

对应未签名归档（仅供追溯，不用于手机安装）：

| 配置 | 文件 | 大小 | SHA-256 |
|---|---|---:|---|
| Debug | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_debug_full_unsigned.sis` | 9,686,384 bytes | `8DA5633AEA739F5B787E1FD4BC3E8838C222B467E73B0997C20752223DF8E0D0` |
| Release | `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_release_full_unsigned.sis` | 9,702,704 bytes | `C92F353E0FF830F651497E9C88E8FD93BFE804A1AAD86F11D845648A74F68FF3` |

两个当前证书包均由 `signsis -o` 验证为 Qt Development Frameworks 证书，有效期 2026-08-24 至 2036-08-21。`dumpsis -l` 验证 EXE capabilities 与 SIS 头一致：`NetworkServices ReadUserData`。

## 3. 构建证明

- Qt 4.7.4 `Symbian3Qt474`、GCCE 4.4.1、SBS 2.17.0；目标 ARMv5；Belle SDK 只保留作后备诊断；
- Debug 和 Release 全量构建均为 `sbs errors: 0`；
- Release 链接的 `.text/.rodata/.ARM.exidx` 末端低于 `0x0032B000`，与固定 `.data=0x00400000` 保持充足间隔；
- qmake 生成的 MMP 同时包含 `ffmpeg_h264_decoder.cpp`、`WILIWILI_ENABLE_FFMPEG_SOFT_DECODER`、`WILIWILI_FFMPEG_ARMV6_ASM`、`libavcodec.lib` 和 `libavutil.lib`；
- `symbian/app/wiliwili_symbian_installer.pkg` 版本头为 `0,9,0`，界面版本和 Symbian User-Agent 已同步为 0.9.0；
- Qt 自动生成的旧 2009–2019 签名已从归档包剥离，再使用当前证书重签；
- 旧 `armsoftprobe1`、`headercontrol1`、BCM2727 直连实验没有进入 0.9 主线。
- 2026-08-28 独立 soft RGB565 表面与 500 ms position cache 合并后的普通 Debug/Release 已重新 qmake/GCCE 构建，各为 `sbs errors: 0`（34 条既有 SDK 警告），并分别生成 `symbian/out/wiliwili-symbian-debug` 与 `symbian/out/wiliwili-symbian-release` 下的自签名 SIS；尚未生成本文替代 current-certificate 归档包，也未完成新表面真机验收。
- 2026-08-29 移除 Belle-only `cookiemanager.dll`，以公共 RHTTP header API 收集重复 `Set-Cookie`；随后在 `Symbian3Qt474` 下 Debug/Release 各以 `sbs errors: 0`、32 条既有 SDK 警告完成。`elftran` 导入表无 CookieManager，应用 SIS 仍声明 Qt 4.7.4 与 Qt Mobility 1.2.0；播放器与解码代码未改变。

## 4. 建议测试顺序

1. 先安装 Release 包，确认应用管理器显示 0.9.0；
2. 播放已知正常样本 `BV1oyhM6AETw`：应记录 `DEVVIDEO_HEADER_PREFLIGHT_ACCEPT` / `ROUTE MMF`，并保持 MMF 画面/声音；
3. 播放原有声无画样本 `BV1Uy8x6AETG` 及其他硬件不兼容样本：应记录 `DEVVIDEO_HEADER_PREFLIGHT_REJECT` / `ROUTE FFMPEG`，随后进入 `FFMPEG_SOFT_READY ... RGB565_LUT2X2`；不应出现 Q6/240P 额外请求，并应显示 FFmpeg 画面、MMF AAC、弹幕和清晰横屏 UI；
4. 横屏日志应依次包含 `PLAYER_NATIVE_PORTRAIT_CHROME_READY`、`PLAYER_NATIVE_LANDSCAPE_640X360_READY`、`PLAYER_NATIVE_LANDSCAPE_VISIBLE`；退出应包含 `PLAYER_NATIVE_PORTRAIT_FULLSCREEN_READY`，不得出现 orientation timeout/failed；
5. 连续播放至少 30 秒，观察画面是否上下颠倒/方向错误、色彩与比例、体感帧率、音画同步、暂停/恢复、拖动进度和退出后再次进入；soft 日志应出现 `SOFT_SURFACE_ACTIVE/FIRST_PAINT`，`SOFT_STATS` 中 `softSurfacePresented>0`、`overlayVideoDrawMs=0`、`overlayPositionCacheHits>0`、`overlayRotateMs=0`；
6. Debug 包用于本轮 CODA/Qt Creator 主线定位；qmake 附加参数中删除 `CONFIG+=applandscape1`，也不要加入 `ffmpeglatedrop1`。

## 5. 已知限制与发布判断

- ARM1176 没有 NEON/ARMv6T2，FFmpeg H.264 的 qpel/chroma/CABAC 等主要仍是 generic C；当前软解默认输出是 CPU RGB565 LUT2X2 和常态跳过 loop filter，GLES YUV420 直出已退役；
- 约 11.4–12.0 fps 是旧 RGB565 包数据，不能用来描述本包；密集弹幕或高复杂度码流仍可能落后；
- 源码在 soft playback 中每 500 ms 校准一次 MMF audio `PositionL()`，中间按播放倍速外推并由取帧、弹幕、控件复用；暂停、seek、倍速和会话切换都会失效缓存。若重新构建后仍明显卡顿，应比较 `softSurfacePaintMs`、`overlayPaintMs`、timer gap 和音画误差，不要继续优化 decoder、RGB565 LUT 或重新启用 GLES；
- 若内置 FFmpeg 对个别视频仍不支持或性能不可接受，按 `PLAYER_1.0_DECODING_POLICY_ZH.md` 设计明确的外部播放器回退，不恢复 BCM 硬解边界探索；
- 旧 soft-only native-landscape 归档仍不得重新启用或引用；当前实现是随后经 app-shell 20 轮验证的 panes/work-area/dynamic-fullscreen 状态机，不是该归档的恢复；
- 用户完成上述测试并确认没有回归后，才可将 0.9 标记为公开发布版本。

## 6. 2026-08-29 NIKINIKI 品牌与 GitHub 公开准备

- 正式产品名改为 **NIKINIKI**；保留 `TARGET = wiliwili_symbian`、UID
  `0xE000B100`、QSettings 键和安装路径，以便旧包原位升级并保留登录/设置；
- `DEPLOYMENT.display_name`、应用窗口/播放器标题、关于页、登录设备名与
  User-Agent 已使用 NIKINIKI；qmake 生成的 LOC、SIS header 和 Vendor 均实测为
  `NIKINIKI`；
- 项目提供的 1254×1254 RGBA 原图被等比缩放为 256×256 PNG，用于应用内
  NanoVG 资源；另新增不使用 filter 的 SVG-T 启动器图标。Belle SDK
  `mifconv 3.3.3` 实测转换成功，qmake 将 1,350-byte MIF 写入
  `/resource/apps/wiliwili_symbian.mif`；
- 删除自动触发的上游桌面/主机、Pages 和 WinGet GitHub Actions，避免公开仓库
  push/release 误构建或误发布 wiliwili 包；Issue 表单改为 Symbian/Nokia Belle
  专用；
- `.gitignore` 已排除根目录 SIS 暂存、`symbian.zip`、host `.obj`、qmake MOC 和
 误生成的 `symbian/app/epoc32/`；公开文档中的 CODA 私网端点已删除；
- 变更后 GCCE Debug/Release 均为 `sbs errors: 0`、32 条既有 SDK warning；
  host `mg_json` 测试为 58/58 通过。

本轮本地自签名产物仅用于构建证明，仍不得上传 GitHub Release：

| 配置 | 大小 | SHA-256 |
|---|---:|---|
| Debug | 9,796,316 bytes | `4F2843473E418B1928AFF95F86E4F81FB3890B50350D61901E1771CE338DCB3F` |
| Release | 9,807,400 bytes | `6D4896383126C4941CB69B861B9F1B6D529363C30C16A25ED87849A7922DF122` |

两者由 Qt SDK 的过期 Self Signed 流程生成，只保存在被忽略的 `symbian/out/`。
NIKINIKI 图标/名称仍需真机确认；恢复正确的 2026 证书私钥并完成设备门槛前，
不生成或发布新的 current-certificate SIS。

## 7. 2026-08-29 独立产品仓库边界验证

- 正式工程不再引用根目录 `wiliwili/`、`library/` 或 `resources/`；
- 实际需要的 NanoVG、QR-Code-generator、字体和卡片背景已迁入
  `symbian/third_party` 与 `symbian/resources`，并附固定来源和许可证；
- 完整上游 `api.h` 不再编译，应用只保留带基线/GPL 说明的 12 个端点；开发
  基线中的该上游文件已恢复为零修改；
- `mongoose_compat` 继续作为 clean-room JSON 实现，正式构建没有 Mongoose
  源码或链接依赖；
- NIKINIKI 字体子集重新生成，内部 family 为 `NIKINIKI CJK`，不保留 OFL
  Reserved Font Name `Source`；
- host JSON 测试仍为 58/58；自包含 Debug/Release 各为
  `sbs errors: 0`、32 条既有 SDK warning。

本轮重新生成的过期 Self Signed 构建证明如下，仍不是公开发布包：

| 配置 | 大小 | SHA-256 |
|---|---:|---|
| Debug | 9,793,444 bytes | `E99289B7F152F9CA4CC74833656B094F0164952D714CC2F8F554ADFA9BBBDFBE` |
| Release | 9,804,436 bytes | `551F9AF86012B37E7134AF03988EC0D7DFDF8721DC79B0C1A8BB5E8360DCE4BF` |

NIKINIKI 独立仓库现为产品源码、构建脚本、公开文档和发布 tag 的唯一主线；完整
上游工作区保留在并列的本地研究仓库，只作行为比较、私有证据和后续研究，不再
维护第二份可编辑产品源码。
