# 初代 Symbian³ 实验性视频兼容模式封存

> 状态：Closed / Historical evidence
> 封存日期：2026-09-07
> 本页不描述当前产品能力；当前实现以 `docs/developer/PLAYBACK_ARCHITECTURE_ZH.md` 为准。

## 封存结论

该候选曾在完整应用设置中提供一个默认关闭、持久保存的开关。会话开始时锁定开关；开启后只把
`CVideoPlayerUtility2` 的首次 `AddDisplayWindowL` 与显示配置从 `OpenComplete` 后移到可接受的
`PrepareComplete` 内、`Play` 前。关闭时保持原时序。实现没有改变 header preflight、MMF/FFmpeg
选择、`-12017` 轨道判断、横屏门槛或对象复用。

E7 完整应用首次实测确认开关和延后绑定均实际执行，但仍为黑屏。该次输入是 8,937,888 字节的
完整下载文件，不是成功最小 C 使用的样本 A；因此结果既不证明延后绑定无效，也不构成稳定修复。
原始设备事实保留在[设备矩阵](../../reference/DEVICE_TEST_MATRIX.md#e7-full-app-compat-first-20260907)，
解释边界保留在[重新评估](E7_BLACK_SCREEN_REASSESSMENT_ZH.md#完整应用兼容候选首次反馈评估)。

## 已移出产品的代码契约

以下设计只作历史记录，不再存在于普通应用设置、播放器会话或 backend 公共接口：

```text
settings key: player/experimental_video_compatibility (default false)
session start: sessionCompatibility = savedPreference
OpenComplete:
  false -> bind + configure -> Prepare
  true  -> mark bind pending -> Prepare
PrepareComplete(0 or -12017), before track policy/Play:
  if pending -> bind + configure
```

已移除的产品接口包括设置页开关、`VideoPlayerWidget` 的兼容偏好/会话字段，以及
`VideoPlaybackBackend::setExperimentalVideoCompatibilityMode()`。研究 CONFIG
`WILIWILI_ENABLE_E7_DELAYED_DISPLAY_BIND_DIAGNOSTIC` 下的最小 C 时机分支属于既有冻结研究输入，
不由普通 MMP 定义，也不重新编译或继续消耗设备预算。

## 未完成实验

曾准备 `e7fullappsamplea1` 完整应用本地样本 A 入口，用于跳过网络下载、强制 MMF 并与冻结最小 C
做同样本四次对照。该 CONFIG 只完成主机 Debug/Release 编译与 SIS 打包，未安装、未启动、无设备
结论；其入口、构建变体和本地产物均已从主线移除。原定四次对照取消，不顺延、不追加启动。

重新开启本方向必须先有新的明确授权和单变量研究问题；不能把本页代码契约恢复为用户功能，
也不能把最小 C 的历史出画或本次构建成功写成 E7/N8 支持。
