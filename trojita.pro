lessThan(QT_VERSION, 4.6) {
    error("Trojita requires Qt 4.6 or greater")
}

trojita_harmattan {
    # we have never run them on the ARM device anyway, so let's speed up the build cycles a bit
    CONFIG += disable_tests
}

TEMPLATE = subdirs
SUBDIRS  = src
!disable_tests: SUBDIRS += tests
CONFIG += ordered

unix:!disable_tests {
    test.commands = cd tests && TESTARGS=-silent $(MAKE) -s check
    QMAKE_EXTRA_TARGETS += test
}

include(configh.pri)
