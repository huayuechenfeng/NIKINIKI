# NIKINIKI 总体架构

> 状态：Active
> 适用版本：1.0.0 至 1.2.0 当前主线
> 本页职责：描述现有模块和依赖方向，不记录移植设想或实验历史

## 系统边界

NIKINIKI 是面向 Symbian³、Anna 和 Nokia Belle 的 Qt 4.7.4/GCCE 应用。产品源码完全位于
`symbian/`，不依赖并列研究仓库运行或构建。上游 wiliwili 只提供可追踪的 API、交互和设计参考。

```text
AVKON / Qt application
        │
        ├─ WiliwiliWidget：NanoVG/GLES2 主 UI、导航和页面状态
        │
        ├─ NativeTransport：Symbian RHTTP、Cookie、TLS 和请求调度
        │
        ├─ Bilibili parsers/WBI：接口参数、签名和轻量解析
        │
        ├─ LoginSession：登录态和本地持久化
        │
        └─ VideoPlayerWidget
                ├─ MMF hardware playback
                ├─ MP4/AVC Range probe and demux
                ├─ local FFmpeg H.264 fallback
                ├─ native RGB565 video surface
                └─ ARGB danmaku/control overlay
```

## 主要模块

| 模块 | 代码入口 | 职责 |
|---|---|---|
| 应用与导航 | `symbian/source/app/wiliwili_widget.cpp` | 主页面、导航历史、网络任务和播放器会话 |
| 主 UI | `symbian/source/ui/` | NanoVG 页面、卡片、设置和交互 |
| 网络 | `symbian/source/network/native_transport.cpp` | RHTTP 请求、Cookie、超时和错误映射 |
| Bilibili 协议 | `symbian/source/network/bilibili_*` | WBI、JSON 解析、登录和媒体选源 |
| 会话 | `symbian/source/model/login_session.cpp` | 登录状态和持久化 |
| 播放器控制 | `symbian/source/ui/video_player_widget.cpp` | 横屏状态机、输入、弹幕、控制和后端协调 |
| MMF 后端 | `symbian/source/platform/video_playback_backend.cpp` | `CVideoPlayerUtility2` 生命周期和媒体状态 |
| MP4/AVC 探测 | `symbian/source/platform/mp4_avc_probe_reader.cpp` | Range、sample table、SPS/PPS、DTS/PTS 和 AU |
| 软件解码 | `symbian/source/platform/ffmpeg_h264_decoder.cpp` | H.264 工作线程、帧队列和 RGB565 输出 |
| 工程入口 | `symbian/app/wiliwili_symbian.pro` | 版本、能力、源码、依赖和 SIS 资源 |

## 依赖方向

```text
UI / application
      ↓
stable NIKINIKI model and service interfaces
      ↓
network / storage / media platform adapters
      ↓
Qt 4.7.4 + Symbian native APIs + pinned third-party code
```

- 页面代码不直接管理 MMF、RHTTP 或 DevVideo 原生对象；
- 平台实现不反向依赖具体页面；
- 第三方代码固定在 `symbian/third_party/`，来源和许可证必须完整；
- `symbian/generated/` 只保存产品构建实际需要、可追踪来源的生成内容；
- 根上游工作树和并列研究仓库不属于构建依赖。

## UI 与窗口

主页是持续映射的 `QGLWidget`，用 NanoVG/GLES2 绘制。播放器是独立、持久的顶层窗口，
由 AVKON 方向状态机在物理 640×360 工作区稳定后显示。视频表面和 ARGB UI overlay 与主页
不组成普通 QWidget 父子嵌套，避免 Qt/WSERV/QGL 生命周期冲突。

首页搜索的浏览态由 NanoVG 绘制输入区和右侧低对比描边“搜索”按钮；编辑态仍只使用一个原生
`QLineEdit` 顶层，其内部嵌入真实 `QPushButton` 提交搜索，从而保留 Belle 输入法焦点链且不增加
第二个原生顶层窗口。

首页拖动事件使用合并刷新，推荐网格只提交当前裁剪区相交的卡片。视频详情使用同一
`view/detail` 响应解析正文与 `Related`，相关推荐在正文下方纵向排列。动态列表按需顺序加载首张
图片或视频封面，CDN 只限定宽度而不裁剪；解码后尺寸写回条目，列表和详情按正文行数与图片
纵横比计算完整卡片高度，滑动期间复用高度缓存。非视频动态进入独立详情后请求
`/x/polymer/web-dynamic/desktop/v1/detail` 补齐图文、文字或专栏正文，再用详情返回或列表保留的
comment id/type 请求评论。列表只显示真实首图或加载占位，不在真实图片上叠加伪造的多图色块。

播放器的详细对象图、选路和状态机见
[PLAYBACK_ARCHITECTURE_ZH.md](PLAYBACK_ARCHITECTURE_ZH.md)。

## 网络与数据

- 使用 Symbian RHTTP 直接访问 Bilibili API/CDN；
- WBI 签名和必要的接口语义来自固定上游基线；
- Cookie 与登录态由应用本地管理，不写入日志或外部程序参数；
- 图片、弹幕、API 和媒体请求各自有有界超时与错误映射；
- 不使用桥接、远端媒体处理或第二套业务服务。

## 构建与发布

- 正式目标：`Symbian3Qt474`、ARMv5、GCCE 4.4.1；
- Belle `SymbianSR1Qt474` 只作显式设备诊断后备；
- 本机软件解码依赖由固定 PPSSPP-FFmpeg 源码使用兼容 GCCE 重建；
- 发布 SIS、许可证、签名和重链接材料必须来自同一 commit/tag；
- 构建方法见 `symbian/README.md`，公开边界见
  [REPOSITORY_POLICY_ZH.md](REPOSITORY_POLICY_ZH.md)。

## 设计依据

当前架构的关键决定分别记录在：

- [原生横屏状态机](../decisions/0001-native-landscape.md)；
- [完整播放器对象图持久复用](../decisions/0002-persistent-player-lifecycle.md)；
- [真实 header preflight 选路](../decisions/0003-header-preflight-routing.md)；
- [手机本机软件回退](../decisions/0004-on-device-software-fallback.md)。

早期总体设计已经封存，不再通过在旧正文顶部叠加勘误的方式维护当前架构。
