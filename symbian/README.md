# Symbian³ development environment

This directory contains the platform-specific environment and toolchain for
**NIKINIKI**, previously known as `wiliwili for Symbian³`. Internal target and
historical names remain unchanged for compatibility and traceability.

## Current status

- The current development mainline is version 1.1.0; 1.0.0 remains the latest
  formally signed and published release until 1.1 device validation completes.
- Version 1.0.0 is the formal release: compatible AVC
  keeps the native MMF route; the known weighted/7-reference AVC template
  uses the on-phone PPSSPP-FFmpeg H.264 fallback with the existing CPU
  YUV420P→RGB565 renderer. The earlier GLES YUV420 renderer is retained only
  as historical research code and is no longer requested by normal playback.
  See `../docs/releases/RELEASE_1.0.0_ZH.md` for the release package and
  `../docs/README_ZH.md` for the documentation entry points.
- The application source is self-contained under `symbian/`, and this NIKINIKI
  repository is the only product source of truth. The pinned wiliwili `yoga`
  checkout and private device research live in a separate sibling repository.
- Qt SDK 1.2.1 provides both the public-build `Symbian3Qt474` target and the
  Belle `SymbianSR1Qt474` fallback used for device diagnosis.
- Release ARMv5/GCCE compilation and SIS packaging have passed for both the minimal UI and SDK capability probes.
- Qt Creator 2.4.1 is registered with the Belle qmake and can build, deploy and debug the minimal probe through WLAN CODA.
- A Nokia 603 has completed debug SIS installation, launch, remote interrupt and debugger step-control verification.
- The formal M1 application under `symbian/app` now builds as ARMv5 Debug, packages as a signed SIS, and connects to the Nokia 603 through CODA.
- The application implements native Bilibili browsing, QR login, WBI search,
  details, comments, account lists, interactions, messages and ordinary video
  playback. Hardware codec and live-container availability still depend on the
  phone firmware.
- Normal builds keep Avkon panes constructed. The verified native-landscape
  state machine waits for `workAreaResized()` and a physical 640x360 work area
  before showing the persistent player surface, then restores portrait on exit.

See [`../docs/reference/TOOLCHAIN_REPORT.md`](../docs/reference/TOOLCHAIN_REPORT.md) for exact versions and missing components.
See [`../docs/ROADMAP_ZH.md`](../docs/ROADMAP_ZH.md) for the current implementation plan and scope.
See [`../docs/research/future/LIVE_PLAYBACK_ARCHITECTURE_ZH.md`](../docs/research/future/LIVE_PLAYBACK_ARCHITECTURE_ZH.md) for the historical upstream live-player analysis.

## 1. Bootstrap host tools

The bootstrap installs fixed versions into `.tools/` and does not modify the system PATH:

```powershell
.\tools\Bootstrap-HostTools.ps1 -Python C:\path\to\python.exe
. .\symbian\env\Enter-HostTools.ps1
```

## 2. Detect the Qt/Symbian SDK

The installed SDK is detected with:

```powershell
.\symbian\env\Detect-SymbianQt.ps1 -SearchRoot C:\QtSDK
```

The environment is considered ready only when the detector finds:

- a Symbian-targeting `qmake`;
- GCCE or another verified Symbian compiler;
- `make`;
- Perl;
- `makesis` and `signsis`.

The detector prefers the lowest `Symbian3Qt474` target when both targets are
installed. `SymbianSR1Qt474` remains available for explicit Belle diagnosis.

## 3. Enter the SDK environment

The script imports the variables produced by the SDK's own `qtenv2.bat`/`qtenv.bat`. Dot-source it so the variables remain in the current terminal:

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
```

If auto-detection does not find the batch file:

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 `
    -EnvironmentBatch C:\QtSDK\path\to\qtenv2.bat
```

## 4. Build the minimal probe

```powershell
.\symbian\Build-Probe.ps1 -Configuration release
```

The script asks qmake which make targets are available, prefers `release-gcce`, and runs `make sis` when the generated makefile exposes that target. Output is kept under the ignored `symbian/out/` directory.

The probe deliberately uses only QtCore and QtGui. Its purpose is to prove that the toolchain can produce an installable application before QML, networking, multimedia, OpenGL ES or borealis are introduced.

## 5. Build the SDK capability probe

```powershell
.\symbian\Build-Probe.ps1 -Project qt-capabilities -Configuration release
```

This second probe links QtNetwork, QtDeclarative/QML, Qt Mobility Multimedia, QtOpenGL and native `libGLESv2`. A successful build proves SDK availability, not runtime performance or hardware support; those still require a Belle phone.

## 6. Debug on the Nokia 603 through WLAN CODA

1. Connect the phone and PC to the same WLAN.
2. Start Public CODA on the phone and leave it in the foreground.
3. In Qt Creator, select the `Qt 4.7.4 for Symbian Belle (Qt SDK)` build configuration.
4. Under **Projects → Run Settings**, choose the WLAN/CODA communication channel and enter the address and port shown by CODA.
5. Press **Start Debugging**. Confirm any installation prompt on the phone.

Use the current WLAN address and port shown by CODA; never commit a private
device endpoint. A Qt Creator/CODA diagnosis may explicitly use
`SymbianSR1Qt474`; public Release builds must use `Symbian3Qt474`.

Qt Creator 2.4.1 needs Windows 7 compatibility mode on this Windows 11 host. Use:

```powershell
.\symbian\Launch-QtCreator.ps1
```

The USB connection currently exposes MTP/WPD only, so USB serial CODA is not configured. WLAN deployment is the working path.

## 7. Build the formal application

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-App.ps1 -Configuration debug
.\symbian\Build-App.ps1 -Configuration release
```

Qt 4.7's Symbian qmake generator does not resolve this multi-directory project
correctly as a shadow build. `Build-App.ps1` therefore builds in
`symbian/app` and copies the signed SIS to
`symbian/out/wiliwili-symbian-debug/`.

Since 0.9, the project file enables the local `ffmpegsoft2` H.264 fallback for
ordinary builds and uses the CPU RGB565 output path for normal soft playback.
`Build-App.ps1` stages the GCCE-built `libavcodec.lib` and `libavutil.lib`
automatically. Do not add the diagnosis-only
`CONFIG+=ffmpeglatedrop1` when producing a user package. The SDK's automatic SIS
step uses its legacy self-signed certificate; for a distributable package,
replace that signature with the current certificate as described in
`docs/releases/RELEASE_1.0.0_ZH.md`.

## SDK constraints

- Use a short ASCII path such as `C:\QtSDK`; legacy Symbian tools often fail on long paths, spaces or non-ASCII characters.
- Do not mix a desktop-only Qt installation with the Symbian qmake mkspec.
- Keep the SDK-provided Perl, make, GCCE, `epoc32`, `makesis` and `signsis` from the same distribution where possible.
- Do not put Bilibili credentials, signing keys or private certificates in the repository.
- The SDK's bundled self-signed certificate expired in 2019. Device installation may require a current developer certificate or the phone's accepted legacy installation procedure.
