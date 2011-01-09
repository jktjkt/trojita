QT += core network
CONFIG += qtestlib
DEFINES -= QT3_SUPPORT
DEPENDPATH += ../../src/ ../
INCLUDEPATH += ../../src/ ../
TEMPLATE = app

# Link with some common stuff which is used by more tests
trojita_libs = test_LibMailboxSync
myprefix = ../
include(../src/linking.pri)

# Link with Trojita
trojita_libs = Imap Streams
myprefix = ../../src/
include(../src/linking.pri)

# ...yes, the order above really matters, why are you asking?


# This defines a "check" target for running one standalone unit test
lessThan(QT_VERSION, 4.7) {
    include(testcase.prf)
} else {
    CONFIG += testcase
}

SOURCES += $$join(TARGET,,,.cpp)
HEADERS += $$join(TARGET,,,.h)
