CONFIG += staticlib
TEMPLATE = lib
TARGET = Common
SOURCES += SettingsNames.cpp \
    SetCoreApplication.cpp \
    FileLogger.cpp \
    DeleteAfter.cpp \
    ConnectionId.cpp
HEADERS += SettingsNames.h \
    SetCoreApplication.cpp \
    SqlTransactionAutoAborter.h \
    PortNumbers.h \
    FindWithUnknown.h \
    Logging.h \
    RingBuffer.h \
    FileLogger.h \
    DeleteAfter.h \
    ConnectionId.h

GITVERSION_PREFIX = $$join(PWD,,,/../..)
include(../gitversion.pri)
include(../../configh.pri)
