CONFIG += staticlib
TEMPLATE = lib
TARGET = Common
DEFINES += QT_STRICT_ITERATORS
SOURCES += SettingsNames.cpp \
    SetCoreApplication.cpp
HEADERS += SettingsNames.h \
    SetCoreApplication.cpp \
    SqlTransactionAutoAborter.h \
    PortNumbers.h \
    FindWithUnknown.h

GITVERSION_PREFIX = $$join(PWD,,,/../..)
include(../gitversion.pri)
