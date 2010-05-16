# -------------------------------------------------
# Project created by QtCreator 2009-12-30T21:03:09
# -------------------------------------------------
QT += core network
CONFIG += qtestlib
DEFINES -= QT3_SUPPORT
DEPENDPATH += ../src/
INCLUDEPATH += ../src/
TARGET = test
TEMPLATE = app
SOURCES += test_rfccodecs.cpp \
    test_Imap_Parser_parse.cpp \
    test_Imap_Responses.cpp \
    test_Imap_Message.cpp \
    test_Imap_LowLevelParser.cpp
HEADERS += test_rfccodecs.h \
    test_Imap_Parser_parse.h \
    test_Imap_Responses.h \
    test_Imap_Message.h \
    test_Imap_LowLevelParser.h \
    qtest_kde.h

LIBS += -L../src/Imap/Parser
LIBS += -lImapParser

PRE_TARGETDEPS += ../src/Imap/Parser/libImapParser.a
