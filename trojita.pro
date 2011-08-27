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

include(configh.pri)
