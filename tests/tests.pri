QT += core network
CONFIG += qtestlib no_lflags_merge
DEFINES -= QT3_SUPPORT
DEPENDPATH += ../../src/
INCLUDEPATH += ../../src/ ../
TEMPLATE = app
HEADERS += ../qtest_kde.h


trojita_libs = Imap/Parser Imap/Model Imap/Parser Streams
myprefix = ../../src/
include(../src/linking.pri)

SOURCES += $$join(TARGET,,,.cpp)
HEADERS += $$join(TARGET,,,.h)
