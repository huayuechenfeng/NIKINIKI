TEMPLATE = app
TARGET = nikiniki_devvideo_ecom_audit
VERSION = 0.3.0

QT += core gui
CONFIG += warn_on

SOURCES += main.cpp

symbian {
    # Read-only research identity, separate from both the NIKINIKI product and
    # the coded-stream capability probe.
    TARGET.UID3 = 0xE000B11E
    DEPLOYMENT.display_name = NIKINIKI DevVideo ECom Audit
    vendorinfo = \
        "%{\"NIKINIKI Research\"}" \
        ":\"NIKINIKI Research\""
    ecom_audit_deployment.pkg_prerules = vendorinfo
    DEPLOYMENT += ecom_audit_deployment
    TARGET.EPOCSTACKSIZE = 0x10000
    TARGET.EPOCHEAPSIZE = 0x020000 0x01000000

    # Only the published ECom registry/resource APIs and the platform SHA-256
    # implementation are used. No DevVideo implementation is created and no
    # CustomInterface is called.
    LIBS += -lecom -lefsrv -lbafl -lhash
}
