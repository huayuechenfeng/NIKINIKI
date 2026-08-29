TEMPLATE = app
TARGET = wiliwili_symbian_probe

QT += core gui
CONFIG += warn_on

SOURCES += main.cpp

symbian {
    TARGET.UID3 = 0xE000B117
    TARGET.CAPABILITY += NetworkServices
    TARGET.EPOCSTACKSIZE = 0x14000
    TARGET.EPOCHEAPSIZE = 0x020000 0x02000000
}

