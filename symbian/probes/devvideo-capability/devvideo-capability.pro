TEMPLATE = app
TARGET = nikiniki_devvideo_capability_probe
VERSION = 0.2.0

QT += core gui
CONFIG += warn_on

SOURCES += main.cpp

symbian {
    # Research-only identity. It is intentionally unrelated to the NIKINIKI
    # product UID (0xE000B100) and cannot upgrade or replace the product SIS.
    TARGET.UID3 = 0xE000B11D
    DEPLOYMENT.display_name = NIKINIKI H264 HwCap Probe
    vendorinfo = \
        "%{\"NIKINIKI Research\"}" \
        ":\"NIKINIKI Research\""
    hwcap_probe_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += hwcap_probe_deployment
    TARGET.EPOCSTACKSIZE = 0x18000
    TARGET.EPOCHEAPSIZE = 0x020000 0x02000000

    # Direct DevVideo plus the platform SHA-256 implementation used to bind
    # every device result to the PC-generated legal test stream.
    LIBS += -ldevvideo -lhash
}
