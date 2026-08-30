# 会话改动报告（2026-08-28）

> 归档状态：Historical worklog。本文只记录当次会话，不是当前架构或任务清单。

> 记录本会话对 `symbian/` Symbian³ 移植（NIKINIKI）的全部代码改动。  
> 纯咨询产出（NIKINIKI 发布方案、mongoose 许可证分析、mongoose 使用边界审计）只形成结论，不在此列为代码改动，相关结论见第 9 节。

## 1. 四项功能修复

### 1.1 播放时屏幕常亮

播放会话打开期间定时重置 Symbian 系统空闲计时，阻止屏保/背光超时。

- `symbian/include/ui/video_player_widget.h`：新增 `m_screenAwakeTimerId` 与 `setScreenAwakeEnabled(bool)`；
- `symbian/source/ui/video_player_widget.cpp`：
  - 构造初始化、析构清理定时器；
  - `openSource()` 会话激活时启动 5 秒定时器（MMF/直播/软解统一入口）；
  - `closePlayer()` 关闭会话时停止；
  - `timerEvent()` 分支在 `Q_OS_SYMBIAN` 下调用 `User::ResetInactivityTime()`（无需额外 capability，self-signed 可用），条件为 `!m_closing && m_sessionActive && isVisible()`；
  - 日志标记 `WW:SCREEN_AWAKE_ON/OFF` 便于真机验证。

仅新增屏幕常亮开关，未触碰任何解码/渲染/MMF 逻辑。

### 1.2 设置页“清理缓存”

- `symbian/include/ui/section_screen.h`、`symbian/source/ui/section_screen.cpp`：
  - 新增 `ClearCacheAction` 与 `m_cacheHitBox`，设置页增加 `CACHE` 卡片；
  - 设置页从固定布局改为可滚动列表（7 张卡片，底部退出按钮固定）；
- `symbian/source/app/wiliwili_widget.cpp`：新增 `clearAppCache()`，删除临时目录 `wiliwili_player_cache.mp4`（FFmpeg 软解本地回退缓存），在 NETWORK 卡显示“缓存已清理（x KB）/ 无缓存文件”；保留登录态与设置项。

### 1.3 导航历史栈修复（核心）

**根因**：原实现用两个标量 `m_detailReturnScreen` / `m_contentReturnScreen` 记录返回目标，嵌套导航（主界面→A详情→UP主页→A详情→UP主页…）会覆盖旧值，返回时在“详情↔UP主页”间循环，永远回不到主界面。

**方案**：替换为真正的历史栈。

- `symbian/include/app/wiliwili_widget.h`：
  - 删除 `m_detailReturnScreen` / `m_contentReturnScreen`；
  - 新增嵌套结构 `NavigationEntry`（`screen` / `restoreHome` / `contentMode` / `contentRefetch`）与 `QVector<NavigationEntry> m_navigationHistory`；
  - 新增 `navigateBack()` / `pushNavigation()` / `popNavigation()`，删除 `returnFromDetail()` / `returnFromContent()`；
  - `submitSearch()` 增加 `pushHistory` 参数（搜索页内“改搜/切用户”原地刷新不压栈）。
- `symbian/source/app/wiliwili_widget.cpp`：
  - 所有前向导航统一 `push`：主页→详情、详情→UP主页/评论、内容→详情、搜索/收藏/关注/私信→列表、评论→回复、收藏夹→文件夹等；
  - 返回统一 `pop` 消费历史；评论回复/收藏夹返回时按原逻辑重新拉取上级列表；
  - 栈带 32 条防御上限。

**行为验证轨迹**：主界面→A详情→UP主页→A详情→返回→UP主页→返回→A详情→返回→主界面（每次返回恰好消费一条历史，不再循环）。

### 1.4 设置页“关于”

- `symbian/include/ui/section_screen.h`、`symbian/source/ui/section_screen.cpp`：
  - 新增 `AboutAction` / `AboutBackAction`、`setAboutVisible()` / `aboutVisible()`、`drawAboutPage()` 与关于页返回命中区；
  - 设置页新增 `ABOUT` 卡片，点击进入关于页：NIKINIKI 版本、基于 wiliwili 移植重构、群号 977410275、B站关注 南国飯店（含空间链接）、谢谢喵；
  - 触屏返回按钮与返回键均可退出；切换底部导航时自动复位。
- `symbian/app/wiliwili_symbian.pro`：`DEFINES += WILIWILI_SYMBIAN_VERSION_STR=\"$$VERSION\"`，版本号与 `.pro` 的 `VERSION = 0.9.0` 同步。

## 2. 代码边界文档

- 新增 `docs/reference/CODE_BOUNDARY_ANALYSIS_ZH.md`：自研代码与上游 wiliwili 代码的边界（目录归属、git 证据、跨边界复用清单、依赖方向、许可证、维护约定）；
- `docs/README_ZH.md`：索引表登记该文档。

## 3. mongoose（GPL-2.0-only）替换

### 3.1 背景

原 Symbian 构建静态编译 `library/mongoose/src/{base64,json,str}.c`（GPL-2.0-only，双许可）。GPL-2.0-only 与项目整体 GPL-3.0 组合存在许可证兼容争议，因此以自研兼容层替换，彻底移除对 mongoose 的编译/链接依赖。

### 3.2 新增兼容层

- `symbian/third_party/mongoose_compat/mg_json.h`：公开面仅 `struct mg_str` + 7 个 API（`mg_str_n`、`mg_json_get_tok`、`mg_json_next`、`mg_json_get_num`、`mg_json_get_bool`、`mg_json_get_str`）；
- `symbian/third_party/mongoose_compat/mg_json.c`：从零实现的只读 JSON scanner（约 712 行，无第三方依赖，除 `mg_json_get_str` 外零动态分配）；
- `symbian/third_party/mongoose_compat/tests/mg_json_test.c` + `Run-Tests.ps1`：独立宿主测试。

**实现要点**：路径解析支持 `$`、`$.a.b`、`$.array[0]`、`$.a.b[3].c`（含 `[N]` 段）；token 返回原始 span（字符串含引号）；`mg_json_next` 保持 0 结束迭代协议；`mg_json_get_str` 用 `calloc` 分配、标准转义 + `\uXXXX`（含代理对，UTF-8）反转义，调用方 `std::free` 释放；数字/布尔严格校验。**未实现** mongoose 的 base64、b64/hex/long、strcmp/match/span 等其他 API。

### 3.3 接线（Phase 3）

- 7 个 parser 仅改 include 路径一行：`../../../library/mongoose/src/json.h` → `../../third_party/mongoose_compat/mg_json.h`（content / playback / section / home / login / detail / wbi），函数体零改动；
- `symbian/app/wiliwili_symbian.pro`：删除 `MONGOOSE_ROOT` 与 3 个 mongoose 源文件行，新增 `$$SYMBIAN_ROOT/third_party/mongoose_compat/mg_json.c`；
- `.gitignore`：忽略兼容层测试目录的 `*.exe` / `*.obj`。

### 3.4 验证（Phase 4）

- 宿主测试（MSVC 2019）：11 组用例、58 项断言全部通过；
- 全仓库搜索：`symbian/source|include|app|probes` 已无 `library/mongoose`、`mg_base64`、`mg_json_get_b64` 等引用；唯一残留为 `symbian/archive/` 历史快照（不参与构建）；
- 测试中发现并修复实现 bug：转义分支重复 `i++` 导致转义符后丢字节；非法数字测试用例改为独立文档（与原 mongoose 的整文档解析失败行为一致）。

## 4. 编译期修复（两轮 Qt Creator 构建日志）

### 4.1 `NavigationEntry` 声明顺序（wiliwili_widget.h:74）

成员声明 `pushNavigation(const NavigationEntry &)` / `NavigationEntry popNavigation()` 出现在嵌套结构体定义之前，GCCE 4.4 拒绝。修复：在声明前加 `struct NavigationEntry;` 前置声明（标准 C++03 模式，MSVC /W4 验证通过）。

### 4.2 `QVector::takeLast()` 不存在（wiliwili_widget.cpp:2351）

Qt 4.7 的 `QVector` 无 `takeLast()`（Qt 5 才有）。修复：`last()` + `remove(size()-1)`。

## 5. 完整文件清单

| 文件 | 类型 | 改动 |
|---|---|---|
| `symbian/source/ui/video_player_widget.cpp` | 修改 | 屏幕常亮定时器（启动/停止/触发/清理） |
| `symbian/include/ui/video_player_widget.h` | 修改 | `m_screenAwakeTimerId`、`setScreenAwakeEnabled` |
| `symbian/source/ui/section_screen.cpp` | 修改 | CACHE/ABOUT 卡片、设置页滚动、关于页渲染、命中区 |
| `symbian/include/ui/section_screen.h` | 修改 | 新 Action、about 状态、命中区成员 |
| `symbian/source/app/wiliwili_widget.cpp` | 修改 | 导航栈接线、`clearAppCache`、设置页处理、`QVector` 修复 |
| `symbian/include/app/wiliwili_widget.h` | 修改 | `NavigationEntry` + 历史栈、前置声明、方法调整 |
| `symbian/app/wiliwili_symbian.pro` | 修改 | 版本宏；移除 mongoose 源、加入 `mg_json.c` |
| `symbian/source/network/bilibili_{content,playback,section,home,login,detail}_parser.cpp`、`bilibili_wbi.cpp` | 修改（各 1 行） | include 路径指向兼容层 |
| `symbian/third_party/mongoose_compat/mg_json.h` | 新增 | 兼容层头文件 |
| `symbian/third_party/mongoose_compat/mg_json.c` | 新增 | 兼容层实现 |
| `symbian/third_party/mongoose_compat/tests/mg_json_test.c` | 新增 | 独立测试 |
| `symbian/third_party/mongoose_compat/tests/Run-Tests.ps1` | 新增 | 宿主测试脚本 |
| `docs/reference/CODE_BOUNDARY_ANALYSIS_ZH.md` | 新增 | 代码边界分析 |
| `docs/README_ZH.md` | 修改 | 索引登记 |
| `docs/SESSION_CHANGES_2026-08-28_ZH.md` | 新增 | 本文档 |
| `.gitignore` | 修改 | 兼容层测试产物忽略 |

## 6. 验证状态

- ✅ 兼容层宿主测试 58/58 通过（MSVC 2019）；
- ✅ 导航声明模式 C++03 合法性验证（MSVC /W4）；
- ⏳ Symbian GCCE 构建：最后状态为 `wiliwili_widget.cpp` 的 `takeLast` 错误（已修复），等待用户重新编译；
- ⏳ Nokia 603 真机验证：未执行。

## 7. 遗留事项

1. 用户重新编译 Debug/Release（确认 `mg_json.c` 进入编译列表、三个 mongoose 源不再出现）；
2. 真机回归所有走 7 个 parser 的页面（首页/搜索/详情/播放地址/评论/动态/消息/历史/收藏/关注/QR 登录），重点看特殊字符标题与空字段；
3. `docs/reference/CODE_BOUNDARY_ANALYSIS_ZH.md` 第 5 节仍把 mongoose 列为编译进 SIS 的组件，待编译验证后更新为“已由 `mongoose_compat` 替换”；
4. `symbian/reuse-manifest.yml` 中的早期映射路径与当前目录结构不一致，后续整理时同步（详见边界文档第 9 节）。

## 8. 构建/测试命令备忘

```powershell
# 兼容层宿主测试（不依赖 Symbian 工具链）
powershell -ExecutionPolicy Bypass -File symbian/third_party/mongoose_compat/tests/Run-Tests.ps1

# Symbian 构建（Qt Creator 或命令行）
# 进入 symbian/app，qmake 后 make debug-gcce / release-gcce
```

## 9. 纯咨询产出（未改代码）

- **NIKINIKI 发布方案**：仓库整体按 GPL-3.0 发布；推荐“瘦身独立仓库”方案（保留 api.h 或重写常量、替换 mongoose、素材自有化），并给出发布前合规清单；
- **mongoose 许可证分析**：仓库内 mongoose 授权为 GPL-2.0-only（无 or-later），与 GPL-3.0 静态组合存在版本兼容冲突，故实施上述替换；
- **mongoose 使用边界审计**：确认编译进 SIS 的 3 个源文件、实际使用的 7 个 symbol（含每符号调用次数与语义）、最小兼容 API、7 个 parser 可零逻辑改动、现有 fixture（仅 `home_fixture.cpp`）无法覆盖替换行为，需补差分/黄金样本测试。
