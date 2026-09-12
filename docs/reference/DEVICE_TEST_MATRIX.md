# Device test matrix

> Status: Active reference
> Recorded: 2026-08-25; latest evidence audit: 2026-09-07 (full-app experimental compatibility candidate host build)
> Scope: this is the only source for build/device pass, failure and pending coverage; Release package hashes belong in `docs/releases/`

## Device baseline

| Device | Firmware | Screen/input | Debug transport | Status |
|---|---|---|---|---|
| Nokia 603 | `113.010.1506` / Belle | touch, portrait test | Public CODA 1.0.6, WLAN（私网端点省略）, KBA | Active development device |
| Nokia E7 | 见下方“运行环境固定”已查询记录 | 本轮固定竖屏参照；滑盖状态未完整记录 | 已有 CODA 运行/重连记录，端点仅留本地 | 成功探针与应用内黑屏均有观察；详见 [Evidence Ledger](#e7-evidence-ledger)，普通 Release 未验收 |

The WLAN address is DHCP-assigned and must be rechecked in CODA before later sessions.

## Completed tests

| Test | Build/artifact | Result | Observation |
|---|---|---|---|
| Minimal Qt application | `qt-minimal`, ARMv5 Debug | Pass | Installed, launched and accepted touch input |
| Native debugger | Qt Creator 2.4.1 + CODA | Pass | Remote interrupt produced SIGTRAP; continue and step controls enabled |
| API endpoint boundary | `upstream-api`, ARMv5 Release | Historical direct-header build pass; current probe is self-contained | The directory name is retained historically; current source validates the focused NIKINIKI endpoint list without the upstream tree |
| NanoVG/GLES2 rendering | `nanovg-gles2`, ARMv5 Debug | Pass | Gradient, cards, upstream font and PNG displayed |
| NanoVG touch list | `nanovg-gles2`, ARMv5 Debug | Pass | User confirmed drag scrolling, card selection and changing footer index |
| M1 formal application build | `symbian/app/wiliwili_symbian.pro`, ARMv5 Debug | Build/deploy pass | GCCE linked with zero errors; self-signed SIS produced; Qt Creator/CODA TCP session reached `ESTABLISHED` |
| M1 formal UI function | `wiliwili_symbian`, Nokia 603 | Pass with minor layout issue | User confirmed launch, scrolling and selection; long title/description overlap recorded and fixed by a two-line clamp for the next build |
| 0.4.1 stability build | `wiliwili_symbian`, ARMv5 Debug/Release | Build pass | Full repeated Set-Cookie capture, QR credential completion, deferred single-dialog search, AppArc foreground recovery and reduced startup resources compile and link; both configurations produced signed SIS packages with `sbs errors: 0` |
| 0.4.1 full SIS icon launch | current-certificate full SIS, Nokia 603 | Pass with font issue | User confirmed the fully installed package launches normally from the icon; this isolates the former spinner to stale/failed silent deployment rather than AppArc executable startup. Installed NanoVG font was not opened through the C runtime path, addressed in 0.5 by Qt file-to-memory loading. |
| 0.5 native player compile | `wiliwili_symbian`, ARMv5 Debug/Release | Build pass | Both GCCE configurations finished with `sbs errors: 0`. The app links `QtMultimediaKit.lib`, `QtNetwork.lib` and Belle MMF; the player is lazy-created. Public API check returned `mp4720`, Q64, one progressive URL plus one backup. Danmaku endpoint returned uncompressed XML. |
| 0.5 package/signature gate | full Debug/Release SIS | Pass | Both packages contain the EXE, AppArc resources and deployed CJK font; capabilities are `NetworkServices ReadUserData`. The replacement self-signed certificate is valid from 2026-08-24 through 2036-08-21. |
| 0.6 live/quality compile | `wiliwili_symbian`, ARMv5 Debug/Release | Build pass | RoomPlayInfo V2/legacy parsers, AVC HLS/FLV selection, quality menu and quality re-request state machine compiled and linked in both configurations with `sbs errors: 0`. |
| 0.6 live API structure | Bilibili public live endpoints | Pass | The live home endpoint returned active `roomid` cards. RoomPlayInfo samples exposed `g_qn_desc` plus AVC HLS/TS, HLS/fMP4 and FLV streams; actual media decoding remains a device test. |
| 0.6 package/signature gate | full Debug/Release SIS | Pass | Both 0.6.0 packages contain the EXE, AppArc registration resource and deployed CJK font and are signed by the current certificate valid through 2036-08-21. |
| 0.6.1 font/player repair compile | `wiliwili_symbian`, ARMv5 Debug/Release | Build pass | The dual full/subset font path, MMF safe-area geometry, corrected control toggle, GLES lifecycle restore and audio-only quality fallback compiled and linked in both configurations with `sbs errors: 0`. A direct 8.3 MB qrc experiment correctly failed the GCCE 4 MB section boundary; the 1,825,668-byte fallback linked successfully. |
| 0.6.1 package/signature gate | full Debug/Release SIS | Pass | Both packages contain the EXE, AppArc resources, full CJK font and OFL license. `signsis -o` reports the current certificate valid from 2026-08-24 through 2036-08-21. |
| 0.6.2 regression repair compile | `wiliwili_symbian`, ARMv5 Debug/Release | Build pass | Top-anchored search editor, QR completion, comment compatibility endpoint, native video layer rollback, live HTTP preference and pre-QApplication S60 pane suppression compiled and linked with `sbs errors: 0` in both configurations. |
| 0.6.2 comments endpoint | Bilibili public reply endpoints | Pass | With the same UA/referer, `/x/v2/reply` returned `code=0` and three replies for the probe AID while `/x/v2/reply/main` returned `code=-352`. |
| 0.6.2 package/signature gate | full Debug/Release SIS | Pass | Package header is 0.6.2; EXE, AppArc resource, full font and license are present. `signsis -o` reports the current certificate valid through 2036-08-21. |
| 0.6.3 player/login/comment repair compile | `wiliwili_symbian`, ARMv5 Debug/Release | Build pass | The in-window MMF player, delayed native-surface binding, Symbian `CCookie` response parsing, upstream-style QR persistence, comment-thread endpoint and tolerant danmaku decoding linked in both configurations with `sbs errors: 0`. |
| 0.6.3 package/signature gate | full Debug/Release SIS | Pass | Package header is 0.6.3. Both packages are signed by the current certificate valid through 2036-08-21. |
| 0.6.8 landscape/search/image compile | `wiliwili_symbian`, ARMv5 Debug/Release | Build pass | Main-window-first orientation sequencing, delayed MMF surface creation, removal of rotating danmaku tool windows, full-row top search editor and 250 ms image response pump linked in both configurations with `sbs errors: 0`. |
| 0.6.8 package/signature gate | full Debug/Release SIS | Pass | Package header is 0.6.8. The EXE declares `NetworkServices ReadUserData`; both packages contain AppArc resources and the full font and are signed by the current certificate valid through 2036-08-21. |
| 0.6.9 foreground-ownership compile | `wiliwili_symbian`, ARMv5 Debug/Release | Build pass | A player-session state now covers the hidden orientation delay and blocks the main AppArc foreground timer, activation recovery, GL refresh and `showMaximized()` path until player close; both configurations linked with `sbs errors: 0`. |
| 0.6.9 package/signature gate | full Debug/Release SIS | Pass | Package header is 0.6.9. The EXE declares `NetworkServices ReadUserData`; both full packages include AppArc resources/font and are signed by the current certificate valid through 2036-08-21. |
| 0.7 native MMF integration | `wiliwili_symbian`, ARMv5 Debug/Release | Build/package pass | `CVideoPlayerUtility2`, fixed portrait AVKON/QGL, per-window 90° rotation, partial-playback handling and the single ARGB overlay compiled and linked with `sbs errors: 0`; current-certificate packages were produced. |
| 0.7 first horizontal playback | Nokia 603, Debug/CODA | Pass | User confirmed picture, audio and overall playback feel were good. Logs reached `OPEN_COMPLETE 0`, `AddDisplayWindow 0`, rotation/scale 0, `PREPARE_COMPLETE 0`, audio/video track availability and `NATIVE_MMF_PLAY`. |
| 0.7 danmaku over native video | Nokia 603, Debug/CODA | Pass | `PLAYER_OVERLAY_ALPHA ... 0` and `DANMAKU_READY` were followed by visible comments over the MMF video; the former risk of video covering danmaku is resolved for the first session. |
| 0.7 horizontal UI clarity | Nokia 603, Debug/CODA | Pass | Controls, text and danmaku rasterized into a reusable 640×360 ARGB image and then rotated 1:1; user confirmed the UI is now clear. |
| 0.7 one-shot teardown | Nokia 603, Debug/CODA trace5 | Pass | First exit reached `PLAYER_NATIVE_SURFACE_PARKED`, `PLAYER_SESSION_DELETE_LATER`, `DESTROY_BEGIN` and `DESTROY_READY`; the old C++/MMF session is no longer the primary unverified step. |
| 0.7 second player entry | Nokia 603, Debug/CODA trace5 | Fail / P0 | A new session and a new native overlay control were created, but after `PLAYBACK_READY` and before the second `PLAYER_REBUILD_BEGIN`, main thread 1000 data-aborted while accessing `0x140` at `0x7d4d55c2`. No second-session MMF construction/open/display/prepare had begun. |
| CODA interpretation | Qt Creator 2.4.1 source + current GDB log | Analysis complete | The CODA adapter maps Symbian exception/panic to GDB `SIGSEGV`; this explains the generic dialog name, not the device-reported data abort. Packet-size limiting and missing shared-library symbols are diagnostic limitations, not causes. |
| 0.7 Release standalone second entry | Nokia 603, Release icon launch | Fail / P0 | User confirmed the optimized installation also crashes on repeat entry without CODA. CODA is not a necessary trigger. |
| 0.7 Debug standalone second entry | Nokia 603, Debug icon launch | Fail / P0 | User confirmed the Debug installation also crashes without CODA. The fault is not limited to UDEB runtime behavior. |
| 0.7 persistent-overlay candidate | Nokia 603, `overlayreuse1` | Fail / P0 | User confirmed that the second playback still freezes/exits. Reusing only the ARGB overlay is not sufficient; this candidate is now a failed comparison package. |
| 0.7 persistent native-surface fix | Nokia 603 + ARMv5 Debug/Release `surfacepersist1` | Device functional pass; 50-loop soak pending | The controller, native video `QWidget/CCoeControl/RWindow`, backend observer, `CVideoPlayerUtility2` and ARGB overlay survive until application exit. User confirmed that playback can now be closed and opened again; the previous deterministic second-entry crash is fixed. |
| 0.7 AVC compatibility A/B | Bilibili Q16 real streams + `ffprobe/trace_headers` | Root-cause boundary identified | `BV1oyhM6AETw` plays with 4 refs/3 reorder/no weighted bi-prediction. `BV1Uy8x6AETG` is audio-only with 7 refs/4 reorder/weighted bi-prediction. Both are 640×360 H.264 High/yuv420p; the failure is not HEVC/AV1 or resolution alone. |
| 0.9 Q6 HTML5 capability/re-audit | Six good/bad BVIDs, early AVs, old PGC sample, four-part `BV15EhG6qEAg`; `qn=6&platform=html5&fnval=1`, 2026-08-28 | No Q6 object; player branch removed | Every audited response reports Q16 and resolves Q6 to the same `...-1-16.mp4` as Q16. `BV15EhG6qEAg` P1 is `183040 ms / 14166873 bytes` in both cases. The temporary Q6 ladder was removed because it added state and latency without a distinct source; known-risk streams again go directly to local FFmpeg. |
| 0.7 failing-source alternatives | Q6/Q16/Q32/Q64, progressive/DASH, platform variants | No compatible Bilibili variant found | Current Q6 is a server-side upgrade to the same Q16 object. Q64 and DASH AVC retain the failing 7/4/weighted structure; platform variants resolve to the same progressive file. |
| 0.7 expanded AVC device matrix | 3 failing + 3 working BVIDs, Q64/Q16 SPS/PPS | Strong classifier confirmed | All failing samples are 7 refs / 4 reorder / DPB 7 / weighted P=1,B=2; all working samples are 4 / 3 / 4 / 0,0. Q16 preserves the same template, ruling out simple 360P fallback, Level and resolution as sufficient classifiers. |
| 0.7 local DevVideo capability probe | ARMv5 Debug/Release `devvideoprobe1` | Build/package pass | Links Belle `devvideo.lib` and enumerates local decoders before MMF controller creation. It logs UID, acceleration, direct-display, limits and formats as `WW:DEVVIDEO_*`; it does not select/initialize a decoder or change playback. |
| 0.7 DevVideo decoder enumeration | Nokia 603 Debug | Pass / route confirmed | `CMMFDevVideoPlay` creation and unfiltered enumeration succeed with 11 decoders. UID `0x10204C21` is accelerated Nokia/Broadcom BCM2727 H.264, advertises `video/h264`, max 1280×720/14 Mbps and High entries through nominal Level 3.1. Failing Q16 size/nominal High@3.0 is not excluded, but its `64001e` differs from firmware `64401e`; exact SPS acceptance still needs sample feed. `video/avc` failure was only a MIME mismatch. Decoder direct-display is false; post-processor/display output remains to be selected. |
| 0.7 DevVideo post-processor enumeration | ARMv5 Debug current Qt Creator build | Build pass; device result pending | Adds exact `video/h264` lookup and read-only enumeration of post-processor acceleration, direct-display, YUV/RGB, rotation and scaling capabilities. GCCE completed with `sbs errors: 0`; no decoder is selected or initialized and existing playback is unchanged. |
| 0.7 DevVideo post-processor device result | Nokia 603 Debug | Pass / memory-output route fixed | Two Nokia post-processors are accelerated and direct-display capable, but report only `ERotateNone` and no usable scaling combinations. Broadcom decoder itself has no direct display. Formal fallback therefore requires memory YUV → GLES2 rotation/scale under the existing ARGB overlay. |
| 0.7 BCM2727 sample-feed probe | ARMv5 Debug `devvideosample1` | Build/package pass; device result pending | Adds bounded MP4 Range parsing, exact SPS/PPS 7/4/7/weighted classification, AVCC→Annex-B and a 5-second `0x10204C21` memory-decode feed. Three builds completed with `sbs errors: 0`. It counts/returns pictures only; no GLES or audio is present yet. |
| 0.7 formal local AVC compatibility | ARMv5 Debug/Release `codeccompat1` | Build/package pass; BCM route rejected on device | The per-coded-picture/sample, memory-output and sync implementation compiled, but later header control proved BCM2727 rejects the target 7-ref/DPB7/weighted stream before initialization. These packages are historical diagnostics, not formal compatibility candidates. |
| 0.7 Direct DevVideo header control | Nokia 603 Debug `headercontrol1`, repeated good/bad streams | Pass / root cause isolated | With MMF fully closed and identical `video/h264` + coded-picture elementary-stream input, BCM returns 0 and correct 640×360 for known-good High@5.1/4-ref streams, but stable -5 for bad High@3.0/7-ref/DPB7/weighted streams. ARM decoder `0x102073EF` returns 0 for the bad header but reports 0×0. General input packaging and MMF resource contention are ruled out. |
| 0.7 headercontrol retirement | Current source, ARMv5 Debug | Build pass / safe default restored | Removed the dedicated BCM/ARM control function and parameter. Good streams use `PROFILE_SKIP`; bad templates log `BCM_REJECTED_KEEP_MMF` and retain MMF AAC. Old BCM takeover requires the explicit `WILIWILI_ENABLE_UNSUPPORTED_BCM_DEVVIDEO` define, absent from project files. Complete GCCE build: `sbs errors: 0`. |
| Pre-1.0 decoding policy | Documentation/product decision | Route frozen | No more BCM2727/BCM2763 limit exploration before 1.0. Use MMF hardware playback, then on-phone software decode, then an explicit external-player handoff if built-in software decode cannot handle the media. No bridge or remote transcode. |
| Pre-1.0 ARM software decoder candidate | Nokia 603 Debug `armsoftprobe1` | Fail / retired | Header parser accepted the bad stream but returned 0×0; `ConfigureDecoderL()` then returned `KErrNotSupported (-5)`. No Initialize or first picture. The system ARM route is retired. |
| Pre-1.0 FFmpeg software compatibility | Nokia 603 Debug `ffmpegsoft1` | Picture pass; performance fail | Previously audio-only videos now produce software-decoded picture through the existing overlay, proving the on-phone libavcodec path. User-observed throughput is only about 2–3 fps, so the pure-C build is not release-usable. |
| Pre-1.0 FFmpeg performance candidate | Nokia 603 Debug `ffmpegsoft2` | Picture pass; improved ~5.2 → ~12.0 fps; late-drop failed | Initial 300 pictures / 57,479 ms; LUT2X2 300 / 25,014 ms and 600 / 52,550 ms. Conversion fell from ~156 to ~42 ms/picture (3.7×); decode remains ~32–38 ms/decoded picture. The 180 ms late-drop control displayed 275 and dropped 491 conversions in 47,701 ms (~5.8 visible fps), then displayed 125/dropped 153 in 17,529 ms (~7.1 visible fps); media advanced only ~0.78× real-time. Normal `ffmpegsoft2` now disables this control. Source audit found that major H.264 ARM fast paths in FFmpeg 3.0.2 require NEON or ARMv6T2, so ARM1176 still executes generic C for the main decode hotspots. |
| 0.9 GLES-YUV Debug build | ARMv5 GCCE 4.4.1 / SBS 2.17.0 | Historical pass | The old candidate included the three-plane GLES2 renderer and linked successfully; it is not the current renderer baseline. |
| 0.9 GLES-YUV Release build | ARMv5 GCCE 4.4.1 / SBS 2.17.0 | Historical pass | The old candidate packaged successfully, but device telemetry measured roughly 216 ms/frame upload and 321 ms/frame presentation; ping-pong did not improve it. |
| 0.9 signed package inspection | `dumpsis -l` + `signsis -o` | Historical pass | The pre-switch Debug/Release packages are signed and remain useful for comparison only. A new package must be rebuilt after the CPU renderer switch. |
| 0.9 CPU RGB565/native-landscape mainline rebuild | ARMv5 GCCE 4.4.1 / SBS 2.17.0, 2026-08-28 Debug | Build and four-video device pass | Normal soft playback selects CPU YUV420P→RGB565 LUT2X2. The final native-orientation state machine and direct 640×360 path linked with `sbs errors: 0` (34 existing SDK warnings); log `2746319` then completed two soft and two MMF sessions with correct picture, overlay, return and final exit. Soft performance remained below target. |
| 0.9 Direct DevVideo header-preflight router | ARMv5 GCCE 4.4.1 / SBS 2.17.0, 2026-08-29 Debug + Release | Build pass; device pending | Replaces the `7/4/7/weighted` SPS heuristic with a synchronous `0x10204C21` `GetHeaderInformationL()` check over real SPS/PPS plus a sync AU. It does not configure/start DevVideo or change the decoder/display pipeline. Both builds ended with `sbs errors: 0`; device acceptance/rejection markers and resulting MMF/FFmpeg route remain the required next gate. |
| 0.9 log 2734939 historical CPU sessions | Nokia 603 UDEB `wiliwili_symbian.exe` | Historical / pre-deferred-conversion | One session reports `decoded=256`, `pictures=255`, `outputDrops=206`, `queueDepth=6`; inferred consumer ≈43 frames/14.066 s ≈3.1 fps. `repackCopyMs=0` and first-frame marker `RGB565` prove that session converted before queue/drop. |
| 0.9 log 2734939 latest CPU session | Nokia 603 UDEB `wiliwili_symbian.exe` | Deferred-conversion smoke evidence; performance still failing | First-frame marker `RGB565_DEFERRED`; at `pts=3400/10366 ms`, counters are `91/90/68/6` and `211/210/168/6`, so inferred selected frames are 16→36 (≈3–4 fps). `convertMs=1125→2380` grows with selected frames while `repackCopyMs=752→1612` grows with decoded frames, confirming conversion moved after stale/drop. CPU `presented/uploaded=0` is not a display verdict because those counters are GLES-only. The log ended when the remote host closed the connection, so no full-duration/final-summary gate passed. |
| 0.9 log 2736142 clock-cache A/B | Nokia 603 UDEB `wiliwili_symbian.exe` | Cache active; no material consumer-rate gain | Both sessions print `RGB565_DEFERRED`. The 640×338 session settles near 3.3 fps and the 640×360 session settles near 3.1 fps despite the per-cycle audio-position cache. End-of-session 640×360 counters are `decoded=671`, `pictures=670`, `outputDrops=552`, `queueDepth=6`; inferred selected frames ≈112. `convertMs=7398`, `repackCopyMs=5260`, `queueMutexWaitMs=1`; Range handoffs and repeat park/detach complete without timeout or new crash. Next diagnosis is paint/event-loop/WSERV segmentation. |
| 0.9 log 2740509 overlay pacing audit | Nokia 603 UDEB `wiliwili_symbian.exe` | Root bottleneck identified; performance still failing | Current CPU RGB565 session prints `RGB565_DEFERRED` and parks cleanly. At `pts=23433`, `decoded=448`, `pictures=447`, `outputDrops=366`, `queueDepth=6`; inferred selected ≈75, or ≈3.2 fps. `decodeMs=23303` (≈52 ms/decoded picture), `convertMs=5121` (≈68 ms/selected picture), `repackCopyMs=3766`, `queueMutexWaitMs=2`; these are not sufficient to explain 3 fps. The final intervals add only 19–20 paint events while `overlayPaintMs` adds 6.5–6.9 s, ≈333–344 ms/paint; `overlayTimerMaxGapMs=632–656`. `PositionL()` totals only 415 ms/82 queries, and Range has no timeout. Main P0 is full-screen ARGB overlay/WSERV `paintEvent()` blocking the Qt event loop; do not retune decoder/catch-up/network/queue or GLES. |
| 0.9 log 2738898 paint-stage profiling | Nokia 603 UDEB `wiliwili_symbian.exe` | Root stage identified; performance still failing | In adjacent 20-frame interval, timer wall is 9008 ms (450.4 ms/frame), `overlayPaintMs=6970` (348.5 ms/paint), `overlayVideoDrawMs=1446` (72.3 ms), `overlayDanmakuMs=990` (49.5 ms), `overlayRotateMs=4486` (224.3 ms), `overlayIntermediateMs=22` (1.1 ms), `overlayOtherMs=26` (1.3 ms), and painter end/clear are approximately zero. The children explain the full paint cost. Static inspection shows `overlayRotateMs` was QPainter full-ARGB `translate/rotate/drawImage`, not `QImage::transformed()`. |
| 0.9 fixed-rotation candidate | Historical source-only intermediate | Superseded by native landscape | The preallocated ARGB fixed-mapping candidate was never given a usable soft-playback device result. The current mainline removes the intermediate frame, fixed copy and second draw entirely; it is not a release baseline. |
| 0.9 executable 2740086 routing smoke | Nokia 603 UDEB `wiliwili_symbian.exe` | Incomplete / no soft-decode verdict | The run reaches `PLAYER_SOURCE_DEFER_MMF` and `DEVVIDEO_RANGE_HEADER_BEGIN 1 0 1572864`, then the pasted log ends. It contains no `DEVVIDEO_MP4_AVC`, `PLAYER_SOURCE_DEFER_MMF_DONE`, `FFMPEG_SOFT_BEGIN/READY/FIRST_FRAME`, `DEVVIDEO_RANGE_TIMEOUT` or process completion marker. The run is stuck/truncated during the first MP4 header Range preflight; rotation code has not executed on a soft frame. |
| Native landscape disposable-window probe | Nokia 603 Debug/CODA, 20 consecutive cycles | Pass / isolated lifecycle stable | All 20 cycles reached landscape `WORKAREA`, `VISIBLE 640x360 fullscreen true`, `FIRST_PAINT_END`, `WINDOW_DELETED` and portrait `RESTORED` without timeout or error. `SetOrientationL()` delivered resize signals synchronously before returning; the deferred `showFullScreen()` boundary remained stable. This rules out bare orientation, opaque raster top-level creation/paint, repeated deletion and portrait restoration as individually deterministic causes, but does not yet cover QGL, MMF, FFmpeg frames or the ARGB player overlay. |
| Native landscape `combined1` | Nokia 603 Debug, icon launch, 20 consecutive cycles | Pass / four-rule combination stable | Black background/text only; no MMF, FFmpeg or QGL; no `resizeEvent()` window-tree work; display/restore commits originate only from `workAreaResized()`. The physical-screen `640x360` pre-gate and post-fullscreen available-area confirmation completed repeatedly. Qt Creator/CODA currently deploys this package but does not issue its launch command; the same installed package launches from its icon and passes all 20 cycles, isolating that symptom to remote launch configuration rather than probe runtime. |
| Native landscape `appshell1` | Separate UID `0xE000B11C`; first icon-launch run | Safe timeout recovery / revised retest pending | The first package conflict (`131073`) was fixed with a UID-specific font directory. The installed build then showed a black host for about six seconds, restored the main UI and stopped without entering landscape, matching the `WaitingLandscapeWorkArea` timeout path rather than a crash. The premature black frame came from activating QGL foreground-owner quiescence before the gate; this is now deferred until exact landscape work-area confirmation. A dedicated `wiliwili_app_landscape_probe.pro` also replaces the stale Qt Creator run configuration that deployed the new target but remained named/bound as `wiliwili_symbian`. Network/player/MMF/FFmpeg remain absent at runtime; revised device result pending. |
| Native landscape app-shell fault isolation | Nokia 603 Debug/CODA, `appshell2`–`appshell4` | Root condition isolated | With panes suppressed, Qt reported landscape available work area while physical screen stayed 360×640; forced fullscreen reached first paint and then deactivated/exited. Keeping QGL visible alone did not fix it. Pre-hiding QGL deactivated the app before the landscape window could be committed. These runs isolated geometry/fullscreen/foreground ordering rather than raster or RGB565 content. |
| Native landscape `appshell7-final-dynamic-fullscreen-rgb565` | Nokia 603 Debug/CODA, 20 cycles | Pass / final app-shell gate complete | With Avkon panes constructed, retained QGL mapped, portrait `showMaximized()`, work-area-only commits, physical 640×360 gate, landscape dynamic fullscreen and exact 360×640 portrait fullscreen restore, the log records 20 landscape gates, 20 first paints, 47 moving RGB565 presentation markers, 20 restores, zero timeout, `COMPLETE cycles 20` and `EVENT_LOOP_EXIT 0`. User visually confirmed color bars/scanning line and no system bar in either orientation. |
| Native landscape mainline merge compile | Normal `wiliwili_symbian`, ARMv5 Debug/Release | Build and device matrix pass | Mainline removes `AA_S60DontConstructApplicationPanes`, makes the persistent player controller an independent top-level, adds the verified work-area state machine, configures MMF with no rotation, and removes overlay/input 90-degree transforms. Fresh qmake generated `moc_video_player_widget.cpp`; both GCCE modes linked with `sbs errors: 0`, followed by the `2746319` four-video device pass. |
| 0.9 executable 2746319 integrated playback matrix | Nokia 603 UDEB, two FFmpeg soft + two MMF streams | Functional pass / soft performance P0 | Both soft streams reached `RGB565_DEFERRED`; both compatible streams reached `PROFILE_SKIP`. Landscape and portrait-coded content, overlay/input, return, native identity reuse and final `EVENT_LOOP_EXIT 0` were correct by log and user observation. `overlayRotateMs=0`, but old full-screen ARGB video drawing still cost about 155–179 ms/presented frame for 640×360 and about 102 ms/frame for the portrait-coded soft source; with danmaku, total overlay paint reached about 183–241 ms/frame. |
| 0.9 opaque soft native-surface + 500 ms position cache | Current source, normal Debug/Release | Both builds pass; device pending | Soft RGB565 now paints in a persistent `WA_NativeWindow`/opaque child; the sole transparent ARGB top-level draws UI/danmaku above it and owns input. MMF and soft surfaces are mutually hidden. `PositionL()` is calibrated every 500 ms and extrapolated between samples, with pause/seek/rate/session invalidation. Both GCCE modes produced SIS output with `sbs errors: 0` and 34 existing SDK warnings. Decoder, catch-up, queue, Range, LUT and codec routing are unchanged. Require `SOFT_SURFACE_ACTIVE/FIRST_PAINT`, `softSurfacePresented>0`, `overlayVideoDrawMs=0`, `overlayPositionCacheHits>0`, visible/clickable UI and clean re-entry. |
| NIKINIKI brand/icon + public-repository gate | Current source, normal Debug/Release | Build/package pass; device pending | Public display name and Vendor are NIKINIKI while target/UID/settings remain compatible. Project PNG is 256×256 RGBA; filter-free SVG-T passed `mifconv 3.3.3` and qmake packaged `wiliwili_symbian.mif`. Both GCCE modes built with `sbs errors: 0`, 32 existing warnings; host JSON tests passed 58/58. Require Nokia 603 launcher name/icon, upgrade-state and playback smoke confirmation. |
| 0.9 Symbian³ / Anna / Belle universal application build | `Symbian3Qt474`, ARMv5 Debug/Release, 2026-08-29 | Build/ABI pass; Belle smoke pass; original/Anna public-beta pending | Removed the sole Belle-only `cookiemanager.dll` import and read repeated Set-Cookie parts through common RHTTP fields plus raw fallback. Both builds finished with `sbs errors: 0`, 32 existing warnings; `elftran` shows no CookieManager import, the generated PKG requires Qt 4.7.4 and Qt Mobility 1.2.0, and host JSON tests pass 58/58. The generated SIS files use the expired SDK certificate and are not release candidates. |
| 0.9 `Symbian3Qt474` universal package on Belle | Nokia 603 / Belle, 2026-08-29 | User smoke pass | User corrected the initial report: the black-screen/click-crash observation came from testing the wrong package and is invalid. The current lowest-SDK package shows no problem in the Nokia 603 test. Original Symbian³ and Anna results are intentionally deferred to public beta; do not describe them as device-verified yet. |
| 1.0.0 formal Release build and signing | `Symbian3Qt474`, ARMv5 UREL, 2026-08-29 | Build/package/signature pass | The user promoted this successfully built mainline to 1.0.0. GCCE completed with `sbs errors: 0` and 32 existing warnings; package and binary version are 1.0.0; the import table excludes CookieManager; code ends at `0x00345FDC` below fixed data base `0x00400000`; SIS capabilities match `NetworkServices ReadUserData`; the current Qt Development Frameworks certificate is valid 2026-08-24 through 2036-08-21; host JSON tests pass 58/58. Original Symbian³/Anna public beta and the 50-cycle soak remain follow-up coverage, not completed evidence. |
| 1.1 playback/decoder policy settings | `Symbian3Qt474`, ARMv5 Debug/Release, 2026-08-30 | Build/package pass; device pending | The 1.1.0 mainline persists three VOD transport routes (`OpenUrlL`, growing-cache `OpenFileL`, complete-download `OpenFileL`) and auto/hardware-only/software-only video routing. Normal Debug and Release each completed with `sbs errors: 0`, 32 existing warnings, and self-signed SIS output; repository/documentation gates and host JSON 58/58 passed. No N8/X7/C7 picture result is inferred from compilation. |
| 1.1 initial playback-policy device check | Nokia 603 / Belle, pre-shared-handle 1.1 test package, 2026-08-31 | Complete-download `OpenFileL` pass; growing-file and forced-software fail | The user reports that complete-download `OpenFileL` plays successfully. Growing-file `OpenFileL` continues downloading for tens of MiB but then shows `ERR14`; this corresponds to Symbian `KErrInUse (-14)` and matches the initial `QFile` writer versus MMF path-open share conflict (the download status masks the earlier player error until the reply finishes). Forced-software shows `SWERR`; one newly introduced path could make rejection of best-effort `SetVideoEnabledL(false)` fatal even though the previously verified FFmpeg path did not require that call to succeed. Mainline now uses shared native `RFile` handles and removes that false-fatal gate, but both fixes remain device-pending; if `SWERR` persists, Range/MP4 and FFmpeg initialization logs are still required. |
| 1.1 shared growing-file / forced-software retest | Nokia 603 / Belle, shared-`RFile` and best-effort-track-close test package, 2026-08-31 | Device pass; progressive startup polish pending | The user confirms the two preceding fixes pass: growing-file `OpenFileL` now plays without the former `KErrInUse (-14)`, and forced-software no longer stops at the false-fatal `SWERR` gate. With the initial 2 MiB progressive threshold, playback can still stutter near startup. Mainline therefore raises the on-disk head start to 8 MiB and adds a file-size/downloaded/percentage progress panel to complete-download mode; those two UX refinements are build-verified separately and still require device observation. |
| 1.1 transport UX refinement and startup acceptance | Nokia 603 / Belle, final 1.1 test package, 2026-08-31 | Device pass | The user confirms the final package works normally in actual use. Growing-file startup waits for 8 MiB instead of 2 MiB; complete-download mode shows preparation, file size, downloaded amount, percentage and a progress bar before `OpenFileL()`; playback and decoder setting cards open explicit three-row radio lists and persist the selected row. The first-install font path presents the built-in subset UI before chunk-loading the full CJK font, removing the former long synchronous black-screen wait. |
| 1.1.0 formal Release build and signing | `Symbian3Qt474`, ARMv5 Debug/Release, 2026-08-31 | Build/package/signature and Nokia 603 acceptance pass | Version 1.1.0 completed both GCCE modes with `sbs errors: 0` and 32 existing warnings. The public asset is named `NIKINIKI_1.1.0_release.sis`; the internal legacy target name is not exposed as a release asset. The SDK signature was removed and the package was signed with the current Qt Development Frameworks certificate valid 2026-08-24 through 2036-08-21. Repository/documentation gates and host JSON 58/58 passed. |
| 1.2.0 live and dynamic first device review | Nokia 603 / Belle, 1.2.0 development SIS, 2026-09-01 | Live fail (`ERR5`); dynamic layout/content fail; corrected mainline pending retest | The user reports that active live playback still repeatedly ends at `ERR5` with no playable picture. No CODA trace accompanied this observation, so it proves the product failure but not whether the final `KErrNotSupported (-5)` came from URL/MIME open, HLS/FLV controller selection, or growing local FLV; the next test must retain one complete fallback-chain log. The supplied Nokia 603 photo also shows dynamic media forced into the wrong aspect ratio and three solid-color placeholder blocks covering a real image; text/image dynamics and columns open without正文. Mainline now removes those blocks, downloads a width-only JPEG, uses decoded dimensions and unrestricted variable card height, and fetches the upstream dynamic-detail endpoint before comments. Those code changes are build evidence only until a new SIS is observed on-device. |
| 1.2.0 live remote-container CODA trace | Nokia 603 / Belle, Debug/CODA, 2026-09-01 | Remote FLV/HLS MMF failure stage confirmed; early local-FLV candidate build pass, device pending | `LIVE_PLAYBACK_READY` reports a valid AVC live source set and native 640×360 reaches `PLAYER_NATIVE_READY`. On FLV, explicit `video/flv` and `video/x-flv` each return `NATIVE_MMF_OPEN_COMPLETE -5`; empty MIME reaches open `0` but `PREPARE_COMPLETE -12008` on the first CDN and `-5` on the next. HLS explicit MIME likewise returns open `-5`, while empty MIME reaches prepare and returns `-5`; later HTTPS FLV repeats open `-5` and empty-MIME prepare `-34`. This rules out the API and native-window transition and proves the old 12-source/three-MIME loop was repeating a firmware container result before local fallback. Mainline now enters growing local FLV immediately after the first FLV triplet and retries only later FLV CDNs. Symbian3Qt474 Debug/Release both build/package with `sbs errors: 0`; local `OpenFileL` picture/audio remains device-pending. |
| 1.2.0 live growing-FLV CODA trace | Nokia 603 / Belle, Debug/CODA, 2026-09-01 | Local FLV MMF route rejected | The reordered build reaches `PLAYER_LIVE_LOCAL_FLV_FALLBACK` immediately after the first remote FLV MIME triplet and begins a progressive local write. At 8,432,361 bytes it records `PLAYER_LOCAL_OPEN progressive-threshold`; `NATIVE_MMF_FILE_HANDLE share-read-write 0` proves the writer/reader handle-sharing fix succeeds, but the asynchronous local `.flv` open completes with `NATIVE_MMF_OPEN_COMPLETE -5`. The first source had already delivered 8.43 MiB, so CDN reachability is not the primary failure. Nokia 603 therefore has no usable MMF FLV controller for either remote or local growing input. Future candidates must demux FLV on-device and retain MMF only for AAC/audio clock; more MIME/CDN/threshold tuning is outside this failure boundary. |
| 1.2.0 on-device FLV demux candidate | `Symbian3Qt474`, ARMv5 Debug/Release, 2026-09-01 | Build/package pass; device pending | Mainline incrementally parses bounded FLV tags, converts AVC length-prefixed NALs plus SPS/PPS into Annex-B access units for the existing FFmpeg/RGB565 path, and writes AAC as ADTS through the proven shared native writer for MMF audio/clock. Startup requires about 96 KiB AAC and an IDR-led 45-AU video batch; parser, incomplete input and pending-video limits are bounded, redirects are capped at three, and CDN failover moves only forward through FLV sources. Both GCCE modes package with `sbs errors: 0` and 33 existing/toolchain warnings. Device acceptance still requires `LIVE_FLV_AVC_CONFIG`, `LIVE_FLV_AAC_CONFIG`, local AAC `NATIVE_MMF_OPEN/PREPARE_COMPLETE`, `LIVE_FLV_VIDEO_START`, `SOFT_SURFACE_FIRST_PAINT`, audible synchronized audio and continued playback beyond the initial AAC extent. |
| Post-1.2 system-player URL/local handoff | `Symbian3Qt474`, ARMv5 Debug/Release, 2026-09-10 | Source/build/package pass; device pending | Playback settings expose internal/system-player variants of stream, growing-file and download-first. All system choices force and verify Q16/360P progressive MP4: stream passes the signed HTTP(S) URL with `video/mp4` to AppArc, while both other choices complete and validate a local MP4 before handoff. The URL is not logged and neither route forwards Cookie/Referer. Normal Debug and Release each completed with `sbs errors: 0`, 33 existing SDK/compiler warnings and self-signed SIS output; documentation/public repository gates and host JSON 58/58 passed. This does not prove a device handler, URL access, picture, audio, return or repeat use. |
| 1.3 system-player candidate user acceptance | User-tested signed candidate based on `168c659`, package version 1.2.0, 2026-09-12 | Functional acceptance: application and new route can run | The user completed testing of the signed system-player candidate and reported that it can run, then authorized promotion to 1.3.0. The report did not identify the phone, firmware, exact external player or six per-mode results, so it establishes release-level user acceptance without upgrading every URL/local-file, picture/audio, return or repeat-use cell to a device-specific pass. `StartDocument()` acceptance alone remains insufficient evidence of actual playback. |
| 1.2 initial Symbian³ experimental compatibility candidate | Normal full `wiliwili_symbian`, `Symbian3Qt474`, ARMv5 Debug/Release, 2026-09-07 | Source/build/package pass; later E7 different-media black-screen feedback; archived | Settings persisted a default-off experimental switch and locked it at session start. Off retained the ordinary first display bind/configure after OpenComplete; on deferred that first bind/configure to an accepted PrepareComplete before Play. Debug (`33` warnings) and Release (`36` warnings) compiled with `sbs errors: 0`; the then-generated qmake SIS used an expired SDK certificate. Later E7 logging proved the enabled path really deferred binding but still black-screened on a different, non-SHA-aligned growing-cache media input. That behavior and its full-app result are recorded below; the switch has since been removed and archived. This row is not the current product backend and does not infer N8/603 results. |
| 1.2 frozen Qt candidate same-media diagnostic | E7 / Belle Refresh `111.040.1511`, `e7fullappqt1` versus frozen `e7bindtiming1` A, sample A, 2026-09-07 | Reference 2/2 visible; full app 2/2 black with sound | A/full/full/A used fresh PIDs 2621/2645/2666/2687, one fixed complete local file and no reinstall between runs. Both A runs had continuous complete motion, normal audio and normal black background; both full-app runs had normal audio/player UI but no video. Full app reached visible 640×360 QVideoWidget/S60VideoWidget and Playing/Buffered/error 0; its position-bearing snapshots through about10 s were 0, while later position is UNKNOWN because the sparse logger suppresses unchanged state. A advanced to about51.8 s. Prepare/first frame/private surface remain UNKNOWN. The client mistakenly timed 60 s from process start, leaving about57–58 s after Play; all four attempts count and no fifth was added. See the detailed row set below. |
| H.264 Direct DevVideo REF1–REF8 matrix | Nokia 603 / Belle, independent `nikiniki_devvideo_capability_probe`, 2026-08-30 | Admission cliff measured at declared ref/DPB 6 → 7 under zero-B/zero-reorder/weighted-off controls | All eight input SHA-256 values matched the legal PC matrix. Decoder UID `0x10204C21` reported accelerated Broadcom BCM2727. REF1–REF6 passed Header, Configure, memory-output selection and Initialize, accepted 100 AU and emitted 99 pictures each; all 594 logged picture CRCs and all 18 saved raw-frame CRCs match the corresponding PC golden frames, with zero picture/slice/packet loss and no fatal callback. REF7 and REF8 both returned `KErrNotSupported (-5)` at `GetHeaderInformationL()`, before Configure/Initialize/AU submission. The six accepted cases retained one final picture and never emitted `StreamEnd`, so the probe ended them by its 20 s decode watchdog; this is an EOS/flush harness gap, not a throughput result. The selected accelerated HwDevice proves the failure is at or below that plugin boundary, but not that compressed REF7 data reached the physical VideoCore decoder. |
| H.264 Direct DevVideo R2 declared-DPB / weighted-P split | Nokia 603 / Belle, independent schema-2 `nikiniki_devvideo_capability_probe`, 2026-08-31 | Pass for declared DPB 4..8 at ref=4 and weighted P off/on at ref=4/6; legal ref=7 still rejected at Header | All 11 input SHA-256 values matched the PC-validated matrix. R6, D4..D8, WP4 off/on and WP6 off/on each passed Header, Configure, memory-output selection and Initialize, accepted 100 AU, and emitted 99 pictures. All 990 logged picture CRCs and all 30 saved raw-frame CRCs match their PC golden frames; picture/slice/packet loss and fatal callbacks are zero. WP-on streams actually use weighted P, so weighted P is not an independent admission or correctness limit through ref=6. D4..D8 keep `max_num_ref_frames=4` and identical non-SPS NALs while raising only VUI `max_dec_frame_buffering`, so declared DPB through 8 is not an independent Header limit. R7 is weighted-off/B-free/reorder-zero yet returns `KErrNotSupported (-5)` at `GetHeaderInformationL()` before any AU submission. In-band EOS plus `InputEnd()` still leaves one final picture and no `StreamEnd`, confirming a harness/plugin drain limitation independent of the admission result. This isolates the current legal Header cliff to `max_num_ref_frames` or a directly coupled reference-count check, but still does not prove compressed R7 data reached VideoCore. |
| H.264 Direct DevVideo original R7 / all-SPS-fake A/B | Nokia 603 / Belle, independent schema-3 `nikiniki_devvideo_capability_probe`, 2026-08-31 | SPS reference declaration controls Header admission; bypass produces corrupted pictures, not legal ref=7 decode | Both input SHA-256 values matched the audited A/B. `ORIGINAL_R7` is the legal weighted-off/B-free/reorder-zero ref=7 stream and again returned `KErrNotSupported (-5)` at Header with zero AU submitted. `FAKE_REF3` differs only in both SPS occurrences' `max_num_ref_frames: 7→3`; PPS (including default active L0=7), slice/reference graph, 100 AU, NAL sequence and every non-SPS NAL are byte-identical, while declared DPB remains 7. The fake passed Header, Configure, memory-output setup and Initialize, accepted all 100 AU and emitted 99 pictures with no reported loss or fatal callback. Against the legal golden, only 20/99 picture CRCs match; corruption starts at frame 7 and temporarily clears after the second IDR. All three saved initial raw frames match because corruption has not begun yet. The device's corrupted output matches PC fake decode only for the 11 pre-corruption/IDR-recovery frames and otherwise differs, which is permitted for an intentionally inconsistent stream. Conclusion: `max_num_ref_frames` is causal to this HwDevice admission decision, and the admission bypass is real; correct legal ref=7 hardware decoding and physical VideoCore handoff remain unproven (`CHIP_HANDOFF_UNPROVEN`). |
| H.264 Direct DevVideo Header/Submit split R4 | Nokia 603 / Belle, independent schema-4 `nikiniki_devvideo_capability_probe`, 2026-08-31 | Untouched legal R7 is decoded correctly after isolating the host Header gate | All admission and submission files matched their audited size/SHA-256. `R6_NATIVE` accepted 100 AU and produced 99/99 picture CRC matches. `R7_NATIVE` again returned `KErrNotSupported (-5)` at Header before submission. In `FAKE_HEADER_ORIGINAL_R7`, fake-ref3 supplied only the Header/Configure input; after a fresh Create/Select/Initialize the probe submitted all 100 AU of byte-exact `ORIGINAL_R7` starting with its original ref=7 SPS. The accelerated `0x10204C21` HwDevice produced 99 pictures: all 99 logged picture CRCs and all three saved raw-frame CRCs match the PC ORIGINAL_R7 golden, with no picture/slice/packet loss or fatal callback. This is not stale R6 output: R6 and R7 goldens differ in 93 of the first 99 frames, and at their first differing index 6 the device emitted R7 CRC `53B6DB72`, not R6 `66DD8DF6`. The full-fake control accepted the same count but diverged from the legal golden from frame 7, confirming the A/B sensitivity. One final delayed picture and `StreamEnd` remain absent in all accepted controls because of the known probe drain gap. Conclusion: the vendor hardware-accelerated path can correctly process this legal 640×360 ref=7 graph once the host parser admission is separated; H1 still requires original-SPS Header/Configure via a narrowly scoped gate change plus long/seek/recreate validation. Result bundle `20260831-181605`, events SHA-256 `E5FD5ADAB6E29C1B2E1BF5910E7FB9B58A4979CE7B5BADDBA05F5F32A0FA2F6E`. |
| H.264 Direct DevVideo original-SPS ref=7 admission R5 | Nokia 603 / Belle `113.010.1506`, manual non-auto ROM shadow experiment plus schema-4 probe, 2026-08-31 | Short-form original legal R7 hardware-decode breakthrough | The experiment changed exactly one Thumb immediate in the audited host AVCParser, `cmp r0,#6` to `cmp r0,#7`, retaining the following `bhi` and every other capability/error check. With the patch active, byte-exact `ORIGINAL_R7` supplied its own Header, Configure and all submitted AUs: Header reported 640×360, Configure/Initialize succeeded, all 100 AU were accepted and the accelerated `0x10204C21` HwDevice emitted 99 pictures. All 99 picture CRCs and all three saved raw-frame CRCs match the PC R7 golden, with zero picture/slice/packet loss and no fatal callback. R6 remained 99/99 correct; split R7 remained 99/99 correct; the full-fake negative control still matched only 20/99 legal-golden CRCs. This closes the original-SPS admission/decode short test and proves the located host gate, not this legal R7 graph's decode capability, caused the former failure. The user also reports that with the patch enabled, formal NIKINIKI successfully played a real video that previously required software decode; route telemetry, hardware-only mode, long playback, pause/seek and repeated lifecycle validation remain pending before final H1/productization. Result bundle `20260831-210944`, events SHA-256 `7C388D01F16094E13B9C2D9F8AEA7F9A86DC8222C553E225E72A86E111B2B022`. |
| H.264 universal-signature ref=7 patch R1 | Nokia 603 / RM-779 / Belle `113.010.1506`, RomPatcher+ `SnR`, 2026-09-01 | Device pass for the generalized signature patch | The wildcard signature `06 28 ?? D8 4B 21 09 03 ?? 42 ?? ?? 06 28` matches exactly once in this phone-visible DLL and selects the same first `cmp r0,#6` instruction proven by R5, while leaving the later high-resolution joint gate unchanged. After replacing the fixed-offset R5 rule with the generalized `SnR` rule, the user confirmed that the patch applies and Nokia 603 playback passes. This validates RomPatcher+ wildcard application on RM-779 SW113; it does not add a second independent CRC capture beyond R5 and does not validate the eight other statically audited firmware images. The public text patch is maintained under `symbian/patches/h264-ref7/`. |
| H.264 DevVideo ECom inventory R1 | Nokia 603 / Belle, independent read-only `nikiniki_devvideo_ecom_audit` 0.1.0, 2026-08-31 | Target ECom identity confirmed; RSC/DLL mapping still pending | Public decoder interface `0x101FB4BE` enumerated 11 implementations. UID `0x10204C21` is a version-1, enabled, ROM-based implementation on `Z:` named `IVE Video Decode AVC Hw Device`, vendor ID `0x101FB657`; it advertises `video/h264` Baseline/Main/High profile patterns, has opaque ASCII `0x1018`, and exposes no extended interface through the public metadata API. Adjacent `0x10204C1F/20/22/29` registrations are the same IVE H.263/MPEG-4/VC-1/VP6 family. All 33 `Z:\resource\plugins\*.rsc` files were readable, but raw little-/big-endian UID search found zero matches and zero read errors. This does not mean the RSC is absent: compiled ECom resources are structurally encoded. R1 therefore confirms the registry identity but not the DLL, Direct-vs-MDF path, Broadcom ownership, or physical VideoCore handoff. |
| H.264 DevVideo ECom inventory R2 | Nokia 603 / Belle, independent read-only `nikiniki_devvideo_ecom_audit` 0.2.0, 2026-08-31 | Direct DevVideo HwDevice path confirmed; registration archive and DLL remain pending | Decoder inventory reproduces R1. MDF Processing Unit interface `0x10273789` enumerates successfully with count 0, so target `0x10204C21` is not an MDF PU. Under the published DevVideo `CreateDecoderL()` branch this selects direct `CMMFVideoDecodeHwDevice::NewL()` rather than generic adapter `0x102737ED`. Of 33 loose `Z:\resource\plugins` files, 27 parse structurally as valid ECom v1/v2 resources and six unrelated non-ECom resources return `KErrCorrupt (-20)`; there are zero target structural/raw matches and zero read errors. Published ECom discovery first opens the read-only drive's `Z:\private\10009D8F\` `ecom` SPI archive set and falls back to loose RSC scanning only when no archive registrations are found, so the zero loose target is consistent with an SPI-contained registration. R2 closes Direct-vs-MDF but does not identify the archive resource/DLL, Broadcom lower component, or physical VideoCore handoff. Inventory SHA-256: `3B2CE5D4D4AD5043ED67380EEBF28F041FFA29B17EBC1E96B93DED28ABF4DB21`. |
| H.264 DevVideo ECom SPI inventory R3 | Nokia 603 / Belle, independent read-only `nikiniki_devvideo_ecom_audit` 0.3.0, 2026-08-31 | Exact ECom registration and DLL mapping confirmed | `RResourceArchive` opened the ROM archive successfully (`status=0`, type `0x10205C2C`) and structurally parsed all 1,359 embedded registration resources with zero parse errors. The sole target resource is `ivevideodecodehwdevice`, format 1, DLL UID `0x10204C1E`, decoder interface `0x101FB4BE`; it contains five IVE implementations: H.263 `0x10204C1F`, MPEG-4 `0x10204C20`, AVC `0x10204C21`, VC-1 `0x10204C22` and VP6 `0x10204C29`. ECom's archive-name rule maps this registration to readable `Z:\sys\bin\ivevideodecodehwdevice.dll`, size 54,564 bytes, SHA-256 `49025AE44F033DCEFEB2A0F4D694D9AA401E69912C0956454AA43EFD4C6B825A`. This closes the ECom-to-DLL identity and confirms the codecs share one IVE HwDevice binary; it does not yet identify that DLL's lower driver/firmware calls or prove that legal ref7 compressed data reaches VideoCore. Inventory SHA-256: `9D02D0CD72D9121ABA71BF8DFD7FD75A42A869B2A51515ECE93C4BB29DBE522C`. |
| RM-779 SW113 IVE decoder offline static audit | Nokia 603 common core `113.010.1506`, read-only ROM/XIP reconstruction, 2026-08-31 | Offline target is byte-identical to the phone-visible decoder DLL; first lower-layer chain recovered | Page-decompressing the common-core XIP entry produces a 54,564-byte `ivevideodecodehwdevice.dll` whose SHA-256 is exactly the phone R3 value `49025AE44F033DCEFEB2A0F4D694D9AA401E69912C0956454AA43EFD4C6B825A`; therefore the APAC1 VPL selection does not introduce a target-DLL mismatch for this audit. The raw ROM-stored entry hash `7667CD717D354493BFD03B2519E27A9034C5D1A02A186B1F570691DF5F7F5958` describes the still-compressed representation and is not a competing DLL hash. Reconstructed `TRomImage` metadata confirms UID3/SID `0x10204C1E`, vendor `0x101FB657`, one export, and format-specific classes including `CIveVideoDecodeHwDeviceAVCParser`. Direct-link pointer recovery across 83 reconstructed core modules matches 28 stubs to `DevVideo.dll`, `rcam.dll` and `ivepolicyserverclient.dll`; the policy server separately links to `ECom.dll` and `rcam.dll`, while RTTI in `rcam.dll` names `RBusLogicalChannel`/`RCam`. This makes the in-DLL AVC parser and the `rcam` channel the priority admission/handoff boundary, but does not yet locate the ref comparison, prove an OMX path, or prove chip delivery. The audited package is RM-779/Nokia 603, not a Nokia 808 cross-device sample. |
| 0.9 `devvideodirectprobe1` | Nokia 603 Debug/CODA, 2026-08-28 | Phase A hard fail / pre-1.0 retired | `CDirectScreenAccess` creation succeeds (`dsaError=0`), but the expected full-screen ARGB UI/弹幕 window leaves no DrawingRegion: overlay hidden and visible samples are both 0 rect / 0 area; the visible transition produces one Abort/Restart whose restart succeeds but still has area 0. The probe correctly emits `DIRECT_PHASE_A NO` / `DIRECT_RESULT phaseA NO phaseB NO`, never creates DevVideo or PP, then restores portrait cleanly. This rejects DSA beneath the formal full-screen overlay, not PP YUV capability in isolation. |

安装包大小、SHA-256 和签名不在测试矩阵中重复维护；正式版本只见
`docs/releases/`，历史诊断包只见对应历史发布说明。


## Pending tests

| Area | Required observation | Milestone |
|---|---|---|
| 1.0 header preflight routing | Known-good stream records `ACCEPT → MMF`; known-risk stream records `REJECT → FFMPEG`; no Q6/CDN/backend loop | Now |
| 1.0 Release repeat-entry soak | 50 MMF/FFmpeg alternating play/back/reopen loops; picture, audio, danmaku and controls remain correct; no crash, residual overlay, background audio or sustained memory growth | Now |
| Soft native surface | Risk stream reaches `FFMPEG_SOFT_READY`, `SOFT_SURFACE_ACTIVE` and `SOFT_SURFACE_FIRST_PAINT`; require `softSurfacePresented>0`, `overlayVideoDrawMs=0`, position-cache hits and visible/clickable UI | Now |
| Long playback | At least 30 minutes with temperature, memory, pause/seek/rate and foreground/background observations | Now |
| Original Symbian³ | Same formal 1.0 SIS plus Qt/Mobility prerequisites: cold start, HTTPS/images, QR login, MMF stream, FFmpeg stream, return and re-entry | Now / public beta |
| Symbian Anna | Same matrix and same application SIS as original Symbian³ | Now / public beta |
| N8/X7/C7 MMF transport A/B | Same MP4 and hardware-only decoder: compare `OpenUrlL`, growing-file `OpenFileL`, then complete-download `OpenFileL`; record `PLAYER_POLICY`, `PLAYER_LOCAL_OPEN`, `NATIVE_MMF_OPEN/PREPARE_COMPLETE/TRACKS/SCALE` and visible picture | Now |
| Frozen Qt compatibility full-app qualification | The exact `e7fullappqt1`/`e7sixr1` source and diagnostic identity remain on the local candidate branch for reproducibility. No further product qualification is scheduled unless ADR-0010 is superseded; any resumed device work remains research-only. | Frozen / research only |
| H.264 optional ref=7 patch qualification | On each additional device/firmware, first prove that the signature selects one semantic gate, then run original legal R7, low-ref controls, long playback, pause/seek and repeated lifecycle tests. Static DLL matching alone is not a device pass; disable the patch and reboot after every research session. | Later / optional maintenance |
| Native media matrix | 360P/480P/720P progressive H.264/AAC, `-12017`, CDN fallback, pause, seek and return | Next |
| QR/session | Cookie summary true/true/true, account data loads, session survives clean restart | Next regression |
| Search and content | Video/user search, input focus, comments/thread replies, history, favorites, later, uploads and following | Next regression |
| Player controls and layers | Controls, drag, quality, danmaku, dense-comment soak, lock/unlock and task switch | Next regression |
| System-player handoff | Test the system stream choice with one 360P MP4 signed HTTP(S) URL; separately test both system local choices, which must complete and validate the download before handoff. Record AppArc acceptance, actual external picture/audio, NIKINIKI deactivation, portrait return, second use and cache cleanup separately. Cover an expired/header-dependent URL, TLS failure, no MP4 handler, forced launch failure and download failure. The URL mode must not pass Cookie/Referer or log the URL; local modes must not hand off Cookie/Referer/signed URL or a growing file. | Now / device acceptance; source and host build are not a pass |
| Live playback | Build and retest the on-device FLV demux candidate. Require FLV AVC/AAC sequence-header markers, MMF opening the extracted AAC clock, FFmpeg first picture on the opaque software surface, audible synchronized audio, bounded queues, one-way CDN failover and clean return. The rejected local-FLV `OpenFileL` route must not run. Live danmaku remains later scope. | Now |
| 1.2 dynamic correction | New SIS keeps portrait/square/landscape media proportions, permits full variable-height scrolling without colored overlays, and opens text/image/column正文 plus comments. | Now |

<a id="e7-investigation-20260904"></a>

## E7 阶段调查：历史报告与本轮复验

以下 21 项观察取自产品 Git `b9e1a8d` 的设备矩阵，来源为用户提供的 2026-09-04 凌晨阶段纪要。
“报告通过/失败”只表示历史报告，不是本轮 CODA 复验；当时逐包身份、固件和原始时间线仍待补齐。实验语义按源码核正，审计字段见下方 Evidence Ledger。
当前 E7 的 Belle Refresh 信息不能反向补作所有历史实验的固件记录；N8 也没有对应通过结果。
代码固定点和实验定义见 [E7 诊断方案](E7_BLACK_SCREEN_DIAGNOSTIC_PLAN_ZH.md)。

| ID | 实验条件 | E7 观察（阶段纪要报告） | 603 对照 / 判断边界 |
|---|---|---|---|
| E7-01 | 纯 QWidget minimal + 原 CVideoPlayerUtility2 后端 | 报告通过：有画有声 | 报告通过；只证明该样本与该无 QGL 环境 |
| E7-02 | QGLWidget host 先初始化，再用原 MMF 后端 | 报告失败：黑屏有声 | 报告通过；QGL-first 是该对照中的触发条件 |
| E7-03 | 上述 QGL host，禁用 overlay | 报告失败：黑屏有声 | 报告通过；overlay 不是该失败的必要条件 |
| E7-04 | QGL host 在 MMF 前 hide | 报告失败：仍黑屏 | 报告通过；简单隐藏不能恢复 |
| E7-05 | 只链接 QtOpenGL，不创建 QGLWidget | 报告通过 | 未提供；不是仅链接该 DLL 就稳定触发 |
| E7-06 | 仅 QGLContext 对象，不 create/makeCurrent | 一次异常后复测正常 | 未提供；保留异常，不能写成无条件稳定通过 |
| E7-07 | QGLWidget 真初始化后 delete，再启动 MMF | 报告失败，3 次 | 未提供；删除 QGL 不足以恢复该旧路径 |
| E7-08 | MMF 已播放并可见，再创建/初始化 QGL | 报告通过：视频继续正常 | 未提供；不能扩展为任意初始化顺序 |
| E7-09 | MMF-first A：仅 NewL 后创建 QGL | 报告失败 | 未提供；仅有 player 对象不足 |
| E7-10 | MMF-first B：NewL、解析 RWindow、WS Flush，不 open | 报告失败 | 未提供；源码未调用 AddDisplayWindowL，不能称已提前绑定 display |
| E7-11 | MMF-first C：OpenFileL/OpenComplete 成功，尚未 Prepare | 报告失败 | 未提供；OpenComplete 已运行普通 display 配置，在 Prepare 前暂停，非纯 Open-only |
| E7-12 | standalone Qt Mobility：QGL 真初始化后 QMediaPlayer + QVideoWidget，完整本地 MP4 | 报告通过：有画有声 | 未提供；不等于真实 NanoVG 负载或产品通过 |
| E7-13 | 上述参照的 landscape / large-maximized 变体 | 报告通过：大面积横屏视频 | 未提供；逐变体包身份待补 |
| E7-14 | 上述参照的 true fullscreen / CBA 隐藏 | 未完成验证，底部系统栏仍在 | 不能将 maximized 写为真正全屏通过 |
| E7-15 | 完整 Qt Mobility 集成，真实 VideoPlayerWidget shell | 报告失败：UI 正常、黑屏有声；音频主观更流畅 | 最终替换版未提供回归；无音频性能测量 |
| E7-16 | Qt 集成完全不创建/显示/raise overlay | 报告失败：纯黑有声，无 UI | 不证明 overlay 在其他组合中无影响 |
| E7-17 | E7-16 基础上恢复默认 QVideoWidget 属性 | 报告失败：黑屏有声 | 旧属性不是这个失败的充分解释 |
| E7-18 | Qt 集成的真实 B 站/HTTP 输入 | 报告失败：黑屏有声 | 单独不能定位 source/controller |
| E7-19 | Qt 集成选择“下载后播放” | 报告失败：仍黑屏 | 网络流式不是全部问题；仍需核对实际本地输入 |
| E7-20 | CONTROL：真实 shell，强制参照本地 MP4 | 报告失败：黑屏有声 | 支持优先查 shell/绑定，但未证明已解码出有效视频帧 |
| E7-21 | MIRROR：延迟创建/绑定，试图复制参照时序 | 实验无效：黑屏、无声、无 UI；缺少媒体已启动证据 | 不计为 lifecycle 假说被证伪；先核对实际 setMedia/play |
### 独立 Player 封存反馈

| 实验 | 来源与结果 | 解释边界 |
|---|---|---|
| 启动与 LOAD / 返回修复候选 | `b9e1a8d` 归档矩阵记录最低 SDK 的双 EXE Debug/Release 编译、签名和包检查通过；启动日志已到子进程读取请求和横屏窗口 | 历史构建证据，不是本轮构建；日志没有提供确切机型/固件，不能外推 E7 |
| 最新设备反馈 | 同一归档矩阵记录用户报告“部分视频可播、部分黑屏、返回仍易闪退”；整体验收未通过 | 实装包身份、逐样本后端和新日志未核验，不能把可播样本写成 E7 Qt 硬解通过，也不能确定崩溃进程或 panic |

独立方案仅在 `codex/archive-independent-player-20260904` / `b9e1a8d` 保存，不在正式主线运行。
### 当前 E7 CODA 复验

以下为 2026-09-04～06 连续调试会话的新证据，与上方历史报告分开。
设备端点、原始日志和媒体仅留本地忽略目录；诊断包及符号留本地构建目录，不作为 Release。
下表保存逐项观察。已经撤回的尺寸推断不作为现行结论；当前归因见[总审计](../research/player/E7_BLACK_SCREEN_ROOT_CAUSE_AUDIT_ZH.md)，下一步只按诊断方案。

| ID / 条件 | 设备与输入 | 结果及证据边界 |
|---|---|---|
| 运行环境固定 | E7 / RM-626 / Belle Refresh `111.040.1511`；CODA 查询 Qt `4.8.0`、Qt Mobility `1.2.2`；本轮未更换运行库 | 构建仍用最低 `Symbian3Qt474` / GCCE 4.4.1；SDK 版本不能当成手机运行库版本。补丁开关和滑盖状态尚未单独核验，本轮未改变它们。 |
| 样本 A 身份 | 原有完整本地 `E:/test/test.mp4`，18,127,827 字节；SHA-256 `E74F8E6B18229977630D9364DA669C8DABAB15F966203FC3F165150379F33C1D` | 只读取回并解析：AVC High、声明 level 5.1、ref 4、reorder 3、DPB 4；编码 640×368、裁剪 640×360、8-bit 4:2:0，VUI 约 29.97 fps；AAC/mp4a 双声道 44.1 kHz。设备媒体时长 727,249 ms。未重封装或转码。 |
| C1-Solo `e7devvideomemorysolo1` / EFalse + 时钟 | E7 / RM-626 / Belle Refresh `111.040.1511`；当前 C1-Solo SIS；本地样本 A；MMF 与 FFmpeg 均禁用，固定竖屏入口；真实 `0x10204C21` DevVideo memory-output | `SelectDecoderL`、`SetInputFormatL`、真实 Header（100/6570015，640×360）、`ConfigureDecoderL`、5 项 output list、`SetOutputFormatL` 均返回 0；选中 YUV420 planar（index 2）。`SetVideoDestScreenL(EFalse)` 返回 0；首次采用 `CSystemClockSource + SynchronizeDecoding(ETrue)`，`MdvpoInitComplete=-44`，0 picture。日志：`.tmp/e7-coda/install-c1solo-portrait2-com4.jsonl`。这一次证明 decoder/header/configure 可达，不证明 memory picture 或显示。 |
| C1-Solo 配对 / 无时钟 + `SynchronizeDecoding(EFalse)` | 同一设备、样本、构建路线；仅改变 DevVideo 时钟同步设置 | Header/configure/output 与上次相同；跳过 `SetClockSource`、使用 `SynchronizeDecoding(EFalse)` 后仍 `MdvpoInitComplete=-44`，0 picture。时钟不是 `-44` 的充分触发项。日志：`.tmp/e7-coda/install-c1solo-nosync-com4.jsonl`。 |
| C1-Solo 配对 / `SetVideoDestScreenL(ETrue)` | 同一设备、样本、其余设置与上次相同；仅把 screen-destination flag 改为 true | `SetVideoDestScreenL(ETrue)` 同步返回 `-5`，未进入 Initialize；E7 固件不接受该替代契约。结合前两次 `EFalse → Initialize=-44`，当前 C1 memory-output 路线在 E7 上停止，不进入 C1-MMF retained；不把 `-44` 归因于黑屏显示层。日志：`.tmp/e7-coda/install-c1solo-screentrue-com4.jsonl`。 |
| C1-PP `e7devvideomemorypp1` / 唯一结构性补测 | E7 / RM-626 / Belle Refresh `111.040.1511`；同一样本 A；MMF 与 FFmpeg 均禁用；应用内同一 `CMMFDevVideoPlay` 会话动态选择 decoder `0x10204C21` 与 E7 实际 PP | `GetPostProcessorListL` 返回 1 个 PP；选中 `0x10273417`（accelerated/direct-display=true，28 个 source formats）。真实 Header/Configure 成功；decoder output 与 PP source 有 3 个共同 YUV 格式，选中 planar YUV，decoder `SetOutputFormatL`、PP `SetInputFormatL`、`SetPostProcessTypesL(PP,0)`、`SetBufferOptionsL` 与 `SetVideoDestScreenL(EFalse)` 均返回 0。PP optional `GetOutputFormatListL` 返回 0 项，但默认 memory 合约继续执行，`Initialize()` 实际发出，异步 `MdvpoInitComplete=-44`、0 picture，未进入 Start。C1 memory-output 正式关闭；direct-display 能力只作 C2/C3 情报，不重新开启已封存 Direct DevVideo/DSA 路径。最终日志：`.tmp/e7-coda/install-c1pp-default-memory-com4.jsonl`。 |
| C2 MMF graphics-surface `e7mmfsurface1` / E7 最终一次 | E7 / RM-626 / Belle Refresh `111.040.1511`；`Symbian3Qt474` GCCE Debug 诊断 SIS `NIKINIKI_1.2.0_debug_e7mmfsurface1.sis`；本地样本 A；现有 `CVideoPlayerUtility2`、controller、AAC/主时钟和原生 video host；不创建独立 Player、不启用 DevVideo probe | GCCE 构建 `sbs errors: 0`、warnings 33。`NATIVE_MMF_OPEN_COMPLETE 0`；不调用 `AddDisplayWindowL`，`Prepare` 返回 `-12017`，音视频轨均存在；`AddDisplayL(WsSession, displayId, handler)` 返回 0，随后 `Play` 和音频状态均正常。90 秒运行没有 `E7_MMF_SURFACE_CREATED`、surface 参数/移除、首帧或 renderer 提交计数；MMF post-processor 统计 `Received 0 / Displayed 0 / Skipped 0`。用户确认实机“无画面、有声音”，并停止后续测试。结论：graphics-surface 公开调用可进入但未交付 surface/视频帧，C2 失败关闭；不能证明黑屏仅由传统 display-window 造成，也不满足进入 C3 的正向条件。原始日志：`.tmp/e7-coda/install-c2-mmfsurface-com4-rerun.jsonl`。 |
| R0 冻结参照 / 两次新进程 | `c5f9ff9` 源码与旧构建 payload 核对一致；只更换过期签名后 CODA 安装至 E 盘；普通 QWidget、8×8 QGL、QMediaPlayer/QVideoWidget，同一样本 A | 用户分别确认竖屏、持续运动画面和声音；第二次补充“算上昨天已经三次正常”。本轮新增两次，历史一次，不计三次本轮配对。两次均有有效 QGL、Loaded/Buffered、音视频轨存在。原参照只打印最初五次 position，早期均为 0，持续可见性依据用户观察。无新编译或 Release 验收。 |
| R1 `e7qtminimal1` | 真实 NIKINIKI 主 UI/GLES 已初始化，固定物理竖屏 360×640，第二 QWidget 宿主/视频区域 360×554，无播放器 overlay，同一样本 A | 用户确认“竖屏，黑屏有声”。CODA 为 Loaded/Buffered、音视频轨 true，position 推进到约 44.9 秒，主 QGL 保持映射；采样 activeWindow 不等于视频宿主。证明失败已在应用内最小宿主出现，不能单独归因到 QGL、窗口激活或 surface。Debug GCCE 编译 0 errors、SIS 打包/重签和实装完成；普通 Release 未复验。 |
| R1B `e7qtactivehost1` | R1 基础上仅在媒体对象创建前主动激活视频宿主 | 用户报告测试仍“黑屏有声”。首轮主页准备超时，点击后进入媒体创建；设 activeWindow 当场返回宿主，但视频 native 化后采样再次不等于宿主。记录到 Loaded/Buffered、音视频轨 true，同时曾进入 Paused；正 position 未捕获，随后应用事件循环退出 0 / process kill 0，没有 panic 证据。因此只记用户黑屏观察和调用未保持，不能声称已经排除激活时序假说，也不把退出记为返回通过。Debug GCCE 编译 0 errors，SIS 打包/重签和实装完成。 |
| 共同 Qt 运行库警告 | R0 与 R1/R1B 均打印 `eglCreateEndpointNOK not found` | 该警告同时存在于成功参照，不能单独作为黑屏根因；也不能根据旧 SDK 源码推断设备使用哪条输出分支。 |
| R0W-S `sharedhost1` / 首次新进程 | 新窗口观测探针，普通 QWidget 同时包含 8×8 QGL 和视频；同一样本 A，Debug/CODA | 用户确认“竖屏，有画面，有声音”；日志为 Playing/Buffered、音视频轨 true，position 持续推进超过 39 秒。窗口记录在媒体创建前后及持续播放期间 `activeWindow()` 均为 null，视频仍可见，因此 activeWindow 为空不是 Qt 出画失败的充分条件。GCCE 0 errors / 4 warnings，SIS 打包、重签和 payload 核对完成；新观测代码本轮通过，尚不是配对重复或 Release 验收。 |
| R0W-T `separatehost1` / 首次新进程 | 与 R0W-S 共用源码，仅在小 QGL 初始化后创建第二个普通顶层 QWidget 作为视频宿主；两个顶层都保持可见 | 用户确认“竖屏，有画面，有声音”；日志 Playing/Buffered、音视频轨 true、position 超过 39 秒，activeWindow 仍为 null。这一次多顶层宿主没有复现 R1 黑屏；其单次因果强度见账本。GCCE 0 errors / 4 warnings，SIS 打包、重签和 payload 核对完成；尚未做重复配对或独立 Release 启动。 |
| R0W-L `largegl1` / 首次新进程 | 保持第二视频宿主、创建顺序和默认 QGL，仅把 8×8 子 QGL 扩大为 360×554；同一样本 A | 用户确认“竖屏，无画面，有声音”；日志 Playing/Buffered、音视频轨 true、position 超过 31 秒。这是本轮首个单因素成功/失败边界候选，需小/大 QGL 配对复测；尚不能区分窗口覆盖、合成和视频输出链，也不能据此宣称 decoder 无有效帧。GCCE 0 errors / 4 warnings，SIS 打包、重签和 payload 核对完成。 |
| R0W-T 第二次 / 从大 QGL 反向换回 | 重装首次通过的 `separatehost1` 同一包，重新启动进程，未重启手机或更换媒体 | 用户确认重新恢复“竖屏、有画面、有声音”；日志 Playing/Buffered、音视频轨 true、position 持续前进。支持尺寸分界可逆，仍需完成其余配对，不把一次反向复测写成最终机制证明。 |
| R0W-L 第二次 / 再换大 QGL | 重装首次失败的 `largegl1` 同一包，重新启动进程，同一样本 A | 用户再次确认“竖屏，无画面，有声音”；日志 Playing/Buffered、音视频轨 true、position 推进超过 47 秒。与两次小 QGL 的视频宿主/QVideoWidget 几何和 flags 一致，观测到的差异是 QGL 的 8×8 与 360×554。第二对结果一致，仍保留覆盖/输出机制的待判定边界。 |
| R0W-T 第三次 | 同一 `separatehost1` 包、同一样本，第三个新进程 | 用户确认“竖屏、有画面、有声音”；日志 Playing/Buffered、音视频轨 true、position 前进。小 QGL 已三次一致通过；该结论只适用于此诊断结构和样本。 |
| R0W-L 第三次 / 配对完成 | 同一 `largegl1` 包、同一样本，第三个新进程 | 用户确认仍“竖屏、无画面、有声音”；日志 Playing/Buffered、音视频轨 true、position 超过 17 秒。三对新进程结果为小 QGL 3/3 有画、大 QGL 3/3 黑屏，均有声，未重启设备、换媒体或运行库。尺寸是这组受控实验的稳定触发条件；覆盖、裁剪、合成及视频帧输出机制仍须下一层观测，尚未等同于正式主程序修复。 |
| R0W-H `halfgl1` | 第二宿主不变，QGL 位于左半部 180×554，首次 show/native 化前确定尺寸 | 用户确认“整幅正常，有画面，播放流畅，声音正常”；不是左黑右有画。日志 Playing/Buffered、音视频轨 true、position 持续前进。这不符合简单按 QGL 像素范围遮盖视频的预测；仍需区分完全覆盖宿主的窗口条件与尺寸/资源门槛，流畅仅为主观观察。GCCE 0 errors / 4 warnings，打包、重签和实装完成。 |
| R0W-G `gapgl1` | 第二宿主不变，QGL 359×554，仅右侧留下 1 像素空隙 | 用户确认“黑屏有声”；日志 Playing/Buffered、音视频轨 true、position 前进。留 1 像素未恢复，不能把完全相等的宿主/QGL rect 当作已证明根因；半宽通过也未排除尺寸/资源条件或底层对齐。GCCE 0 errors / 4 warnings，打包、重签和实装完成。 |
| R1C `e7qthalfmain1` | 真实 NIKINIKI 初始化 UI/GLES 后、第二视频宿主显示前，主 QGL resize 为 180×554 并保持映射；没有 R1B 的主动激活调用 | 用户确认“黑屏有声”；日志 Playing/Buffered、音视频轨 true、position 超过 40 秒，主 QGL 持续为 180×554、视频仍 360×554。最小探针的半宽结果未直接迁移；还存在初始大尺寸、主窗口类型、真实绘制和 GL 状态等差异，不能只凭最终 geometry 定位。Debug GCCE 0 errors / 35 warnings，SIS 打包、重签、payload 核对及实装完成，未做返回验收。 |
| R0W-I `latehalfgl1` | 最小子 QGL 先按 360×554 初始化，再缩为 180×554，显式 doneCurrent 后才创建第二视频宿主 | 用户确认“画面声音正常”；日志 Playing/Buffered、音视频轨 true、position 前进。显式 doneCurrent 后的快照为 null，但媒体创建前已因后续事件重新绑定为该 QGL context，仍能出画。因此先前的大尺寸初始化、以及媒体创建时 currentContext 为 QGL，分别都不足以解释 R1C；不能推广为任意 GL 绘制状态均无影响。GCCE 0 errors / 4 warnings，打包、重签和实装完成。 |
| R0W-Q `topgl1` | 初始顶层主窗口直接为 QGLWidget，showMaximized 初始化后缩为 180×554，再创建第二视频宿主 | 用户确认“画面声音正常”；日志主窗口与 QGL 为同一对象、无父控件，媒体 Playing/Buffered、音视频轨 true、position 前进。顶层 QGL 类型结合先大后缩半也不足以复现 R1C；主程序 fullscreen 往返、真实绘制和资源状态仍待拆分。GCCE 0 errors / 4 warnings，打包、重签和实装完成。 |
| R0W-F `fullcycle1` | 顶层 QGL 按主程序隐藏 retained panes、showFullScreen/setFocus，再 showMaximized 并缩半 | 用户确认“画面声音正常”。日志先确认物理竖屏下主窗口 360×640 fullscreen，再确认工作区及主窗口 360×554 maximized，最后半宽播放；Playing/Buffered、音视频轨 true、position 前进。该最小全屏往返不是 R1C 黑屏的充分条件，也不是视频真全屏或返回生命周期验收。GCCE 0 errors，打包、重签和实装完成。 |
| R0W-C `cleargl1` | R0W-F 基础上在 paintGL 执行主程序相同的背景色和 color/stencil clear | 用户确认“画面声音正常”；日志在 360×554、360×640 阶段实际清屏，GL error 0，之后半宽下 Playing/Buffered、音视频轨 true、position 超过 34 秒。简单 GL 清屏不足以复现 R1C；没有覆盖 NanoVG、字体/纹理或持续 UI 负载。CODA 多次断线期间的后续状态不作长播验收；重新接上日志时旧进程为 Stopped/InvalidMedia，原因未完整捕获，另保留该边界。GCCE 0 errors / 4 warnings，打包、重签和实装完成。 |
| R0W-N `nvginit1` / 首次新进程 | R0W-C 基础上链接产品同一份 NanoVG C/GLES2 backend，并以相同 flags 创建 context；尚无 NanoVG 绘制、字体或图片加载 | 用户确认“黑屏有声”；日志 `NVG_CREATED true 0`，主 QGL 最终 180×554、视频宿主 360×554，Playing/Buffered、音视频轨 true、position 至 9.615 秒。首次出现清屏正常而 NanoVG 初始化后黑屏的分界候选；新增链接、shader/初始纹理资源和 GL 状态尚未分别控制，不能直接写成 NanoVG 或显存根因。之后 CODA 断线，不作长播结论。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成；待反向与重复配对。 |
| R0W-C 第二次 / 从 NanoVG 反向换回 | 重装先前成功的同一 `cleargl1` 包，新进程、同一样本 A | 用户确认“画面、声音都正常”；日志 Playing/Buffered、音视频轨 true、position 超过 19 秒。清屏参照反向恢复；当前仍待同一 NanoVG 包复测，不能据一次初始化失败认定底层机制。 |
| R0W-N 第二次 / 同包复测 | 同一 `nvginit1` 包、同一样本 A，新进程 | 用户再次确认“黑屏有声”；日志初始化成功、Playing/Buffered、音视频轨 true、position 超过 33 秒。当时两对为清屏 2/2 有画、NanoVG 初始化 2/2 黑屏，均有声；这是跨包对照，后续默认包恢复已否定无条件触发，仍不是完整主程序修复。 |
| R0W-NC `nvgcontrol1` / 跳过初始化 | 保留 NanoVG 链接，以 `--skip-nvg-init` 启动新进程；窗口过程、清屏和样本 A 不变 | 用户确认“画面声音正常”；日志确认运行的是同二进制对照变体且输出 `NVG_INIT_SKIPPED`，Playing/Buffered、音视频轨 true、position 超过 37 秒。安装回执曾因 CODA 断线缺失，重连启动后的变体和分支日志确认新代码已运行；不将这次断线计作播放失败。GCCE 0 errors / 6 warnings，打包、重签和 payload 核对完成；等待同包默认初始化对照。 |
| R0W-NC 同包开启初始化 | 不重新安装，去掉跳过参数后启动新进程；同一已安装二进制和样本 A | 用户确认“黑屏有声”；日志 `NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 超过 49 秒。与上一行构成同包跳过/执行初始化对照，新增链接本身不足以解释该分界；在这一对运行中，实际初始化路径与结果相关。尚未区分 shader、纹理、同步或其他状态/分配副作用，不能将其直接写成 NanoVG 代码缺陷或显存不足。 |
| R0W-X `gltextures1` / 首次新进程 | 只创建并保留 NanoVG 初始化对应的 1×1、512×512 GL_LUMINANCE 纹理及相同参数，不创建 NanoVG context、shader、VBO 或绘制 | 用户确认“画面声音正常”；日志两张纹理 handle 非零、GL error 0、`LUMINANCE_TEXTURES_READY true`，Playing/Buffered、音视频轨 true、position 前进。该纹理组及其状态调用不足以单独触发黑屏；未分别改变两张纹理尺寸，不外推为所有纹理/资源大小都无关。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-CPU `nvgcpu1` / 首次新进程 | 产品同一 NanoVG core 创建 CPU context，空 backend 回调不执行任何 GLES 资源或绘制调用；窗口过程和样本 A 不变 | 用户确认“画面声音正常”；日志 `NVG_CPU_CONTEXT_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。NanoVG path cache、默认 state、fontstash/CPU atlas 初始化不足以单独触发黑屏；此观察只覆盖 CPU core 与空 backend，不覆盖真实 shader/VBO/glFinish。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-SA `nvgsmallatlas1` / 首次新进程 | 完整 NanoVG GLES2 backend 初始化、真实 shader/VBO/glFinish，初始 font atlas 和 dummy texture 均为 1×1；尚无 NanoVG 绘制 | 用户确认“画面声音正常”；日志 `NVG_ATLAS_DIMENSION 1`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。这一次完整 backend 小 atlas 初始化成功；默认 512 后续也成功，不能维持资源尺寸边界或排除状态相关的组合因素。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-A256 `nvgatlas256` / 首次新进程 | R0W-SA 只把初始 font atlas 改为 256×256；完整 GLES backend、窗口过程和样本 A 不变 | 用户确认“黑屏有声”；日志 `NVG_ATLAS_DIMENSION 256`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。该次失败成立；同包后续两次成功，原先据此建立的单调触发区间已撤回。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-A128 `nvgatlas128` / 首次新进程 | R0W-A256 只把初始 font atlas 降为 128×128；完整 GLES backend、窗口过程和样本 A 不变 | 用户确认“画面声音正常”；日志 `NVG_ATLAS_DIMENSION 128`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。只记本次初始化和可见性；后续反例已撤回尺寸边界，不用它选择产品 atlas。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-A192 `nvgatlas192` / 首次新进程 | R0W-A128 只把初始 font atlas 提高为 192×192；完整 GLES backend、窗口过程和样本 A 不变 | 用户确认“画面声音正常”；日志 `NVG_ATLAS_DIMENSION 192`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。尺寸边界已撤回。非 2 次幂 atlas 在无 repeat/mipmap 且不绘制的本轮创建成功，不外推为真实字体渲染通过。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-A224 `nvgatlas224` / 首次新进程 | R0W-A192 只把初始 font atlas 提高为 224×224；完整 GLES backend、窗口过程和样本 A 不变 | 用户确认“画面声音正常”；日志 `NVG_ATLAS_DIMENSION 224`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。尺寸边界已撤回。仍未执行字体或 NanoVG 绘制，不能把创建通过记成该 atlas 的完整 UI 能力。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-A240 `nvgatlas240` / 首次新进程 | R0W-A224 只把初始 font atlas 提高为 240×240；完整 GLES backend、窗口过程和样本 A 不变 | 用户确认“画面正常”；日志 `NVG_ATLAS_DIMENSION 240`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 超过 9 秒；此前同类轮次均有声，本轮用户未单独重复声音描述。只记这次有画；尺寸边界已撤回，仍不是正式 UI/字体通过。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-A255 `nvgatlas255` / 首次新进程 | R0W-A240 只把初始 font atlas 提高为 255×255；完整 GLES backend、窗口过程和样本 A 不变 | 用户确认“画面声音正常”；日志 `NVG_ATLAS_DIMENSION 255`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。与首次 256 的观察不同，但该差异不足以决定阈值或尺寸特异性；后续同包翻转已否定固定尺寸规律。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-A257 `nvgatlas257` / 首次新进程 | R0W-A255 只把初始 font atlas 提高为 257×257；完整 GLES backend、窗口过程和样本 A 不变 | 用户确认“声音画面正常”；日志 `NVG_ATLAS_DIMENSION 257`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。当时出现非单调结果；随后同一 256 也正常，256 特异性推断已撤回。GCCE 0 errors / 6 warnings，打包、重签、payload 核对和实装完成。 |
| R0W-A256 第二次 / 原包复测 | 重装首次黑屏的同一个 256×256 包，新进程、同一样本 A | 用户确认“这次画面声音正常”；日志仍为 `NVG_ATLAS_DIMENSION 256`、`NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 超过 9 秒。首次 256 失败未稳定复现，255/256/257 的尺寸特异性结论撤回；该次不能确定使结果变化的状态。 |
| R0W-A256 第三次 / 不重装新进程 | 保持同一个已安装 256×256 包，仅停止第二个进程并启动第三个；样本 A 不变 | 用户再次确认“画面声音正常”；日志同样初始化成功、Playing/Buffered、音视频轨 true、position 前进。256 为首次 1 次黑、随后 2 次正常，不能作为稳定尺寸触发点。 |
| R0W-N 默认 512 第三次 / 原包回测 | 重装此前连续两次黑屏的同一个 `nvginit1` 包；完整 backend、默认 512×512 atlas 和样本 A 不变 | 用户确认“画面声音正常”；日志 `NVG_CREATED true 0`、Playing/Buffered、音视频轨 true、position 前进。默认 512 从此前 2/2 黑屏变为正常，atlas 尺寸不能作为当前稳定根因；CODA 曾卡住/重连；OSID `36489 → 748` 位于首次 256 失败之前，并非首次失败与成功复测之间的已知重启点。关机位置 UNKNOWN。 |
| R1C 第二次 / 默认 512 正常后原包回测 | 停止最小探针后，重装此前 R1C 的同一个真实 NIKINIKI 诊断包；主 QGL 最终 180×554、第二视频宿主 360×554、样本 A 不变 | 用户确认“无画面，黑屏”；日志 UI 与 CJK 资源初始化完成，Playing/Buffered、音视频轨 true，position 超过 153 秒，主 QGL 持续映射。用户补充手机在 9:49 前关过机，但现有记录不足以把关机精确放入两轮之间，因此不据此归因。同一记录阶段出现默认 512 探针有画而真实 R1C 黑屏；atlas 尺寸不能单独解释，DebugSession 因果也未被证实。UI/CJK 标记须按各自时间排序，不能追认为进入视频前都已完成。 |
| R0W-Y `yuvinit1` / 首次新进程 | 在当前可出画的默认 512 NanoVG 探针中，先执行真实 `initializeYuvRenderer()` 同一 shader 编译/链接、location 查询和六个未分配像素存储的纹理参数设置；程序与纹理持续存活，窗口、媒体和样本 A 不变 | 用户确认“画面声音正常”。包内代码只在 YUV 与 NanoVG 初始化都成功后创建播放器，因此出画也确认两个初始化门已通过；CODA 在进程启动后断线，未采到本轮应用日志，不能记录具体 GL handle 或 position。该次 YUV 初始资源存在仍出画；未覆盖真实 YUV 帧上传、UI 资源使用或组合负载。GCCE 0 errors / 6 warnings，SIS 打包、重签、payload 核对和实装完成。 |
| R0W-P `uifont1` / 首次新进程 | 继承 R0W-Y，只加入产品同一未压缩内置首帧字体的注册与进程期驻留；不加载 UI PNG、不请求字形或执行 NanoVG 绘制，样本 A 不变 | 用户确认“画面声音正常”；日志 `YUV_INIT_READY true`、`NVG_CREATED true 0`、`UI_FONT_READY 0 1826432 0`，随后确认全屏往返、主 QGL 缩半、Playing/Buffered、音视频轨 true，position 至 11.645 秒。此字体注册在该条件下不足以触发黑屏，不能外推到字形栅格化/atlas 上传、PNG 或完整 CJK fallback 的组合。CODA 随后断线，不作长播或独立启动结论。GCCE `sbs errors: 0 / warnings: 7`，打包、重签、payload 核对和实装完成。 |
| 六次观察固定 R 包 / 构建与身份 | 完整 NIKINIKI 正式播放器，`e7sixr1` / `e7-six-r-v1`，UID `0xE000B100`；同一 EXE 支持 full/empty，样本 A 和既有强制 MMF 不变；本地冻结 SIS/EXE SHA-256 分别为 `318823800475215C8A100D223D57A5CC0B08CA791C30D612EDE8FD0CBA74493B` / `ADA73E000C5C1012187AF81E10CB24F3651824CAF84CB01B0D8D354350922F1A` | GCCE 0 errors / 34 warnings，SIS 打包与 payload 匹配；USB CODA 安装回执和每次 runtime identity 匹配。设备安全策略拒绝读回 `E:/sys/bin`，实装 EXE 完整哈希仍为 UNKNOWN。诊断包不作为 Release。 |
| 六次观察 #1～4 / R ABBA | 同一已安装 R EXE，四个新进程依次为 full / empty / empty / full；PID 746 / 769 / 807 / 832；固定样本 A、正式横屏窗口和 native MMF；未加入 sanitize | 用户四次均确认程序及播放器 UI 正常、视频无画面；声音依次正常 / 间歇卡顿 / 正常 / 正常。每次 identity、模式和目标 PID 匹配，正式 MMF AddDisplayWindow/configure 成功，音视频轨存在且 position 前进；empty 两次的首页 draw/calls/vertices/dirty/upload 均为 0，full 两次恢复真实绘制。ABBA 为黑/黑/黑/黑，降低“实际首页 draw 是黑屏必要条件”，但不证明全部 NanoVG/资源/状态无关；R 的 controller identity、自然首帧、renderer 和 surface submit 仍为 UNKNOWN。第 2 次另见 `MMF_RATE_ERROR -2147467262`；不据此证明音频卡顿原因。 |
| 六次观察 #5 / P-clear v1 | P `e7sixp1` / `e7-six-p-v1`，PID 898，预定 clear 校准 | 用户确认“没有进入播放器”。运行日志的 run/session 为空、`modeValid=0`，媒体未启动，故为 INCONCLUSIVE 且照契约消耗第 5 次。直接原因限于诊断初始化：未显式 start 的 `QElapsedTimer::isValid()` 被用作初始化 guard；不归因于视频链。 |
| 六次观察 #6 / P-clear v2 分支复验 | 仅把 P 的诊断初始化 guard 改为显式 boolean，并更新 identity 为 `e7-six-p-v2`；窗口、绘制、资源与媒体代码不变。归档 SIS/EXE SHA-256 分别为 `6DFD8BD926793CC6BF7962454E1F4FDACFEE65FE764AD75EF3DB21B3E8A96BD3` / `0FC0D0BED900466C5F8161DFD17543E788E1322D566324EBB68ADEB56961BD7B`；PID 939 | runtime identity/clear 有效，YUV/NanoVG512/UI font 资源门通过；clear 首帧无 NVG frame/draw，Playing/Buffered、音视频轨 true、position 前进。用户确认竖屏中横版视频画面完整、左右可见、上下留黑、声音正常且无播放器 UI，记 A。观察窗口与人工结论之后、正常退出期间出现 Paused→InvalidMedia / `ServiceMissingError Symbian:-12015`，不否定此前有效播放。六次预算已用尽，P-empty 未执行；因此没有 P clear/empty 配对，也未证明根因。 |

六次均经 USB CODA 启动，不能替代菜单独立启动；每轮正常退出后确认 PID 消失。原始日志和结果卡位于
本地忽略目录 `.tmp/e7-coda/six-observations/E7SIX-20260905-A/`，不发布端点或设备日志。

<a id="e7-minimal-backend-ab-20260906"></a>
### 最小 P 宿主 backend A/B 四次对照

本组使用同一个已安装诊断 EXE、同一样本 A、同一 minimal-P 外层宿主、主 QGL、资源和窗口过程；
每个新进程只按必填 runtime switch 创建一种媒体后端。A 是 `QMediaPlayer + QVideoWidget`；B 是
正式 `VideoPlaybackBackend + CVideoPlayerUtility2` 和普通 `AddDisplayWindowL` 路径。Qt 内部
`S60VideoWidget` child 与 B 的普通 native-video `QWidget/RWindow` 是不同对象，不能写成同一
`RWindow` 对照。配对期间只安装一次，未重新链接、重装或计划/观察到重启；boot identity/uptime 为
UNKNOWN。每轮人工确认后均由 CODA `Processes.terminate` 固定终止，下一轮前核对目标 PID 消失；
这不是应用正常退出，不能证明析构清理。USB CODA COM4 在新进程间重新连接。
四次都使用 E7 / RM-626 / Belle Refresh `111.040.1511` 和 `E:/test/test.mp4`（18,127,827 bytes）；
mediaSession 都为1，session 名都是 `backend-ab-observations`，且各新进程的事件序号都从1单调递增。
安装查询仍显示该诊断 UID 的版本 `0.1.0`，该字段不能区分新旧包；实际运行身份由每次 runtime
identity 核对。

| 观察 | 固定身份与前序 | 实际播放链与日志事实 | 人工结果与退出 |
|---|---|---|---|
| #1 A | `e7-backend-ab-v1` / `e7backendab1`，UID `0xE000B153`，PID 1613；前序为新诊断包一次安装后的首次启动 | host 与 `QVideoWidget` 均为 360×554、可见，Qt 内部 `S60VideoWidget` 在加载后可见；`setVideoOutput`、`setMedia(file URL)`、`play` 均已调用。Utility、controller、内部 NewL/Open/Prepare/真实绑定调用与返回码均 UNKNOWN。45秒主日志末 position 37565 ms、音视频 available、无错误；这些字段不冒充首帧证明 | 用户确认“有画面，播放正常，声音正常”。日志 `symbian/out/e7-backend-ab/e7-backend-ab-v1/obs01-A.jsonl`：11、161、183、194、197、212、218～228、258、396行。之后 CODA terminate；退出日志139、148～149行确认 PID 1613 停止 |
| #2 B | 同一安装，PID 1637；前序 #1 A 已由 CODA terminate，重连后进程列表为空 | native child/绑定对象 360×554、可见；Utility2 NewL 返回0，priority 0 / preference 3；shared `RFile` + resolver Open 返回0；controller 查询成功：`0x101F8514` Real Video Player / Real / v1；`AddDisplayWindowL` 与 geometry/rotation/scale 配置均返回0；OpenComplete 0，PrepareComplete -12017，音视频轨均存在，正式 Play 返回0。45秒末 position 37540 ms、无错误；AddDisplay/available/position 均不证明首帧 | 用户确认“无画面，除了上侧和底层系统栏外，全黑，声音正常”。主日志 `obs02-B.jsonl`：11、165、179～181、187～206、396～408、417～424、435～440、455～467、602行。之后 CODA terminate；退出日志196、204～205行确认停止 |
| #3 B | 同一安装，PID 1661；前序 #2 B 已由 CODA terminate，重连后进程列表为空 | 与 #2 相同的事件顺序、Utility/controller、Open/Prepare/绑定与 Play 返回；native child 是本进程的新对象。PrepareComplete -12017 后 Play 返回0；45秒末 position 37500 ms、无错误 | 用户再次确认同样的内容区全黑、系统栏可见、声音正常。主日志 `obs03-B.jsonl` 对应关键行为仍在205、396、399、403、417、420、422～424、435、438～440、455～467、602行。之后 CODA terminate；退出日志105、113～114行确认停止 |
| #4 A | 同一安装，PID 1686；前序 #3 B 已由 CODA terminate，重连后进程列表为空 | 与 #1 相同的 Qt child、输出设置和媒体调用顺序；45秒末 position 37655 ms、音视频 available、无错误；A 的 Utility/controller/内部 MMF 调用仍 UNKNOWN | 用户确认“画面正常，声音正常”。主日志 `obs04-A.jsonl`：11、161、183、194、197、212、218～228、258、396行。之后 CODA terminate；退出日志91、100～101行及最终 inspect 确认目标进程为空 |

本地冻结包为 `NIKINIKI_E7_BACKEND_AB_V1_debug.sis`；包内 runtime identity 四次均匹配。
CODA 对 `E:/sys/bin/nikiniki_e7_qt_window_probe.exe` 的读回请求返回 Code -21，因此手机实装 EXE
SHA-256 为 UNKNOWN。主日志 SHA-256 依次为 `FDCF4D92A1B3812A...`、`5E5805789C5824A0...`、
`D555D5F7B036CAB7...`、`F8FD5CABFEA94656...`，完整值只留本地 build manifest。B 中出现的
DevVideo/PP 信息来自 Utility 创建前的独立只读能力枚举；新 B 主日志没有正式 renderer 的
Received/Displayed/Skipped 计数。A 的 Qt 内部 PP 实例与计数为 UNKNOWN。

本组预算严格止于 A→B→B→A，没有第五次观察。方法、构建固定点和解释见
[重新评估](../research/player/E7_BLACK_SCREEN_REASSESSMENT_ZH.md)。

<a id="e7-bind-timing-observations-20260906"></a>
### 最小 P 宿主 Prepare 后绑定时机四次预算

本组仍为 E7 / RM-626 / Belle Refresh `111.040.1511`，CODA 查询 Qt 为 `4.8.0`；样本仍是
`E:/test/test.mp4`（18,127,827 bytes）。只安装一次
`NIKINIKI_E7_BIND_TIMING_V1_debug.sis`（1,762,168 bytes，SHA-256
`2F6C7BC3CE7A3DF9D99A8C190110F58DC3778507C2768FF35F7DF2E60F595D3A`），UID3
`0xE000B153`，冻结 EXE 为 1,922,568 bytes、SHA-256
`915E8B095B3BEBF40FC721EBE4E83A16BBC76B700957721939728555C3A19481`。安装日志第8～9行确认
安装成功及包名/UID/版本 `0.1.0`；该版本字段不能独立区分诊断包。手机 EXE 受保护无法读回，
所以实装 EXE SHA-256 为 UNKNOWN；有效运行的 runtime identity 均匹配
`package=ABC config=e7bindtiming1 build=e7-bind-timing-v1`。

同一 EXE 中 A 为 Qt/QVideoWidget 校准；B 为在 OpenComplete 绑定的 native 后端；C 为只把同一
`AddDisplayWindowL + configureDisplay` 整体移到可接受 PrepareComplete 后、Play 前。配对期间
未重新链接或重装，没有记录到手机重启；boot identity/uptime 仍为 UNKNOWN。每个有效启动都是
新进程、mediaSession=1、session=`bind-timing-observations`，事件序号从1递增。人工结果确认后用
CODA terminate；这不是应用正常退出。第3次在主机参数解析阶段即失败，没有设备进程，仍按约定
消费预算；实际计数序列为 A → C → B（无效启动）→ C，之后没有第五次。

| 观察 | 身份、前序与窗口/绑定对象 | 调用与运行事实 | 人工结果、退出与日志 |
|---|---|---|---|
| #1 A | PID 1776；一次安装后的首次启动。外层视频 host 360×554；`QVideoWidget` 360×554。Qt 内部 `S60VideoWidget` 初现时不可见，加载后为 360×554、可见；它与 native 模式 child/RWindow 不是同一对象 | `setVideoOutput/setMedia/play` 已调用；插件内部 Utility、controller、NewL/Open/Prepare 和真实绑定调用/返回仍 UNKNOWN。只读 `GetBackgroundSurface` 在 Qt child 上返回 error -1/null；末次 position 37620 ms，音视频 available、error 0，这些都不替代人工首帧 | 用户确认画面正常、声音正常。主日志 `symbian/out/e7-bind-timing/e7-bind-timing-v1/obs01-A.jsonl`：11、194、212～214、229、281、398行；SHA-256 `BB3B2F0EAD4B3F315E4B8D7C5462F7CBC8E4FFEC48603AA767C6142FA579FF52`。退出日志118～119行确认 terminate PID 1776；下一轮 inspect 第1、12～13行确认退出、进程为空且包身份未变 |
| #2 C | PID 1801；前序 #1 A 已 terminate，CODA 重连、未重装。独立 host 360×554；native child 360×554、可见，绑定到本进程 `RWindow` generation 1 | Utility2 NewL 返回0，priority 0 / preference 3；shared `RFile` + resolver Open 返回0；controller=`0x101F8514` Real Video Player / Real / v1。OpenComplete 0 后明确延后绑定；PrepareComplete `-12017`；随后 AddDisplayWindow 返回0，geometry/rotation/scale 均完成，音视频轨存在，Play 返回。背景 surface 查询在绑定前后均 error -1/null；末次 position 37540 ms、error 0 | 用户更正确认“无画面、声音正常”；未进一步确认局部黑或内容区全黑，范围为 UNKNOWN。主日志 `obs02-C.jsonl`：11、165、177、205～208、396、399～403、417、420～427、442～475、611行；SHA-256 `4275AE23FB998696285DABB0BCAC91F255DC9240C0CA2C5473D977A860E912BC`。退出日志407～408行确认 terminate PID 1801；下一 inspect 第1、11～12行确认退出/空进程/同包 |
| #3 B | 预定 run `BT3-B`；前序 #2 C 已 terminate，inspect 仍见同设备、同包且进程为空 | 主机调用把以 `--` 开头的值错误地作为分离参数传给 `--argument`，参数解析器在连接设备和发出启动请求前返回 `expected one argument`。没有设备进程、PID、媒体会话或设备日志，均为 UNKNOWN/不存在；不能用于判断 B 黑屏机制 | 记 `INVALID_START`，按契约消费一次且不补跑。记录 `obs03-B-host-launch-failure.txt`，SHA-256 `F1D02318F314C1AB7D64D2601253A95063472653AAC9A049B514296B41998E97`；画面/声音/退出均 NOT OBSERVED |
| #4 C | PID 1835；前序仍是 #2 C（#3 未创建进程），启动前 inspect 进程为空、同设备/同包，未重装。host/native child 和 `RWindow` generation 1 条件与 #2 相同 | 同一 Utility2、priority/preference、输入和 Real controller；OpenComplete 0 后延后绑定。本次 PrepareComplete 为 **0**；AddDisplayWindow 返回0，显示配置完成，音视频轨存在，Play 返回；背景 surface 查询仍 error -1/null。末次 position 37420 ms、error 0 | 用户在 2026-09-06 15:03 确认视频画面正常、声音正常，并指出视频窗口外背景变白；白色外框单列为窗口/覆盖现象，不改写为视频失败。主日志 `obs04-C.jsonl`：11、165、177、205～208、396、399～403、417、420～469、604～605行；SHA-256 `132F81FC76FBCE8422D44706AF4D01A2EB27599BCB2FAF0A07604772DCDA7E79`。退出日志289～290行确认 terminate PID 1835 |

本轮 C 的两次有效运行发生可见性翻转，并与 Prepare 回调结果不同同时出现：#2 为 `-12017`/无画，
#4 为0/有画。它是设备事实相关性，不证明 Prepare 错误导致黑屏，也不证明延后绑定修复；两次 C 的
首帧、renderer 和底层输出 surface 实例均 UNKNOWN。日志中的 DevVideo 列表/能力信息来自正式
Utility 创建前的独立枚举，不能归给 mediaSession=1 renderer。离线复核发现两份 C 日志均在
330～335行有枚举阶段 PP 零统计；失败 `obs02-C.jsonl` 另在429～434行出现第二组 PP 零统计，
位于 Prepare 调用返回（427行，应用 elapsed 2938 ms / 主机接收 3984 ms）与
PrepareComplete(-12017)（441～442行，应用6387 ms / 主机7406 ms）之间。成功 `obs04-C.jsonl`
的 Prepare 返回同在427行（应用2951 ms / 主机3937 ms），对应区间没有第二组统计，
PrepareComplete(0) 在435～436行（应用6432 ms / 主机7390 ms）。归一化比较到回调为止，除进程
元数据、这六行及回调 error 外没有其他消息序列差异；它只说明现有日志没有更早的可见分叉。
针对性回看前一包的两个 B，`obs02-B.jsonl` 与 `obs03-B.jsonl` 也都在442～447行出现第二组零统计，
随后454～455行为 `-12017`，两次人工结果均为黑屏有声。因此目前三次 native 黑屏 B/C 都有该标记，
成功 C 没有；这是小样本相关性，不是模块归属或因果证明。
第二组没有实例/PID/会话标识，不能自动归给正式 renderer，也不能称为全程播放累计零帧；其各行
只有主机接收 `ms`，没有应用 elapsed，主机接收次序也不能证明底层执行次序。这里只记录日志出现
阶段的差异，原始日志哈希复核与上表一致。
`UI_MEDIA_SAMPLE` 是主 QGL 的 clear/绘制统计，也不是 MMF PP
统计。`available`、position 与 `GetBackgroundSurface` 同样不作为首帧证据。

方法偏离和判定规则见[诊断方案](E7_BLACK_SCREEN_DIAGNOSTIC_PLAN_ZH.md#e7-bind-timing-20260906)；
解释与剩余缺口见
[重新评估](../research/player/E7_BLACK_SCREEN_REASSESSMENT_ZH.md#e7-bind-timing-result-20260906)。原始
JSONL、传输端点和敏感材料仅留本地忽略目录，不发布。

<a id="e7-c-lifecycle-pair-observations-20260906"></a>
### 冻结 C 的 cold/warm 生命周期四次配对

本组沿用上节已安装的 `e7bindtiming1` EXE/SIS、UID `0xE000B153`、模式 C 和样本 A；没有重新链接、
重装、换媒体或加入 surface/dummy 改动。四次 runtime identity 均为
`package=ABC config=e7bindtiming1 build=e7-bind-timing-v1 mode=C`，手机受保护 EXE 仍无法读回，
所以实际文件哈希为 **UNKNOWN**。用户在 cold-1 与 cold-2 前分别明确执行了手机重启并在 USB CODA
重连后通知；两次重启后的 inspect 都确认目标进程为空、包名/UID/版本与样本大小未变。CODA 没有
提供可复核 boot identity/uptime，故二者为 **UNKNOWN**。

四次均创建新进程、`session=e7-c-lifecycle-pair`、`mediaSession=1`，事件序号从1递增。窗口事实也
相同：物理屏 360×640、available 360×554；native child 为 360×554、global `(0,26)`、visible=1，
绑定本进程 generation 1 `RWindow`。Utility2 创建返回0，priority 0 / preference 3；输入为 shared
`RFile` + resolver；controller 查询为 `0x101F8514` Real Video Player / Real / v1。每次人工确认后均
使用 CODA terminate，并在下一次启动前或最终 inspect 中确认 PID 已消失；这不是应用正常退出。

| 观察 | 固定前序与身份 | Open → Prepare → 绑定 → Play | 人工结果、退出与本地日志 |
|---|---|---|---|
| #1 cold-1 | 明确重启后的首次且唯一媒体应用；PID 711 | OpenComplete 0；Prepare 调用返回为应用 3003 ms / 主机接收 4078 ms，PrepareComplete 0 为应用 7847 ms / 主机 8922 ms；AddDisplayWindow、geometry/rotation/scale 与 Play 均成功，音视频轨均存在，position 前进 | 用户确认“有运动画面，声音正常，视频画面完整，画面外为白色”。`obs01-cold1-C.jsonl` 关键行11、128～130、165～167、205～206、396、399～403、417、420、425～427、435～469、478；SHA-256 `FC4F2AC951DE731B5A6ADDFC9AAD7596AA46B3AEE70C1EDBE8F1670BC25DF630`。`obs01-exit.jsonl` 77、86～87行及 `pre-warm1-inspect.jsonl` 1、12行确认 terminate / exited / 空进程 |
| #2 warm-1 | #1 terminate、PID 消失并重连后立即启动，中间无其他媒体应用；PID 747 | OpenComplete 0；Prepare 返回为应用 3316 ms / 主机 4360 ms，PrepareComplete 0 为应用 6698 ms / 主机 7735 ms；后续绑定、配置、Play 均成功，position 前进 | 同样确认完整运动画面、声音正常、外背景白色。`obs02-warm1-C.jsonl` 同类关键行为在11、128～130、165～167、205～206、396、399～403、417、420、425～427、435～469、478行；SHA-256 `55A9328C06F74A48037BC023BE88DE602A307BFC3232982A38ACFA8A387E3A9E`。`obs02-exit.jsonl` 55、64～65行及重启前 inspect 第1、12行确认退出/空进程 |
| #3 cold-2 | 第二次明确重启后的首次且唯一媒体应用；PID 703 | OpenComplete 0；Prepare 返回为应用 3128 ms / 主机 4110 ms，PrepareComplete 0 为应用 7877 ms / 主机 8828 ms；后续绑定、配置、Play 均成功，position 前进 | 同样确认完整运动画面、声音正常、外背景白色。`obs03-cold2-C.jsonl` 同类关键行为在11、128～130、165～167、205～206、396、399～403、417、420、425～427、435～469、478行；SHA-256 `6A715DAE9EC15AB9B6DE83D695C4AA2E552C3E27C791E09EABAEA72A0F2EC65F`。`obs03-exit.jsonl` 238、247～248行及 `pre-warm2-inspect.jsonl` 1、12行确认退出/空进程 |
| #4 warm-2 | #3 terminate、PID 消失并重连后立即启动，中间无其他媒体应用；PID 745 | OpenComplete 0；Prepare 返回为应用 2979 ms / 主机 4016 ms，PrepareComplete 0 为应用 6438 ms / 主机 7469 ms；后续绑定、配置、Play 均成功，position 前进 | 同样确认完整运动画面、声音正常、外背景白色。`obs04-warm2-C.jsonl` 同类关键行为在11、128～130、165～167、205～206、396、399～403、417、420、425～427、435～469、478行；SHA-256 `511F4E76D538F6592E78400BF1C1BDDAC5E0819C8627700DF3433A0880353A7D`。`obs04-exit.jsonl` 210、219～220行及最终 inspect 第1、12行确认退出/空进程 |

每份主日志仅在正式 Utility 创建前的枚举阶段出现一组无实例标识的
`Statistics of Post Processor`（均从330行开始）；四份在 Prepare 调用返回至 PrepareComplete 的
区间都没有第二组。该字段仍不能归属正式 renderer，也不是首帧证明。四次人工结果与 Prepare 结果
完全相同；这是本组设备事实，不抹去上节同一 C 曾出现 `-12017`/黑屏的既有观察。四次预算至此
耗尽，没有第五次启动。方法解释与下一项最小建议见
[重新评估](../research/player/E7_BLACK_SCREEN_REASSESSMENT_ZH.md#e7-c-lifecycle-pair-result-20260906)。

#### 用户提供的 Mobility 插件候选副本

本项只做主机静态读取，没有安装、替换或启动手机程序，也不增加真机观察。用户提供了一个称为
来自 E7 的 `qtmultimediakit_mmfengine.dll` 副本；本地接收文件为 59,797 bytes，SHA-256
`44EE2383965365B40D35CEE9EB0FB8D5C9A7A18F0343533A3CE953C87B8D86B4`。E32 header 显示 ARMv5、
SID/UID3 `0x2002AC76`、非 debug、BYTEPAIR 压缩、26 个 DLL 引用和2个导出；它链接
`QtMultimediaKit{00010202}`，并导入 EGL、GLESv2、OpenVG、31项 `mediaclientvideo` 与16项 `ws32`。

这些是**所提供文件本身的 FACT**。CODA 先前不能从受保护的手机路径读回文件，因此该副本是否
就是本轮 A 进程实际加载的 ROM 二进制、是否未经中间替换，仍为 **UNKNOWN**。原始二进制和完整
工具输出只留在本地忽略目录，不进入产品或发布包；接口含义及对黑屏解释的影响见
[重新评估](../research/player/E7_BLACK_SCREEN_REASSESSMENT_ZH.md#e7-mobility-binary-match-20260906)。

<a id="e7-full-app-compat-first-20260907"></a>
### 完整应用实验兼容模式首次反馈

用户反馈开启兼容模式后视频仍黑屏。原始日志为本地忽略目录
`.tmp/e7-coda/full-app-compat-20260907/full-app-compat-live.jsonl`，SHA-256
`7CDF3D390CF06731080A6EF0DB4028239BF8769C0910392A6D2F14E18D8A2CF2`。
日志22～25行标识 CODA 启动的 `wiliwili_symbian.exe`、UID `0xE000B100`、PID 2211；
此次是 E7 兼容任务的用户反馈，但本文件未独立核实固件/运行库及实装 EXE 哈希，不回填为 N8 结果。

| 阶段 | 日志事实与定位 |
|---|---|
| 开关 | 87行上层 session 1 enabled/locked=true；146行 MMF session 1 enabled=true、bind-phase=prepare-complete。90/141行 active=false 属于 session 0 的待生效偏好，不是播放时开关失效 |
| 方向与界面 | 99～115行物理640×360门槛、横屏显示、overlay 首次绘制及 native ready 均可见；这是窗口流程证据，不是视频首帧 |
| 输入 | 96行 playback=1 / decoder=0 / live=false；123行 AVC解析尺寸640×360，134～135行真实header接受、路由MMF。98行页面元数据1920×1080不代替码流尺寸。137～138行启动progressive本地下载；342行在8,473,606 bytes门槛打开共享缓存，343行下载完成8,937,888 bytes，早于344行OpenComplete。不是原18,127,827 bytes样本A；本次媒体SHA未知 |
| MMF | 335～340行NewL priority0/preference3、shared RFile、Open同步返回0；344行OpenComplete0，345行首次绑定延后；346～347行Prepare发出/返回。未记录实际controller身份 |
| Prepare分叉 | 271～276行为独立枚举PP零统计；348～353行在Prepare期间出现第二组无实例标识PP零统计，354行PrepareComplete=-12017。第二组来源及是否属于正式renderer仍未知 |
| 绑定与播放 | 355～366行回调后AddDisplayWindow、rotation/scale/extent/clip均返回0，display/geometry=true；367行音视频轨查询均true，373～376行Play调用完成、音频enabled=true。没有同会话视频帧/position统计；音频enabled不等于人工听到声音，本次人工声音结果未提供 |
| 结束 | 主机接收时间从Play标记到开始退出约7.7秒；381～395行解绑、媒体关闭、恢复物理竖屏/fullscreen及对象保留。不能按长播或再次进入通过记录；后续stop-after-capture日志确认目标进程为空 |

判定：兼容行为确实执行，本次未恢复画面；没有开关关闭或相同本地样本的完整应用对照，不能据此
归因增长文件、preflight、横屏或overlay中的任一项。完整应用窗口、媒体/输入、初始化过程均与最小C不同。

<a id="e7-full-app-qt-same-media-20260907"></a>
### 完整应用 Qt 与冻结最小 Qt 同媒体对照

本组使用 E7 / RM-626 / Belle Refresh `111.040.1511`，CODA 查询 Qt 为 `4.8.0`。两侧固定读取本页
唯一记录的完整样本 A：`E:/test/test.mp4`，18,127,827 bytes，SHA-256
`E74F8E6B18229977630D9364DA669C8DABAB15F966203FC3F165150379F33C1D`；没有下载、增长写入、
重封装或换媒体。冻结参照为已有成功证据的 `e7bindtiming1` A，UID `0xE000B153`，签名 SIS 为
1,762,168 bytes、SHA-256
`2F6C7BC3CE7A3DF9D99A8C190110F58DC3778507C2768FF35F7DF2E60F595D3A`，payload EXE 为
1,922,568 bytes、SHA-256
`915E8B095B3BEBF40FC721EBE4E83A16BBC76B700957721939728555C3A19481`；没有重编译、改源码或换
运行库。先前本节所写 `2F6C…E7FC…` / `915E…50CD…` 不匹配原件或原冻结 manifest，且把 EXE
大小误作 SIS 大小，现定性为记录转写错误，不是已证明的重签或代码变化。完整应用诊断为
`e7fullappqt1`，UID `0xE000B100`，SIS SHA-256
`39DF195E77B897D1950C9D576DED7A31FDE147A6C2986C400CE10803B286F076`，真实首页进入正式
`VideoPlayerWidget`，固定当前 Qt 后端、全硬解和完整 file URL。该诊断跳过 preflight，明确不同于
普通自动点播；它不写用户设置、不启用 retry。两个 UID 共存，开始前只安装一次完整应用包。

顺序严格为 A → 完整 Qt → 完整 Qt → A。每次为新进程；前一进程由 CODA terminate，下一次启动
回执确认旧 PID 已退出，间隔至少15秒；期间未重装、换媒体或运行其他媒体应用。原契约要求 Play 后
固定60秒，但 client 的 `--seconds 60` 从进程启动计时，四份主日志从 Play 返回到
`capture_complete` 实际约58.0 / 56.9 / 57.0 / 57.9秒。这是协议偏差；四次均已占用，没有第5次。
CODA terminate 也不是产品正常返回/持久复用验收。

| 次数/模式 | 运行身份与公开层事实 | 人工结果与证据 |
|---|---|---|
| #1 冻结 A | PID 2621；identity、样本大小/哈希匹配。QMediaPlayer 进入 Playing/Buffered，音视频 available、error 0，position 最大51,865 ms；Qt 内部 Utility/controller/Open/Prepare/绑定、自然首帧均 UNKNOWN | 用户确认连续运动画面、声音正常、画面完整、背景为正常黑色。`obs01-reference-A.jsonl` 11、217、224～228、284～290、440、446行；SHA-256 `2E8892EB94E0D199A64D08184F03D8D179210AB62929BE12A908D51B92323E72` |
| #2 完整 Qt | PID 2645；identity、样本匹配；真实640×360横屏、主QGL、正式视频 host、overlay/controls 保留。公开 QVideoWidget 及加载后的内部 S60VideoWidget 均可见；setVideoOutput/setMedia/play 返回，进入 Playing/Buffered，音视频 available、error 0。约10秒前的已记录 position 均为0；之后因 logger 只按 state/status/error/available 变化输出而为 UNKNOWN。Prepare/首帧/实际私有 surface 分支 UNKNOWN | 用户确认无画面、声音正常、播放器 UI 正常；未把 UI 之外背景另行判作 surface 事实。`obs02-full-app-qt.jsonl` 20、69、170～185、197～202行；SHA-256 `D95D307C4A6A85BC7128ACFB082091B2FC9B5C8C211F711D2C98AEB4B80A3C0B` |
| #3 完整 Qt | PID 2666；对象、窗口、调用顺序和公开状态复现 #2；约10秒前的已记录 position 均为0，之后 UNKNOWN；Prepare/首帧/私有分支仍 UNKNOWN | 用户再次确认无画面、声音正常、播放器 UI 正常。`obs03-full-app-qt.jsonl` 18、67、168～183、195～200行；SHA-256 `55642C1FF02B78A69490B10D1A91BFDB414D6996618E90BB48C4A4BF057381EF` |
| #4 冻结 A | PID 2687；身份/样本匹配；Playing/Buffered、音视频 available、error 0，position 最大51,765 ms；私有阶段仍 UNKNOWN | 用户确认连续运动画面、声音正常、画面完整、背景为正常黑色。`obs04-reference-A.jsonl` 17、223、230～234、287～296、446、452行；SHA-256 `1823A918DFCA7BBB0D6F7A815795CCD6AC10158039B2ACC655A7A9197F1C67C4` |

本组 A 为2/2可见且未翻转，完整应用为2/2黑屏。最早可证明的运行分界位于公开 Play 请求返回后、
约8～10秒时首次已记录 position 推进/人工可见帧之前：参照已推进，完整应用两次的同期快照仍为0；
更晚 position 为 UNKNOWN。position 0 只定位公开
会话边界，不证明零有效帧；声音正常也不能反推视频 Prepare 成功。完整应用两次各出现一组 Prepare
时间附近的无归属 PP 0/0/0/0 统计，没有实例/PID/会话标识，不能归给 renderer 或充当零帧证据。
因此当时不归因 overlay、QGL、surface 或驱动；该阶段后续取证映射见
[Qt 内部会话观测映射](../research/player/E7_QT_INTERNAL_SESSION_OBSERVATION_ZH.md)。

<a id="e7-qt-internal-session-20260907"></a>
### Qt 内部会话 A/完整/完整/A 对照

本组沿用上一节同一设备、两包和完整样本 A，不重装、不换媒体、无下载/preflight。实际加载
`qtmultimediakit_mmfengine.dll` 四次均在 `0x7BA40000`，设备内 prepare/apply/play fingerprint
全部匹配主机候选；每次在内部 MMF Play 后观察60秒并由 CODA terminate。正式四次之前的 Python
依赖错误、PID 2761 寄存器命名错误和 PID 2910 `modeValid=0` 均未形成有效媒体播放；依用户明确
约定只记录、不计四次预算。相邻有效启动间隔均超过15秒。

| 次数/模式 | 实例关联内部事实 | 人工结果 |
|---|---|---|
| #1 冻结 A / PID 2787 | session `0x2A32DE8`、utility `0x803B28`；`MvpuoPrepareComplete(0)`；同 session 的 pending `3→7→0`，真实 window `0→0x85FE98`，apply 前 TRAP error 0，随后命中 MMF Play；四点暂停15/15/78/15 ms | 画面正常、声音流畅、视频外背景黑色；完整性未单独确认 |
| #2 完整 Qt / PID 2916 | session `0x4D7EEE0`、utility `0x969434`；`MvpuoPrepareComplete(-12017)`；pending `3→7→0`，真实 window `0→0x969040`，TRAP error 0，随后 Play；暂停32/16/15/16 ms | 无画面、声音正常、播放器 UI 正常 |
| #3 完整 Qt / PID 2933 | session `0x4D7EEC8`、utility `0x969434`；`MvpuoPrepareComplete(-12017)`；pending `3→7→0`，真实 window `0→0x969040`，TRAP error 0，随后 Play；暂停16/15/16/16 ms | 无画面、声音正常、播放器 UI 正常 |
| #4 冻结 A / PID 2950 | session `0x2A32DE8`、utility `0x803B28`；`MvpuoPrepareComplete(-12017)`；pending `3→7→0`，真实 window `0→0x85FE98`，TRAP error 0，随后 Play；暂停15/15/15/15 ms | 无画面、声音正常、背景黑色 |

唯一出画运行的 Prepare error 为0，三个黑屏运行均为实例归属明确的 `-12017`；这是本轮最早的
结果相关分界。末次冻结 A 同模式翻为黑屏，故停止完整应用结构性归因：不能据此证明
`-12017` 是原因，也不能宣布 output/window、overlay、QGL 或重复 `setVideoOutput` 是根因。当前
只进入实际 `mediaclientvideo.dll` / controller 的原始
`KMMFEventCategoryVideoPrepareComplete (0x101F7F86)` 及此前初始化/清理取证；缺少匹配 ROM
二进制或符号映射时停止猜地址。原始日志与逐文件 SHA-256 只保留在本地忽略目录，完整说明见
[Qt 内部会话观测映射](../research/player/E7_QT_INTERNAL_SESSION_OBSERVATION_ZH.md)。

<a id="e7-mediaclient-code-read-20260907"></a>
### E7 MediaClientVideo 首次运行代码读取

用户授权的四进程总上限中只执行 #1，目的仅为代码获取，不作为播放对照。冻结 A 新进程
PID `34779` 在实际 `Z:\sys\bin\MediaClientVideo.dll` 加载事件处给出
`CodeAddress=0x809E1338`、`DataAddress=0x00400000`，但没有 `CodeSize`。该 code base 恰为离线
E7 core 候选的 `TRomEntry.iAddressLin=0x809E12C0` 加 `0x78`；这与目标 ABI 的完整
`TRomImageHeader` 大小一致，只支持结构关联，不证明整套 ROM 逐字节一致。

脚本随后在同一上下文只读候选 header 的首32 bytes，CODA `Memory.get` 返回
`functionality is not supported`、AltCode `-5`，没有返回数据。因此没有 UID/header CodeAddress/
CodeSize 校验，也没有读取 code；脚本未退回 `DataAddress-CodeAddress` 或 ROM entry size，立即
terminate 并确认 PID 列表为空。没有设置断点、没有进入 Play；剩余 #2～#4 未启动。原始日志仅在
本地忽略目录，4,729 bytes，SHA-256
`3EA25875F1EAB5CC3FA8B941A0F6753A5B5DAA783731623627FA2FD5B1113509`。此后没有新增设备启动；用户
提供的目标手机逻辑文件已在主机侧恢复出可靠 E7 Prepare event handler 映射；在这个代码读取步骤
结束时原预算的剩余三次尚未使用，后续用户已另行改为四份有价值数据，见下节。
该静态成果及下一 raw-event 契约不改写本节设备结果，完整边界见
[MMF Prepare 错误来源](../research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。

首次 raw-event 脚本启动产生 PID `35466`，但漏传冻结包必填的 `--e7-backend=A`。应用明确记录
`mode=UNKNOWN / modeValid=0`，没有创建媒体、没有加载 Qt MMF 模块，也没有设置断点；约35秒等待
第二模块超时后由脚本 terminate，进程查询为空。该次只证明参数门失败，不产生 Prepare、画面或
声音事实。用户随后明确将工具/参数失败只记录、不计入所需四份有价值数据。脚本已在主机修正并
增加显式参数测试。原始日志位于忽略目录 `.tmp/e7-mediaclient-runtime/run02/coda.jsonl`，41,420 bytes，
SHA-256 `7E07F3B43C4C6D459E9B43805BA2A71FE50772DC8ED74530E2F046464AA77492`。

修正参数后的 setup-2 为 PID `35479`，明确进入 `mode=A / modeValid=1`，并捕获实际
`MediaClientVideo.dll` / Qt MMF 模块基址 `0x809E1338/0x7B320000`。但对已映射
`MediaClientVideo+0x763A` 的首个4-byte code fingerprint执行 `Memory.get` 时，CODA仍返回
`functionality is not supported / AltCode -5`。脚本依门停止，未设置断点、未到 Prepare或Play，
terminate后进程查询为空。该能力失败同样不产生画面/声音事实，也按用户最新口径不计四份有价值
数据；截至setup-2，V1～V4仍为0/4。日志41,244 bytes，SHA-256
`3F39077D5157D0CF03643D178AC5BF137DF39A425995B682F0DAFB8857B3620E`。

第三个setup进程PID `35491`使用用户授权的同机文件派生身份门：实际模块路径/base和Qt两块运行
fingerprint均通过，但CODA拒绝在ROM地址`0x809E8972`设置breakpoint，仍返回NotSupported/-5；未到
Prepare，terminate并确认退出。它同样只记工具能力失败，不计所需四份数据。

<a id="e7-mediaclient-raw-prepare-20260908"></a>
### E7 MediaClientVideo raw Prepare 四次观察

用户随后明确授权以同机复制文件的header code base、同次模块路径/base、Qt运行fingerprint和回调
现场语义/对象关系作为替代身份门；本文不把该门写成运行MediaClient代码逐字节匹配。由于CODA不能
在ROM下断点，四次都只在已验证可用的Qt Prepare回调入口暂停：目标E7 handler在observer调用前把
原始event指针保存在callee-saved `r5`，返回LR应为`MediaClientVideo+0x762B`。脚本读取`r5`所指
12-byte event，并要求UID/error、LR、utility/body/observer/session及后续Play关系全部成立。

四次仍使用冻结`e7bindtiming1` A、`E:/test/test.mp4`、同一安装包，不重装、不换媒体；每次为新
进程，Play后固定观察60秒并由CODA terminate，进程退出均确认。用户最后统一回复四次“画面、声音、
背景均正常，无播放器ui”；按其此前说明记为V1～V4共同人工结果。无播放器UI是该冻结最小A的预期
宿主差异，不作为失败。

| 次数 | PID / 对象 | raw Prepare与调用现场 | Play、人工与退出 |
|---|---|---|---|
| V1 | 35507；session `0x02A32DA0`，utility `0x00803B28` | event `0x00435C70`，UID `0x101F7F86`，error 0；LR `0x809E8963`匹配；observer=`session+0x50`；暂停15 ms | 同session Play，暂停62 ms；画面/声音/背景正常，无UI；约60秒末仍Playing/Buffered且position前进；退出确认 |
| V2 | 35527；session/utility同V1 | 同一event地址、UID、error 0和LR；对象关系通过；暂停16 ms | 同session Play，暂停63 ms；人工结果同上；退出确认 |
| V3 | 35547；session `0x02A32D50`，utility `0x00803B28` | 同一event地址、UID、error 0和LR；对象关系通过；暂停16 ms | 同session Play，暂停16 ms；人工结果同上；退出确认 |
| V4 | 35568；session `0x02A32BC8`，utility `0x00803AF8` | 同一event地址、UID、error 0和LR；对象关系通过；暂停31 ms | 同session Play，暂停31 ms；人工结果同上；退出确认 |

四次MediaClient/Qt模块base均为`0x809E1338/0x7B320000`，Qt fingerprint匹配；MediaClient运行代码
仍不可读，故身份限制未改变。四份本地忽略日志的大小/SHA-256依次为
`127352 / 26AC375E36D045F7FA1ED05A99ACBBF9129AB995818AD687F199C065A30BE935`、
`127349 / F2E763AD8963D46D23358AFFFC6ED0A452628EA7F66DFB284B71060BDF4D5C5A`、
`125995 / 2C6CF9CEC67F12FC3C319ECC4709A62A3465A6B2D63FFD74037096FA49C0A2AD`、
`126818 / 8083EFDAB9E9F95B395A97996FAD4EFB9108A194EC00A85ACDC9D59C10137F86`。

离线回看上一节内部会话四次保存的Qt Prepare入口寄存器，LR也全为`0x809E8963`，对应参数为
`0/-12017/-12017/-12017`；E7目标指令在该调用点明确执行`r1=[r5+4]`。因此旧三个黑屏会话的
`-12017`在进入MediaClientVideo时已经存在，不是utility转换。另一个稀疏但有区分力的运行事实是：
旧成功会话与本节四个成功会话均出现`HLX_MDF_VIDEO_SERV*`线程；旧三个`-12017`/黑屏会话均没有，
但八次都有`MMFControllerProxyServer-*`。这把最早已观察分界推进到controller proxy创建/启动Helix
视频服务之前或该步骤本身；线程名尚不提供原始返回码，也未把静态implementation UID
`0x101F8514`与当前utility/controller对象动态绑定。下一唯一取证点见
[MMF Prepare错误来源](../research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)，本轮4/4预算已结束。

<a id="e7-hx-prepare-boundary-20260908"></a>
### E7 Hx Prepare原始结果四次观察

用户另行授权冻结`e7bindtiming1` A的四份有价值内部数据；均使用样本A，并在首个Hx Prepare事件
处停止和terminate，因此本组不作人工首帧判断。实际加载的`HxMmfCtrl.dll` base四次均为
`0x7AF00000`，所用短code fingerprint均匹配同机复制文件。

| 次数 | PID | Hx `SendEvent(event=2)` | 条件观测 | 退出 |
|---|---:|---|---|---|
| H1 | 35694 | `r2=0x00040024`，LR=`Hx+0x40AC`；同次Qt event/error为`0x101F7F86/-12017` | controller vtable及`+0x80=0x00040024`匹配；未预置OnError；未见Helix视频服务线程 | 确认 |
| H2 | 35712 | `r2=0`，LR=`Hx+0x40AC` | `OnError+0x3A98`零命中；出现`HLX_MDF_VIDEO_SERV*` | 确认 |
| H3 | 35743 | `r2=0x00040024`，LR=`Hx+0x40AC` | `OnError+0x3A98`零命中；未出现Helix视频服务线程 | 确认 |
| H4 | 35761 | `r2=0`，LR=`Hx+0x40AC` | `OnError+0x3A98/+0x3E10`均零命中；出现Helix视频服务线程 | 确认 |

这动态证明至少一个真实失败中，Hx controller的聚合结果已经是`0x00040024`，经该E7 DLL的映射表
成为`-12017`并到达Qt；utility转换与Qt窗口不是此错误的来源。`0x00040024`仍是汇总结果而非底层
初始化错误。H3主`OnError+0x3A98`未命中，但不能仅凭此排除执行；调整接口`+0x3E10`在H4的code
匹配且安装获接受，没有实际命中，`getStatus`不可用，尚缺有效失败路径观察。另有PID 35730在Prepare前自行退出，以及两次
连接创建前的Python依赖差错，均按用户口径只记录、不计四份数据。该阶段后续点随后已经执行；
日志摘要和当前唯一方向见
[MMF Prepare错误来源](../research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。

随后为捕获调整接口的失败分支，又按预先声明上限执行三个新进程F1/F2/F3（PID
35823/35842/35860）。三次均匹配同机Hx四个code fingerprint，在同一controller关联下得到
`SendEvent(event=2, r2=0)`、LR=`Hx+0x40AC`，主/调整接口`OnError`均零命中，并均出现
`HLX_MDF_VIDEO_SERV*`线程；三次均terminate并确认退出。用户当时无法回报屏幕，因此这三项只记
内部Prepare成功分支，不补写可见性。没有捕获新的`0x00040024`，所以调整接口在失败分支的作用仍
UNKNOWN；达到三次上限后停止设备启动。逐次暂停、日志摘要、有限writer复核及CODA不支持数据
watchpoint的限制见同一[错误来源报告](../research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。

用户再次授权同一冻结A链的三个有界进程F4/F5/F6（PID 35925/35944/35963）。三次均在同次
`HxMmfCtrl base=0x7AF00000`匹配四个fingerprint，也在
`hxmedpltfm base=0x7CAC0000`匹配`CreateEngine+0x46F8` fingerprint；Prepare controller均为
`0x1E300678`，`SendEvent(event=2,r2=0)`、LR=`Hx+0x40AC`，两处OnError零命中并出现Helix视频
服务线程。三次均terminate并确认退出，画面/声音因用户不在现场为UNKNOWN。没有捕获失败分支，
不能排除OnError；日志大小/摘要见错误来源报告。

随后获准把同一观测切换到完整应用`e7fullappqt1`，最多三个到达Prepare的有效进程。两个setup
PID 35984/35995均正确匹配FULL运行身份和完整样本A，但横屏只到available `640x284`，日志physical
（Qt `screenGeometry()`快照）持续`360x640`，约6秒后产品自身`PLAYER_NATIVE_ORIENTATION_TIMEOUT stage 2`并park会话；两次都
未加载Hx、未设Hx断点、未到MMF Prepare，画面/声音为UNKNOWN。脚本随后terminate并确认退出。
按当时契约它们不计有效上限，当时为0/3；相同前置阻塞连续复现且用户不在现场，当时停止继续setup。
两份忽略日志为`32206 / D5121C4327B044D4EA0E5F2DBFEE1939BE0778BF62F119CB14C0AD2D72DBF630`、
`32192 / BB66207EBFC25DDE232D14228975C7A6299006101D0592405F8576E84291D164` bytes/SHA-256。

<a id="e7-hx-timed-20260908"></a>
### E7 完整Qt限时错误链取证（香港时间09-08）

本段最初授权取消此前三次上限，允许在10:00前结果驱动续查，独占CODA与手机。用户随后因无法现场
解锁，明确暂停所有新增设备进程，10:00前转为主机离线分析；不再查询CODA或重复横屏阻塞启动。
已有目标已确认退出。只用既有`e7fullappqt1`完整Qt诊断及本页完整样本A；画面、声音、背景全部UNKNOWN。
本组终点改为首个Hx Prepare SendEvent或明确pre-MMF中止，不是旧60秒播放对照，不作验收。

08:38左右只读inspect：无在运行的目标，固件仍RM-626/111.040.1511，包UID/版本为
`0xE000B100/1.2.0`，样本fstat大小仍18127827；该查询不重新证明包payload或媒体SHA。
08:41左右CODA `getHalInfo([])`返回`EDisplayState=0`、`EDisplayXPixels/YPixels=360/640`。
SDK将DisplayState 0定义为屏幕关闭；不能据此命名锁屏所有者，也不能替代人工画面观察。

| 记录 / 香港启动时间 | PID / 参数 | 可证明的终点 | 暂停、退出与错误链 |
|---|---|---|---|
| FULL-timed-1 / 08:43:19 | 36020；`--e7-mode=full --e7-run-id=fulltimed01 --e7-session-id=hx-onerror-boundary` | FULL身份/样本日志正确；work area变为`640x284`但Qt screenGeometry仍`360x640`；约6秒后横屏stage 2超时 | 只有EXE加载暂停，主机处理到resume ACK约16 ms；无Hx load、Hx断点或Prepare，原始错误链UNKNOWN；在明确中止后立即terminate，10.328s处确认PID消失 |

本次focusWg始终59、appWg312；此前35984/35995分别也是59对308/310。
历史完整Qt有效内部记录PID2933（`run03-FULL.jsonl`）则focusWg=appWg=197。
这是前台状态差异证据，不是本次Prepare错误根因；NIKINIKI已有成功进入横屏事实，不能概括成应用
不能横屏。相同pre-MMF条件复现后停止重复启动；按最新指示也不继续设备状态轮询。没有OnError排除结论。

原始记录本地忽略路径`.tmp/e7-hx-prepare-runtime/timed-20260908/fulltimed01/coda.jsonl`，
24997 bytes，SHA-256 `0DC8CEA36F6F221AC0B06BCFCD8781460C7EC2BFCAADE5087B2F2310CFFCB781`。
09:05附近已向用户报告主机新增证据：保存错误选择条件及旧日志字段缺口，未新增设备启动。
09:40:27阶段检查仅核对本地冻结清单：10项文件SHA仍全部匹配；单点切换共83项离线测试已通过，
没有新增设备进程或错误链，OnError仍未排除。10:00截止收尾已完成，定时跟进于10:01:52确认为停用；
用户暂停后无新增设备进程，PID36020先前已确认退出，本次不连接CODA，也不声称重新检查了手机状态。
保存错误的静态条件与单点切换脚本已交付并冻结；原始失败来源、实际写入和画面/声音仍UNKNOWN。
设备恢复及后续执行等待用户重新安排，旧限时授权不再适用。

<a id="e7-hx-selectiondevice-20260908"></a>
#### E7 Hx OnError选择链目标捕获（09-08，设备恢复后）

用户确认设备亮屏并重新授权后，继续使用既有完整应用Qt诊断及完整本地样本A。终点为目标Prepare
错误链，不等待60秒、不作播放验收；无人现场，画面、声音、背景均为UNKNOWN。

| 记录 | PID | 有效性与事实 | 暂停、退出与原始材料 |
|---|---|---|---|
| `selectiondevice01` | 36088 | FULL身份及Hx模块已匹配；首个断点add成功后，多余的独立enable被CODA以NotSupported拒绝；未进入Play/OnError/Prepare | 工具预置差错，按既定规则不计有价值数据；terminate并确认PID消失；`coda.jsonl` 52398 bytes / `6EF2FAD06035F53C334E9791DD5168CB664EE3784B8EDEEA6F46BFB1F9F2CA1C` |
| `selectiondevice02` | 36102 | FULL身份/样本、Hx及`hxmedpltfm`fingerprint匹配；同线程、同controller `0x22900678`依次命中event1、调整OnError、单个选择点、Prepare event2 | 目标链约10秒完成后提前terminate，PID消失和断点清理确认；画面/声音/背景UNKNOWN；`coda.jsonl` 93854 bytes / `A542BEAA1F0EC800436780C1C74D3138CAE4F97135716BB95D2191995D079513` |

PID36102的原始链为：event1时`+48/80/90/94=7/0/0/0`；调整OnError入口取得
`severity=4, r2=0x00040024, userCode=0`及`+48/80/90/94=2/0/0/0`，LR=`Hx+0x684C`；
选择前点取得`r5=r7=0x00040024, r8=4`，推算选择当前输入但没有观察store；随后同controller
Prepare event2实际取得`r2=0x00040024, +80=0x00040024, +90=0, +94=0`。
这证明该次错误不是旧保存值替换，且`0x00040024`在controller OnError入口已经存在；仍未定位其上游
初始化来源。主机暂停服务下限为16/16/78/172/78/110 ms，不是设备暂停总时长上界。

动态LR与同机ARM代码把调用者限定到`HXMMFStateCtrl`调整接口OnError转发体`Hx+0x6798`：它经
`[state+0x94]`observer虚槽`+0x1C`在`+0x6848`调用controller，且不转换原r2。这里state的`+0x94`
不是controller保存错误字段。该上游点随后已执行，结果以下一节取代本段当时的“下一点”建议。

<a id="e7-hx-statectrl-20260908"></a>
#### E7 Hx StateCtrl上游边界捕获（09-08）

| 记录 | PID / 身份 | 有价值的动态事实 | 终点、退出与限制 |
|---|---|---|---|
| `statectrl01` | 36141；完整Qt诊断/完整本地样本A；Hx `0x7AF00000`、`hxmedpltfm` `0x7CAC0000`，所有观测短块匹配 | 同线程观察`state=0x2290A8F0` 在`HXMMFStateCtrl +0x6798`入口已有severity 4 / `HXCode=0x00040024` / userCode 0；`[state+0x94]=0x22900694`与随后controller调整OnError接口一致，`controller=0x22900678`；同controller选择后Prepare实读`r2=+0x80=0x00040024` | 约10秒取得完整目标链后terminate；PID消失、动态gate和四初始点移除均确认；画面/声音/背景UNKNOWN；日志113452 bytes / `2E0948F0D2468990D089E08FF1BFEF4DF62FA7C0FD84AC4EC563C9D97D5FA144` |

上游LR为`0x7CA3C2B0`，同次内存实读证明调用指令是
`0x7CA3C2AC: 0xE12FFF3C / BLX r12`，当时`r12=0x7AF06798`。因此StateCtrl只是已证转发者，
`0x00040024`在进入它时已存在。该LR不在本次订阅的Hx或`hxmedpltfm`已验证范围；
因未订阅第三模块load事件，且闭环后已按约退出，同次运行模块归属是UNKNOWN。
后续同机`hxmedplyeng.dll`展开code已静态逐字匹配该调用窗，并恢复`+0x11F54`的唯一literal
`0x00040024`生成调用；它仍不是PID36141的同次load身份证据。
新点主机服务暂停下限为StateCtrl 94 ms；完整各点数值、模块门槛和唯一后续见
[错误来源报告](../research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。

无人现场时一次V6前置尝试PID36262只到横屏stage 2，available为`640x284`而physical仍为
`360x640`，随后触发orientation timeout；未加载Hx、未装断点、未到Play/Prepare，terminate后PID
消失。画面/声音/背景均UNKNOWN，不计内部错误样本。待设备可完成真实`640x360`后才执行V6
同次模块身份与完整生成链捕获，不重复该横屏阻塞启动。

设备恢复解锁后，V6 `playeronerror03`（PID36354）一次闭合目标链：同次`hxmedplyeng` base
`0x7CA00000`及fingerprint匹配，player caller为精确`+0x11F58`，同entry `0x22919808`实际
`+0xAC=1`；`severity/HXCode/userCode=4/0x00040024/0`沿同线程StateCtrl、observer
`0x22900694`、controller `0x22900678`到Prepare event 2。约10秒终点terminate，PID消失和断点
移除确认；画面/声音/背景UNKNOWN，不作播放验收。日志131048 bytes /
`DC60656BFE17A42FC3C0778835EF5C17ED579DC754177176D30D95FDC962F1EB`。

据此冻结的V7只新增helper写前`+0x2DAD8`。`partialhelper01`（PID36377）和
`partialhelper02`（PID36388）均在Hx加载前停于横屏stage 2：available `640x284`、physical
`360x640`，未装V7断点、未到Play/Prepare；两者均terminate并确认PID消失，画面/声音/背景
UNKNOWN，不计内部错误数据。连续同边界后停止重试；日志分别为
`25292 / B36BB07B7FBCE8D4A68008B4E68096728F356133A6541D63A6B21DCEDC9A53AF`、
`25278 / EE684919D007B7EDD4DD6170B0AC90BD39D7CE73F67C349F07532149F4E81B10`。

设备恢复后，`partialhelper03`（PID36405）在helper写前实际读到同entry `0x22919808`的
`r5=0 / r6=0x80004005`、output和partial marker均为0，随后marker变1并闭合到Prepare
`0x00040024`。进程与断点清理确认，画面/声音/背景UNKNOWN；日志139679 bytes /
`45351D990CD5C80FC9C94560B9FECB4D41D20438B45545366553C1BA20E35AD8`。

V8 `rendererreturn01`（PID36430）先证明同一对象vslot `+0x10`返回0、vslot `+0x18`实际方法
`0x7C981C18`返回`0x80004005`，但因动态点随后命中其他对象而在Prepare前耗尽业务命中上限，只作
部分链；日志115661 bytes / `7C52616E3ED056464EFAAE0F7FEF5371589B32C8ABD25468803EA9E054AE48DF`。
V9 `rendererreturn02`（PID36450）在首次非零结果时立即移除动态点，完整闭合
`vslot+0x18 return 0x80004005 -> helper同entry r6 -> player生成0x00040024 -> StateCtrl ->
controller -> Prepare event 2 0x00040024`。约10秒终点后terminate，PID消失及全部断点移除确认；
画面/声音/背景UNKNOWN。日志152261 bytes /
`8A2C3250949DEA1A3920E425B352602D60692DCE0CD275DA4E7A255B93ADFE68`。

后续同机`mdfvidrender.dll`定点身份/内部返回记录如下；均使用同一完整Qt诊断和完整本地样本A，
终点不是播放验收，画面、声音、背景全部UNKNOWN：

| 记录 | PID | 有价值事实 | 停止与原始材料 |
|---|---:|---|---|
| `mdffailure01` | 36591 | 同次`mdfvidrender.dll` load base为`0x7C980000`；首版16字节静态哈希误含运行重定位，身份门拒绝，未设mdf内部点、未观察方法 | 立即terminate，PID消失，唯一外层点移除；52,062 bytes / `BAE533BB1D1224E6E38F5C2E59235EA6ADB8EAD799959AE21FD9B52B53770D91` |
| `mdffailure02` | 36606 | 同次mdf base、四个不可重定位代码窗口及9项重定位vtable全部匹配；同线程/同renderer第一下层调用raw=0，`mdf+0x91C8` raw=`0x80004005`，外层同方法立即收到同值 | 约10秒闭合后terminate；PID消失、三个断点移除；68,540 bytes / `B9B9379BEF4F901CD999883F8C40D9B65DD5A01A5238F86DB21C96C3B7493464` |
| `mdfinitselect01` | 36659 | `mdf+0x9764`实际命中，寄存器签名把EFAIL路径限定到`+0x754C`返回；旧脚本把调用后已覆盖的LR当必需条件，错误记录为public-point miss，原始现场仍有效 | terminate及清理确认；66,152 bytes / `9F04CD2ED0F32C81A07CA122DA7EBA4A15162D8D3273D39C85357BCE53CD9FEE` |
| `mdfcall754c01` | 36684 | `mdf+0x96D0`在`+0x754C`返回后实际读到raw `0x80004005`；同线程、同adapter/renderer，外层同方法随后收到同值 | terminate，PID消失、两点移除；69,370 bytes / `0E2819B899177EB5C73B5F3D42C83A1874C668B83C4AE80113F696159BAA06F2` |
| `mdfcall754cexit01` | 36727 | `mdf+0x7E1C`保存的原始寄存器为`LR=mdf+0x7E18, r6=r0=0x80004005`，结合相邻指令证明`+0x12A30`mapper实际返回EFAIL；旧r8入口假设错误，故只作内部路径证据 | terminate，PID消失、两点移除；58,695 bytes / `1B1B14E39E353E7518223934F332FF78A714B729416A7913625F4CA6E0E0FB33` |
| `mdfexceptionmap01/02` | 36762 / 36773 | 均在Hx加载前停于横屏stage 2，没有目标模块或业务断点；只记前置阻塞 | 各自terminate并确认；25,151 bytes / `3B4CB48B0D5554C59BB348EA436CEB603C73DD75F92DBBF7C14FFE845E844A61`；25,145 bytes / `1813F36E85B0CAE8B019767D4F3DD4D5987F06C8D00B9E27E77D255EBA2B2C88` |
| `mdfexceptionmap03` | 37026 | 模块已加载，但首版mapper身份窗误含运行重定位literal；身份门停止，未设mapper点 | terminate并确认；54,483 bytes / `DC933C7A5A310B4E890E39BF5B865FCDD37FFBA925EB652673EEE975A786B63D` |
| `mdfexceptionmap04` | 37041 | 修正的68字节fingerprint通过；`+0x12A30`第一指令前，同线程/同renderer实际捕获raw `0xFFFFFFD4`=`-44`，work sentinel为`-1`；静态表不含-44，随后外层实际返回EFAIL | 约10秒目标链后terminate；PID消失、两点移除、session end确认；72,894 bytes / `40764B2D4808BC5ABEF0CFE6523D2333BCC8F4AA5398FCDE04BAB52578FB0D46` |

因此V9方法所属模块已由UNKNOWN提升为本机`mdfvidrender.dll`，最早实际失败边界又从
`+0x91C8`的EFAIL推进到`+0x754C`内部进入异常映射器的`KErrHardwareNotAvailable(-44)`。
每次终点都是错误链取证，不是播放验收；画面、声音、背景仍全部UNKNOWN。`-44`的具体生产调用
现已由后续PID69811收窄到CreateAndInit-like `+0x1060C`直接返回`-44`；指令映射、限制和唯一资源生命周期对照见
[错误来源报告](../research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。

<a id="e7-mdf-lifecycle-contrast-20260908"></a>
#### MDF 生命周期 A/B 对照与直接生产点准备

同一完整Qt诊断、完整本地样本A和同一组`mdf+0x96D0`／Qt Prepare点。A前序由CODA terminate，
B前序由用户先返回播放器、再从首页正常退出应用；B须同时满足应用关闭、event loop退出、Qt player
析构返回、player析构完成、processExited和PID消失。前序结束至目标启动间隔为实际观测值，没有加入
经验延时。

| 记录 | PID | 退出／人工事实 | 内部结果 | 原始材料 |
|---|---:|---|---|---|
| A1前序 `lifecycle-a1-prep` | 69389 | CODA terminate及PID消失；不作播放观察 | `+0x754C=0x80004005`；Prepare `-12017` | 60,286 bytes / `A93C47FC4AF121A027071202A379E69F478581E045426EE4EE2CDECAB6EF4ACA` |
| A1目标 `lifecycle-a1-target` | 69413 | 约10秒取证后脚本terminate；用户见进程很快退出，画面／声音／背景UNKNOWN | 同左 | 60,373 bytes / `8494CC70FD032EA607096BCBE8F92740DD16DCBD8FA2913F4470367FE2930608` |
| B1前序 `lifecycle-b1-prep` | 69437 | 全部正常析构门通过；用户确认无画面、有声、UI正常后手动退出，背景UNKNOWN | 同左 | 78,441 bytes / `3E8FE5E1756772C190623A23CD8CB9B9097C712D8E82BDDC5F398B2CEBE36F0C` |
| B1目标 `lifecycle-b1-target` | 69459 | 约10秒后脚本terminate；人工播放状态UNKNOWN | 同左 | 60,372 bytes / `C65ADD8E3D59D13EB52287CDB3E6F4F0888BF748E120F963A240324E6E8EE739` |
| A2前序 `lifecycle-a2-prep` | 69483 | CODA terminate及PID消失；不作播放观察 | 同左 | 60,305 bytes / `BB781BEA2DBF6B4521EBAAF27E148E9123B2C3F4D2E2A5E92D466D23797C05A1` |
| A2目标 `lifecycle-a2-target` | 69507 | 约10秒后脚本terminate；人工播放状态UNKNOWN | 同左 | 60,375 bytes / `4EC9008292201BED9C262511287C512A70A6252ABCDCA41DB0C5249B4D827DDE` |
| B2前序 `lifecycle-b2-prep` | 69531 | 全部正常析构门通过；用户确认无画面、有声、UI正常后手动退出，背景UNKNOWN | 同左 | 77,718 bytes / `64E0A8E0D6883A98FD1C6E75D9D890B1A1644E53973BD0F0BDF856C910B9C921` |
| B2目标 `lifecycle-b2-target` | 69553 | 约10秒后脚本terminate；人工播放状态UNKNOWN | 同左 | 60,367 bytes / `C1D1F1C3F534A4ABC7881E3F99ED46F938097572A16EEE5493F5F84582F78F75` |

A1/B1/A2/B2前序至目标的实际间隔分别约18.5／15.5／11.8／13.9秒。四个目标内部结果完全相同，
因此只否定“当前有证据的正常析构方式足以稳定消除下一进程初始化失败”；不排除所有资源因素，
不证明泄漏，也不把脚本在取证终点的主动terminate写成应用闪退。

后续`mdfrawproducer01`（PID69720）在同次模块身份和三段fingerprint通过后，于公共`mdf+0x7E04`
再次实际读到raw `-44`；但现场LR为callee遗留值，不能区分`+0x10374/+0x1060C`，故不作生产者
结论。异常清理未在当次确认退出；手机完整重启及USB重连后的只读枚举确认没有遗留NIKINIKI进程。

修正版`mdfrawproducer02`（PID69811）在同次模块/vtable/代码fingerprint门通过后，观察两个BL后的
首条指令：同线程、同adapter/work/renderer及保存调用帧中，`mdf+0x7754`实际r0为0，随后
`mdf+0x7D20`实际r0为`-44`，Qt session `0x04D7EF40`随后Prepare `-12017`。因此本次
Connect-like `+0x10374`成功，CreateAndInit-like `+0x1060C`直接产生`-44`；其内部具体失败来源
仍UNKNOWN。本次约10.2秒闭合后terminate，PID消失及断点清理确认；画面／声音／背景UNKNOWN，
不作播放验收。日志67,064 bytes / `8C16068DBAA418F6DF273448D9678E72830DCF2A712AF81A4992B4E049B7E7D4`。

下一点位脚本把`+0x1060C`细分为TRAP捕获结果`+0x10698`与后续`+0x10494`调用直接返回
`+0x106B8`，并保留同一client/work、adapter、renderer及调用帧关联。首个启动
`mdfcreateinitsource01`（PID69840）在Hx/mdf加载前即因物理屏仍为360×640触发既有横屏超时；没有
业务断点或Prepare结果。约10.4秒后清理，PID消失确认；画面／声音／背景UNKNOWN，不计错误链样本，
也不机械补跑。日志24,989 bytes / `8F99A7C17E6006BADD2137630DFFE8BE4AFF69212446C072397D53F15FCB0E35`。

解锁后的`mdfcreateinitsource02`（PID69853）通过全部同次模块、代码和对象身份门。同线程TRAP结果
在`+0x10698`实际为0；随后`+0x10494`调用在`+0x106B8`直接返回`-44`，请求号为18，参数块保留
同一work。该wrapper调用`euser` ordinal 1547，即目标SDK ABI命名的
`RSessionBase::DoSendReceive(int, TIpcArgs const*) const`，并原样返回服务端结果。同进程Qt session
`0x04D7EF80`随后Prepare `-12017`。因此目标链已推进到MDF DevVideo同步IPC function 18服务端响应
`-44`；服务端handler及具体失败条件仍UNKNOWN。约10.1秒后terminate，PID消失与三个断点清理确认；
画面／声音／背景UNKNOWN，不作播放验收。日志69,201 bytes /
`E1A31E2024765F4579DF658CBDCF9B74DC33156983AA677145E006DB4C1F4C99`。

随后把同一IPC请求追入本机`mdfvidrender.dll`内的服务端线程。`mdfserverinit01`（PID70505）把断点
放在装载TRAP槽的指令上，读到的r0无效，只保留为工具差错（67,258 bytes /
`25245F18F7D23559121175ED7712C2770009432E57DA6B7D059E1CBEC3CAE217`）。修正后的
`mdfserverinit02`（PID70527）通过同次base、代码fingerprint、请求号、server/message/work对象门：
`p70527.t70534`（`HLX_MDF_VIDEO_SERVd8ade8b`）处理function 18，保存同一0x28字节message；同步
CreateAndInit TRAP在`mdf+0xE884`实际为0，server内DevVideo对象非空、decoder/PP id为1/2。随后
同一线程的异步MdvpoInitComplete以`-44`回调同一server，并用`-44`完成同一message；客户端同一work
返回`-44`，Qt session `0x04D7EF40`收到Prepare `-12017`。捕获后terminate、PID消失与断点清理确认；
画面／声音／背景UNKNOWN。有效日志71,318 bytes /
`2DD387E6973C3DEE6DCC09E583F0DFDAD460E0838B5495AF550141C35C6AE358`。

`devvideoinitsource01`（PID70552）因首版脚本等待实际不会报告的第三个module load而在ServiceL处停止，
无业务结果；日志62,621 bytes / `382E42738BA1ECA0EF33880E41887EE12C68A537C8B532DE9000F71340792CD9`。
修正版`devvideoinitsource02`（PID70569）在实际Initialize调用前，通过同机mdf代码/vtable fingerprint和
完整输入身份门，取得同一DevVideo对象选择decoder UID `0x10204C21`（id 1，对象`0x2291EE90`）与
postprocessor UID `0x10273417`（id 2，对象`0x2291F508`），并验证observer及对象字段布局。由于
`DevVideo.dll`没有load event，ROM header内存读取被CODA以AltCode -5拒绝，无法建立code range和回调
fingerprint；脚本未执行Initialize、未取得Prepare，随即terminate并确认清理。它只是选择/身份数据，
不是播放或错误样本；画面／声音／背景UNKNOWN。日志71,156 bytes /
`5721DA74D72B225BFF47DB16014DB1C3EDCCDA79270DFC33806B7F2704D391DD`。

取得同机`DevVideo.dll`后，`devvideoinitsource03`（PID70803）验证同一live对象的primary/proxy vptr
与手机副本完全对应，但CODA在proxy callback地址设置ROM断点时明确返回AltCode -5。脚本在Initialize
前停止并确认PID/断点清理；未取得Prepare，不是错误或播放样本，画面／声音／背景UNKNOWN。日志
72,541 bytes / `FD32CE4551CD191735207A21AC6CE926AF16BDEBE240185F57290442D8E8F5CE`。

改用可断的RAM observer入口后，`devvideoinitsource04`（PID70824）闭合了HwDevice分流。同一
server/work/DevVideo、相同decoder `0x10204C21`和postprocessor `0x10273417`选择下，observer实际
收到`-44`、DevVideo状态8，LR为`0x806057C7`。该值精确对应同机`DevVideo.dll` decoder handler的
observer调用返回地址；postprocessor对应值应为`0x8060577B`，本次未走。随后同进程Qt session
`0x04D7EF60`收到Prepare `-12017`。约10.4秒后terminate并确认清理；画面／声音／背景UNKNOWN，
不作播放验收。日志78,342 bytes /
`9D23F31666A94BE53C3118906C8DC058A5E214508521E5FE0512AB7CAF8FC53C`。本次证明`-44`来自decoder
初始化回调而非postprocessor；decoder内部具体失败调用仍待同机IVE实现分析。

用户随后提供当前E7复制的`ivevideodecodehwdevice.dll`。`devvideoinitsource05`（PID70899）在MMF前
横屏准备超时，没有业务命中或Prepare；terminate及空PID确认，只记前置阻塞。下一次
`devvideoinitsource06`（PID70914）在同一decoder失败链取得嵌套IVE LR `0x80D27451`，因当时分类表
尚未收录该出口而停止；未等待Prepare，进程及断点清理确认。该LR经同机文件指令和目标SDK同component
trace ID离线恢复为`CIveVideoDecodeHwDevice::AccessDenied`中的observer返回地址。

修正后的`devvideoinitsource07`（PID70939）完成闭环。同一完整应用Qt诊断、完整样本A、同一
server/work/DevVideo下仍选择decoder UID `0x10204C21`和postprocessor UID `0x10273417`；live decoder
vptr精确等于同机IVE AVC vtable `0x80D317B4`。RAM observer入口同时取得DevVideo decoder LR
`0x806057C7`与嵌套IVE LR `0x80D27451`；observer尚未返回时实际读取decoder `0x2291EE90 + 0xBC`
为`0x00000000`，其中bit `0x20`未置。同机IVE代码在该条件下用立即数当场构造`-44`并回调，随后同进程
Qt session `0x04D7EF70`收到Prepare `-12017`。约11秒取证终点后脚本terminate，`processExited`、空PID
枚举及owned断点清理确认；画面／声音／背景全部UNKNOWN，不作播放验收。日志81,372 bytes / SHA-256
`22C32C4599AE6E9F7B129C1A3C8C03939B363B38DC9EF71508E083687E2FD50F`。本次设备事实证明
AccessDenied→IVE本地`-44`→Prepare链；policy为何拒绝仍UNKNOWN。

用户从本次E7复制的`ivepolicyserverclient.dll`和`ivepolicyserver.exe`已与RM-626/SW111固件重建件
逐字节一致。`ivepolicy01`（应用PID71154）同次观察到server PID71162从
`Z:\sys\bin\ivepolicyserver.exe`、code base `0x80D22308`加载；CODA在第一个server ROM hardware
breakpoint上返回NotSupported/AltCode -5，未进入业务观察、未取得Prepare，随后只terminate并确认应用
PID退出。原始枚举显示共享server仍为`p71162`，脚本未终止它且其后自然退出；首版汇总的空数组是PID
解析错误，不作为事实。画面／声音／背景UNKNOWN。日志51,951 bytes / SHA-256
`1D7FA2BBE961151FA4D9767702B835054E3A8A2A3E0EDD61E55DC7393FD99E53`。

`ivepolicytrace01`（应用PID71176）不设policy ROM断点，只保留Qt Prepare RAM点。server PID71184同样
从`0x80D22308`加载；同一Qt session `0x04D7EFD8`实际收到Prepare `-12017`。CODA Logging没有输出
RequestIveAccess、逐规则、allocator或Granted/Denied业务trace，故本次只证明失败在无ROM断点时复现，
没有观察到具体拒绝规则。应用terminate并确认退出；共享server未被脚本终止且后续自然退出。画面／声音／
背景UNKNOWN，不作播放验收。日志55,653 bytes / SHA-256
`2852F2B204A96D785E22C0644557BFE30C92D24B0B144E1673E0AC0E7A6B197B`。

`ulogger-capability01`只进行独立能力探针安装／启动，不启动媒体或trace。GCCE ARMv5探针
`e7_ulogger_probe.exe`为9,319 bytes / SHA-256
`4782A144D1859BFEC3FCAD63EE77E03396DEC7DA42355F6E58E4F3912183EB4E`，E32头确认UID/SID
`0xE000B15B`、EKA2且capability位全零；自签名SIS为10,456 bytes / SHA-256
`D945A6AD71F1EE136711F50A273C36502AD6C3F056EE9DD30D490E71251BF615`。CODA安装返回0，随后
`getPackageInfo`确认独立包`E7 ULogger Capability Probe`版本1.0.0；包保留在设备中，可按名称／UID卸载，
没有覆盖NIKINIKI。安装前探针与`uloggerserver.exe`进程枚举均为空。分别按文件名和完整安装路径
`E:\sys\bin\e7_ulogger_probe.exe`创建进程都返回`KErrNotFound(-1)`；没有PID、没有进入`E32Main`，
所以`Connect`、输出插件及当前配置接口均为NOT OBSERVED，而不是接口返回`-1`。随后对已安装EXE和
`Z:\sys\bin\uloggerclient.dll`的CODA只读打开均被相同的受保护目录策略以`-21`拒绝，不能借此区分
payload不可见与依赖缺失。没有修改filter/config，没有开始trace、媒体，也没有重启或终止共享服务。
最终只读复查确认包仍已安装，探针与`uloggerserver.exe`进程仍为空。第二阶段OST/TraceCore采集条件
未成立；本次不构建更高权限变体。用户随后用X-plore确认`E:\sys\bin\e7_ulogger_probe.exe`确实存在，
并在已检查位置未发现ULogger文件；检查位置的逐项清单未保存，不把这条人工观察单独写成全盘枚举。
主机import表与同固件core模块清单另行证明：四个公共直接依赖均存在，唯一缺失的直接装载项是
`uloggerclient.dll`，因此ULogger支线结束；未新增设备启动。

<a id="e7-hx-old-fields-audit"></a>
#### 旧Hx日志的字段保存审查（离线，不是新增设备结果）

逐行复查H1/H2/H3/H4、F1..F6共十份有效Hx原始JSONL，未改写来源。
`audit_hx_saved_fields.py`输出每个命中的行号、PID/thread、controller及确实保存的字段；缺失值不填0。

| 历史记录 | Prepare原始值 / 同时保存的`+0x80` | `+0x48 / +0x90 / +0x94` | 原始位置 |
|---|---|---|---|
| H1 / PID35694 | `0x00040024 / 0x00040024` | 全部UNKNOWN | `hx04/coda.jsonl`，event 1第264行、event 2第283行 |
| H3 / PID35743 | `0x00040024 / 0x00040024` | 全部UNKNOWN | `hxonerror03/coda.jsonl`，event 1第265行、event 2第285行 |
| H2/H4/F1..F6 | 每次均`0 / 0` | 全部UNKNOWN | `hxonerror01/04/05/06/07/08/09/10`各自同controller event 2 |

上述两个失败在各自event 1时`+0x80`为0；不能据此推导保存错误`+0x94`也是0。十份日志均无
OnError命中记录、GetDownloadID/选值变更trace或原始`Memory.get`返回。旧读取使用quiet方式，
SendEvent时读取的`0x84`字节也未整体落盘；`+0x48`虽在瞬时缓冲区内但没保存，`+0x90/94`则在其外。
保存的寄存器来自调用现场，并不是这三个成员的替代快照。不同PID复用了相同数值的controller地址，
仍是不同进程实例，不构成跨进程字段保留证据。

离线报告`.tmp/e7-hx-prepare-runtime/timed-20260908/old-fields-audit.json`，SHA-256
`F623671EE37A3C5577F5DB20D39A05235E714E9B5A4034DCCE1E0183A104FCD8`；原始材料保持本地忽略。
字段指令映射、条件伪代码和唯一后续取证方案见[错误来源报告](../research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。

<a id="e7-post-six-review"></a>
### 六次与 C2 的日志归属核对

以下是已有日志的离线核查，不增加真机观察或改变上述人工结果。

| 证据 | 归属与限制 |
|---|---|
| R 四次 Prepare 回调均为 `-12017` | 六次目录中的 01～04 USB 主日志：01/02/03 第375行、04第376行。仍须区分轨道声明与有效视频输出，不把 partial playback 改判成通用致命错误 |
| R 四次 PP 零统计 | 01/02/03 第270～275行、04第271～276行，位于 `DEVVIDEO_ALL_POSTPROCESSORS` 与能力信息输出之间、正式 utility 创建之前。属于枚举阶段，不是正式播放会话的首帧或 renderer 计数 |
| C2 实际调用组合 | `install-c2-mmfsurface-com4-rerun.jsonl:91` 跳过枚举，92 priority/preference均0，97使用文件名；102记录实际 controller `270501140`（`0x101F8514`）/Real Video Player。该身份只属于C2，不回填普通R或P |
| C2 PP 零统计的阶段 | 同一C2日志105～110行，位于Open成功之后、Prepare回调111行及AddDisplay 113～114行之前。不是运行90秒后同一renderer的累计遥测；日志没有PP实例ID，不确定是哪一个实例及为何输出统计 |
| P v2 非零PP统计 | 六次目录 `06-exit-confirm.jsonl:107`～110 的pre-hello排队日志：Received4520、Displayed4498、Skipped22、overflow0。结合该次退出时间线和人工可见性，支持P实际输出；不能据此确定首帧时间或移用于R |

当前解释和后续优先级见[重新评估](../research/player/E7_BLACK_SCREEN_REASSESSMENT_ZH.md)。

普通 NIKINIKI Debug 已重新 GCCE 编译和 SIS 打包，0 errors / 33 warnings；生成的 MMP 不含
E7 诊断宏或 helper 源码。该普通构建未安装到手机，不能计作普通包真机回归。

<a id="e7-evidence-ledger"></a>
### E7 policy ROM 定点读回（独立探针，2026-09-09）

用户授权可逆 ROM shadow 诊断，并确认本会话独占 CODA；本阶段仅执行读取，未写 shadow。
手机为 RM-626 / 111.040.1511，RomPatcher+ 3.1；用户报告仅免签补丁启用。

- PID 71410：探针 1.0.0 因报告文件共享标志错误返回 `KErrArgument(-6)`，未进入驱动读取。
- PID 71442：修正后的 1.0.1 正常进入，`channel_open=-1`，没有读取 ROM。
- PID 71474：1.0.2 显式加载 `patcherS3.ldd` 后，`driver_load=0`、`channel_reopen=0`；
  control 101 读取 `0x80D23076` 的 76 字节及 `0x80D24374` 的 48 字节，两次均返回 0，
  全部与同机 policy 文件的离线指令窗口逐字节一致。随后 `driver_unload=0`、进程退出码 0。

三次均确认探针进程消失；未设置断点、未启动播放器、未改正式播放逻辑。
独立包 `E7 Shadow Readback Probe` 1.0.2（UID/SID `0xE000B15C`，CAPABILITY NONE）仍安装，
可在应用程序管理卸载。它不是诊断插桩补丁，也没有验证 ROM 写入与撤销。
成功报告 492 字节，SHA-256 `0D52341FF13ADE6045F1D5130594739148A0700B9E57D460111EFD6F833D5475`。
私有证据位于 `.tmp/e7-policy-shadow-v1/readback-probe/device-readback-v3.jsonl` 与
`device-report-v3.txt`。该结果证明有替代 ROM 读回通道，不包含 IVE 请求、规则或 allocator 的动态返回。

### E7 policy ROM shadow v1（可逆性与CODA能力，2026-09-09）

设备仍为RM-626 / 111.040.1511，用户确认本会话独占CODA。独立包
`E7 Shadow Patch Controller` 1.0.0（UID/SID `0xE000B15D`、CAPABILITY NONE）安装成功，未覆盖
NIKINIKI、readback probe或系统DLL。它只有四个固定halfword和audit/selftest/prime/restore命令，
不接受任意地址，不调用FreePage。

| 阶段／PID | 实际设备事实 | 结论边界 |
|---|---|---|
| selftest / 71540 | 无NIKINIKI和policy server；`0x80D230A0`从`0028`写为`00BE`并立即恢复`0028`，随后四点均逐字节回读原值；driver load/unload均0 | 写入、IMB、回读和正常恢复链通过；未执行临时BKPT，不证明动态stub安全 |
| full preflight / 71547 | 完整Qt诊断在MMF前横屏超时，physical仍360×640；未加载policy模块、未装policy断点；terminate并确认退出 | 仅前置阻塞，不是policy或播放样本 |
| invalid tool / 71665 | 冻结A进入媒体后，主机误把Hx ARM点声明为Thumb并发生code-abort；已移除断点、确认退出并恢复四点 | 全部作废；不用于设备机制归因，脚本已改为4-byte ARM并加测试 |
| Auto gate / 71784 | 修正后的Hx ARM点正常命中；同次server PID71791从base `0x80D22308`加载。对shadow页`server+0xD98`的Auto breakpoint返回NotSupported/AltCode -5 | byte-identical shadow不使CODA Auto可用；未进入policy业务观察 |
| Software gate / 71904 | Hx ARM点再次正常命中；同次server PID71911从相同base加载。显式Software breakpoint仍返回NotSupported/AltCode -5 | 当前CODA三类中Hardware（既有）、Auto、Software均不可用；不再重复类型尝试 |
| final restore / 71943 | 应用已退出、共享server自然退出后执行；四点before/after/audit分别为`FFB5/206C/0028/0028`，operation 0、driver unload 0 | 代码字节恢复通过；FreePage未调用，含原字节shadow页可能保留至重启 |

两次有效能力门均在policy业务执行前停止，画面／声音／背景均UNKNOWN，不作播放验收。已接受的Hx/Qt
RAM断点均获remove确认；脚本从未terminate共享policy server。设备仍留有控制器包、
`E:\Data\e7_shadow_patchctl_unsigned.sis`及`E:\test\e7_shadow_patch_*.txt`；一次性command文件已删除。
详细运行日志与报告在`.tmp/e7-policy-shadow-v1/runs/`，不进入公开发布物。

### E7 policy ROM shadow v2 CMP能力与两点观察（2026-09-09/10）

本节不是播放验收；所有行的画面、声音和背景均为UNKNOWN。早期PID72046/PID780分别证明异常接管／
PC偏移并暴露`ThreadRawWrite(-38)`、错误选择local CodeModifier及Close count误判；两次残留均由后续
重启清除，详细失败事实保留在调查报告。

| 阶段／PID | 实际设备事实 | 清理与结论 |
|---|---|---|
| CMP gate 1.0.3 | 原始、临时`DE00`异常模拟、恢复后三阶段对`r0=0/1/-1`均返回`6/2/10`；PC偏移0，4次handler均handled，simfail/overflow为0，NZCV与原CMP一致；`RestoreCode=0`、回读`2800` | handler Close count=1、driver unload=0、PASS；首次证明CPU执行临时指令且CMP语义透明。报告SHA-256 `4A7BE6623810ED1FA673F817632A29FB85DA449978F0FCA20EE7F47693378367` |
| policy smoke | 只安装`0x80D230A0/0x80D24392`两个CMP点，不启动媒体；original/sentinel均1/1，随后restore 0、restored 1/1、Close count=1、driver unload=0 | 两点安装／恢复链通过；0 hit符合无policy进程前序。报告SHA-256 `7D14801A817ACF96A7420F5515CD41E98EAC0EB24CB321EB68835E95E69AA416` |
| real1／observer nonce `ABD5FF55`、媒体PID3384 | observer已安装两点，但CODA启动第二进程清除了全局CodeModifier登记；0 hit，显式Restore为`-1/-1` | 工具干预导致本次无效，不解释为规则未执行；只读probe PID6467随后确认两个76/48-byte代码窗全为原字节。失败报告SHA-256 `455E6188F59230B0873DEB1D0607DF6417CCCBCC705CF818304AABFBF86CC895` |
| real2／controller PID1127、媒体PID1132、policy PID1140/TID1141 | observer自身创建唯一完整应用Qt诊断；同一`current=0x00720890`依次记录：rule0 `r0=0`、rule1 `r0=0`、Resource allocator `r0=0xFFFFFFFE (-2)`、rule2 `r0=0`。4 hit/handled，simfail/overflow为0，CPSR前后逐条相同 | 首次证明前两条rule未阻止继续、Resource实际运行且allocator返回`-2`；有效live报告SHA-256 `E400EE48795FC8A36D620DC708AEA30E0FBE7F1FA9CDFA0914A125BF3712C1EF`。observer随后卡在媒体退出等待，未生成final |
| reboot postflight | 用户重启后，CODA只读查询确认observer、媒体、policy、CMP gate及v1控制器进程均不存在；ROM仍为RM-626/111.040.1511，`target_write=false` | 安全清理确认；postflight SHA-256 `60B94054A3B643BBAC65631458917B6E6096B433C19F13CDEEA6AF1B2D5553EE` |
| MOVS gate 1.0.0 stack-fix | 初版因16 KiB栈descriptor未产出报告；改为heap后，原始／异常模拟／恢复后三阶段各12组`0/1/-1 × C/V`的r6/NZCV全部一致，PC bias 0，13 hit/handled，simfail/overflow为0 | 恢复回读`0006`、Close count 1、driver unload 0、PASS；报告SHA-256 `FA127D891F69617FA4B670CF473062E98DB19A37C1C8CB5E55BB320291795C13`。同一能力门不再重跑 |
| RCam observer 1.0.0／媒体PID916、policy PID924/TID925 | `0x80D235E8`单点记录原始`r0=-2`；同一`current=0x00720890`，`current+0x14=1`、`stack0=sp+0x18`、`stack1=camera=0`，三份PID/token/resource/camera与两份level全一致，`match=3F`；参数为PID916/token2/resource5/camera0，9个level words为`34363248/02800168/00000000/02800168/72707768/01900000/00720984/00000020/00000001` | 分支和ordinal-15专属outgoing stack共同锁定带resource-level重载。实测LR `80484F75`使旧`lr_ord=0`，证明旧LR分类假设错误；live-only报告SHA-256 `2AAD3E131DE8EC1729F4C76041C7091E3F8F8F12D9FB0A36ADE0FE406F65F96D`，不声称final清理 |
| RCam observer reboot postflight | 上次live-only捕获后用户重启；只读查询确认observer、媒体、policy及全部相关gate/控制进程为空，`target_write=false` | 安全清理确认；SHA-256 `10A653F6FB2199453A02E4D1E522DC489485ECDBF99DE3697516E7369590DA6D` |
| RCam 1.0.1 mode 0同机映射／PID846 | 不装sentinel、不启动媒体、`codemod=0`；RCam header及七个有界窗通过，veneer `80BC9088→80484F6D`；EUser UID `10000079/1000008D/100039E5`、code `80478808+497D4`、2564 exports，ordinal491=`80484F6D`，16-byte目标窗读回 | PASS、driver unload 0；该映射解释公共点共享LR `80484F75`，LR不能区分重载。报告SHA-256 `C7B7F6F643B894324066B9A6E06295BE45961E8EA1199D159FA3AD22F85C28C1` |
| RCam 1.0.1 mapping postflight | 只读查询确认全部相关进程为空，包版本1.0.1，`target_write=false`、`process_start=false` | SHA-256 `FDD65B84C1BD3CC37DCEE8706450E0DBAD1560588D6BD410436D9DF4CF73DC56` |

本轮已确认ordinal 15带resource-level的`RCam::SetClientInfo`实际返回`-2`并取得有界输入。两个重载
共同进入`RCam 0x80BC9302`，再经同机EUser ordinal491的三参数
`RBusLogicalChannel::DoControl(function=0, a1=(void*)7, a2=request)`；`-2`被RCam和allocator原样返回。
`-2`仍只是`KErrGeneral`数值，不解释camldd为何产生它。没有同次Qt Prepare记录，也没有人工播放结果；
画面、声音和背景仍全部UNKNOWN。下一轮不再重跑CMP、MOVS或SetClientInfo捕获。同固件ROM候选的
`camldd.ldd` control 0/subcommand 7已完成离线恢复，但这不是新增设备事实：camldd handler原样返回
policy结果，usecase值5的有限下层表仍含policy一次性初始化、BaseCreate、8条decoder错误透传及2条
decoder本地`-2`入边。单一initializer公共点不能覆盖两个上层旁路，且ARM透明模拟门尚不存在，故没有
进入新真机观察。集中研究转长期小投入；唯一方向是纯离线证明覆盖该有限表的单点分类条件，否则不恢复
设备执行。详细表见[调查报告](../research/player/E7_MMF_PREPARE_ERROR_SOURCE_ZH.md)。
## E7 Black Screen Evidence Ledger

此账本是上方设备记录的归一化索引与证据补全，不增加真机观察。审计截点为 2026-09-06。
`FACT` 表示实际日志/源码/哈希事实；`STRONG EVIDENCE` 表示身份、运行和人工结果相互支持或有重复；
`WEAK EVIDENCE` 表示单次/来源不完整；`HYPOTHESIS` 是待证机制；`INVALIDATED` 是已被反例否定的推论；
`UNKNOWN` 表示无法核实。目视结果与因果强度分开：一次真实报告不等于强因果证据。

### 共同字段和读取规则

- **设备/固件/运行库 E**：本页“运行环境固定”行；证据为本地 `inspect.jsonl` 的 getRomInfo、
  `runtime-versions.jsonl` 的 Qt 查询/包清单。只证明本轮设备/包版本，不证明插件源码或编译宏。
- **历史环境 U**：只知 E7；固件、Qt/Mobility、boot、CODA、安装盘、实际前序程序均 UNKNOWN。
  历史纪要日期不等于每项的原始发生时刻，E7-01～21 编号不是已证实的执行时间线。
- **样本 A**：本页“样本 A 身份”行是唯一媒体身份记录。额外 SPS/PPS 静态复核：
  SPS id=0；PPS id=0→SPS 0，CABAC=true，bottom_field_pic_order_in_frame_present=false，
  num_slice_groups_minus1=0，L0/L1 默认 active-minus1=0，weighted_pred=false，weighted_bipred_idc=0。
  SPS SHA-256 `0D1C32E51C0009AF4AB0CD36AAEC17FEF8714830346ECEEFF1B93D0131374CA2`；
  PPS SHA-256 `4C9829F6B49A6CFDA2FAFD4B06C9E2CB9AC863391ACE7D0C0A56D312A4D2BF86`。
  本轮读取的完整文件与留存样本一致；未验证全片 slice reference graph。历史本地路径相同不足以追认当时 SHA。
- **时间/前序**：下表 O 序号是 2026-09-04～06 可恢复的启动顺序；“前序”为前一个被记录的实验，
  不保证设备间隙没有其他活动。PID 不是 boot-id。绝不把所有 O 行称同一次 boot。
- **CODA**：Q=有目标 PID 的应用日志；D=有日志但出现断线；L=启动回执后断线、无应用日志。
  均为 CODA 启动，不能计菜单独立启动。具体 connection-id 未完整留存，UNKNOWN；日志里可查重连/暂停/恢复。
- **重启**：U=未明确定位；N=当时记录未重启（非 uptime 证明）。O27 前存在 OSID 大幅跳变；
  O27 自己仍黑。用户“9:49 前关机”不足以给 O27～O37 之间任意边界填 YES。
- **窗口 G / 资源 R**：见后两表，按代码与本轮日志对应；数字为 QWidget 逻辑 geometry，非 GPU 分配大小。
  各次实际 EGLSurface、group ordinal、WS clipping、背景 surface、真实 MMF 绑定代次均 UNKNOWN。
- **首帧 F**：所有运行的解码/renderer/submit 直接证据均 UNKNOWN。V 表示用户见运动画面；B 表示黑屏报告；
  两者来自上方逐项记录。声音 Y=明确听到，U=未单独确认。audioAvailable 不换算成 Y。
- **position**：只取指定 PID 可归属的 STATE 样本最大值，单位 ms，不是实测播放总时长。
  稀疏日志漏报不等于停止；R0 只有早期五个 0，持续播放仍可由目视支持。
- 原始日志都在本地忽略目录 `.tmp/e7-coda/`；表内只列文件名/行，不发布原日志、端点或媒体。
  `run-*.jsonl` 常含旧 PID，不能把累计快照当作独立复测；`run-r0-02/02b` 未成功启动，不加次数。

### 窗口与资源条件字典

| G | QGL / window geometry 与映射历史 |
|---|---|
| S | 普通主宿主 360×554，子 QGL 8×8；视频同宿主 360×554 |
| T | 同 S 的 QGL，另建第二顶层视频宿主 360×554，两顶层可见 |
| L | 同 T，只把子 QGL 设为 360×554 |
| H / G | 同 T，子 QGL 分别 180×554 / 359×554 |
| I | 子 QGL 360×554 初始化→180×554，第二视频宿主 360×554 |
| Q | 顶层 QGL 360×554 初始化→180×554，第二视频宿主 360×554 |
| F | 顶层 QGL 360×554→360×640 fullscreen→360×554 maximized→180×554；第二视频宿主 360×554 |
| R / RC | 真实首页顶层 QGL fullscreen 往返；R 最终全宽，RC 最终 180×554；第二视频宿主 360×554 |
| 6R | 六次观察 R：真实首页与正式播放器，物理横屏 640×360，正式 native MMF 视频窗口；无第二 Qt 视频宿主 |
| 6P | 六次观察 P：R0W-P 的 fullscreen/maximized/半宽 QGL 与第二 Qt 视频宿主；设备最终保持竖屏 |

| R | NanoVG 初始化 | 实际 NVG frame/draw | YUV 初始化/视频上传 | 字体注册/字形绘制 | UI 图片上传 |
|---|---|---|---|---|---|
| 0 | 否 | 否/否 | 否/否 | 否/否 | 否 |
| N | GLES backend，默认 atlas | 否/否 | 否/否 | 无 UI 字体/否（有 fontstash 空 atlas） | 否 |
| NC0 | 仅链接，跳过初始化 | 否/否 | 否/否 | 否/否 | 否 |
| X | 否；仅 dummy/atlas 对应两张亮度纹理 | 否/否 | 否/否 | 否/否 | 否 |
| CPU | CPU core + 空 backend | 否/否 | 否/否 | 无 UI 字体/否 | 否 |
| A尺寸 | 完整 GLES backend，指定初始 atlas | 否/否 | 否/否 | 无 UI 字体/否 | 否 |
| Y | 同 N | 否/否 | 是/否，六个未分配图像存储的纹理对象 | 否/否 | 否 |
| P | 同 N | 否/否 | 是/否 | 内置字体/否 | 否 |
| UI | 真实产品初始化 | 是/是，逐帧数量 UNKNOWN | 是/此 Qt 宿主路径无软件视频上传 | 内置字体及实际字形；CJK 完成是否先于入口逐次 UNKNOWN | loadUiResources 两张 PNG；逐次完整 GL 上传遥测 UNKNOWN |
| UIF | 六次 R full：真实产品初始化 | 是/是，已记录 calls/vertices/dirty/upload | 是/无软件视频上传 | 内置字体及实际字形 | 两张 UI PNG 已加载；逐次上传细节只按日志计数 |
| UIE | 六次 R empty：资源同 UIF | 是/空 begin/end，draw=0 | 同 UIF | 注册/异步加载保留，无 UI 字形 draw | PNG 加载保留，无 UI 图片 draw |
| PC | 六次 P clear：YUV + 默认 NanoVG backend | 无 NVG frame/draw；只执行原 GL clear | 是/无软件视频上传 | 内置字体注册/无字形 draw | 否 |

RC 就绪条件不等待 CJK 完成，不能把“稍后 CJK_READY”倒放成进入视频前的共同门槛。
成功 S 日志已有内部 `S60VideoWidget`：`run-r0ws-01.jsonl:71` 在早期为不可见且偏移，
`:96` Loaded 后为可见且布局改变。R/RC 外层 QVideoWidget geometry 不足以与这个内层绑定窗口比较。

### 历史实验字段补全

全部环境 U、精确日期/前序/重启/CODA/实装 EXE/SIS 哈希 UNKNOWN；“提交”只固定可审阅源码，
不追认为每次实装二进制。F 均 UNKNOWN，position 无逐 PID 数字。观察与声音以 E7-01～21 原行
为准，未明确写声音的行保持 UNKNOWN；结论强度为历史报告 **WEAK EVIDENCE**，代码语义为 FACT。

| 旧 ID | 源码固定点 | 同二进制 | QGL / 显示条件（源码，不是逐帧测量） | 资源 | 媒体/编码 |
|---|---|---|---|---|---|
| 01 | `5875fab`，`2e2c385` 参照 | UNKNOWN | 纯 QWidget/native MMF；实际 extent UNKNOWN | 0 | 本地路径；SHA/SPS/PPS UNKNOWN |
| 02 | `f8159db` | 与01不同 | QGL host→MMF；逐次 geometry UNKNOWN | 0 | 同上 |
| 03 | `e90f4e3` / `2748aeb` | 与02不同 | 禁 overlay timer/不建 overlay 是不同变体，纪要未逐包拆分 | 0 | 同上 |
| 04 | `373a901` | 不同 | QGL 初始化后 hide | 0 | 同上 |
| 05 | `9cc3068` | 不同 | 仅链接 QtOpenGL，无 QGL | 0 | 同上 |
| 06 | `ecc1484` | 两次实装身份 UNKNOWN | 仅 QGLContext 对象，无 EGL create/current | 0 | 同上 |
| 07 | `8f76815` | 三次实装身份 UNKNOWN | 初始化并 delete QGL 后 MMF | 0 | 同上 |
| 08 | `00ea94e` | 不同 | MMF-first，QGL 由 5500 ms timer 加入，非源码首帧屏障 | 0 | 同上 |
| 09 | `45d7a92` | 不同 | NewL 后 QGL；独立 timer，非异步完成屏障 | 0 | 同上 |
| 10 | `b678b97` | 不同 | 解析窗口≠绑定 display；未 open | 0 | 同上 |
| 11 | `04413b1` | 不同 | OpenComplete 普通配置后、Prepare 前 hold | 0 | 同上 |
| 12 | `c5f9ff9` | 历史/本轮 payload 留存可比较；旧实装 UNKNOWN | 小 QGL/普通 maximized QWidget/QVideoWidget | 0 | 参照本地路径，旧 SHA UNKNOWN |
| 13～14 | `471df25` | 子变体归属 UNKNOWN | landscape/large-max；true fullscreen 未验收 | 0 | 同上 |
| 15 | `ce51e26` | 不同 | 真实 VideoPlayerWidget + Qt backend | UI（源码）；逐次负载 UNKNOWN | 逐样本 SHA/SPS/PPS UNKNOWN |
| 16 | `fb5cc85` | 不同 | 同15，删 overlay 创建 | UI（源码）；逐次负载 UNKNOWN | 同上 |
| 17 | `7ece4b2` | 不同 | 同16，默认 QVideoWidget 属性 | UI（源码）；逐次负载 UNKNOWN | 同上 |
| 18～19 | Qt 集成分支，逐次提交 UNKNOWN | UNKNOWN | 同系列；HTTP / 完整下载 | UI（源码）；逐次负载 UNKNOWN | HTTP/下载对象的逐次身份 UNKNOWN |
| 20～21 | `1353543` / `8947bdb` / `3eded8d` | 模式与最终包映射 UNKNOWN | CONTROL 真实 shell；MIRROR 先 native 化再建 media | UI 是否完成 UNKNOWN | CONTROL 指定参照路径，SHA UNKNOWN；MIRROR 未证实媒体启动 |

`a146f76` object-only 有代码，未找到独立设备观察，记 UNKNOWN，不插入失败计数。
独立 Player 两项反馈沿用上方表，机型/包/媒体/首帧/声音逐次 UNKNOWN，整体不能算 E7 修复。
当前正式 `.pro` 仍有 root/bare/legacy/noprobe/controller/Helix/surface/memory 等诊断 CONFIG；
存在入口只证明可构建意图，未找到可逐 PID 配对的本轮结果时均为 UNKNOWN。

### 本轮逐次运行索引

本表每行继承环境 E、样本 A 与 F=UNKNOWN。默认每次新进程。声音/可见性在此只作原行的紧凑索引。
同包仅指同族重跑或 NC 开关；不同变体即不同二进制。控制因素的因果强度：T/L 重复与 C/N 局部
重复为 STRONG EVIDENCE（仍有跨包混杂）；NC 一对及其他单次差分为 WEAK EVIDENCE。
包翻转为 STRONG EVIDENCE，固定 atlas 阈值为 INVALIDATED；H1～H9 一律仍是 HYPOTHESIS。

| 顺序 / 实验 | 包键 / 同包关系 | 前序 | G / R | 重启 / CODA | PID | V或B / 声音 | position ms | 日志：归属 TRACE 行 |
|---|---|---|---|---|---|---|---|---|
| O01 R0#1 | `R0` / 首见 | UNKNOWN | S / 0 | U / Q | 34854 | V / Y | 0 | `run-r0-01.jsonl`：QTMW，手工按启动回执归属 |
| O02 R0#2 | `R0` / 同 O01 | O01 | S / 0 | U / Q | 34873 | V / Y | 0 | `run-r0-02c.jsonl`：QTMW，手工按启动回执归属 |
| O03 R1 | `R1` / 首见 | O02 | R / UI | U / Q | 34913 | B / Y | 45945 | `run-r1-01.jsonl`：23～242 |
| O04 R1B | `r1b` / 首见 | O03 | R / UI | U / Q | 35037 | B / Y | 0 | `run-r1b-01.jsonl`：23～114 |
| O05 S | `sharedhost1` / 首见 | O04 | S / 0 | U / Q | 35685 | V / Y | 111645 | `run-r0ws-01.jsonl`：10～282 |
| O06 T#1 | `separatehost1` / 首见 | O05 | T / 0 | U / Q | 35714 | V / Y | 105785 | `run-r0wt-01.jsonl`：10～283 |
| O07 L#1 | `largegl1` / 首见 | O06 | L / 0 | U / Q | 35743 | B / Y | 73645 | `run-r0wl-01.jsonl`：10～244 |
| O08 T#2 | `separatehost1` / 同 O06 | O07 | T / 0 | N / Q | 35772 | V / Y | 49770 | `run-r0wt-02.jsonl`：10～199 |
| O09 L#2 | `largegl1` / 同 O07 | O08 | L / 0 | N / Q | 35801 | B / Y | 49550 | `run-r0wl-02.jsonl`：10～205 |
| O10 T#3 | `separatehost1` / 同 O06 | O09 | T / 0 | N / Q | 35830 | V / Y | 41710 | `run-r0wt-03.jsonl`：10～187 |
| O11 L#3 | `largegl1` / 同 O07 | O10 | L / 0 | N / Q | 35859 | B / Y | 39805 | `run-r0wl-03.jsonl`：10～190 |
| O12 H | `halfgl1` / 首见 | O11 | H / 0 | U / Q | 35888 | V / Y | 85500 | `run-r0wh-01.jsonl`：10～253 |
| O13 G | `gapgl1` / 首见 | O12 | G / 0 | U / Q | 35918 | B / Y | 84060 | `run-r0wg-01.jsonl`：10～259 |
| O14 R1C#1 | `r1c` / 首见 | O13 | RC / UI | U / Q | 35973 | B / Y | 101690 | `run-r1c-01.jsonl`：23～410 |
| O15 I | `latehalfgl1` / 首见 | O14 | I / 0 | U / Q | 36006 | V / Y | 73730 | `run-r0wi-01.jsonl`：10～254 |
| O16 Q | `topgl1` / 首见 | O15 | Q / 0 | U / Q | 36035 | V / Y | 73585 | `run-r0wq-01.jsonl`：10～256 |
| O17 F | `fullcycle1` / 首见 | O16 | F / 0 | U / Q | 36064 | V / Y | 9515 | `run-r0wf-01.jsonl`：10～185 |
| O18 C#1 | `cleargl1` / 首见 | O17 | F / 0 | U / D | 36139 | V / Y | 34625 | `run-r0wc-connected-01.jsonl`：17～236 |
| O19 N#1 | `nvginit1` / 首见 | O18 | F / N | U / D | 36201 | B / Y | 9615 | `run-r0wn-01.jsonl`：177～368 |
| O20 C#2 | `cleargl1` / 同 O18 | O19 | F / 0 | U / Q | 36241 | V / Y | 19765 | `run-r0wc-02.jsonl`：411～617 |
| O21 N#2 | `nvginit1` / 同 O19 | O20 | F / N | U / Q | 36288 | B / Y | 35775 | `run-r0wn-02.jsonl`：657～901 |
| O22 NC-skip | `nvgcontrol1` / 首见 | O21 | F / NC0 | U / D | 36336 | V / Y | 37720 | `run-r0wnc-skip-01.jsonl`：968～1203 |
| O23 NC-init | `nvgcontrol1` / 同 O22 | O22 | F / N | U / Q | 36361 | B / Y | 49695 | `run-r0wnc-keep-01.jsonl`：1236～1500 |
| O24 X | `gltextures1` / 首见 | O23 | F / X | U / Q | 36404 | V / Y | 32555 | `bridge-events.jsonl`：1568～1804 |
| O25 CPU | `nvgcpu1` / 首见 | O24 | F / CPU | U / Q | 36447 | V / Y | 17790 | `run-r0wcpu-01.jsonl`：1877～2080 |
| O26 SA | `nvgsmallatlas1` / 首见 | O25 | F / A1 | U / Q | 36489 | V / Y | 35415 | `run-r0wsa-01.jsonl`：2121～2363 |
| O27 A256#1 | `nvgatlas256` / 首见 | O26 | F / A256 | U / Q | 748 | B / Y | 78530 | `run-r0wa256-01.jsonl`：2390～2716 |
| O28 A128 | `nvgatlas128` / 首见 | O27 | F / A128 | U / Q | 791 | V / Y | 91510 | `run-r0wa128-01.jsonl`：2824～3166 |
| O29 A192 | `nvgatlas192` / 首见 | O28 | F / A192 | U / Q | 826 | V / Y | 115650 | `run-r0wa192-01.jsonl`：3261～3647 |
| O30 A224 | `nvgatlas224` / 首见 | O29 | F / A224 | U / Q | 861 | V / Y | 99475 | `run-r0wa224-01.jsonl`：3746～4102 |
| O31 A240 | `nvgatlas240` / 首见 | O30 | F / A240 | U / Q | 896 | V / U | 115650 | `run-r0wa240-01.jsonl`：4220～4606 |
| O32 A255 | `nvgatlas255` / 首见 | O31 | F / A255 | U / Q | 931 | V / Y | 87555 | `run-r0wa255-01.jsonl`：4719～5054 |
| O33 A257 | `nvgatlas257` / 首见 | O32 | F / A257 | U / Q | 966 | V / Y | 43670 | `run-r0wa257-01.jsonl`：5168～5422 |
| O34 A256#2 | `nvgatlas256` / 同 O27 | O33 | F / A256 | U / Q | 1001 | V / Y | 61655 | `run-r0wa256-02.jsonl`：5462～5749 |
| O35 A256#3 | `nvgatlas256` / 同 O27 | O34 | F / A256 | U / Q | 1020 | V / Y | 51710 | `run-r0wa256-03.jsonl`：5783～6046 |
| O36 N#3 | `nvginit1` / 同 O19 | O35 | F / N | U / Q | 1055 | V / Y | 75680 | `run-r0wn-03.jsonl`：6086～6397 |
| O37 R1C#2 | `r1c` / 同 O14 | O36 | RC / UI | U / Q | 1094 | B / U | 162900 | `run-r1c-after-state-reset-02.jsonl`：6469～7106 |
| O38 Y | `yuvinit1` / 首见 | O37 | F / Y | U / L | 1204 | V / Y | UNKNOWN | `bridge-events.jsonl`：启动 job；应用日志缺失 |
| O39 P | `uifont1` / 首见 | O38 | F / P | U / D | 1300 | V / Y | 11645 | `run-r0wp-uifont1-01.jsonl`：7178～7376 |
| O40 六次 R-full | `e7-six-r-v1` / ABBA 首项 | O39 | 6R / UIF | U / Q(USB) | 746 | B / Y | 214455 | `six-observations/E7SIX-20260905-A/01-r-full-usb.raw.jsonl`；退出补录见 `01-exit-reset-3.jsonl` |
| O41 六次 R-empty | `e7-six-r-v1` / 同 O40 | O40 | 6R / UIE | U / Q(USB) | 769 | B / Y（间歇卡顿） | 301610 | `six-observations/E7SIX-20260905-A/02-r-empty-usb.raw.jsonl`；退出补录见 `02-exit-confirm.jsonl` |
| O42 六次 R-empty 重复 | `e7-six-r-v1` / 同 O40 | O41 | 6R / UIE | U / Q(USB) | 807 | B / Y | 166865 | `six-observations/E7SIX-20260905-A/03-r-empty-repeat-usb.raw.jsonl`；退出补录见 `03-exit-confirm.jsonl` |
| O43 六次 R-full 反向 | `e7-six-r-v1` / 同 O40 | O42 | 6R / UIF | U / Q(USB) | 832 | B / Y | 130475 | `six-observations/E7SIX-20260905-A/04-r-full-reverse-usb.raw.jsonl`；退出补录见 `04-exit-confirm.jsonl` |
| O44 六次 P-clear v1 | `e7-six-p-v1` / 校准失败 | O43 | 6P / UNKNOWN | U / Q(USB) | 898 | INCONCLUSIVE / U | 0 | `six-observations/E7SIX-20260905-A/05-p-clear-usb.raw.jsonl`；run/session 空、mode 无效、媒体未启动 |
| O45 六次 P-clear v2 | `e7-six-p-v2` / 分支修复 | O44 | 6P / PC | U / Q(USB) | 939 | V / Y | 149810 | `six-observations/E7SIX-20260905-A/06-p-clear-calibration-v2-usb.raw.jsonl`；退出补录见 `06-exit-confirm.jsonl` |
| O46 backend-AB A | `e7-backend-ab-v1` / A | O45 | 6P / PC | U / Q(USB) | 1613 | V / Y | 37565 | `symbian/out/e7-backend-ab/e7-backend-ab-v1/obs01-A.jsonl`；固定 CODA terminate |
| O47 backend-AB B | 同 O46 / B | O46 | 6P / PC | U / Q(USB) | 1637 | B / Y | 37540 | `symbian/out/e7-backend-ab/e7-backend-ab-v1/obs02-B.jsonl`；固定 CODA terminate |
| O48 backend-AB B 重复 | 同 O46 / B | O47 | 6P / PC | U / Q(USB) | 1661 | B / Y | 37500 | `symbian/out/e7-backend-ab/e7-backend-ab-v1/obs03-B.jsonl`；固定 CODA terminate |
| O49 backend-AB A 反向 | 同 O46 / A | O48 | 6P / PC | U / Q(USB) | 1686 | V / Y | 37655 | `symbian/out/e7-backend-ab/e7-backend-ab-v1/obs04-A.jsonl`；固定 CODA terminate |


O35（256 第三次）与 O23（NC 开启）明确不重装；其他重装与新进程按原观察行和安装 job 核对。
O38 的 YUV/NanoVG 门通过来自源码 guard 加目视出画的推论，缺本轮应用遥测，强度低于有日志的 P。
O04 没有正 position，曾暂停/退出；不能把它算作已完整启动的稳定黑屏对照。

### 留存包身份与可复核性

下表哈希为 SHA-256 前 16 位便于索引；完整值保留在相应本地 `manifest.json`，不作为 Release 哈希。
23 个 window probe 的 manifest 位于 `symbian/out/e7-qt-window/<包键>/manifest.json`，
r1b/r1c 位于 `symbian/out/e7-r1b-coda/`、`symbian/out/e7-r1c-coda/`；六次观察 R/P v1 与
P v2 校准包位于 `symbian/out/e7-six-observations/`；backend A/B 包与日志位于
`symbian/out/e7-backend-ab/e7-backend-ab-v1/`。原留存包与本次诊断 EXE/SIS
均已按 manifest 或结果卡重算一致（FACT）；不等于每次手机 EXE 已读回。
R0 为冻结源码 payload 重签；R1 无同结构 manifest，按 build/install/launch 记录追溯，不能补造哈希。

| 包键 | EXE SHA-256 前缀 | SIS SHA-256 前缀 |
|---|---|---|
| `cleargl1` | `BECEB602D9786C8B` | `166B1B61A6F720B5` |
| `fullcycle1` | `011BA4DB95B7DF93` | `1A016D5533FCB07F` |
| `gapgl1` | `611E8963EC6AC0A1` | `EE67815857A1A9E9` |
| `gltextures1` | `7D6824E0947225D6` | `EB7F6788A81EF928` |
| `halfgl1` | `FEA0FC251870D022` | `D97F85E57F90BD48` |
| `largegl1` | `17459CA4F223F1E6` | `EE56829FE75585B5` |
| `latehalfgl1` | `3992A8D5E5EE6775` | `46719AC2AFABAAB6` |
| `nvgatlas128` | `179B42AEE5987DF3` | `2F6EB85F2D55C308` |
| `nvgatlas192` | `9E084F1EA0070073` | `03B71FA3AF569FC7` |
| `nvgatlas224` | `B8C21B0E93D9ED9C` | `FD4708ACA19BA351` |
| `nvgatlas240` | `78405B632297DEA2` | `0D93AC483E6A2857` |
| `nvgatlas255` | `AE5184CFBE13F6AC` | `CCA7C0A93FA06EBF` |
| `nvgatlas256` | `FB459BEE0FD01916` | `90A05B21019003E3` |
| `nvgatlas257` | `643694128B703064` | `B214BDF852E050A3` |
| `nvgcontrol1` | `99A310597D5E44F0` | `FE51D65FD855B3BC` |
| `nvgcpu1` | `ED66580DD2A2EF56` | `3C8E97EDA7FE7EB8` |
| `nvginit1` | `717CA9E40386B3F1` | `C433F5E933FB9C9F` |
| `nvgsmallatlas1` | `F301CCF87B98FA7D` | `DE9295FB55931288` |
| `separatehost1` | `A1A6AB9016F4772A` | `393F28B1AB40F42A` |
| `sharedhost1` | `DCF49C279E505E1A` | `BEAB9E6A6CB52A3A` |
| `topgl1` | `F71472DBB766600C` | `54B4FD89AF187AA4` |
| `uifont1` | `65815F8A6F6D25F7` | `EBD3075019934238` |
| `yuvinit1` | `BD77746EE11C1400` | `86CA451F6CDB47E3` |
| `r1b` | `AFEA4749496DC322` | `B968E1614DBF07AD` |
| `r1c` | `C575D2773AC479B0` | `586D666F9D831FE6` |
| `e7-six-r-v1` | `ADA73E000C5C101` | `318823800475215C` |
| `e7-six-p-v1` | `C6434DE6FAB1761C` | `FEEFF5782E05DA9A` |
| `e7-six-p-v2` | `0FC0D0BED900466C` | `6DFD8BD926793CC6` |
| `e7-backend-ab-v1` | `A1C77C7890FE572B` | `32B107D7EC3E12C1` |


六次观察为 O40～O45；本次严格四次对照为 O46～O49。O44 为已消耗预算的无效启动，不能计为 B。
离线提取命令示例：

```powershell
python tools/research/e7/summarize_coda_run.py --input .tmp/e7-coda/run-r0wn-03.jsonl --pid 1055 --output .tmp/e7-root-cause-audit/n1055.json
```

输出拒绝覆盖已有文件，过滤其他 PID；first_frame/visible_motion/audible_sound 永远不从 STATE 推断。
原始 STATE 缺 PID，只能继承最近的同族 TRACE；多进程并发输出时仍须人工核对。
工具遇到目标 PID 的会话回退、变体变化或序号重置会拒绝混合，用 `--first-line`/`--last-line` 指定一个
启动 job 的闭区间继续拆分；没有序号重置的 PID 复用仍不能只靠 PID 自动识别。
该限制不能由脚本自动补成完整证明。
同进程递增的媒体 session 分别输出 position 最大值；session 0→1 的初始化与重试不误判为 PID 复用。
