# Third-party boundary

Only the dependencies listed here are part of the NIKINIKI source boundary.
The full wiliwili `library/` submodule tree is not required.

| Directory | Origin | License | Distribution form |
|---|---|---|---|
| `nanovg/` | NanoVG snapshot carried by pinned borealis | zlib-style | Required source and license are vendored |
| `qrcodegen/` | Project Nayuki QR Code generator | MIT | Required C source/header and license are vendored |
| `mongoose_compat/` | NIKINIKI clean-room implementation | GPL-3.0 | Project source; not Mongoose code |
| `ppsspp_ffmpeg/` | PPSSPP-FFmpeg / FFmpeg 3.0.2 | LGPL-2.1-or-later | Version, build scripts and license are committed; generated libraries are ignored |

Additions must record an upstream URL, immutable version/commit, applicable
license, local modifications and whether the material is compiled or merely a
build-time tool.
