# wiliwili for Symbian³ 0.6.4 测试版

## 安装包

Debug 完整包（本轮优先使用）：

`symbian/out/releases/v0.6.4/wiliwili_symbian_0.6.4_debug_full_currentcert.sis`

- 大小：9,140,492 字节
- SHA-256：`601F7473C3985BE15BA040014A36F3EDE0847B8310E33A249D4AB0091BC6F32B`

Release 完整包：

`symbian/out/releases/v0.6.4/wiliwili_symbian_0.6.4_release_full_currentcert.sis`

- 大小：9,144,316 字节
- SHA-256：`696C709CA87379B7ED27509EA3A8050F89DAC32865D588E7C440F04603053204`

两个配置均由 Belle Qt 4.7.4/GCCE 构建，`sbs errors: 0`。SIS 包头
为 0.6.4，包含 EXE、AppArc 资源、完整中文字体及许可证；签名证书
有效期为 2026-08-24 至 2036-08-21。

## 本版修改

### 扫码登录

- 按 wiliwili 上游 `get_login_info_v2` 重写登录会话：每次轮询生成当次
  UUID，成功后将该 UUID 保存为 `_uuid`，再生成新 `buvid3`。
- 认证凭据只来自成功轮询响应的 `Set-Cookie`，不再从 `data.url`、
  JSON 兼容字段或第三方跨域线路拼接会话。
- 修复多行 `Set-Cookie` 中换行未结束前一 Cookie 值的错误；0.6.3 中
  1,836 字节的异常 Cookie 头就是因为一个值吞入了后续头部。
- 登录后首先请求上游同款 `/x/space/myinfo`，`/nav` 只作备用诊断。

### 评论区

- 根评论优先使用上游 `/x/v2/reply/main`，支持 `mode=3` 最热和
  `mode=2` 最新；右上角可直接切换。
- 如主接口返回 `-352`，自动改用传统接口，排序和新 UI 保持不变。
- 不下载、不绘制评论头像。每条根评论分层显示作者、时间/IP 属地、
  正文、点赞数和回复数。
- 点击根评论后优先请求上游 `/x/v2/reply/detail`，以“层主评论 +
  楼中楼回复”展示；同样保留 `-352` 兼容回退。
- 已登录时排序按钮旁保留“写评论”入口。

### 播放器和弹幕

- 播放器不再调用 `SetOrientationL`。日志证明强制横屏才是 CODA 启动时
  任务失去前台的直接触发点。
- 播放页使用普通无边框顶层 QWidget，不再作为 QGLWidget 子控件；后者
  在 Symbian Qt 4.7 上没有有效栅格绘制引擎，会造成大量 `QPainter::begin`
  错误。
- 视频页显示并创建原生窗口后才绑定 MMF `QVideoWidget`。
- 原生 HTTP 请求显式使用 `Accept-Encoding: identity`，避免 Qt 4.7 将压缩的
  `list.so` 响应直接当 XML 解析。

## 建议真机测试顺序

1. 先完整安装 Debug SIS，并在应用管理器中确认版本为 0.6.4。
2. 打开评论，切换“切最新/切最热”，检查赞数和回复数，点击一条
   有回复的根评论进入楼中楼。
3. 刷新二维码后重新扫码。日志应依次出现三条 `WW:QR_COOKIE_CAPTURE`、
   `WW:LOGIN_COOKIE_SUMMARY true true true`和 `WW:PROFILE_READY`。Cookie 总长度应
   显著小于 0.6.3 的 1,836 字节。
4. 从 Qt Creator 打开之前测试过的同一视频。`WW:PLAYER_PAGE_READY` 最后
   应为 `true`，然后出现 `WW:PLAYER_NATIVE_READY`；期间不应退回 CODA，也不应
   再有连续 `QPainter::begin` 警告。
5. 进入画面后点一次视频区显示/隐藏控制栏，再返回详情页。

日志中不要粘贴 Cookie 值或完整鉴权 URL；`WW:QR_COOKIE_CAPTURE` 只输出
名称和长度，可以安全保留。
