TEMPLATE = app
TARGET = wiliwili_symbian
VERSION = 1.1.0
DEFINES += WILIWILI_SYMBIAN_VERSION_STR=\"$$VERSION\"

# NIKINIKI is the public product name. Keep the proven executable target,
# UID, settings keys and install paths so existing wiliwili_symbian builds
# upgrade in place without losing user state.
DEPLOYMENT.display_name = NIKINIKI

# Diagnosis-only layer 2 native-landscape probe. It builds the complete
# application shell but never opens playback. Keep the production executable
# name and UID so Qt Creator 2.4/WLAN CODA uses its already verified launch
# path for wiliwili_symbian.exe.
contains(CONFIG, applandscape1) {
    DEFINES += WILIWILI_ENABLE_APP_LANDSCAPE_WINDOW_PROBE
    message(Enabling app-shell native landscape probe in production launch identity)
}

# Diagnosis-only two-phase DevVideo direct-display probe. It retains the
# production executable name/UID so the already working Qt Creator/CODA launch
# configuration can deploy and start it. Normal builds do not compile the
# probe source and cannot enter its runtime path.
contains(CONFIG, devvideodirectprobe1) {
    DEFINES += WILIWILI_ENABLE_DEVVIDEO_DIRECT_PROBE
    message(Enabling DevVideo DSA plus ARGB overlay probe)
}

QT += core gui network opengl
CONFIG += warn_on mobility
MOBILITY += multimedia

# Symbian qmake otherwise compiles the upstream NanoVG C source in a
# pre-C99 mode. STBI_NO_THREAD_LOCALS avoids unsupported GCCE TLS syntax.
QMAKE_CFLAGS.GCCE += -std=gnu99
DEFINES += STBI_NO_THREAD_LOCALS NANOVG_GLES2
DEFINES += MG_ARCH=MG_ARCH_NEWLIB

# Mainline: MMF remains the first path for compatible streams, while the
# on-phone PPSSPP-FFmpeg H.264 fallback is compiled into every normal package.
# The optimized CPU RGB565 path is the ffmpegsoft2 renderer used by the normal
# soft fallback. Its H.264 hot paths remain generic C because this CPU has
# neither NEON nor ARMv6T2; ARMv6 assembly is limited to safe utility
# primitives. The experimental GLES-YUV renderer remains in source for future
# research but is not requested by normal soft playback after device testing.
FFMPEG_SOFT_BUILD = 1
FFMPEG_SOFT_SKIP_NONREF_LOOP_FILTER = 1
contains(CONFIG, ffmpegfullfilter) {
    FFMPEG_SOFT_SKIP_NONREF_LOOP_FILTER = 0
}
equals(FFMPEG_SOFT_BUILD, 1) {
    DEFINES += WILIWILI_ENABLE_FFMPEG_SOFT_DECODER \
               WILIWILI_FFMPEG_ARMV6_ASM \
               __STDC_CONSTANT_MACROS
    equals(FFMPEG_SOFT_SKIP_NONREF_LOOP_FILTER, 1) {
        DEFINES += WILIWILI_FFMPEG_SKIP_NONREF_LOOP_FILTER
        message(FFmpeg soft fallback: skip loop filter on non-reference frames)
    } else {
        message(FFmpeg soft fallback: full loop filter A/B baseline)
    }
    message(Enabling mainline PPSSPP-FFmpeg H.264 fallback)
}

# Keep UDEB diagnostics detailed, but make UREL suitable for real-device
# performance measurements. Critical errors, first-frame markers and the
# five-second SOFT_STATS aggregate remain enabled in both configurations.
CONFIG(debug, debug|release) {
    DEFINES += WILIWILI_SYMBIAN_VERBOSE_PLAYER_LOG
} else {
    DEFINES += WILIWILI_SYMBIAN_RELEASE_PLAYER_LOG
}
contains(CONFIG, ffmpegsoft2):contains(CONFIG, ffmpeglatedrop1) {
    # Diagnosis-only switch. Device testing showed that dropping RGB
    # conversion after 180 ms of lateness advances the media clock, but leaves
    # only about 6-7 visible fps. Keep it available for controlled comparison
    # without making it part of the normal ffmpegsoft2 candidate.
    DEFINES += WILIWILI_ENABLE_FFMPEG_LATE_PRESENTATION_DROP
    message(Enabling diagnosis-only FFmpeg late-presentation drop)
}

SYMBIAN_ROOT = $$PWD/..
NANOVG_ROOT = $$SYMBIAN_ROOT/third_party/nanovg
QRCODE_ROOT = $$SYMBIAN_ROOT/third_party/qrcodegen

INCLUDEPATH += $$SYMBIAN_ROOT/include \
               $$NANOVG_ROOT \
               $$QRCODE_ROOT

equals(FFMPEG_SOFT_BUILD, 1) {
    FFMPEG_ROOT = $$SYMBIAN_ROOT/third_party/ppsspp_ffmpeg
    INCLUDEPATH += $$FFMPEG_ROOT/include
    SOURCES += $$SYMBIAN_ROOT/source/platform/ffmpeg_h264_decoder.cpp
    # Build-App stages these archives in the SDK's armv5 udeb/urel directory.
    # SBS then places them after application objects in the final link group.
    MMP_RULES += "STATICLIBRARY libavcodec.lib"
    MMP_RULES += "STATICLIBRARY libavutil.lib"
}

SOURCES += $$PWD/main.cpp \
           $$SYMBIAN_ROOT/source/app/wiliwili_widget.cpp \
           $$SYMBIAN_ROOT/source/model/login_session.cpp \
           $$SYMBIAN_ROOT/source/model/video_detail.cpp \
           $$SYMBIAN_ROOT/source/network/bilibili_detail_parser.cpp \
           $$SYMBIAN_ROOT/source/network/bilibili_home_parser.cpp \
           $$SYMBIAN_ROOT/source/network/bilibili_login_parser.cpp \
           $$SYMBIAN_ROOT/source/network/bilibili_playback_parser.cpp \
           $$SYMBIAN_ROOT/source/network/bilibili_content_parser.cpp \
           $$SYMBIAN_ROOT/source/network/bilibili_wbi.cpp \
           $$SYMBIAN_ROOT/source/network/bilibili_section_parser.cpp \
           $$SYMBIAN_ROOT/source/network/native_transport.cpp \
           $$SYMBIAN_ROOT/source/platform/platform_metrics.cpp \
           $$SYMBIAN_ROOT/source/platform/mp4_avc_probe_reader.cpp \
           $$SYMBIAN_ROOT/source/platform/video_playback_backend.cpp \
           $$SYMBIAN_ROOT/source/ui/detail_screen.cpp \
           $$SYMBIAN_ROOT/source/ui/content_screen.cpp \
           $$SYMBIAN_ROOT/source/ui/home_screen.cpp \
           $$SYMBIAN_ROOT/source/ui/login_screen.cpp \
           $$SYMBIAN_ROOT/source/ui/navigation_rail.cpp \
           $$SYMBIAN_ROOT/source/ui/section_screen.cpp \
           $$SYMBIAN_ROOT/source/ui/video_player_widget.cpp \
           $$SYMBIAN_ROOT/source/ui/view_node.cpp \
           $$SYMBIAN_ROOT/source/ui/nanovg_gles2_backend.cpp \
           $$SYMBIAN_ROOT/generated/fixtures/home_fixture.cpp \
           $$NANOVG_ROOT/nanovg.c \
           $$SYMBIAN_ROOT/third_party/mongoose_compat/mg_json.c \
           $$QRCODE_ROOT/qrcodegen.c

# VideoPlayerWidget owns the native-orientation workAreaResized() state
# machine and therefore requires Qt 4's generated meta-object code.
HEADERS += $$SYMBIAN_ROOT/include/ui/video_player_widget.h

contains(CONFIG, applandscape1) {
    HEADERS += $$SYMBIAN_ROOT/include/platform/app_landscape_window_probe.h
    SOURCES += $$SYMBIAN_ROOT/source/platform/app_landscape_window_probe.cpp
}

contains(CONFIG, devvideodirectprobe1) {
    HEADERS += $$SYMBIAN_ROOT/include/platform/devvideo_direct_probe.h
    SOURCES += $$SYMBIAN_ROOT/source/platform/devvideo_direct_probe.cpp
}

RESOURCES += $$PWD/resources.qrc

symbian {
    TARGET.UID3 = 0xE000B100
    ICON = $$PWD/icons/nikiniki.svg

    # Shown by the native installer. Using the product name here avoids the
    # qmake placeholder "Vendor" in public SIS metadata.
    vendorinfo = \
        "%{\"NIKINIKI\"}" \
        ":\"NIKINIKI\""
    nikiniki_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += nikiniki_deployment
    LIBS += -lhttp -linetprotutil -lbafl -llibz \
            -lcone -leikcore -leikcoctl -lavkon -lws32 -lapgrfx \
            -lmediaclientvideo \
            -lmediaclientvideodisplay -ldevvideo
    # NetworkServices opens the bearer; ReadUserData permits certificate-store
    # access. Both capabilities are available to a self-signed SIS.
    TARGET.CAPABILITY += NetworkServices ReadUserData
    TARGET.EPOCSTACKSIZE = 0x20000
    TARGET.EPOCHEAPSIZE = 0x040000 0x04000000

    # Ship the complete font for dynamic Bilibili text.  resources.qrc also
    # contains a small OFL-renamed GB2312 fallback, because a real Belle phone
    # can occasionally report a different install drive during cold start.
    cjk_font.sources = $$SYMBIAN_ROOT/resources/font/switch_font.ttf \
                       $$SYMBIAN_ROOT/resources/font/LICENSE.txt
    cjk_font.path = /resource/apps/wiliwili_symbian
    DEPLOYMENT += cjk_font

    # Make the product GPL notice and the statically linked FFmpeg LGPL text
    # available on every installed device, alongside the existing font notice.
    legal_notices.sources = $$SYMBIAN_ROOT/../LICENSE \
                            $$SYMBIAN_ROOT/../NOTICE.md \
                            $$SYMBIAN_ROOT/third_party/ppsspp_ffmpeg/COPYING.LGPLv2.1
    legal_notices.path = /resource/apps/wiliwili_symbian/licenses
    DEPLOYMENT += legal_notices
}
