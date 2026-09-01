TEMPLATE = app
TARGET = nikiniki_anna_a2_media

QT += core gui
CONFIG += warn_on mobility
MOBILITY += multimedia

DEPLOYMENT.display_name = NIKI_A2_MEDIA

INCLUDEPATH += $$PWD/../../probes/anna-common
HEADERS += $$PWD/../../probes/anna-common/anna_probe_ui.h
SOURCES += main.cpp

symbian {
    TARGET.UID3 = 0xE000B132
    TARGET.CAPABILITY += NetworkServices
    TARGET.EPOCSTACKSIZE = 0x14000
    TARGET.EPOCHEAPSIZE = 0x020000 0x02000000
    LIBS += -lhal -lbafl -lsysutil

    vendorinfo = \
        "%{\"NIKINIKI\"}" \
        ":\"NIKINIKI\""
    anna_a2_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += anna_a2_deployment
}
