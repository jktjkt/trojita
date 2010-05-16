QT += core network
CONFIG += qtestlib no_lflags_merge
DEFINES -= QT3_SUPPORT
DEPENDPATH += ../../src/
INCLUDEPATH += ../../src/ ../
TARGET = test_rfccodecs
TEMPLATE = app
SOURCES += test_rfccodecs.cpp
HEADERS += test_rfccodecs.h \
    ../qtest_kde.h

LIBS += -L../../src/Imap/Parser -L../../src/Imap/Model -L../../src/Streams
LIBS += -lImapParser -lImapModel -lImapParser -lStreams

PRE_TARGETDEPS += ../../src/Imap/Parser/libImapParser.a \
    ../../src/Imap/Model/libImapModel.a \
    ../../src/Streams/libStreams.a
