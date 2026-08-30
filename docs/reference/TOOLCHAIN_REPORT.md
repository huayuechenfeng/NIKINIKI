# Symbian Qt toolchain report

Recorded: 2026-08-24  
Status: **ready for one Symbian³ / Anna / Belle ARM/GCCE application build; Belle WLAN deployment and CODA debugging verified on a Nokia 603**

## Host environment

| Component | Detected value | Status |
|---|---|---|
| Operating system | Windows build `10.0.26200.9168`, AMD64 | Ready; Qt reports this modern Windows version as untested |
| Git | `2.43.0.windows.1` | Ready |
| Java | Eclipse Adoptium JDK `11.0.16.8` | Ready; not required by the command-line probe |
| Visual Studio | Community 2019 / MSVC `19.29.30145` | Ready for host-side audits |
| CMake / Ninja | repository-local CMake `3.31.6`, Ninja `1.11.1` | Ready |
| WSL | Ubuntu on WSL2 | Available as a fallback host |
| Upstream source | wiliwili `88e5876`, all nine submodules pinned | Ready |

Activate the ignored repository-local host tools with:

```powershell
. .\symbian\env\Enter-HostTools.ps1
```

## Installed Symbian target toolchains

Installation root: `C:\QtSDK`  
Public application target SDK: `C:\QtSDK\Symbian\SDKs\Symbian3Qt474`  
Belle fallback/diagnostic SDK: `C:\QtSDK\Symbian\SDKs\SymbianSR1Qt474`

| Component | Measured value | Status |
|---|---|---|
| Qt for Symbian | `4.7.4`; qmake `2.01a`; mkspec `symbian-sbsv2` | Ready |
| Original Symbian³ SDK | `Symbian3Qt474` | Default public build target |
| Belle SDK add-on | `SymbianSR1Qt474` | Ready as fallback/diagnostic target |
| GCCE | Symbian ADT Sourcery G++ Lite `4.4-172`, GCC `4.4.1` | Ready |
| Symbian Build System | SBS `2.17.0`, build `0ac6c4c3a570` | Ready |
| qmake | `bin\qmake.exe` | Ready |
| make | `epoc32\tools\make.exe` | Ready |
| Perl | `C:\QtSDK\Symbian\tools\perl\bin\perl.exe` | Ready |
| packaging | `epoc32\tools\makesis.exe` and `signsis.exe` | Ready |
| environment | `env.bat`; default `EPOCROOT=\QtSDK\Symbian\SDKs\Symbian3Qt474\` | Ready |
| QtNetwork | ARM link probe succeeded | Ready for runtime testing |
| QtDeclarative/QML | ARM link probe succeeded | Ready for runtime testing |
| Qt Mobility Multimedia | version `1.2.1`; `QMediaPlayer` ARM link probe succeeded | Ready for runtime testing |
| OpenGL ES 2.0 | native headers present; generated MMP links `QtOpenGL.lib`, `libGLESv2.lib` and `libEGL.lib` | Ready for rendering probe |
| Symbian MMF | headers and Qt Mobility MMF engine libraries present | Ready for runtime testing |

Both targets remain installed. `Detect-SymbianQt.ps1` deliberately prefers `Symbian3Qt474` so the public ARMv5 binary cannot accidentally acquire a Belle-only import; `SymbianSR1Qt474` remains available when explicitly selected.

On 2026-08-29 the current 0.9 application built and packaged in both Debug and Release against `Symbian3Qt474` with `sbs errors: 0` and 32 existing SDK warnings. The former `cookiemanager.dll` dependency was removed; import-table inspection confirms it is absent. See `SYMBIAN3_ANNA_BELLE_COMPATIBILITY_ZH.md` for hashes, runtime prerequisites and the pending three-system device gate.

## Archived packages and integrity

Both packages were obtained from the Symbian Archive index and its Internet Archive mirrors.

| Package | Size | SHA-256 | Archive SHA-1 / verification |
|---|---:|---|---|
| `QtSdk-offline-win-x86-v1.2.1.exe` | 1,779,180,088 bytes | `29906299823321E540E7AE7DDB2EDA3085FA5674B15349917CB25A6929317CFC` | `644B0D847B89DE609C85A663BE4511576490A013`; Authenticode valid, signed by Digia Plc |
| `Belle_SDK_for_QtSDK_v1.2.1_SymbianSR1Qt474.7z` | 84,090,665 bytes | `FE6B301709266BB92C4A03449D736E5DDFDA72F21A07E138228AF7464544D0E6` | `B6210C48D1E90CE81199E29CC1F55C84B5645C78`; 7-Zip integrity test passed |

Sources:

- <https://github.com/mrRosset/Symbian-Archive/blob/master/SDKs-UIQ%26S3.md>
- <https://archive.org/details/nokia_sdks_n_dev_tools>
- <https://archive.org/details/nokia_sdks_n_dev_tools2>

Windows Defender was disabled/unavailable on this host, so an on-demand Defender scan could not be recorded. This does not invalidate the signature or hashes, but it remains a security caveat for the archived add-on.

The Maemo/MADDE/Harmattan components were intentionally excluded. They are unrelated to Belle and would install obsolete Nokia USB/RNDIS drivers whose modern Windows compatibility is poor.

## Verified build workflow

From PowerShell at the repository root:

```powershell
. .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
.\symbian\Build-Probe.ps1 -Configuration release
.\symbian\Build-Probe.ps1 -Project qt-capabilities -Configuration release
```

Both release probes completed with SBS configuration `arm.v5.urel.gcce4_4_1`, zero errors and two SDK/toolchain warnings. The broader probe compiled and linked QtNetwork, QtDeclarative, Qt Mobility Multimedia, QtOpenGL and native GLES2/EGL.

| Artifact | Size | SHA-256 |
|---|---:|---|
| `symbian/out/qt-minimal-release/wiliwili_symbian_probe.sis` | 6,580 bytes | `2F791563B9CF6E14920C3A8DD201EDADB204966C8260F023C48F61C4F654B485` |
| `symbian/out/qt-capabilities-release/wiliwili_symbian_capabilities.sis` | 6,212 bytes | `25F65939D677C1CA3064AC3837396C3E5FE9C0C91AB11488DBB71DC559409D07` |
| `epoc32/release/armv5/urel/wiliwili_symbian_probe.exe` | 5,513 bytes | `54F44C7402BC5327833E139FA8851ECEE5A372ECC1607ECC5608B539A1DF81A3` |
| `epoc32/release/armv5/urel/wiliwili_symbian_capabilities.exe` | 5,043 bytes | `E95803DD90BE1563C8365FB01339EC3A25FDA3FB25968F8A1ECAFC4CE0191D25` |

Generated outputs are ignored by Git. Rebuilding may change signed-package hashes because packaging metadata can be time-dependent.

## Signing and installation caveat

The SDK packaged both probes with its bundled Qt Development Frameworks self-signed development certificate. That certificate's validity period ended in 2019, so a Belle phone may reject the package based on its current date or security settings. For device testing, use a current developer certificate where available or the device's accepted legacy installation procedure; do not commit private keys.

## Physical-device deployment and debugger verification

Verified on 2026-08-24 with a Nokia 603 running firmware `113.010.1506` and Public CODA `1.0.6`:

- CODA transport: WLAN（私网端点不公开）, breakpoint mode KBA;
- Qt Creator: `2.4.1`, launched with Windows 7 compatibility mode on the Windows 11 host;
- registered build kit: `Qt 4.7.4 for Symbian Belle (Qt SDK)`, qmake at `C:\QtSDK\Symbian\SDKs\SymbianSR1Qt474\bin\qmake.exe`;
- build: `arm.v5.udeb.gcce4_4_1`, zero build errors;
- deployment: the debug SIS was transferred, installed and launched through CODA;
- debugger: the remote session started, negotiated a 16 KiB packet limit, interrupted the target with `SIGTRAP`, and enabled continue/step-in/step-over/step-out controls.

| Belle debug artifact | Size | SHA-256 |
|---|---:|---|
| `symbian/probes/qt-minimal/wiliwili_symbian_probe.sis` | 7,344 bytes | `15CC6A1202638B1A43E73CA7F9BFDB75A15E66B16832647CEF3CEB239813D2F5` |
| `epoc32/release/armv5/udeb/wiliwili_symbian_probe.exe` | 6,555 bytes | `A0E5CD038FEDE70EA2E0FB33F502F7DBC75DE5DF0683A0B9C79883957C74326C` |

The generated Qt Creator project file stores the active Belle kit and the WLAN endpoint locally. If the phone obtains a new DHCP address or CODA is configured with another port, update **Projects → Run Settings → Device Configuration** before launching.

## Remaining M0 runtime checks

The build/deploy/debug path is complete. The remaining checks concern runtime functionality rather than toolchain setup:

1. maximized display, touch button and Back-key behavior;
2. startup time, idle memory, foreground/background and clean exit;
3. QtNetwork HTTPS/TLS behavior against current Bilibili endpoints;
4. QML and GLES2 rendering on device;
5. Qt Mobility `QMediaPlayer` hardware playback and codec coverage.

USB serial deployment is not configured because Windows currently exposes the phone only through MTP/WPD. This does not block development: WLAN CODA deployment and native debugging are working.
