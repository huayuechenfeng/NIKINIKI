# Symbian porting audit

Baseline: wiliwili `88e5876bea9502d06f46a8656e3530684d3aaf7d`

Last updated: 2026-08-29

## Gate results

| Gate | Probe | Result |
|---|---|---|
| Host upstream baseline | CMake + MSVC configure | Reaches dependency discovery; the full desktop comparison build still needs an MPV SDK |
| Symbian Qt packaging | `symbian/probes/qt-minimal` | Pass: ARMv5 Debug/Release, signed SIS, CODA installation and native debugging on Nokia 603 |
| Focused API endpoint boundary | `symbian/probes/upstream-api` (historical directory name) | Pass baseline: GCCE compiled the former upstream header; current probe compiles the 12-endpoint self-contained NIKINIKI header |
| Upstream Result Model | `.tools/result-model-audit.pro` | Direct reuse fails: pinned nlohmann/json requests `<initializer_list>`, which the GCCE/STLPort target does not provide |
| NanoVG/OpenGL ES 2.0 | `symbian/probes/nanovg-gles2` | Pass: direct upstream NanoVG build plus font, PNG, cards, scrolling and selection verified on Nokia 603 |
| M1 formal application | `symbian/app/wiliwili_symbian.pro` | Pass: ARMv5 Debug package, CODA launch, retained UI scrolling and selection verified on Nokia 603; five-minute memory soak remains |
| Full borealis C++ core | header/CMake audit | Direct compilation rejected: current core explicitly requires C++17 and uses lambdas, `enum class`, `std::function`, smart pointers, `string_view` and inline static members |
| System video | 0.7 native `CVideoPlayerUtility2` on Nokia 603 | First-session pass: progressive H.264/AAC picture/audio, per-window rotation and danmaku overlay verified; second-entry ARGB-window lifecycle data abort remains P0 |

## Measured compiler boundary

- Target compiler: GCCE 4.4.1 (`4.4-172`).
- Default target dialect reports `__cplusplus == 1`; `-std=c++0x` enables GCC's early experimental mode but does not provide the standard library required by current upstream code.
- STLPort supplies C++03 strings and containers and successfully builds upstream `api.h`.
- The pinned nlohmann/json header itself excludes GCC older than 4.8 and includes C++11 headers such as `<initializer_list>` before a Result Model can be instantiated.
- Current borealis sets `CXX_STANDARD 17` and has modern-language constructs in its public core headers, so compiling it directly is not a credible maintenance path for GCCE 4.4.1.

## Reuse classification

| Upstream area | Level | Symbian strategy | Evidence |
|---|---|---|---|
| `wiliwili/include/api/bilibili/api.h` | B — focused derivative | Record only the 12 mappings used by NIKINIKI in `symbian/include/network/bilibili_endpoints.h`; retain commit and GPL provenance | Formal app no longer includes the upstream tree; endpoint probe covers representative values |
| NanoVG C renderer/backend | B — direct with target flags | Compile upstream source as GNU C99; provide Belle's missing `GLchar`; disable stb thread-local storage | ARMv5 Debug SIS built and rendered correctly on device |
| borealis fonts/images | A — direct | Embed selected upstream assets with Qt resources | Font atlas and PNG texture displayed on device |
| Result Model field definitions | B — generated compatibility | Preserve upstream class/field/parse semantics but generate C++03 structs and Qt-based parsers for the MVP subset | Direct `setting.h` compilation stops at missing `<initializer_list>` |
| WBI behavior | B — compatible port | Preserve algorithm and fixtures; replace cpr/std::function/modern containers with Qt equivalents | Header audit shows transport and C++11 coupling |
| Presenter/Intent behavior | C — structural | Keep page flows, pagination, cancellation and navigation semantics; express callbacks through Qt 4 signals/slots or C++03 interfaces | Current headers use lambdas, smart pointers and modern templates |
| borealis core/XML/View | C — structural plus selected source | Keep NanoVG, resources, visual vocabulary and layout structure; implement a small C++03/Qt platform shell and an XML compatibility mapper | Renderer passed; current full core is C++17-only |
| cpr/curl transport | D — backend replacement | QtNetwork facade with Direct/Bridge transports | Platform dependency boundary |
| mpv/FFmpeg | D — backend replacement | Native `CVideoPlayerUtility2`/MMF player behind an upstream-event-compatible facade; do not restore `QMediaPlayer/QVideoWidget` as the formal 0.7 backend | First-session picture/audio/rotation passed on Nokia 603; repeat-entry stability remains P0 |

## UI route decision

The unique Symbian UI route is now **NanoVG hosted by a Qt/Symbian platform shell**. QML will not be developed as a parallel production UI.

This decision preserves the part of borealis that the device and compiler demonstrably support: NanoVG rendering, fonts, images, styling concepts and page/layout structure. The modern borealis C++17 runtime cannot be copied wholesale; a deliberately small C++03 compatibility layer will map the MVP XML/View vocabulary onto NanoVG and Qt input. Upstream Activity/Fragment code remains the behavioral source and is classified file by file instead of being blindly compiled.

## M1 implementation status

The passing NanoVG probe has been promoted into `symbian/app` with:

1. a small retained view tree for box, label, image and scrolling-card primitives;
2. a generated fake-home fixture derived from the upstream recommendation model;
3. lifecycle and memory counters visible in a debug footer;
4. an ARMv5 Debug target, reproducible PowerShell build and CODA deployment configuration;
5. a clean boundary where the later QtNetwork/API facade can replace the fixture.

The formal target compiles and packages with GCCE and has reached an established
Qt Creator/CODA debugging session. Visual/touch acceptance and the five-minute
memory soak on the Nokia 603 remain before M1 is closed.
