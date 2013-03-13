include(../../../configh.pri)
CONFIG += staticlib
TARGET = ModelGlue
TEMPLATE = lib
QT += network sql

INCLUDEPATH += ../../
DEPENDPATH += ../../

SOURCES = ImapAccess.cpp
HEADERS = ImapAccess.h

trojita_libs = Imap Streams Common
myprefix = ../../
include(../../linking.pri)
include(../../Streams/ZlibLinking.pri)
