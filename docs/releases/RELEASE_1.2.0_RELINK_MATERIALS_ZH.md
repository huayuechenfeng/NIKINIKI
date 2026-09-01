# NIKINIKI 1.2.0 LGPL 重链接材料

GitHub Release `v1.2.0` 的 `NIKINIKI_1.2.0_relink_materials.zip` 让接收者可以取得构建 1.2.0
时静态链接的 FFmpeg 源码、完整 NIKINIKI 产品源码和未签名安装包，进行修改、重建和自行签名。
材料包不包含任何证书、私钥、账号资料或 SDK 构建目录。

## 内容

- `NIKINIKI_1.2.0_release_unsigned.sis`：与正式 SIS 对应的未签名包；大小和 SHA-256 见材料包内
  `SHA256SUMS.txt`；
- `NIKINIKI-source-v1.2.0.zip`：与 `v1.2.0` tag 相同的产品源码快照；
- `ppsspp-ffmpeg-b87f7c6d522d1edba77cfc4fac96ce48a236f806-source.zip`：
  `hrydgard/ppsspp-ffmpeg` 的固定源码快照（FFmpeg 3.0.2）；
- 产品 GPL、NOTICE、FFmpeg LGPL 2.1 文本、本说明和 SHA-256 清单。

## 重建步骤

1. 解压产品源码快照；将 FFmpeg 源码快照解压到产品源码根目录的
   `.tmp/ppsspp-ffmpeg-research/`。
2. 在 Symbian `Qt 4.7.4 for Symbian Anna` / `Symbian3Qt474` 环境中运行：

   ```powershell
   .\symbian\third_party\ppsspp_ffmpeg\Build-Gcce-H264.ps1 -EnableArmAssembly
   .\symbian\Build-App.ps1 -Configuration release
   ```

3. 使用自己的合法签名材料对生成 SIS 签名；不得使用原发布者的证书或私钥。

构建脚本会拒绝不是 `b87f7c6d522d1edba77cfc4fac96ce48a236f806` 的 FFmpeg checkout，以保证
与此 Release 对应。应用本身以 GPL-3.0 发布；FFmpeg 依 LGPL 2.1 or later 使用，完整许可文本
随材料包与安装包提供。
