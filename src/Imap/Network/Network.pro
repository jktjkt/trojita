QT -= gui
QT += network
CONFIG += staticlib
DEFINES -= QT3_SUPPORT
INCLUDEPATH += ../ ../..
DEPENDPATH += ../ ../..
TARGET = ImapNetwork
TEMPLATE = lib
SOURCES += \
    MsgPartNetworkReply.cpp \
    ForbiddenReply.cpp \
    FormattingReply.cpp \
    MsgPartNetAccessManager.cpp \
    AuxiliaryReply.cpp
HEADERS += \
    ForbiddenReply.h \
    AuxiliaryReply.h \
    MsgPartNetAccessManager.h \
    MsgPartNetworkReply.h \
    FormattingReply.h
