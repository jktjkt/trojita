QT += core network
CONFIG += qtestlib
DEFINES -= QT3_SUPPORT
DEPENDPATH += ../../src/ ../
INCLUDEPATH += ../../src/ ../
TEMPLATE = app


trojita_libs = Imap Streams
myprefix = ../../src/
include(../src/linking.pri)

# This defines a "check" target for running one standalone unit test
lessThan(QT_VERSION, 4.7) {
    include(testcase.prf)
} else {
    CONFIG += testcase
}

HEADERS += TagGenerator.h

SOURCES += $$join(TARGET,,,.cpp)
HEADERS += $$join(TARGET,,,.h)
