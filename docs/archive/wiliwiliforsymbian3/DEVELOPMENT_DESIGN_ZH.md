# wiliwili for Symbian³ 开发设计方案

> 归档状态：Historical。本文保留早期移植设想，其中 Bridge、Qt Quick、旧播放器和里程碑内容
> 已被后续实现取代。当前架构见 `docs/developer/ARCHITECTURE_ZH.md`。

> 文档版本：0.2（上游优先复用版）  
> 首要目标：Nokia 808 PureView / Belle FP2  
> 次要目标：N8、C7、E7、C6-01 等 Symbian³/Anna/Belle 设备  
> 本文中的 “Symbian³” 不指 S60 3rd Edition。

> **2026-08-26 状态说明：**本文是早期总体设计，仍用于范围、上游复用和工程原则。实际开发设备已经以 Nokia 603 为主，播放器已演进为 0.7 原生 `CVideoPlayerUtility2`、固定系统竖屏、旋转视频 `RWindow` 和单 ARGB 弹幕层。1.0 前的最新策略是“MMF 硬解 → 本机软解 → 用户确认后调用外部播放器”，不再探索 BCM 硬解边界，也不采用本文早期提出的远端媒体 bridge/remux/transcode。接手前必须先阅读 `docs/README_ZH.md`、`docs/archive/plans/PLAYER_1.0_DECODING_POLICY_ZH.md` 与阶段报告。

## 1. 方案摘要

本项目定位为 **wiliwili 的 Symbian³ 下游移植版**，不是只模仿界面的独立客户端。默认策略是最大化复用上游成果，只在工具链或平台能力确实不兼容时替换实现：

- 从上游 `xfangfang/wiliwili` 的稳定主线建立 fork，并长期保留 `upstream` remote；
- 优先复用 Bilibili API 定义、数据模型、WBI/签名算法、Presenter 业务流程、导航语义、设置项、页面信息架构、图片规格、翻译和许可允许使用的资源；
- 对能通过 Symbian 编译器的代码直接编译，对仅受现代 C++/依赖阻碍的代码做小范围回移植或生成兼容版本；
- 先用最小样例验证 borealis + nanovg + OpenGL ES 2.0 能否在 Symbian 上运行；通过则保留上游 XML UI 和大量 View/Activity，未通过才用 Qt Quick/QML 重建显示层；
- 不强行复用与平台深度绑定的 GLFW/SDL、cpr/curl、mpv/FFmpeg，分别以 Qt/Symbian 输入、QtNetwork、Qt Mobility/MMF 后端替换；
- 兼容服务降级为可选的 TLS 中继、媒体解析/remux 和登录辅助，而不是默认复制一套新的 Bilibili 业务 API；
- 以 Nokia 808 完成首个闭环，再对 N8 级设备做内存和渲染降级。

上游当前采用 C++17，界面使用 borealis/nanovg，网络层使用 cpr 与 nlohmann/json，播放使用 mpv/FFmpeg。因此项目首先设置两个技术门：

1. **全 UI 移植门**：Symbian 工具链能否编译必要的 borealis 子集，nanovg/GLES2 能否稳定显示且内存可接受；
2. **核心代码复用门**：上游 API、模型、算法和 Presenter 中有多少可以直接编译，剩余部分能否以机械化兼容改造保留。

这两个门的结果决定“直接复用”与“源代码级移植”的比例，但不会退回到从零重新设计业务功能。

### 1.1 上游依据

- 上游仓库：<https://github.com/xfangfang/wiliwili>
- 上游稳定主分支：`yoga`；开发分支：`dev`；
- 上游许可证：GPL-3.0；Symbian 移植版如包含其代码，应保持 GPL-3.0 兼容、保留版权与许可证，并提供相应源代码；
- 上游 README 明确说明 UI 基于 nanovg，输入可由 GLFW/SDL 或自定义平台层提供，并欢迎针对支持 OpenGL(ES) 的新设备进行移植讨论；
- 所有复用结论都必须绑定具体上游 commit，不能只写“来自最新版”。

## 2. 范围与版本目标

### 2.1 v0.1（MVP）

- 推荐页与分页加载；
- 搜索与搜索结果；
- 视频详情、UP 主摘要、基础评论；
- 异步封面加载与有界磁盘缓存；
- H.264 + AAC 视频播放、暂停、拖动和全屏；
- 滚动、顶部、底部三类弹幕；
- 网络错误、空数据、播放失败和重试状态；
- 808 真机上的安装包、日志和性能基线。

### 2.2 v0.2

- Bridge 辅助二维码登录与可撤销会话；
- 收藏、历史、点赞、关注；
- 清晰度选择；
- 弹幕透明度、字号、速度、密度和屏蔽配置；
- N8 级设备的低资源模式。

### 2.3 v0.3 候选

- 动态、番剧/影视页、订阅；
- 发评论、发弹幕；
- 下载与离线缓存；
- 直播；
- 更完整的个人中心。

### 2.4 前期明确不做

- 在移植探针尚未通过前，承诺整个现代依赖树可以原样编译；
- AV1、HDR、复杂 ASS、高级定位弹幕、AI 遮挡；
- 所有设备统一支持 1080p；
- S60v3/S60v5 兼容；
- 客户端直接保存 Bilibili 密码；
- 为追求动画效果牺牲播放稳定性或内存安全。

## 3. 总体架构

```text
                      上游 wiliwili/yoga
       API/模型/Presenter/导航/设置/资源/XML/翻译/测试知识
                              │
             ┌────────────────┴────────────────┐
             │ 可直接编译/小改移植            │ 仅作设计源
             ▼                                ▼
┌──────────────────────── Symbian Port ──────────────────────────┐
│ wiliwili-core-symbian                                         │
│ API 常量、DTO、WBI、业务流程、导航语义、配置与事件语义           │
├───────────────────────────────────────────────────────────────┤
│ UI Gate A：borealis/nanovg/XML（探针通过时优先）                │
│ UI Gate B：Qt Quick/QML（Gate A 失败时按上游页面逐页映射）       │
├───────────────────────────────────────────────────────────────┤
│ Symbian Adapters                                              │
│ 输入/窗口 │ QtNetwork Transport │ Qt/MMF Player │ 存储 │ 日志   │
└───────────────┬──────────────────────────────┬────────────────┘
                │ Direct Transport             │ Bridge Transport
                ▼                              ▼
           Bilibili API/CDN       可选兼容服务：TLS 中继、媒体解析、
                                  remux、图片转换、QR 登录辅助
```

### 3.1 复用分级

| 级别 | 上游成果 | 策略 |
|---|---|---|
| A：直接复用 | API URL/参数、纯算法、轻量模型、翻译、部分图片与配置定义 | 尽量保持文件、命名空间和接口不变，只加平台宏 |
| B：兼容移植 | WBI、JSON DTO、Presenter、Intent、事件、图片 URL 规则、弹幕调度 | 替换现代 C++/依赖用法，保留行为和测试样本 |
| C：结构复用 | Activity/Fragment、XML 布局、RecyclingGrid、设置页面 | borealis 探针通过则直接编译；否则一对一映射为 QML |
| D：后端替换 | cpr/curl、GLFW/SDL、mpv/FFmpeg、桌面文件系统 | 保持上层语义，改由 QtNetwork、Qt/Symbian 输入、MMF 实现 |
| E：首版剔除 | Anime4K、复杂 SVG/OpenCC、直播 WebSocket、高级弹幕 | 保留功能开关和接口位置，待资源允许再恢复 |

“复用”包括直接编译和有来源可追踪的源代码级移植。不能为了看起来像 fork 而强行携带在 Symbian 上不可运行的二进制依赖。

### 3.2 移植决策树

```text
Symbian 编译器能编译所需 C++17 子集？
  ├─ 是 → 尽量保持上游源码，只新增 PLATFORM_SYMBIAN
  └─ 否 → 提取 wiliwili-core-symbian，机械回移植到已验证的 C++ 子集

borealis + nanovg + GLES2 最小页面稳定且内存合格？
  ├─ 是 → 沿用 Activity/Fragment/View/XML，新增 Symbian 平台后端
  └─ 否 → Qt Quick/QML 作为显示层，保持 Presenter/Intent/模型语义

QtNetwork 能直连 Bilibili HTTPS？
  ├─ 是 → 复用上游 API/WBI，默认 Direct Transport
  └─ 否 → Bridge Transport 仅做安全 TLS 中继，尽量原样返回上游 JSON

系统播放器能直接播放所选媒体？
  ├─ 是 → Qt Mobility/MMF 直接硬解
  └─ 否 → 服务端选择兼容流或 remux；最后才转码
```

### 3.3 设计边界

1. 先保留上游层次和语义，再决定某个实现是否需要 Symbian 替身。
2. 业务调用统一通过上游风格的 `bilibili::HTTP`/Presenter 边界，底层传输可切换。
3. UI 不直接依赖 cpr、mpv 或 Symbian 原生 API。
4. Qt Mobility 与原生 MMF 实现都藏在播放器适配层后面，并向上游事件语义做映射。
5. 兼容服务优先透明中继和媒体适配，避免重新定义整套业务模型。
6. Symbian 专用代码放在 `symbian/` 和明确的平台宏下，减少与上游合并冲突。
7. 每个复制或改写的上游文件都在复用清单中记录原路径、commit、改动理由和同步状态。

## 4. 客户端模块设计

| 模块 | 主要职责 | 关键约束 |
|---|---|---|
| `wiliwili/include/api` | 上游 API 常量、结果模型、WBI 和解析语义 | 尽量保持原路径；兼容改动必须可追踪 |
| `presenter` / `Intent` | 页面业务流程、分页、导航和异步生命周期 | 优先移植上游实现，不在 QML 中重写业务 |
| `symbian/compat` | 现代 C++、JSON、线程/回调和少量 STL 兼容 | 只提供项目实际用到的最小子集 |
| `symbian/network` | `QNetworkAccessManager` 传输、队列、重试 | 对上模拟上游 HTTP 语义；支持 Direct/Bridge |
| `borealis port` 或 `symbian/qml` | 显示、输入和页面组件 | 由 UI Gate 决定；两条路线不并行长期维护 |
| `symbian/image` | 上游 `ImageHelper` 的 Qt 实现、缩放与缓存 | 保留图片规格规则；LRU；禁止存原始超大图 |
| `symbian/player` | 映射上游 MPV 事件语义的系统播放器后端 | 不尝试在首版完整模拟 libmpv API |
| `danmaku` | 复用上游数据/设置/行为，替换渲染层时保留调度语义 | 活跃对象硬上限；可切换低资源模式 |
| `symbian/storage` | 映射 `ProgramConfig`、会话、历史、缓存索引 | 保持配置 key 兼容，底层可用 QSettings |
| `symbian/platform` | 屏幕方向、触摸/按键、路径、MMF、日志 | 所有 Symbian 头文件限制在此层 |

### 4.1 关键接口草案

播放器接口使用 Qt 4 可接受的 C++ 风格，最终签名以实际编译器验证结果为准。其外侧另设一层事件适配，将状态映射为上游 `MPV_E` 中页面真正依赖的事件，以便复用播放页 Presenter 和控制逻辑。

```cpp
class IPlayerBackend : public QObject {
    Q_OBJECT
public:
    virtual void setSource(const QUrl &url) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(qint64 positionMs) = 0;
    virtual qint64 positionMs() const = 0;
    virtual qint64 durationMs() const = 0;

signals:
    void stateChanged(int state);
    void positionChanged(qint64 positionMs);
    void bufferingChanged(int percent);
    void errorOccurred(int code, const QString &message);
};
```

播放器状态统一为：

```text
Idle → Resolving → Preparing → Buffering → Playing ↔ Paused
                                 ↓              ↓
                               Error ←──────────┘
```

所有后端错误先映射为稳定的应用错误码，再由 UI 决定提示和重试方式。

## 5. 页面与交互设计

### 5.1 导航结构

导航命名和页面边界沿用上游 `Intent`、Activity、Fragment 结构；下图是 MVP 对上游功能的裁剪，不是另建一套导航模型。

```text
启动/恢复
  └─ 首页
      ├─ 推荐/热门/分区
      ├─ 搜索 → 搜索结果
      ├─ 视频详情 → UP 主页 / 评论
      │              └─ 播放器（全屏）
      └─ 我的（v0.2）→ 历史 / 收藏 / 设置
```

### 5.2 布局原则

- 浏览页面采用现代卡片式设计，深色主题优先；
- 360×640 竖屏默认两列紧凑卡片或单列详情，640×360 横屏使用两到三列；
- 播放器默认横屏全屏，退出后恢复原方向；
- 图片统一请求与卡片接近的缩略图尺寸，避免先下载大图再显示；
- 动画仅保留 120–180 ms 的淡入/位移等低成本过渡，并提供关闭选项；
- 焦点、触摸、实体返回键均通过统一导航服务处理；
- 每个异步页面都必须有加载、空结果、失败、离线缓存四种状态。

### 5.3 UI 性能规则

- borealis 路线优先复用上游 `RecyclingGrid`、XML 资源和 View；QML 路线为其建立一对一组件映射；
- 列表必须使用 `RecyclingGrid` 或可回收 QML delegate，不把整个分页结果一次性实例化；
- 页面离开后释放不再使用的模型和大图；
- 禁止模糊、实时阴影、大面积透明叠层和常驻循环动画；
- QML 路线避免在 delegate 内创建定时器、网络对象或复杂 JavaScript；
- 首版同屏卡片数量和预取距离由真机数据决定，不写死为桌面值。

## 6. 上游 API 复用与可选兼容服务

### 6.1 API 复用原则

客户端业务代码继续使用上游的 API 常量、结果模型、WBI 签名和 Presenter 调用方式。移植层提供与 `bilibili::HTTP` 行为等价的传输接口：

```text
上游 Presenter / API / Result Model
                 │
         Symbian HTTP Facade
          ├─ DirectTransport → QtNetwork → Bilibili
          └─ BridgeTransport → QtNetwork → Compatibility Bridge → Bilibili
```

这样 Direct 与 Bridge 模式共享同一套 Bilibili 业务代码。兼容服务优先原样返回 Bilibili JSON，使上游结果结构和解析逻辑仍然可用；只有上游数据过大、格式设备无法处理或接口彻底失效时，才定义少量规范化端点。

移植时优先保留以下上游位置的语义：

- `wiliwili/include/api/bilibili/api.h`：API 地址和路由定义；
- `wiliwili/include/api/bilibili/result/`：结果模型；
- `wiliwili/include/api/bilibili/util/wbi.hpp`：WBI 行为；
- `wiliwili/include/api/bilibili/util/http.hpp`：异步请求、错误和解析入口；
- `wiliwili/include/presenter/`：页面请求、分页和操作流程。

若 nlohmann/json 或现代模板无法通过 Symbian 编译，先为 MVP 所需 Result Model 编写兼容解析器，并用上游 JSON fixture 做逐字段一致性测试；不要另起一套与上游无关的领域模型。

### 6.2 兼容服务只承担平台缺口

- **TLS Bridge**：在 Symbian TLS 无法连接上游时安全转发白名单路由；
- **Media Resolve**：使用上游同等播放参数解析资源，并返回设备可播放的 H.264/AAC 方案；
- **Remux/Transcode**：优先直链，其次 remux，确实无法播放才转码；
- **Image Adapt**：将 WebP/AVIF 转为按尺寸裁剪的 JPEG/PNG；
- **QR Auth Assist**：在服务端完成现代登录握手，向设备发放短期可撤销会话；
- **Emergency Normalize**：仅为个别无法在设备端解析的超大或不稳定接口提供精简响应。

### 6.3 Bridge 最小端点

| 方法与路径 | 用途 | 与上游复用的关系 |
|---|---|---|
| `GET /v1/config` | Bridge 能力、版本、时间和媒体 profile | 不涉及 Bilibili 业务模型 |
| `POST /v1/relay/{routeId}` | 转发白名单 API | 尽量原样返回上游 JSON，由上游模型解析 |
| `GET /v1/media/{bvid}?cid=&profile=` | 解析/合并兼容媒体 | 复用上游清晰度、分P和鉴权参数语义 |
| `GET /v1/image?sourceId=&size=` | 转换和缩放图片 | 对应上游 `ImageHelper` 的尺寸策略 |
| `POST /v1/auth/qr/start` | 开始二维码登录 | v0.2 |
| `GET /v1/auth/qr/{id}` | 轮询二维码状态 | v0.2 |

`routeId` 必须是服务端白名单中的逻辑路由，不接受任意 URL，防止 Bridge 变成开放代理。媒体响应可在上游结果结构外增加设备字段，例如 `container`、`videoCodec`、`audioCodec`、`expiresAt` 与 `seekable`。

客户端通过 `profile` 声明能力，例如 `symbian_808_low`、`symbian_n8_low`。具体 H.264 Profile/Level、码率和分辨率必须由已知样片在真机上验证后固化，不能仅凭规格表决定。

### 6.4 安全约束

- 不在明文 HTTP 上发送密码、Cookie、会话令牌或个人操作；
- Direct 与 Bridge 模式都复用同一会话抽象，但令牌存储方式必须适配 Symbian；
- 若目标机 TLS 无法可靠连接公网服务，公开浏览可使用受控 Bridge，账户功能必须使用已验证的安全链路；
- 客户端不保存 Bilibili 密码，二维码登录产生的令牌必须可撤销；
- 日志默认打码令牌、Cookie、查询隐私和完整媒体签名 URL；
- Bridge 需要鉴权、限流、响应大小限制、路由白名单和 SSRF 防护。

## 7. 视频播放设计

不重写上游播放页业务。先统计 Activity/View/Presenter 实际调用的 `MPVCore` 方法和 `MPV_E` 事件，形成精简 `PlayerFacade`；Symbian 后端实现该外观，继续复用清晰度、分P、进度、弹幕开关和播放页控制逻辑。只有 libmpv/FFmpeg 本体因工具链、体积或硬解不可行时才替换后端。

### 7.1 后端选择顺序

1. 对上游 `MPVCore` 做调用面审计，并以最小构建探针确认 libmpv 路线是否存在现实可行性；
2. 并行用 Qt Mobility `QMediaPlayer` 播放本地已知 H.264/AAC MP4，验证系统硬解；
3. 验证 HTTP 渐进式播放、拖动、暂停恢复、耳机和来电中断；
4. 若 Qt 封装不稳定，实现基于 Symbian MMF/`CVideoPlayerUtility` 的后端；
5. 选择能稳定调用硬解且维护成本最低的后端，映射到 `PlayerFacade`；
6. 仅在系统后端都不可用时评估定制解复用或软件解码，且不作为 MVP 默认路线。

### 7.2 播放器与弹幕时钟

- 播放器位置是唯一主时钟，禁止用 QML 动画累计时间推算播放进度；
- 正常播放时每 100–250 ms 采样位置，精度由真机 CPU 占用决定；
- 拖动、暂停、缓冲、前后台切换后清空活动弹幕并按新位置重建；
- 若位置发生明显跳变，立即重新定位事件游标；
- 兼容服务返回资源过期时间，播放失败且资源已过期时只重新解析一次。

## 8. 弹幕引擎设计

弹幕首先从上游 `wiliwili/include/view/danmaku_core.hpp` 及相关实现做文件级审计，复用其数据字段、设置含义、过滤逻辑、时间同步行为和渲染参数。若 borealis/nanovg 路线通过，优先裁剪上游渲染器并降低容量；若采用 QML，则只替换 Overlay Renderer，轨道/时间/设置行为以差分测试保持与上游一致。

### 8.1 数据流

```text
上游弹幕结果/设置 → Symbian 紧凑事件数组 → 时间游标 → 轨道分配器
                                          ↓
                                    活跃对象池
                                          ↓
                                单层 Overlay Renderer
```

事件字段以固定上游 commit 的模型为准；Symbian 内部可映射为 `timeMs`、`mode`、`text`、`color`、`fontScale` 等紧凑字段。进入内存后保持按时间排序，并只保留当前位置附近的时间窗。

### 8.2 轨道与碰撞

- 滚动弹幕只有在前一条不会被追上时才能进入同一轨道；
- 顶部和底部弹幕按显示结束时间占用轨道；
- 无可用轨道时按配置丢弃，绝不动态增加无限轨道；
- 文本过长时截断或缩放至设定下限，避免超大纹理；
- 高密度场景优先保留时间更早的事件，可选按用户屏蔽规则过滤。

### 8.3 初始档位（待真机校准）

| 档位 | 同屏上限 | 目标设备 | 效果 |
|---|---:|---|---|
| 标准 | 32 | Nokia 808 | 滚动 + 顶部 + 底部，简单描边 |
| 节能 | 18 | N8 级设备 | 减少轨道与刷新，禁用额外效果 |
| 最低 | 10 | 内存或帧率告警后 | 只保留滚动和固定弹幕基本可读性 |

这些值只是压测起点。最终档位由“播放不掉帧、音画同步稳定、内存无持续增长”决定。

## 9. 网络、缓存与存储

### 9.1 网络策略

- 全应用共享一个 `QNetworkAccessManager`；
- 元数据请求初始并发上限 3，图片请求初始并发上限 2；
- 列表快速滚动时取消已离屏且未开始的图片请求；
- 连接超时、总超时和最大响应体大小均设硬限制；
- 仅对 GET 做有限指数退避，播放解析和写操作不盲目重放；
- 服务端支持 gzip 时先用真实设备评估 CPU/流量收益。

### 9.2 缓存分层

```text
内存缩略图 LRU：当前页和临近页
        ↓ 淘汰
磁盘 JPEG 缓存：按 URL + 尺寸 + 版本散列
        ↓ 过期
网络/兼容服务
```

初始软预算如下，均需通过工具链和真机测量调整：

| 项目 | 808 档 | N8 档 |
|---|---:|---:|
| 客户端非播放器内存目标 | ≤ 48 MiB | ≤ 32 MiB |
| 缩略图内存缓存 | 8 MiB | 4 MiB |
| 磁盘图片缓存 | 48 MiB | 24 MiB |
| 单次 JSON 正文 | ≤ 512 KiB | ≤ 256 KiB |
| 弹幕时间窗 | 前 30 s / 后 120 s | 前 15 s / 后 60 s |

内存目标不包含系统播放器内部缓冲，因为其成本需实测；应用必须记录总进程内存峰值，并在压力告警后主动清理非关键缓存。

### 9.3 本地数据

- `ProgramConfig` 的 key、默认值和迁移语义沿用上游，Symbian 存储适配器可用 `QSettings` 承载主题、弹幕、服务地址和功能开关；
- 小型 JSON/二进制索引：图片缓存元数据；
- QtSql/SQLite：只有在历史、收藏离线索引确有需要且模块可用性已验证后加入；
- 令牌：使用平台可提供的最安全存储；无法安全存储时允许仅保存在内存并在退出后失效。

## 10. 工程结构

```text
wiliwili/                         # xfangfang/wiliwili 的 fork 根目录
├─ CMakeLists.txt                 # 尽量保留上游桌面/掌机构建
├─ wiliwili/include/              # 上游 API/Presenter/View 等
├─ wiliwili/source/
├─ resources/                     # 上游 XML、i18n、字体和图片
├─ library/                       # 上游 submodules；Symbian 只选择性构建
├─ symbian/
│  ├─ wiliwili_symbian.pro        # qmake/Symbian 入口
│  ├─ include/
│  │  ├─ compat/                  # 已验证的最小 C++/JSON 兼容层
│  │  ├─ network/
│  │  ├─ player/
│  │  └─ platform/
│  ├─ source/
│  ├─ qml/                        # 仅在 borealis Gate 失败时启用
│  ├─ generated/                  # 可从上游生成的 API/DTO 兼容代码
│  ├─ probes/                     # borealis、GLES2、QtNetwork、MMF 探针
│  ├─ packaging/                  # UID、能力、签名、SIS/SISX
│  └─ reuse-manifest.yml          # 每个上游文件的复用与同步状态
├─ bridge/                        # 可选 TLS/媒体/图片/登录兼容服务
│  ├─ src/
│  ├─ tests/
│  └─ openapi/
├─ docs/
│  ├─ DEVELOPMENT_DESIGN_ZH.md
│  ├─ UPSTREAM_BASELINE.md
│  ├─ PORTING_AUDIT.md
│  ├─ TOOLCHAIN_REPORT.md
│  ├─ DEVICE_TEST_MATRIX.md
│  └─ ADR/
└─ tools/sync_upstream/            # API/模型同步检查与可选代码生成
```

开发仓库应直接是上游 fork，而不是把 wiliwili 当作无版本信息的复制目录。`origin` 指向 Symbian fork，`upstream` 指向官方仓库；Symbian 改动集中在独立目录和平台条件中。上游原文件必须修改时，优先提交小而单一的兼容补丁。

`reuse-manifest.yml` 至少记录：上游路径、基线 commit、复用级别、Symbian 对应文件、补丁原因、最后同步 commit、适用测试。MVP 阶段可手工维护；重复同步成本明显后再引入生成脚本。

## 11. 构建与兼容基线

Milestone 0 必须产生一份 `TOOLCHAIN_REPORT.md`，记录实际测得的：

- fork 所绑定的上游 `yoga` commit、submodule commit 和可成功构建的上游桌面基线；
- Symbian SDK、目标平台和设备固件版本；
- Qt、Qt Mobility、qmake、编译器及链接器版本；
- 可用 C++ 语言特性，尤其是异常、RTTI、模板和 STL 限制；
- QtNetwork、QtDeclarative、QtMultimedia/Qt Mobility、QtSql、OpenGL ES 模块；
- Debug/Release 构建命令；
- UID、能力声明、证书、签名、SIS/SISX 打包与安装流程；
- Windows 11 上 IDE、SDK 路径、环境变量、签名工具和驱动问题；
- 最小程序在真机的启动时间、空闲内存和退出行为。

同时产生 `PORTING_AUDIT.md`：扫描 MVP 涉及的上游文件，统计 C++17 特性、cpr/nlohmann/borealis/mpv 耦合点，并给每个文件分配 A–E 复用级别。在报告完成前，不选择 JSON 兼容方案、不承诺具体 C++ 标准，也不批量改写上游代码。

## 12. 开发里程碑与验收

| 阶段 | 交付物 | 退出条件 |
|---|---|---|
| M0 工具链 | 上游桌面基线、最小 Qt 应用、SIS/SISX、工具链报告 | 绑定上游 commit；808 可安装、启动、响应触摸和返回键 |
| M0.5 移植门 | 复用审计、C++ 核心编译探针、borealis/nanovg/GLES2 页面 | 确定每类代码的 A–E 级和唯一 UI 路线，有数据支持而非主观放弃 |
| M1 UI 验证 | 复用上游 XML/View 或一对一 QML 页面、内存日志 | 呈现上游首页假数据；滚动 5 分钟无持续内存增长 |
| M2 网络验证 | 上游 API/模型 + Symbian HTTP Facade、图片队列、缓存 | Direct 或 Bridge 至少一条可用；上游 fixture 解析一致 |
| M3 浏览闭环 | 推荐、搜索、详情、用户摘要、评论 | 真机完成浏览路径，分页和返回栈稳定 |
| M4 播放验证 | 播放器抽象、系统后端、已知样片 | 连续播放 30 分钟，暂停/拖动/前后台切换可恢复 |
| M5 在线播放 | 复用上游播放解析语义、媒体 Bridge、资源过期恢复 | 至少三类公开视频可稳定播放，不依赖软件解码 |
| M6 弹幕 | 复用上游数据/设置语义，适配渲染、对象池和降级 | 播放 30 分钟音画同步稳定，活动对象数始终有界 |
| M7 账户 | QR 登录、历史、收藏、点赞/关注 | 不传输密码，令牌可撤销，失败操作不会重复提交 |

### 12.1 MVP 性能验收建议

- 冷启动到可操作首页：808 目标 ≤ 5 s，最终以实测基线调整；
- 列表连续滚动 5 分钟不崩溃，返回首页后内存回落到稳定区间；
- 已知兼容媒体连续播放 30 分钟，不发生可感知音画漂移；
- 标准弹幕档开启时播放器仍稳定，弹幕对象数不超过配置硬上限；
- 重复进入/退出播放器 20 次无句柄、网络回复或纹理持续泄漏；
- 断网、服务端 5xx、资源过期、图片损坏均有可恢复 UI。

## 13. 测试策略

### 13.1 自动测试

- 对能直接复用的上游测试保持原测试逻辑，对回移植代码增加桌面上游版与 Symbian 兼容版的差分测试；
- 用同一组上游 JSON fixture 测试两套 DTO 解析结果，覆盖字段缺失、类型错误和超大响应；
- 测试 WBI、BV/AV、分页参数、错误映射、缓存淘汰和弹幕轨道行为与固定上游 commit 一致；
- Bridge 使用契约测试保证透明转发不会擅自改变 Bilibili JSON，媒体扩展字段保持兼容；
- 播放接口用短小、许可证明确的本地 H.264/AAC 样片回归；
- 弹幕用可重复的时钟和密集事件 fixture 测试 seek、pause、跳时钟。

### 13.2 真机矩阵

| 优先级 | 设备/系统 | 重点 |
|---|---|---|
| P0 | Nokia 808 / Belle FP2 | 全功能、标准档、安装签名、长时间播放 |
| P1 | Nokia N8 / Belle 或 Symbian³ | 低资源档、内存压力、解码兼容 |
| P1 | C7/E7/C6-01 中至少一台 | 屏幕方向、按键、GPU/固件差异 |
| P2 | Anna 设备 | Qt/Qt Mobility 版本差异和安装依赖 |

模拟器只验证逻辑和布局，播放器、内存、GPU、触摸和网络恢复的结论必须来自真机。

## 14. 可观测性与故障诊断

- 日志分为 `APP`、`NET`、`API`、`IMAGE`、`PLAYER`、`DANMAKU`、`MEMORY`；
- Release 默认记录环形日志，容量建议 256–512 KiB；
- 每次播放记录媒体参数、后端、缓冲事件、seek、错误码和内存水位；
- 隐藏设置页支持导出脱敏诊断包；
- 开发构建显示 FPS、活动 QML delegate、图片缓存、弹幕对象和网络请求数；
- 不记录 Cookie、访问令牌、完整签名 URL 或用户私密内容。

## 15. 主要风险与降级方案

| 风险 | 早期验证 | 降级/应对 |
|---|---|---|
| Windows 11 工具链不兼容 | M0 最小工程和打包 | 固定可复现 VM/容器外旧系统工具链，并记录镜像依赖 |
| 上游 C++17 无法由 Symbian 编译器接受 | M0.5 文件级编译审计 | 只回移植 MVP 核心；用 fixture/差分测试保持行为一致 |
| borealis/nanovg 端口成本或内存过高 | M0.5 GLES2 最小页面 | 保留上游 Presenter/结构/资源，显示层一对一映射 QML |
| Symbian TLS 无法连接现代服务 | M2 握手矩阵 | 兼容网关；账户功能只允许安全链路 |
| Qt Mobility 网络视频不稳定 | M4 已知样片 | 切换 MMF 后端；代理提供渐进 MP4 或本地临时文件 |
| DASH/编码不兼容 | M5 多类视频样本 | 服务端直链选择 → remux → 最后转码 |
| QML 内存或纹理泄漏 | M1 压测和重复导航 | 缩小 delegate、主动卸载页面、减少内存图片缓存 |
| 弹幕影响解码 | M6 密集 fixture | 降低上限/采样频率、关闭描边、最低档 |
| 上游 wiliwili 持续演进导致 fork 漂移 | 每次同步的 manifest 与差分测试 | Symbian 代码外置、小补丁、固定周期挑选上游提交 |
| Bilibili 接口频繁变化 | 上游 API fixture + Bridge 契约测试 | 优先同步上游修复；Bridge 只补旧设备无法承担的部分 |
| 登录风控和隐私 | Bridge 辅助 QR 流程 | 延后账户功能；公开浏览不受影响 |
| 第三方子模块或资源许可不同 | 引入前逐项审查 | 项目保持 GPL-3.0 兼容；保留 LICENSE/NOTICE 和来源清单 |

## 16. 首轮开发任务拆分

### Sprint 0：工具链与设备基线

1. fork 官方仓库，固定 `yoga` 基线 commit 和全部 submodule commit；
2. 在桌面构建未修改的上游版本，保留行为参考和 fixture；
3. 盘点 SDK、Qt、Qt Mobility、编译器、签名工具和真机固件；
4. 构建一个只含窗口、文本、触摸和返回键的 Qt 应用并生成 SIS/SISX；
5. 分别验证 QtNetwork、QML、Multimedia/MMF、OpenGL ES 的最小样例；
6. 扫描 MVP 上游文件的 C++17 和依赖使用，创建 `reuse-manifest.yml`；
7. 提交 `UPSTREAM_BASELINE.md`、`PORTING_AUDIT.md`、`TOOLCHAIN_REPORT.md` 和构建脚本。

### Sprint 1：最大复用移植门

1. 尝试编译上游纯算法、API 常量和一个 Result Model，量化直接复用比例；
2. 将 borealis 裁剪到一个窗口、文字、图片、列表和触摸，移植 nanovg GLES2 最小页面；
3. 若 borealis 探针达标，确定 XML/View 路线；否则冻结 QML 映射路线，不长期维护两套 UI；
4. 用同一首页 fixture 比较上游桌面版与 Symbian 探针的模型和显示行为；
5. 用三组 H.264/AAC MP4 探测硬件播放边界，完成 `IPlayerBackend` 和上游事件适配；
6. 输出首版 `DEVICE_TEST_MATRIX.md`，并携带结果准备与上游维护者讨论 Symbian 端口。

### Sprint 2：上游 API 最小闭环

1. 移植上游推荐、详情和播放所需 API/DTO/WBI 子集；
2. 用 QtNetwork 实现上游 HTTP Facade 的超时、取消、Cookie 和错误语义；
3. 先测试 Direct Transport，再为失败的 TLS/媒体路径实现最小 Bridge；
4. 对相同 JSON 输入运行桌面上游版与 Symbian 兼容版差分测试；
5. 接入真实推荐和详情，并用兼容媒体完成“浏览 → 播放”演示；
6. 记录每个上游文件的直接复用、修改行和后续同步策略。

## 17. 关键决策与待验证项

已确定：

- 项目作为 wiliwili 的 GPL-3.0 兼容下游 fork，以上游优先复用为基本原则；
- 先实测 borealis/nanovg 移植，只有不达标才使用 Qt Quick/QML 显示层；
- 系统多媒体是播放主线，但对上保持上游播放页需要的事件和配置语义；
- 直接连接是首选，兼容服务只补 TLS、媒体和登录等旧平台缺口；
- 首发设备是 Nokia 808/Belle FP2；
- 登录晚于浏览、播放和弹幕闭环。

必须通过实验决定：

- 实际 Qt/编译器版本和可用 C++ 子集；
- MVP 上游源码可直接编译、轻改移植和仅能结构复用的实际比例；
- borealis/nanovg/GLES2 是否满足输入、内存和列表性能要求；
- Qt Mobility 与 MMF 哪个作为首选播放器后端；
- 各设备稳定支持的 H.264 Profile/Level、码率、容器和 seek 方式；
- 图片解码是否需要独立工作线程及其线程安全边界；
- 808/N8 的真实进程内存上限和缓存预算；
- 上游弹幕渲染能否随 borealis 复用；若采用 QML，Text 对象池是否足够还是需要自绘层；
- 公网安全连接的可行方案和账户功能部署模型。

## 18. 完成定义

MVP 完成不是“页面基本齐全”，而是在 Nokia 808 真机上稳定完成以下路径：

1. 安装并启动应用；
2. 浏览推荐或完成搜索；
3. 打开详情并读取评论；
4. 通过 Direct 或 Bridge 路径播放视频；
5. 暂停、拖动和恢复播放；
6. 观看与播放器位置同步的三类弹幕；
7. 在断网、返回、前后台切换和多次进出播放器后仍可继续使用；
8. 所有缓存、请求、UI 对象和弹幕对象均保持在明确上限内；
9. `reuse-manifest.yml` 能说明 MVP 每个核心模块复用了哪些上游成果、为何修改及如何同步，GPL-3.0 源码与署名要求完整落实。

达到该闭环后，再扩展登录、个人功能和更多内容分区。功能扩展优先从上游挑选已有实现进行移植，并用同一 fixture/行为测试验证，而不是在 Symbian 分支重新设计一份同名功能。
