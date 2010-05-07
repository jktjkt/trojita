QT += core network
CONFIG += staticlib
DEFINES -= QT3_SUPPORT
INCLUDEPATH += ../
TARGET = MSA
TEMPLATE = lib
SOURCES += AbstractMSA.cpp \
    SMTP.cpp \
    Sendmail.cpp
HEADERS += AbstractMSA.h \
    SMTP.h \
    Sendmail.h
