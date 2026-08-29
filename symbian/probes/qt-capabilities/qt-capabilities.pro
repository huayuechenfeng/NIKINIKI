TEMPLATE = app
TARGET = wiliwili_symbian_capabilities

QT += core gui network declarative opengl
CONFIG += warn_on mobility
MOBILITY += multimedia

SOURCES += main.cpp

symbian {
    TARGET.UID3 = 0xE000B118
    TARGET.CAPABILITY += NetworkServices
    TARGET.EPOCSTACKSIZE = 0x14000
    TARGET.EPOCHEAPSIZE = 0x020000 0x02000000
}
