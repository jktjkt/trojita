QT += core network
CONFIG += staticlib
include(../../configh.pri)
INCLUDEPATH += ../
DEPENDPATH += ../
TARGET = qwwsmtpclient
TEMPLATE = lib
SOURCES += qwwsmtpclient.cpp
HEADERS += qwwsmtpclient.h
