QT += core network
CONFIG += staticlib
include(../../configh.pri)
INCLUDEPATH += ../
DEPENDPATH += ../
TARGET = MSA
TEMPLATE = lib
SOURCES += AbstractMSA.cpp \
    SMTP.cpp \
    Sendmail.cpp \
    FakeMSA.cpp \
    ImapSubmit.cpp
HEADERS += AbstractMSA.h \
    SMTP.h \
    Sendmail.h \
    FakeMSA.h \
    ImapSubmit.h
