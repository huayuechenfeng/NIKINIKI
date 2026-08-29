# wiliwili for Symbian³ 0.6.8 测试版

## 安装包

Debug 完整包（真机日志测试优先）：

`symbian/out/releases/v0.6.8/wiliwili_symbian_0.6.8_debug_full_currentcert.sis`

- 大小：9,149,640 字节
- SHA-256：`4DE02B8BCDE70600697E85CF37CC3ED85A6A8C15A061BF9F94A8D58579329233`

Release 完整包：

`symbian/out/releases/v0.6.8/wiliwili_symbian_0.6.8_release_full_currentcert.sis`

- 大小：9,152,732 字节
- SHA-256：`AB8F1D3BCCCEC7464D48CF4441864C7ADDE379C84A50831ADAAD1D76CEC66AAA`

两个配置均由 Belle Qt 4.7.4/GCCE 构建，`sbs errors: 0`。SIS 包头
为 0.6.8，应用能力为 `NetworkServices ReadUserData`，签名证书有效期为
2026-08-24 至 2036-08-21。

## 本版修改

### 横屏与播放器前台时序

- 横屏退到桌面/CODA 的基础问题早于弹幕实现，不把它误归因于弹幕。
- 原时序是先激活第二个顶层播放器窗口，再调用 AVKON `SetOrientationL`；
  真机日志显示旋转成功后应用随即 `APPLICATION_DEACTIVATE`。
- 新时序改为隐藏播放器、恢复原始应用窗口组、完成旋转并再次请求前台，
  等待 620 毫秒稳定期后才显示播放器及绑定 MMF 原生视频表面。
- 退出播放器采用逆序：先隐藏 MMF/播放器并恢复主窗口，再切回竖屏。
- 画质切换时方向未变化，不重复旋转系统界面。

### 弹幕稳定性

- 0.6.7 使用最多 12 个透明顶层窗口把文字覆盖到 MMF 原生视频层；横屏日志
  随后出现多次 `WSERV 3`。这是横屏基础问题之外的附加风险。
- 0.6.8 不再创建这些顶层窗口，改在视频上方预留 28 像素的应用内弹幕带，
  先保证横屏与播放稳定。受 Belle 原生视频表面合成层级限制，本版不会把
  多行弹幕直接覆盖在画面中央。

### 搜索与首页

- 搜索入口独占首页最上方一整行，推荐/热门/番剧/直播标签移到下一行。
- 搜索输入框不再作为 `QGLWidget` 的 raster 子控件，而是短时全宽工具窗口，
  避免日志中连续的 `QPainter::begin: Paint device returned engine == 0`，并使
  输入内容始终位于屏幕上方、不被软键盘遮挡。

### 缩略图加载

- 网络完成队列由每 1000 毫秒轮询改为每 250 毫秒轮询，内存采样仍保持每秒
  一次；上一张图片完成后可以更快发起下一张。
- 首页和内容列表请求 224×126 裁剪图，单图上限降为 512 KiB，超时降为
  12 秒，减少慢线路长时间阻塞整个串行图片队列。

## 建议真机测试

1. 安装 Debug 完整包，在应用管理器确认版本为 0.6.8。
2. 从手机图标冷启动，确认首页搜索框独占一行；点击后连续输入中文或英文，
   日志不应再刷 `QPainter::begin`。
3. 播放一个横屏视频。预期日志顺序为
   `PLAYER_ORIENTATION_REQUEST true true` → `PLAYER_ORIENTATION true 0` →
   `PLAYER_NATIVE_READY`；期间不应退到 CODA/桌面。
4. 播放 30 秒、显示/隐藏控制栏、切换一次画质、退出播放器；主 UI 应完整恢复，
   日志不应出现 `Category: WSERV; Reason: 3`。
5. 再播放一个竖屏视频，预期第二个布尔值为 `false`，不触发横屏切换。
6. 对比首页图片出现速度；若个别图片失败，记录对应 `Thumbnail failed`，不要
   只记录最终卡片数量。

