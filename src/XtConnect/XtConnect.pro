QT += core \
    network \
    sql
QT -= gui
include(../../install.pri)
INCLUDEPATH += ../
DEPENDPATH += ../
DEFINES -= QT3_SUPPORT
TARGET = xtconnect-trojita
TEMPLATE = app
SOURCES += \
    main.cpp \
    MailSynchronizer.cpp
HEADERS += ComposeWidget.h \
    MailSynchronizer.h

xtconnect-trojita_libs = Imap MSA Streams

myprefix = ../
include(../linking.pri)

GITVERSION_PREFIX = $$join(PWD,,,/../..)
include(../gitversion.pri)
