TEMPLATE = app
TARGET = nikiniki_anna_a1_gl

QT += core gui opengl
CONFIG += warn_on

DEPLOYMENT.display_name = NIKI_A1_GL

INCLUDEPATH += $$PWD/../../probes/anna-common
HEADERS += $$PWD/../../probes/anna-common/anna_probe_ui.h
SOURCES += main.cpp

symbian {
    TARGET.UID3 = 0xE000B131
    TARGET.CAPABILITY += NetworkServices
    TARGET.EPOCSTACKSIZE = 0x14000
    TARGET.EPOCHEAPSIZE = 0x020000 0x02000000
    LIBS += -lhal -lbafl -lsysutil -llibGLESv2

    vendorinfo = \
        "%{\"NIKINIKI\"}" \
        ":\"NIKINIKI\""
    anna_a1_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += anna_a1_deployment
}
