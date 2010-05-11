QT -= gui
QT += network
CONFIG += staticlib
DEFINES -= QT3_SUPPORT
INCLUDEPATH += ../ \
    ../..
DEPENDPATH += ../ ../..
TARGET = ImapParser
TEMPLATE = lib
SOURCES += Parser.cpp \
    Command.cpp \
    Response.cpp \
    LowLevelParser.cpp \
    Data.cpp \
    Message.cpp \
    3rdparty/rfccodecs.cpp \
    3rdparty/kcodecs.cpp \
    3rdparty/qmailcodec.cpp \
    ../Encoders.cpp \
    ../Exceptions.cpp
HEADERS += Parser.h \
    Command.h \
    Response.h \
    LowLevelParser.h \
    Data.h \
    Message.h \
    3rdparty/rfccodecs.h \
    3rdparty/kcodecs.h \
    3rdparty/qmailcodec.h \
    ../Encoders.h \
    ../Exceptions.h \
    ../ConnectionState.h
