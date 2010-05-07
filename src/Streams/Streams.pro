QT += core network
CONFIG += staticlib
DEFINES -= QT3_SUPPORT
INCLUDEPATH += ../
TARGET = Streams
TEMPLATE = lib
SOURCES += SocketFactory.cpp \
    IODeviceSocket.cpp
HEADERS += Socket.h \
    SocketFactory.h \
    IODeviceSocket.h
