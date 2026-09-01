TEMPLATE = app
TARGET = nikiniki_anna_a3_font

QT += core gui
CONFIG += warn_on

DEPLOYMENT.display_name = NIKI_A3_FONT

INCLUDEPATH += $$PWD/../../probes/anna-common
HEADERS += $$PWD/../../probes/anna-common/anna_probe_ui.h
SOURCES += main.cpp

symbian {
    TARGET.UID3 = 0xE000B133
    TARGET.CAPABILITY += NetworkServices
    TARGET.EPOCSTACKSIZE = 0x10000
    TARGET.EPOCHEAPSIZE = 0x040000 0x04000000
    LIBS += -lhal -lbafl -lsysutil

    vendorinfo = \
        "%{\"NIKINIKI\"}" \
        ":\"NIKINIKI\""
    anna_a3_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += anna_a3_deployment

    anna_test_font.sources = $$PWD/../../resources/font/switch_font.ttf
    anna_test_font.path = /resource/apps/nikiniki_anna_a3_font
    DEPLOYMENT += anna_test_font
}
