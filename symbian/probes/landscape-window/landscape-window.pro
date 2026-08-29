TEMPLATE = app
TARGET = wiliwili_landscape_window_probe
VERSION = 0.2.0

QT += core gui
CONFIG += warn_on

SOURCES += main.cpp

symbian {
    TARGET.UID3 = 0xE000B11B
    TARGET.EPOCSTACKSIZE = 0x14000
    TARGET.EPOCHEAPSIZE = 0x020000 0x02000000

    # CEikonEnv / CAknAppUiBase orientation control.
    LIBS += -lcone -leikcore -lavkon
}
