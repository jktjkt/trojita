CONFIG += staticlib
TEMPLATE = lib
TARGET = AppVersion
SOURCES = SetCoreApplication.cpp
HEADERS = SetCoreApplication.h

GITVERSION_PREFIX = $$join(PWD,,,/../..)
include(../gitversion.pri)
include(../../configh.pri)
