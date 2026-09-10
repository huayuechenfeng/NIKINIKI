# ADR-0008：允许 E7 应用内 Qt 视频的受控诊断

> 状态：Superseded by ADR-0009（历史研究准入仍有效）
> 决定日期：2026-09-04
> 基线：`v1.2.0` / `fc9a0ea`

## 背景

[设备矩阵](../reference/DEVICE_TEST_MATRIX.md#e7-investigation-20260904) 的成功参照与失败集成
支持继续比较窗口与媒体生命周期；不能再把 QGL 与 Qt 视频绝对互斥当作前提。
这些历史观察来自用户纪要，逐包身份、当前设备环境和原始时间线仍需本轮补齐。

独立 Player 已在产品 Git 的 `codex/archive-independent-player-20260904` / `b9e1a8d`
封存；该提交中的 ADR-0007 记录暂停产品化。此处不恢复其双进程实现，也不导入该分支对
正式播放器架构的替换。独立方案的设备反馈缺少明确机型与逐样本后端，不能当作 E7 硬解通过。

## 决定

- 允许在独立诊断 CONFIG 或探针中研究同进程 `QMediaPlayer + QVideoWidget`，由正式基线
  新建 `codex/` 研究分支，只提取已审核的最小依赖。
- 这限定性重新开放的是 Qt 视频诊断；“QMediaPlayer + QVideoWidget 不作正式后端”的
  现有边界保持有效。正式化须另作决定，并验收旧功能、设备兼容和返回生命周期。
- 冻结成功参照，按真实调用顺序建立可解释的对照；无声、未启动的实验先归入启动问题。
- 保留正式 MMF/FFmpeg、真实 header preflight、单向回退和完整播放器对象图持久复用。
  不用退出时重建窗口树解决返回问题。
- 保留 AVKON panes 与主 QGL 映射；横屏视频顶层仍须等待有效 `workAreaResized()` 和物理
  640×360，返回仍在物理 360×640 后恢复主 fullscreen。固定竖屏参照不冒充横屏验收。
- 独立进程、Direct DevVideo/DSA 产品显示、GLES 三平面输出和 ref7 扩展不因本决定重新开启。

## 验收与维护

执行顺序由[路线图](../ROADMAP_ZH.md)维护，实验方法由
[E7 诊断方案](../reference/E7_BLACK_SCREEN_DIAGNOSTIC_PLAN_ZH.md)维护。
源码静态检查、GCCE、SIS 打包、CODA 运行和菜单独立启动分别报告。
诊断出画不等于普通构建通过，也不改变当前初代设备不支持公告。
