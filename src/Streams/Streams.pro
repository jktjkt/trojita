QT += core network
CONFIG += staticlib
DEFINES -= QT3_SUPPORT
INCLUDEPATH += ../
DEPENDPATH += ../
TARGET = Streams
TEMPLATE = lib

include(ZlibLinking.pri)
unix {
    HEADERS += 3rdparty/rfc1951.h
    SOURCES += 3rdparty/rfc1951.cpp
}

SOURCES += Socket.cpp \
    SocketFactory.cpp \
    IODeviceSocket.cpp \
    DeletionWatcher.cpp \
    FakeSocket.cpp
HEADERS += Socket.h \
    SocketFactory.h \
    IODeviceSocket.h \
    DeletionWatcher.h \
    FakeSocket.h \
    TrojitaZlibStatus.h
