QT += core network
CONFIG += staticlib
DEFINES -= QT3_SUPPORT
INCLUDEPATH += ../
DEPENDPATH += ../
TARGET = Streams
TEMPLATE = lib
SOURCES += SocketFactory.cpp \
    IODeviceSocket.cpp \
    DeletionWatcher.cpp
HEADERS += Socket.h \
    SocketFactory.h \
    IODeviceSocket.h \
    DeletionWatcher.h
