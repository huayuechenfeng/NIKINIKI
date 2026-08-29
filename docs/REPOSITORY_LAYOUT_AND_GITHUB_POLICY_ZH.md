# NIKINIKI 仓库整理与 GitHub 发布边界

> 更新日期：2026-08-29  
> 适用基线：NIKINIKI 0.9.0 Symbian³ 真机验证候选

## 单一产品主线与并列研究仓库

- **NIKINIKI 公开仓库**：产品代码、资源、构建脚本、公开文档、提交和发布 tag
  的唯一事实来源。所有正式修改及外部贡献都先进入这里。
- **本地研究仓库**：与 NIKINIKI 并列保存完整 wiliwili v1.6.0、submodule、失败
  实验、设备日志、构建产物和签名材料。它只提供对照与证据，不复制维护第二份
  可编辑产品主线，也不直接产生公开 Release。

公开仓库不携带完整上游工作树，但必须保留 `LICENSE`、`NOTICE.md`、
`docs/UPSTREAM_BASELINE.md`、`docs/CODE_BOUNDARY_ANALYSIS_ZH.md` 和
`symbian/reuse-manifest.yml`。

## 公开仓库允许清单

| 路径 | 内容 |
|---|---|
| `README.md`、`LICENSE`、`NOTICE.md` | 产品说明、GPLv3 和来源/第三方声明 |
| `AGENTS.md` | 产品主线边界、当前事实、冻结规则与后续代理接手约束 |
| `.github/` | NIKINIKI Issue 模板和 Actions 策略说明；无活动构建 workflow |
| `docs/` | 当前状态、决策、设备证据和历史发行说明 |
| `symbian/app` | Qt 工程、入口、图标和资源清单 |
| `symbian/source`、`symbian/include`、`symbian/generated` | 正式应用源码 |
| `symbian/env`、`symbian/tools`、`symbian/Build-*.ps1` | 可复现构建与诊断工具 |
| `symbian/probes` | 仍可由公开树独立构建的能力探针 |
| `symbian/resources` | 字体、图片和资源许可证 |
| `symbian/third_party` | 固定的必要源码、许可证及 FFmpeg 重建脚本 |
| `tools/` | 主机工具、字体子集与公开仓库边界检查脚本 |

## 必须排除

- 根 `wiliwili/`、`library/`、`resources/` 以及 CMake/xmake/桌面主机构建系统；
- `symbian/archive/` 失败实验源码快照；结论只通过 `docs/` 发布；
- `symbian/out/`、根 `wiliwili_symbian/`、`*_currentcert/` 和 `.tmp/`；
- `*.sis`、`*.sisx`、`*.exe`、`*.dll`、`*.lib`、`*.a`、`*.o`、`*.obj`；
- qmake/SBS/MOC/RCC/RSS/PKG 自动生成物和 SDK 镜像；
- 证书、私钥、Cookie、账号数据、CODA 地址、设备转储及未经脱敏日志；
- AI 附件、个人工作记录和本地编辑器配置；产品级 `AGENTS.md` 明确保留。

## 公开仓库验收门

每次准备提交或发布都必须通过：

1. 正式 `.pro`/QRC/源码不存在指向根上游树的相对路径；
2. Debug 和 Release 直接从公开仓库 checkout 的同一份 `symbian/` 成功构建；
3. JSON 兼容测试全部通过；
4. `tools/Test-PublicRepository.ps1` 通过，且仓库没有危险扩展名、私钥标记、
   用户绝对路径和局域网端点；
5. 所有第三方目录均有许可证和固定来源；
6. 构建产物、测试记录和 Release 说明指向同一个公开 commit/tag。

## GitHub Release

源码仓库公开与 SIS 二进制发布是两个独立门槛。在 Nokia 603 完成最终验收、取得
正确签名材料并准备 FFmpeg LGPL 静态重链接包后，GitHub Release 才可附加单一
Release SIS、SHA-256、版本说明和重链接材料。Debug、unsigned、旧诊断候选和
中间包不发布。
