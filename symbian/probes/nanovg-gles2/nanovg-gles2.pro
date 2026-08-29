TEMPLATE = app
TARGET = wiliwili_nanovg_gles2_probe

QT += core gui opengl
CONFIG += warn_on

# Symbian's qmake spec otherwise compiles C sources in a pre-C99 mode.
QMAKE_CFLAGS.GCCE += -std=gnu99
DEFINES += STBI_NO_THREAD_LOCALS

NANOVG_ROOT = ../../third_party/nanovg
INCLUDEPATH += $$NANOVG_ROOT

SOURCES += main.cpp \
           $$NANOVG_ROOT/nanovg.c

RESOURCES += ../../../symbian/probes/nanovg-gles2/resources.qrc

symbian {
    TARGET.UID3 = 0xE000B11A
    TARGET.CAPABILITY += NetworkServices
    TARGET.EPOCSTACKSIZE = 0x20000
    TARGET.EPOCHEAPSIZE = 0x040000 0x04000000
}
