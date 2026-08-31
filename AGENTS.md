# AGENTS.md — NIKINIKI 产品主线指南

NIKINIKI 是本仓库唯一可编辑的产品主线。产品代码、资源、构建脚本、公开文档、
贡献和发布标签只在这里维护。

完整上游 wiliwili、私有设备日志、失败实验、构建输出和签名材料位于并列研究仓库。
研究仓库只提供只读证据，不能从那里维护或导出第二份产品源码。

## 接手入口

开始产品工作前只需依次阅读：

1. `docs/STATUS_ZH.md`：当前实现、已验证能力和待验证边界；
2. `docs/ROADMAP_ZH.md`：当前执行顺序和验收门槛；
3. 与任务相关的 `docs/developer/` 架构文档或 `docs/reference/` 参考文档。

历史阶段报告、会话记录和失败实验不再充当当前事实来源。完整文档分层与维护规则见
`docs/developer/DOCUMENTATION_GUIDE_ZH.md`。

## 产品边界

- Symbian 应用位于 `symbian/`；
- 正式统一包使用 Qt 4.7.4、GCCE 4.4.1 和最低 `Symbian3Qt474` 目标；
- 内部 `wiliwili_symbian` target、UID 和部分路径为升级兼容与来源追踪而保留；
- 一个 ARMv5 应用 SIS 覆盖 Symbian³、Anna 和 Belle，不按系统拆包；
- 不引入桥接、远端重封装或远端转码，媒体兼容必须在手机本机完成；
- 公开仓库与 Release 的允许内容、排除项和验收门见
  `docs/developer/REPOSITORY_POLICY_ZH.md`。

## 播放器不变量

- Progressive MP4 先进行只读 Broadcom H.264 header preflight；接受时走 MMF，明确拒绝时走本机 FFmpeg；preflight 本身不初始化或显示 DevVideo；
- 软件视频由裁剪的 H.264-only PPSSPP-FFmpeg 解码，AAC 和主时钟继续使用 MMF；
- 软件帧采用 CPU YUV420P→RGB565 LUT2X2，并由独立不透明原生表面显示；唯一透明 ARGB 顶层只绘制弹幕和控制；
- player controller、native video host、backend/observer、MMF utility、soft surface 和 overlay 都跨播放会话持久复用；退出播放只停止、解绑和隐藏；
- 原生横屏必须保留 AVKON panes 和主 QGL 映射，只能在 `workAreaResized()` 确认物理 640×360 后显示播放器顶层窗口；返回时先恢复物理 360×640，再动态恢复主 fullscreen；
- MMF、软件帧、弹幕、控制和输入统一使用直接 640×360 坐标，不恢复整帧 90° 旋转；
- `KErrMMPartialPlayback (-12017)` 可恢复，但必须根据实际音视频轨决定是否进入软件回退；
- 每个媒体会话只允许单向回退，不能在 CDN、清晰度、MMF 和软件后端之间循环。

当前实现细节与代码入口统一见 `docs/developer/PLAYBACK_ARCHITECTURE_ZH.md`。

## 已封存路线

除非当前路线图明确重新开启研究，不要恢复或推广以下路径：

- 系统 ARM H.264 decoder：header 接受但 `ConfigureDecoderL()` 返回 `KErrNotSupported`；
- Broadcom Direct DevVideo/DSA 正式显示：全屏 ARGB UI 下 DrawingRegion 为零，且 decoder 报告不支持 direct display；
- GLES 三平面 YUV 正式输出：Nokia 603 真机上传和提交成本过高；
- 旧虚拟横屏、soft-only native-landscape、`AA_S60DontConstructApplicationPanes` 或过早隐藏主 QGL；
- `QMediaPlayer + QVideoWidget` 正式后端、大量透明顶层窗口、只持久化 overlay；
- Q6/240P 能力阶梯、SPS/DPB 伪装、全局 `-O2/-O3` FFmpeg 构建；
- 诊断用 `ffmpeglatedrop1`、`headercontrol1`、`codeccompat1`、`armsoftprobe1` 作为普通或发布构建基线。

这些结论的原始证据保存在 `docs/research/`，不是活文档。

## 工作与验证规则

- 修改当前行为时同步更新唯一对应的活文档；不要把同一结论复制到多个文件；
- 不在活文档末尾追加按日期排列的会话流水账；旧结论移入 `docs/research/` 或 `docs/archive/`；
- 保留用户已有的未提交修改，不把研究仓库内容复制进产品树；
- 普通构建不要加入诊断 CONFIG；不要恢复全库 `-O2`/`-O3`；
- 构建结论必须区分静态检查、GCCE 编译、SIS 打包和真机验证；
- Qt Creator/CODA 运行结果不能替代手机独立启动的 Release 验证；
- 不提交证书、私钥、Cookie、签名媒体 URL、私网 CODA 端点、设备转储或未脱敏日志；
- 发布事实只记录在 `docs/releases/`，真机结果只记录在
  `docs/reference/DEVICE_TEST_MATRIX.md`。

## 当前发布

1.1.0 已于 2026-08-31 正式发布。安装包、大小、SHA-256、签名和重链接材料的唯一记录是
`docs/releases/RELEASE_1.1.0_ZH.md`；不要在计划、状态或研究文档中维护第二份校验值。

旧版完整指南已封存在
`docs/archive/wiliwiliforsymbian3/AGENTS_LEGACY_2026-08-29.md`，只用于追溯。
