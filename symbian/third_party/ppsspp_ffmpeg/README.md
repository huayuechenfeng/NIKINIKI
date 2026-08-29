# PPSSPP-FFmpeg H.264-only GCCE build

This directory contains the Symbian software-decoder dependency used by the
0.9 mainline local H.264 fallback. The current source baseline is
`hrydgard/ppsspp-ffmpeg` commit `b87f7c6d522d1edba77cfc4fac96ce48a236f806`
(FFmpeg 3.0.2). It is used under LGPL 2.1 or later; GPL components are not
enabled.

The locally generated archives are built by GCCE 4.4.1 for ARM1176JZF-S,
ARMv6K, softfp with VFPv2 at `-Os`. A whole-library `-O2` build makes the
final executable's `.rodata` cross GCCE's fixed `0x400000` data boundary;
`-O3` also makes this old compiler ICE in `libavutil/tea.c`. The
`ffmpegsoft2` archives retain ARM/ARMv5TE/ARMv6
assembly and inline helpers while explicitly disabling NEON, which the Nokia
603 CPU does not provide. Source inspection after the Nokia 603 performance
run established an important limitation: in this FFmpeg revision the H.264
qpel, chroma, intra-prediction, weighted-prediction, deblock and IDCT ARM fast
paths are NEON-only, while the CABAC inline path requires ARMv6T2. On the
ARM1176, the H.264 hot path is therefore still predominantly generic C; the
available ARMv6 object mainly accelerates start-code scanning. Threads,
demuxers, protocols, encoders and every decoder except H.264 remain disabled.

`CONFIG+=ffmpegsoft2` no longer enables the failed 180 ms presentation-drop
experiment. It can be reproduced only with the additional diagnosis flag
`CONFIG+=ffmpeglatedrop1`; device logs showed that it advanced media time but
reduced visible output to roughly 6-7 fps.

Rebuild from the workspace source checkout:

```powershell
git clone https://github.com/hrydgard/ppsspp-ffmpeg .tmp/ppsspp-ffmpeg-research
git -C .tmp/ppsspp-ffmpeg-research checkout b87f7c6d522d1edba77cfc4fac96ce48a236f806
symbian\third_party\ppsspp_ffmpeg\Build-Gcce-H264.ps1 -EnableArmAssembly -Reconfigure
```

`Build-Gcce-H264.ps1` verifies that exact clean Git commit before compiling and
defaults to `Symbian3Qt474`, the public Release SDK. `SdkRoot`, Git Bash,
MSYS2 Bash, make and GCCE paths are explicit parameters for non-default local
installations; a mismatched checkout is rejected rather than silently built.

The generated `build/`, `include/`, and `lib/` trees are local build outputs
and are intentionally ignored by Git. Then build the ordinary application:

```powershell
. symbian\env\Enter-SymbianQt.ps1
symbian\Build-App.ps1 -Configuration debug
```

`Build-App.ps1` copies `libavcodec.lib` and `libavutil.lib` into the active
SDK's `epoc32/release/armv5/{udeb,urel}` folders because SBS only resolves
MMP `STATICLIBRARY` entries there. 0.9 ordinary builds stage and link these
archives; they are not committed because they are reproducible GCCE outputs.

Before a public release, preserve this build script, the corresponding source
offer and the application relink materials required by LGPL static linking.
