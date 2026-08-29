TEMPLATE = app
TARGET = nikiniki_endpoint_probe

QT += core gui
CONFIG += warn_on

INCLUDEPATH += ../../include

SOURCES += main.cpp

symbian {
    TARGET.UID3 = 0xE000B119
    TARGET.CAPABILITY += NetworkServices
    TARGET.EPOCSTACKSIZE = 0x14000
    TARGET.EPOCHEAPSIZE = 0x020000 0x02000000
}
