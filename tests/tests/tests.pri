QT += core network
lessThan(QT_MAJOR_VERSION, 5) {
    CONFIG += qtestlib
} else {
    QT += testlib
}
DEFINES -= QT3_SUPPORT
DEFINES += QT_STRICT_ITERATORS
DEPENDPATH += ../../../src/ ../../
INCLUDEPATH += ../../../src/ ../../
TEMPLATE = app

# Link with some common stuff which is used by more tests
trojita_libs = test_LibMailboxSync
myprefix = ../../
include(../../src/linking.pri)

# Link with Trojita
trojita_libs = Imap Streams
myprefix = ../../../src/
include(../../src/linking.pri)
include(../../configh.pri)
include(../../src/Streams/ZlibLinking.pri)

# ...yes, the order above really matters, why are you asking?


# This defines a "check" target for running one standalone unit test
lessThan(QT_VERSION, 4.7) {
    include(testcase.prf)
} else {
    CONFIG += testcase
}

SOURCES += $$join(TARGET,,,.cpp)
HEADERS += $$join(TARGET,,,.h)
