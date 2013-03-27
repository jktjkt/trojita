CONFIG += staticlib
TEMPLATE = lib
TARGET = Common
SOURCES += SettingsNames.cpp \
    FileLogger.cpp \
    DeleteAfter.cpp \
    ConnectionId.cpp
HEADERS += SettingsNames.h \
    SqlTransactionAutoAborter.h \
    PortNumbers.h \
    FindWithUnknown.h \
    Logging.h \
    RingBuffer.h \
    FileLogger.h \
    DeleteAfter.h \
    ConnectionId.h

include(../../configh.pri)
