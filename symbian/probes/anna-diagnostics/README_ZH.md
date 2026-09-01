# NIKINIKI Anna 启动分层自检

这四个程序使用独立 UID，不覆盖 NIKINIKI，也不联网、不登录、不播放媒体。
请只安装交付目录中以 `01_` 到 `04_` 开头的已签名 SIS，不要安装 SDK 自动生成的
`*_installer.sis`。

## 用户操作顺序

每个程序安装后单独运行，等待至少 5 秒：

1. `01_NIKI_A0_BASE.sis`：基础 QtCore / QtGui 与固件、内存信息；
2. `02_NIKI_A1_GL.sis`：QtOpenGL、GLES2 和实际 GL context；
3. `03_NIKI_A2_MEDIA.sis`：Qt Mobility Multimedia 和 `QMediaPlayer` 服务；
4. `04_NIKI_A3_FONT.sis`：按 1.0.0 的方式一次读取 8.36 MB 完整字体。

出现结果页后拍下完整屏幕。若某一步出现以下情况，也要记录步骤编号：

- 安装器直接拒绝或提示缺少组件：拍安装器提示；
- 点击图标完全无反应：记录“Ax 图标无反应”；
- 先显示 `LOADER PASS - WAIT`，约两秒后程序消失：记录消失前的步骤编号；
- 结果页为黄色边框或包含 `FAIL`：拍下完整结果。

## 判读

| 结果 | 初步结论 |
|---|---|
| A0 无法安装或启动 | Qt 4.7.4 的 QtCore/QtGui 未安装、损坏，或 AppArc/安装本身异常 |
| A0 通过、A1 无法启动或两秒后退出 | QtOpenGL、GLES2/EGL 或 Anna 图形驱动路径 |
| A0/A1 通过、A2 无法安装或启动 | Qt Mobility 1.2.x / QtMultimediaKit 缺失或损坏 |
| A2 显示 `API PASS / SERVICE FAIL` | DLL 已装载，但多媒体 service/plugin 不可用 |
| A0 通过、A3 两秒后退出 | 完整字体分配/读取触发低内存或存储异常 |
| A3 显示读取字节不足或耗时异常 | 安装盘/字体文件/冷存储读取问题 |
| 四项全部通过而正式 1.0.0 仍不启动 | 转查正式 EXE 的原生 MMF/DevVideo imports、NanoVG 字体注册或完整应用构造路径 |

内置任务切换器不是完整进程查看器，因此请以程序是否出现结果页、是否在两秒阶段消失为准。

测试完成后可以正常卸载四个自检程序。
