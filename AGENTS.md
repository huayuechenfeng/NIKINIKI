# AGENTS.md — NIKINIKI Product Mainline Guide

NIKINIKI is the canonical Qt 4.7.4/GCCE Symbian³ product repository. Product
code, resources, build scripts, public documentation, contributions and release
tags are changed here and nowhere else.

The complete upstream wiliwili checkout, private device logs, failed experiments,
build outputs and signing materials live in a separate sibling research
repository. Treat that repository as read-only evidence during NIKINIKI product
work. Never maintain or export a second editable copy of the product source.

## Active 0.9 Checkpoint (updated 2026-08-29)

- Symbian application: `symbian/`;
- documentation index: `docs/README_ZH.md`;
- current factual status: `docs/DEVELOPMENT_STAGE_REPORT_2026-08-25_ZH.md`;
- immediate work order: `docs/NEXT_WORK_PLAN_ZH.md`;
- resolved repeat-entry evidence: `docs/PLAYER_0.7_SECOND_ENTRY_CRASH_ANALYSIS_ZH.md`;
- current media-compatibility evidence: `docs/PLAYER_0.7_CODEC_COMPATIBILITY_ZH.md`;
- active pre-1.0 decoding policy: `docs/PLAYER_1.0_DECODING_POLICY_ZH.md`;
- active software-decoder execution plan: `docs/PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md`;
- deferred BCM2763/“BCM2727” firmware research: `docs/POST_1.0_BCM2763_HWDEVICE_RESEARCH_ZH.md`;
- device verification history: `docs/DEVICE_TEST_MATRIX.md`;
- release artifact details: `docs/RELEASE_0.9.0_ZH.md` (0.7 remains historical).

Current facts that later sessions must preserve:

- 0.7 historically kept AVKON/QGL portrait and rotated only the MMF video `RWindow`; the 0.9 source now uses the device-verified native-landscape window state machine for both MMF and local FFmpeg playback;
- the Nokia 603 has confirmed first-session horizontal picture, audio, danmaku over video, and clear controls/UI;
- `KErrMMPartialPlayback (-12017)` is treated as recoverable and playback continues after track probing;
- controls and danmaku share one persistent top-level ARGB overlay; in current native landscape, MMF video, CPU RGB565 soft frames, overlay UI and input all use direct 640×360 coordinates with no 90-degree transform;
- the earlier one-shot MMF/video session and `deleteLater()` implementation reached `DESTROY_READY`, but it still crashed on the next entry;
- both Release and Debug packages also crash on repeat entry when launched independently from the phone. CODA is therefore not required to trigger the fault, and the fault is not UDEB-only;
- the failed packages data-abort while accessing `0x140` after `PLAYBACK_READY` and before the second `PLAYER_REBUILD_BEGIN`. No second-session MMF create/open/display/prepare has begun;
- Nokia 603 testing confirmed that `overlayreuse1` still freezes/exits on the second playback. Persistent ARGB-overlay reuse alone is therefore ruled out as a sufficient fix;
- `surfacepersist1` retains the complete player controller, native video `QWidget/CCoeControl/RWindow`, backend/observer, `CVideoPlayerUtility2`, and ARGB overlay until application exit. Closing playback only stops, detaches and hides; the next entry reuses all native object identities and only closes/reopens media on the existing MMF utility;
- the user has now confirmed on the Nokia 603 that `surfacepersist1` can leave and re-enter playback repeatedly. The original deterministic second-entry crash is functionally fixed; the separate 50-cycle Release soak gate is still pending;
- the current player P0 is codec coverage: `BV1oyhM6AETw` plays, while `BV1Uy8x6AETG` is audio-only. Both API sources are 640x360 H.264 High/yuv420p; the failing stream declares 7 reference frames, 4 reorder frames and weighted bi-prediction, versus 4/3/no weighted bi-prediction in the working stream. Its 720P and DASH AVC variants retain the same aggressive structure;
- an expanded device/API matrix confirms the same split without exceptions: failing `BV1AxhK6BE2j`, `BV1oX876REZK`, `BV1iL8K6HEMy` are 7 refs / 4 reorder / DPB 7 / weighted P=1,B=2; working `BV1eohG6rESA`, `BV16uhN68EUC`, `BV1wqhg62ERJ` are 4 / 3 / 4 / 0,0. Q16 preserves each template, so lowering to 360P is not a fix;
- Q6 remains a documented legacy HTML5 progressive-MP4 identifier, but the 2026-08-28 audit across four failing/two working BVIDs, early AV samples, the old PGC example, and all four parts of user-specified `BV15EhG6qEAg` returned Q16 and the same `...-1-16.mp4` object for Q6. The attempted Q16/Q6 capability ladder was removed at the user's request because it added state and network latency without producing a 240P object. Current source therefore keeps the simple product route: safe streams stay on MMF; a known hardware-risk stream goes directly to local FFmpeg;
- the user requires codec compatibility to be implemented entirely on the phone: do not introduce a bridge, remote remux or remote transcoding path;
- Nokia 603 DevVideo enumeration found the accelerated Nokia/Broadcom BCM2727 H.264 decoder at UID `0x10204C21`: `video/h264`, max 1280x720/14 Mbps, Baseline/Main/High entries through nominal Level 3.1;
- the 2026-08-26 header control experiment is decisive: with identical Annex-B/`EDuCodedPicture`/`EDuElementaryStream` input and MMF fully closed, BCM `GetHeaderInformationL()` returns success with correct 640x360 dimensions for the known-good High@5.1 4-ref/DPB4/weighted-off stream, but stable `KErrNotSupported (-5)` for the bad High@3.0 7-ref/DPB7/weighted stream. General input-format/header packaging and MMF resource contention are therefore ruled out;
- ARM H.264 decoder UID `0x102073EF` accepts the same bad header (`GetHeaderInformationL()==0`) but reports zero dimensions. This proves header acceptance only, not initialization, performance, frame output, or production suitability;
- the system ARM H.264 route is retired: on Nokia 603 it accepts the bad header but reports 0×0 and `ConfigureDecoderL()` returns `KErrNotSupported (-5)`. The shipped PPSSPP libraries also fail the GCCE 4.4.1 linker with mandatory EABI object attribute 44 and must not be linked or attribute-stripped;
- `Mp4AvcProbeReader` now parses MP4 `ctts` version 0/1 and carries both DTS and PTS; DevVideo inputs receive both decoding and presentation timestamps. This prerequisite is implemented, but device timestamp behavior still needs validation;
- a GCCE 4.4.1 H.264-only PPSSPP-FFmpeg/libavcodec backend is implemented. `ffmpegsoft1` proved on Nokia 603 that previously audio-only streams now produce picture, but its pure-C throughput is only about 2–3 fps and is not release-usable;
- `ffmpegsoft2` is the software fallback baseline: ARM1176JZF-S/ARMv6/VFPv2, size-safe `-Os`, ARM assembly enabled, NEON disabled, `AV_CODEC_FLAG2_FAST`, audio-clock catch-up and `FFMPEG_SOFT_TIMING/CATCHUP` logs. Whole-library `-O2` makes `.rodata` overlap GCCE's fixed `.data` base at `0x400000`; `-O3` also ICEs in `libavutil/tea.c`, so neither may be restored globally;
- first `ffmpegsoft2` device timing was decisive: 300 output pictures took 57,479 ms (about 5.2 fps); libavcodec decode consumed 9,583 ms (~32 ms/picture) while YUV420→RGB565 consumed 46,785 ms (~156 ms/picture, about 81% of wall time). This motivated the now-verified full-resolution LUT2X2 conversion optimization;
- `RGB565_LUT2X2` improved the same path to 300 pictures / 25,014 ms (~12.0 fps) and 600 / 52,550 ms (~11.4 fps), reducing conversion from ~156 to ~42 ms/picture. The subsequent 180 ms presentation-drop control advanced media time only to ~0.78× real-time while showing ~5.8 fps in the first stream and ~7.1 fps in the second; it is therefore retired from normal `ffmpegsoft2` and available only with the extra diagnosis flag `CONFIG+=ffmpeglatedrop1`;
- source inspection corrected the meaning of “ARM assembly enabled”: FFmpeg 3.0.2's ARM H.264 qpel, chroma, prediction, weighted prediction, deblock and IDCT fast paths are NEON-only, and its CABAC inline path requires ARMv6T2. Nokia 603's ARM1176 has neither, so the main H.264 hot path remains generic C; ARMv6 mainly accelerates start-code scanning;
- the 0.9 GLES-YUV experiment was measured on Nokia 603 and retired: three `GL_LUMINANCE` uploads plus the QGLWidget presentation took about 216 ms and 321 ms per displayed frame respectively, and Y/U/V ping-pong did not improve it. CPU YUV420P -> RGB565 remains the default conversion, but current presentation uses the separate opaque native soft surface described below rather than drawing video into the transparent ARGB overlay;
- the soft path keeps the persistent native player/MMF graph and queues compact YUV planes; after stale/drop selection, the consumer converts only the chosen frame to RGB565. Log `2746319` still painted that image inside the full-screen ARGB overlay and measured RGB565 video drawing as the remaining main display cost. Current source instead presents RGB565 through a persistent opaque native child surface, while the single transparent ARGB top-level draws only danmaku and controls above it. This split passes normal GCCE Debug and Release builds (`sbs errors: 0`, 34 existing SDK warnings) but still awaits Nokia 603 device validation. The GLES YUV renderer source remains isolated for future research only; normal soft playback no longer requests YUV420 output or calls the GLES upload path;
- following the transferable parts of PotatoStream, software fallback keeps one decoder thread, retains the existing non-reference-frame catch-up, skips non-reference loop filtering, and uses the existing division-free CPU YUV420P→RGB565 LUT2X2 conversion with a 40 ms presentation/overlay cadence (25 fps ceiling). Do not copy PotatoStream's 3DS-only Y2RU API, hard-float ABI, `-mtune=mpcore`, or frame policy into Symbian;
- the supplied UCPlayerEx evidence proves that UC contains a substantial static FFmpeg software engine and a direct-bitmap renderer, but its imports also include the complete `CVideoPlayerUtility` open/prepare/play/window/DSA lifecycle. Treat UC as a hybrid MMF + FFmpeg reference, not proof that every H.264 stream is software-decoded. Its reported near-30-fps result becomes a valid software ceiling only after the exact failing bitstream is confirmed to have used UC's software path; proprietary UC code is not a source donor;
- the exact non-NEON `put_h264_qpel8_arm` name reported in UC was not found in the inspected official FFmpeg n0.5.15/n0.8 or PPSSPP-FFmpeg 3.0.2 sources. Treat it as a likely UC/custom patch, not an assumed upstream routine that can simply be cherry-picked;
- `headercontrol1` is a retired diagnosis-only package. Its source path has been removed because it deliberately closed MMF for every stream. Normal builds keep known-good streams on MMF and keep MMF audio alive for the known-bad template; the rejected BCM experiment is additionally gated behind the explicit `WILIWILI_ENABLE_UNSUPPORTED_BCM_DEVVIDEO` define;
- until the 1.0 public release, do not continue exploring BCM2727/BCM2763 hardware-decoder limits. The required product order is MMF hardware playback, then on-phone software decode, then an explicit user-approved external-player handoff when built-in software decode cannot handle the media. Do not use a bridge, remote remux, or remote transcode;
- the reported BCM2763 hardware versus “Broadcom BCM2727” DevVideo identifier mismatch is post-1.0 research only. Nokia 808 firmware may be added later for offline comparison; keep reported chipset/1080p claims labeled as research leads until firmware evidence confirms them;
- the Broadcom decoder itself reports `SupportsDirectDisplay=false`; post-processor device results ruled out the rotated/scaled direct-display route. The current FFmpeg candidate therefore reuses the existing memory-frame/ARGB composition path. Do not reopen the hardware-output choice before 1.0;
- `FindDecodersL("video/avc") == KErrNotFound` was only a MIME mismatch: this firmware advertises `video/h264`. Always keep the unfiltered `GetDecoderListL()` fallback;
- Qt Mobility and the current Symbian target do not contain a separate usable software decoder. BCM rejects the failing stream and the system ARM decoder failed Configure. The only remaining built-in pre-1.0 path is the local 360P libavcodec backend, followed by explicit external-player handoff if its performance gate fails;
- 0.9 promotes `ffmpegsoft2` plus the optimized CPU RGB565 output path to the mainline build: normal qmake/Build-App invocations compile the local H.264 fallback; `ffmpeglatedrop1` remains diagnosis-only and is not in the package. The previously built GLES-YUV SIS files are historical and must not be treated as the current renderer baseline.
- the user-supplied `2734939` UDEB log contains both historical pre-deferred sessions (`RGB565`, `repackCopyMs=0`) and a later session with `RGB565_DEFERRED`; the latter confirms stale/drop-before-convert is active but still shows only about 3–4 fps consumer rate. Do not mix the old 3.1 fps evidence with the new session, and do not treat CPU `presented/uploaded=0` as a display verdict because those counters are GLES-only;
- the soft overlay first reduced MMF audio `PositionL()` to one sample per update timer cycle and shared it with decoder, danmaku and controls; ordinary MMF playback keeps its previous direct-position behavior. Log `2740509` showed that this cache was not the main bottleneck. Log `2746319` later measured roughly 14–39 ms per query on the native-landscape mainline. Current source therefore calibrates `PositionL()` every 500 ms and extrapolates between samples using playback rate; pause, seek, rate change and session changes invalidate the cache. This further cache is pending device validation and must not be confused with a decoder change;
- aggregated `SOFT_STATS` pacing telemetry is available for overlay paint time, PositionL time/query count, timer event count/elapsed time and maximum timer gap. Historical portrait-window log `2740509` showed about 3.2 fps and 333–344 ms/paint; later stage profiling assigned about 224 ms/frame to the now-removed full-frame 90-degree rotation. Decoder, queue and catch-up remain frozen until native-landscape mainline device telemetry is collected;
- the next measurement-only build now splits `paintEvent()` cumulative timing into `overlayClearMs`, `overlayVideoDrawMs`, `overlayIntermediateMs`, `overlayRotateMs`, `overlayDanmakuMs`, `overlayControlsMs`, `overlayPainterEndMs` and `overlayOtherMs`, using `User::FastCounter()` with a one-time local frequency calibration. It does not change playback behavior; no device result exists yet.
- log `2738898` measured historical `overlayRotateMs≈224 ms/frame` while `videoDraw≈72 ms`. Current native-landscape source deletes both the intermediate ARGB frame and fixed `(x,y) -> (H-1-y,x)` copy; log `2746319` confirmed `overlayIntermediateMs=0` and `overlayRotateMs=0` in both soft sessions.
- the subsequent `2740086` pasted run is not a soft-renderer result: it stops after `PLAYER_SOURCE_DEFER_MMF` and `DEVVIDEO_RANGE_HEADER_BEGIN 1 0 1572864`, before `DEVVIDEO_MP4_AVC`, `PLAYER_SOURCE_DEFER_MMF_DONE`, `FFMPEG_SOFT_BEGIN/READY/FIRST_FRAME` or a Range timeout. Treat it as an incomplete/stalled header-preflight log, not evidence that fixed rotation disabled soft decode; the same attachment's earlier `2738898` run still entered soft decode twice.
- the old GLES software-playback experiment hid the persistent player controller and forwarded key events through the main `QGLWidget`; that path is historical and is no longer entered by normal soft playback;
- 0.9 is a user validation candidate, not a public stability claim: the user must still verify the signed package on the Nokia 603, including MMF normal streams, former audio-only streams, pause/seek, danmaku and repeat entry.
- the first 0.9 GLES-YUV Debug device log did not reach FFmpeg at all: the concurrent MP4 header Range request stayed at HTTP status 0 / 0 bytes for ten seconds and ended as `DEVVIDEO_RANGE_TIMEOUT 1 0 0 20 20`, before MMF returned `KErrMMPartialPlayback`. This is a routing failure, not a GLES or decode-performance measurement;
- current source fixes that ordering for the Symbian FFmpeg build: it defers MMF URL opening only for the first MP4 source, prefetches header and (for a risk stream) the initial AU batch, then starts MMF AAC and the existing LoadedMedia/FFmpeg handoff. Every prefetch failure explicitly falls back to MMF. Existing signed 0.9 SIS files predate the CPU renderer switch. The required current device markers are `PLAYER_SOURCE_DEFER_MMF`, `DEVVIDEO_MP4_AVC ... true`, `PLAYER_SOURCE_DEFER_MMF_DONE soft-prefetch-ready`, and `FFMPEG_SOFT_READY ... RGB565_LUT2X2`;
- the original soft-only native-landscape experiment remains a failed historical snapshot under `symbian/archive/soft-native-landscape-2026-08-28/`; do not restore it. A separate app-shell investigation found the real window-state boundary: keep Avkon panes constructed, keep the main QGL mapped, leave fullscreen in stable portrait, wait `workAreaResized()` for physical 640×360 before showing a new fullscreen raster top-level, then hide it and confirm portrait before dynamically restoring main fullscreen. Final `appshell7-final-dynamic-fullscreen-rgb565` completed 20 cycles with 20 landscape first paints, 47 RGB565 presentation markers, 20 exact fullscreen portrait restores, zero timeout, and no system bars by visual confirmation;
- the verified state machine is now merged into 0.9 mainline. `VideoPlayerWidget` is an independent persistent top-level; normal playback keeps the full `surfacepersist1` controller/video-host/MMF/overlay identities while switching native orientation. Normal main sets no `AA_S60DontConstructApplicationPanes`; MMF is configured with no rotation; the soft frame, danmaku, controls and input use direct native coordinates. GCCE Debug and Release both build with `sbs errors: 0`;
- log `2746319` is the first integrated native-landscape mainline matrix: two FFmpeg-soft streams (one 640×360 and one 360×640-coded source) and two MMF `PROFILE_SKIP` streams all displayed correctly, returned cleanly, preserved native object identities, kept `overlayRotateMs=0`, and exited with code 0. The remaining soft bottleneck was the old full-screen ARGB video draw: about 155–179 ms/displayed frame with danmaku on and about 102 ms/frame for the portrait-coded soft source. This evidence is why the current source separates the opaque RGB565 video surface from the transparent UI/danmaku overlay. Decoder, queue, catch-up, network, RGB565 LUT and codec policy remain frozen before 1.0;

The following signed test candidates are private research artifacts. Their paths
are relative to the sibling research repository and do not exist in the product
checkout. Never publish or rebuild from them as the current release candidate.

Current signed test candidates:

- Release 0.9 mainline: `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_release_full_currentcert.sis`;
- Debug 0.9 mainline: `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_debug_full_currentcert.sis`.

- Release surface persist 1: `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_surfacepersist1_currentcert.sis`;
- Debug surface persist 1: `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_surfacepersist1_currentcert.sis`.
- Release local DevVideo capability probe: `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_release_devvideoprobe1_currentcert.sis`;
- Debug local DevVideo capability probe: `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_devvideoprobe1_currentcert.sis`.
- Debug FFmpeg pure-C history package: `symbian/out/releases/v0.7.0/wiliwili_symbian_0.7.0_debug_ffmpegsoft1_currentcert.sis` (SHA-256 `5FCA1231349D67088C93F0A2D7AEF5D6A90E2DB0E6ECBCD382EC8ED40B7E609A`; picture pass, about 2–3 fps).
- Release 0.9 GLES-YUV historical package: `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_release_full_currentcert.sis` (same bytes as `release_glesyuv1_currentcert`; SHA-256 `17CEE045699489B08143793CF3F70A37DF06EEC74BDDAB24CC75E622B60E8F04`; 9,703,416 bytes). Do not use as the current renderer candidate.
- Debug 0.9 GLES-YUV historical package: `symbian/out/releases/v0.9.0/wiliwili_symbian_0.9.0_debug_full_currentcert.sis` (same bytes as `debug_glesyuv1_currentcert`; SHA-256 `70ABE6972E52AD533A82A14520B425D6FAF5EB2C344C76AE178C3CE435DC3698`; 9,687,100 bytes). Do not use as the current renderer candidate.

`devvideoprobe1` signed packages contain the initial decoder query. The current source mainline includes the `ffmpegsoft2` code and optimized CPU RGB565 output by default; do not add `ffmpeglatedrop1`. Normal streams still remain on MMF (`PROFILE_SKIP`), while the known risk template uses local FFmpeg. The 11.4–12.0 fps figure is the last measured CPU RGB565 baseline; the later GLES-YUV packages measured about 3 fps end-to-end on Nokia 603 and are historical only. A new SIS must be built after this renderer switch before release.

`wiliwili_symbian_0.7.0_debug_headercontrol1_currentcert.sis` is also historical evidence only. Never use it as a playback or release baseline: it intentionally stops MMF after collecting the header result.

---

## Legacy upstream reference (research repository only)

The remainder of this file documents the full upstream wiliwili architecture.
Those directories are absent from the NIKINIKI product repository and must not
be used as the Symbian implementation baseline. Consult this section only when
comparing behavior with the separate sibling research checkout.

## Architecture Layers (Android-inspired)

| Layer | Directory | Role |
|---|---|---|
| Activity | `wiliwili/include/activity/`, `wiliwili/source/activity/` | Top-level full-screen pages |
| Fragment | `wiliwili/include/fragment/`, `wiliwili/source/fragment/` | Sub-pages composed within Activities |
| Presenter | `wiliwili/include/presenter/` | Async data-fetching logic; Fragments inherit Presenters |
| View | `wiliwili/include/view/` | Custom reusable UI components (MPVCore, RecyclingGrid, VideoView…) |
| API | `wiliwili/include/api/bilibili/` | Bilibili REST/gRPC wrappers via cpr + nlohmann/json |
| Utils | `wiliwili/include/utils/` | Intent (navigation), ProgramConfig (settings), ImageHelper, EventHelper |

The UI framework is a custom fork of **borealis** (`library/borealis/`). All views inherit `brls::Box` or its subclasses.

---

## Key Patterns

### Presenter / async safety
Every Fragment that fetches data inherits a Presenter class. Use the macros in `wiliwili/include/presenter/presenter.h` to guard callbacks against use-after-free:
```cpp
void requestSomething() {
    ASYNC_RETAIN  // captures token, tokenCounter
    bilibili::HTTP::getResultAsync<MyType>(url, params, [ASYNC_TOKEN](auto result) {
        ASYNC_RELEASE  // returns early if the view was destroyed
        brls::Threading::sync([this, result]() {
            // safe to update UI here (main thread)
        });
    });
}
```
Use `CHECK_AND_SET_REQUEST` / `UNSET_REQUEST` to prevent duplicate in-flight requests.

### XML-driven UI
- Layout files live in `resources/xml/` (`activity/`, `fragment/`, `views/`).
- Inflate in constructor: `this->inflateFromXMLRes("xml/fragment/home_recommends.xml");`
- Bind a child by XML id: `BRLS_BIND(RecyclingGrid, recyclingGrid, "home/recommends/recyclingGrid");`
- Custom views must be registered before use: `brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);` (done in `Register::initCustomView()` called from `main.cpp`).
- Valid custom XML element names (all registered in `Register::initCustomView()`):
  - **UI widgets:** `AutoTabFrame`, `RecyclingGrid`, `VideoView`, `VideoProfile`, `QRImage`, `SVGImage`, `TextBox`, `VideoProgressSlider`, `GalleryView`, `CustomButton`, `HintLabel`
  - **App views:** `UserInfoView`, `UpUserSmall`, `VideoComment`, `ButtonClose`, `CheckBox`, `SelectorCell`, `AnimationImage`, `ShareBox`, `DynamicVideoCardView`, `DynamicArticleView`
  - **Fragments:** `HomeTab`, `DynamicTab`, `MineTab`, `HomeRecommends`, `HomeHotsAll`, `HomeHotsHistory`, `HomeHotsWeekly`, `HomeHotsRank`, `HomeHots`, `HomeLive`, `HomeBangumi`, `HomeCinema`, `MineHistory`, `MineLater`, `MineCollection`, `MineBangumi`, `SearchTab`, `SearchOrder`, `SearchVideo`, `SearchCinema`, `SearchBangumi`, `SearchHots`, `SearchHistory`
- i18n key syntax in XML: `@i18n/wiliwili/some/key`; translation files are in `resources/i18n/{en-US,zh-Hans,zh-Hant,ja,ko,it,ja-RYU}/wiliwili.json`.

### Navigation — use `Intent`, never push directly
`wiliwili/include/utils/activity_helper.hpp` defines all navigation entry points:
```cpp
Intent::openBV("BV1Da411Y7U4");         // video by BV id
Intent::openSeasonByEpId(323434);       // bangumi episode
Intent::openLive(1942240);              // live room
Intent::openSearch("keyword");
Intent::openSetting();
Intent::openCollection("2511565362");   // favorites folder
Intent::openPgcFilter("/page/home/pgc/more?type=2&..."); // PGC filter
```

### HTTP API calls
All calls are async via `bilibili::HTTP` (`wiliwili/include/api/bilibili/util/http.hpp`):
```cpp
// Standard GET → parses {"code":0,"data":{...}}
HTTP::getResultAsync<MyResult>(Api::SomeEndpoint, params, callback, error);

// For WBI-signed endpoints (most web-interface APIs since 2023):
HTTP::getResultWithWbiAsync<MyResult>(Api::SomeEndpoint, params, callback, error);

// For app-signed endpoints (pass needSign=true):
HTTP::getResultAsync<MyResult>(url, params, callback, error, /*needSign=*/true);

// POST with typed response:
HTTP::postResultAsync<MyResult>(url, params, payload, callback, error);
```
- API URL constants: `wiliwili/include/api/bilibili/api.h`; JSON result structs: `wiliwili/include/api/bilibili/result/`
- `parseJson` looks for `data` (object/array) first, then `result` (object) as fallback when `code==0`
- **WBI signing** (`getResultWithWbiAsync`): fetches `img_key`+`sub_key` from `Api::Nav`, computes `mixin_key`, appends `wts` + `w_rid`. Keys cached for 1 hour. Required for most `/x/web-interface/` endpoints since 2023.
- HTTP defaults: `User-Agent: wiliwili`, `Referer: https://www.bilibili.com/client`, timeout 10000ms. Proxy and TLS verify are configurable via `SettingItem::HTTP_PROXY*` / `SettingItem::TLS_VERIFY`.

### RecyclingGrid (custom recycler view)
Implement `RecyclingGridDataSource` and call `recyclingGrid->setDataSource(...)`. Example from `player_activity.cpp`:
```cpp
class DataSourceFoo : public RecyclingGridDataSource {
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        auto* item = (MyCell*)recycler->dequeueReusableCell("Cell");
        item->setData(list[index]);
        return item;
    }
    size_t getItemCount() override { return list.size(); }
    void onItemSelected(RecyclingGrid* recycler, size_t index) override { /* handle tap */ }
    void clearData() override { list.clear(); }
};
```
`recyclingGrid->onNextPage([]{...})` triggers when the user scrolls to the end (infinite scroll).

### Image loading
Always append a platform-aware suffix:
```cpp
ImageHelper::with(imageView)->load(url + ImageHelper::h_ext);    // horizontal thumbnail (@672w_378h on PC)
ImageHelper::with(imageView)->load(url + ImageHelper::v_ext);    // vertical thumbnail (@312w_420h on PC)
ImageHelper::with(imageView)->load(url + ImageHelper::face_ext); // avatar (@96w_96h on PC)
```
`IMAGE_EXT` resolves to `.webp` when `USE_WEBP` is defined, else `.jpg`. PSV uses smaller sizes (e.g. `@256w_144h` for horizontal).

### Global event bus
Defined in `wiliwili/include/utils/event_helper.hpp`:
- `MPV_E` — player state (`MPV_LOADED`, `MPV_PAUSE`, `MPV_RESUME`, `UPDATE_PROGRESS`, `END_OF_FILE`, `RESET`, `RESTART`, `CACHE_SPEED_CHANGE`, `VIDEO_SPEED_CHANGE`, …)
- `APP_E` — app-wide custom events (string key + void*)
- `SEARCH_E` — search page events

Subscribe: `MPV_E->subscribe([](MpvEventEnum e){ ... });`  Fire: `MPV_E->fire(MPV_PAUSE);`

### Settings / ProgramConfig
`ProgramConfig::instance()` is a singleton that must be initialized with `init()` **before** `brls::Application::init()`. All settings use the `SettingItem` enum (defined in `wiliwili/include/utils/config_helper.hpp`). Notable items:
- `PLAYER_HWDEC` / `PLAYER_HWDEC_CUSTOM` — hardware decode method
- `APP_RESOURCES` — active custom theme ID (requires restart to apply)
- `KEYMAP` — button icon set: `"xbox"` (PC default), `"ps"`, `"keyboard"`
- `HTTP_PROXY`, `HTTP_PROXY_STATUS`, `TLS_VERIFY` — network settings
- `SHORTCUT_*` — rebindable keyboard shortcuts (modifier-key strings, e.g. `"ctrl-r"`)

### Custom Theme / Layout System
Themes live in `{configDir}/theme/{id}/` and can override any file under `resources/`.
- A theme directory must contain `resources_meta.json`: `{"name":"…","desc":"…","version":"…","author":"…"}`
- Discovered by `ProgramConfig::loadCustomThemes()` at startup; theme ID stored in `SettingItem::APP_RESOURCES`
- Switching themes calls `DialogHelper::quitApp()` — restart is required
- Reference theme repo: https://github.com/xfangfang/wiliwili_theme

### Custom Fonts & Icons
Drop these files into the config directory to override built-in assets:

| Filename | Purpose |
|---|---|
| `font.ttf` | Main UI font |
| `icon.ttf` | Button icon font |
| `emoji.ttf` | Emoji font |
| `danmaku.ttf` | Danmaku overlay font |
| `gamecontrollerdb.txt` | SDL gamepad database (desktop only) |

Built-in keymap fonts in `resources/font/`: `keymap_xbox.ttf`, `keymap_ps.ttf`, `keymap_keyboard.ttf`.

### Anime4K / Shader System
`ShaderHelper` (singleton, `wiliwili/include/utils/shader_helper.hpp`) manages profiles loaded from `{configDir}/shader.json`:
```json
{
  "profiles": [
    {
      "name": "Anime4K Mode A",
      "shaders": ["/path/to/Anime4K_Clamp_Highlights.glsl"],
      "settings": [["set", "scaler", "ewa_lanczossharp"]]
    }
  ],
  "animeList": [{ "anime": 28223043, "profile": "Anime4K Mode A" }]
}
```
- `settings` entries: `["set","key","val"]`, `["change-list","key","op","val"]`, or `["run","cmd"]`.
- Apply/clear with `ShaderHelper::instance().setShader(index)` / `clearShader()`.

### MPV Rendering Modes

| Mode | Trigger | Notes                                                  |
|---|---|--------------------------------------------------------|
| Framebuffer (default) | `MPV_USE_FB` (auto) | GL 3.2+ / GLES 2.0+; best performance                  |
| No framebuffer | `-DMPV_NO_FB=ON` | MPV draws fullscreen, UI is overlaid; PS4, PSV-GL, GL2 |
| Software render | `-DMPV_SW_RENDER=ON` | CPU only; for UWP/D3D12 porting                        |
| deko3d | `BOREALIS_USE_DEKO3D` | Switch native; 4K@60 with hardware decode              |
| GXM | `BOREALIS_USE_GXM` | PSVita native                                          |
| D3D11 | `BOREALIS_USE_D3D11` | Windows Native/UWP                                     |

Default hardware decode: Switch/GXM → `"auto"`, PSV-GL → `"vita-copy"`, PS4 → `"no"`, Desktop → `"auto-safe"`.

### Video Quality Codes

| Code | Quality | Notes |
|---|---|---|
| 127 | 8K | |
| 120 | 4K | |
| 116 | 1080P60 | Default on non-PSV |
| 80 | 1080P | |
| 64 | 720P | Default PSV GXM |
| 32 | 480P | Default PSV OpenGL |
| 16 | 360P | Default when logged out |

---

## Config Directory Locations

| Platform | Path |
|---|---|
| Nintendo Switch | `/config/wiliwili/` |
| PS4 | `/data/wiliwili/` |
| PSVita | `ux0:/data/wiliwili/` |
| macOS (release) | `~/Library/Application Support/wiliwili/` |
| Linux (release) | `$XDG_CONFIG_HOME/wiliwili/` or `~/.config/wiliwili/` |
| Windows (release) | `%LOCALAPPDATA%\xfangfang\wiliwili\` |
| Any platform (debug build) | `./config/wiliwili/` (next to binary) |

Main config file: `{configDir}/wiliwili_config.json` (cookie, refreshToken, all SettingItem values, GA client ID, search history).

---

## Build Commands

### Desktop (macOS)
```sh
brew install mpv webp
cmake -B build -DPLATFORM_DESKTOP=ON
make -C build wiliwili -j$(sysctl -n hw.ncpu)
# macOS app bundle:
make -C build wiliwili.app
```

### Desktop (Linux/Ubuntu)
```sh
sudo apt install libssl-dev libmpv-dev libwebp-dev
cmake -B build -DPLATFORM_DESKTOP=ON
make -C build wiliwili -j$(nproc)
# System install with .desktop entry:
cmake -B build -DPLATFORM_DESKTOP=ON -DINSTALL=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
make -C build wiliwili -j$(nproc) && sudo make -C build install
```

### Nintendo Switch (Docker, recommended)
```sh
docker run --rm -v $(pwd):/data devkitpro/devkita64:20251117 bash -c "/data/scripts/build_switch.sh"
# deko3d variant (4K@60 hardware decode):
# see scripts/build_switch_deko3d.sh
```
Switch requires custom ffmpeg/mpv (devkitpro defaults don't support network streams):
```sh
base_url="https://github.com/xfangfang/wiliwili/releases/download/v0.1.0"
sudo dkp-pacman -U $base_url/switch-ffmpeg-7.1-1-any.pkg.tar.zst \
                   $base_url/switch-libmpv-0.36.0-3-any.pkg.tar.zst
cmake -B cmake-build-switch -DPLATFORM_SWITCH=ON
make -C cmake-build-switch wiliwili.nro -j$(nproc)
```

### PSVita / PS4 (Docker)
```sh
# PSVita GXM (recommended):
docker run --rm -v $(pwd):/src/ xfangfang/wiliwili_psv_builder:latest-gxm \
    "cmake -B cmake-build-psv -G Ninja -DPLATFORM_PSV=ON -DUSE_GXM=ON \
     -DUSE_SYSTEM_CURL=ON -DUSE_VITA_SHARK=OFF -DCMAKE_BUILD_TYPE=Release && \
     cmake --build cmake-build-psv"
# PS4:
docker run --rm -v $(pwd):/src/ xfangfang/wiliwili_ps4_builder:latest \
    "cmake -B cmake-build-ps4 -DPLATFORM_PS4=ON -DMPV_NO_FB=ON \
     -DUSE_SYSTEM_CPR=ON && make -C cmake-build-ps4 -j$(nproc)"
```

### Useful CMake flags
| Flag | Purpose |
|---|---|
| `-DUSE_SDL2=ON` | Use SDL2 instead of GLFW |
| `-DMPV_NO_FB=ON` | No framebuffer (PS4, PSV GL, GL 2.0) |
| `-DMPV_SW_RENDER=ON` | CPU software rendering |
| `-DMPV_BUNDLE_DLL=ON` | Bundle mpv.dll into exe (Windows + USE_LIBROMFS=ON) |
| `-DDISABLE_OPENCC=ON` | Skip Chinese conversion library |
| `-DDISABLE_WEBP=ON` | Skip WebP support |
| `-DINSTALL=ON` | Linux system install with desktop entry |
| `-DDEBUG_SANITIZER=ON` | Enable ASan/UBSan (debug only) |
| `-DUSE_LIBROMFS=ON` | Embed resources into binary |
| `-DBUILTIN_NSP=ON` | Embed NSP forwarder (Switch only) |
| `-DAPP_PLATFORM_CUSTOM_LIBS=ON` | Manual deps: `APP_PLATFORM_INCLUDE` + `APP_PLATFORM_LINK_OPTION` |

---

## Debugging

- Run with `-d` for debug log level, `-v` for borealis visual debug overlay, `-t` for mpv terminal output, `-o <file>` to write log to file.
- To test a specific page without navigating the UI, uncomment the relevant `Intent::open*()` line in `main.cpp` (many test BV IDs are already commented there).
- `ProgramConfig::instance().init()` loads cookies and all settings; it must run before `brls::Application::init()`.
- In-app network diagnostics: Settings → Tools → Network Diagnostics (`fragment/setting_network`). Shows API connectivity, system/server time diff, WiFi state, IP, DNS.
- On Switch: if the app is black-screen on startup, delete `/config/wiliwili/` from SD card and retry.

---

## Platform Macros

| Macro | Platform |
|---|---|
| `__SWITCH__` | Nintendo Switch |
| `__PSV__` | PlayStation Vita |
| `PS4` | PlayStation 4 |
| `BOREALIS_USE_OPENGL` / `BOREALIS_USE_DEKO3D` / `BOREALIS_USE_D3D11` / `BOREALIS_USE_GXM` | Rendering backend |
| `MPV_USE_FB` / `MPV_NO_FB` | Framebuffer mode (auto-derived from above) |
| `USE_WEBP` | WebP image decoding enabled |

---

## Key Files at a Glance

| File | Description |
|---|---|
| `wiliwili/source/main.cpp` | Entry point; registers views, launches activity |
| `wiliwili/include/utils/activity_helper.hpp` | `Intent` — all navigation |
| `wiliwili/include/utils/config_helper.hpp` | `ProgramConfig` singleton, `SettingItem` enum |
| `wiliwili/include/utils/event_helper.hpp` | Global event buses (`MPV_E`, `APP_E`, `SEARCH_E`) |
| `wiliwili/include/presenter/presenter.h` | `ASYNC_RETAIN`/`ASYNC_RELEASE` macros |
| `wiliwili/include/api/bilibili/util/http.hpp` | `HTTP::getResultAsync`, `getResultWithWbiAsync` |
| `wiliwili/include/api/bilibili/util/wbi.hpp` | WBI signing implementation |
| `wiliwili/include/api/bilibili/api.h` | All Bilibili API URL constants |
| `wiliwili/include/view/recycling_grid.hpp` | Recycler list widget |
| `wiliwili/include/utils/image_helper.hpp` | Async image loading + platform-sized URL suffixes |
| `wiliwili/include/utils/shader_helper.hpp` | Anime4K / mpv shader profile management |
| `wiliwili/include/view/mpv_core.hpp` | MPV singleton, playback control, rendering backend |
| `wiliwili/include/view/danmaku_core.hpp` | Danmaku (bullet comment) overlay rendering |
| `wiliwili/include/api/live/danmaku_live.hpp` | Live room WebSocket danmaku via mongoose |
| `resources/xml/` | All UI layout files |
| `resources/i18n/` | Translations (en-US, zh-Hans, zh-Hant, ja, ko, it, ja-RYU) |
| `scripts/` | Build scripts for each platform; `README.md` covers custom ffmpeg/mpv for Switch |

