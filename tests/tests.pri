QT += core network
CONFIG += qtestlib
DEFINES -= QT3_SUPPORT
DEPENDPATH += ../../src/ ../
INCLUDEPATH += ../../src/ ../
TEMPLATE = app
HEADERS += ../qtest_kde.h


trojita_libs = Imap Streams
myprefix = ../../src/
include(../src/linking.pri)

SOURCES += $$join(TARGET,,,.cpp)
HEADERS += $$join(TARGET,,,.h)
