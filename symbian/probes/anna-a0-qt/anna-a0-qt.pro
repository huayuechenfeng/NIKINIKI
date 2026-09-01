TEMPLATE = app
TARGET = nikiniki_anna_a0_qt

QT += core gui
CONFIG += warn_on

DEPLOYMENT.display_name = NIKI_A0_BASE

INCLUDEPATH += $$PWD/../../probes/anna-common
HEADERS += $$PWD/../../probes/anna-common/anna_probe_ui.h
SOURCES += main.cpp

symbian {
    TARGET.UID3 = 0xE000B130
    TARGET.CAPABILITY += NetworkServices
    TARGET.EPOCSTACKSIZE = 0x10000
    TARGET.EPOCHEAPSIZE = 0x020000 0x02000000
    LIBS += -lhal -lbafl -lsysutil

    vendorinfo = \
        "%{\"NIKINIKI\"}" \
        ":\"NIKINIKI\""
    anna_a0_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += anna_a0_deployment
}
