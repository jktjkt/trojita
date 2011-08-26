lessThan(QT_VERSION, 4.6) {
    error("Trojita requires Qt 4.6 or greater")
}

TEMPLATE = subdirs
SUBDIRS  = src tests
CONFIG += ordered

unix {
    test.commands = cd tests && TESTARGS=-silent $(MAKE) -s check
    QMAKE_EXTRA_TARGETS += test
}

OTHER_FILES += \
    qtc_packaging/debian_harmattan/rules \
    qtc_packaging/debian_harmattan/README \
    qtc_packaging/debian_harmattan/copyright \
    qtc_packaging/debian_harmattan/control \
    qtc_packaging/debian_harmattan/compat \
    qtc_packaging/debian_harmattan/changelog
