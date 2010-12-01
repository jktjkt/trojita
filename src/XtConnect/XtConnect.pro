# needs GUI because of the item models...
QT += core gui network sql
include(../../install.pri)
INCLUDEPATH += ../
DEPENDPATH += ../
DEFINES -= QT3_SUPPORT
TARGET = xtconnect-trojita
TEMPLATE = app
SOURCES += \
    main.cpp \
    MailSynchronizer.cpp \
    MailboxFinder.cpp \
    XtConnect.cpp
HEADERS += ComposeWidget.h \
    MailSynchronizer.h \
    MailboxFinder.h \
    XtConnect.h

trojita_libs = Imap MSA Streams Common

myprefix = ../
include(../linking.pri)

GITVERSION_PREFIX = $$join(PWD,,,/../..)
include(../gitversion.pri)
