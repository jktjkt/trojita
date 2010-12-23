# -------------------------------------------------
# Project created by QtCreator 2009-12-30T21:03:09
# -------------------------------------------------
QT += core \
    gui \
    webkit \
    network \
    sql
include(../../install.pri)
INCLUDEPATH += ../
DEPENDPATH += ../
DEFINES -= QT3_SUPPORT
TARGET = trojita
TEMPLATE = app
SOURCES += \
    ../Imap/Model/ModelTest/modeltest.cpp \
    main.cpp \
    MessageView.cpp \
    EmbeddedWebView.cpp \
    PartWidgetFactory.cpp \
    PartWidget.cpp \
    SimplePartWidget.cpp \
    Rfc822HeaderView.cpp \
    AttachmentView.cpp \
    LoadablePartWidget.cpp \
    Window.cpp \
    SettingsDialog.cpp \
    ComposeWidget.cpp \
    MsgListView.cpp \
    ExternalElementsWidget.cpp \
    ProtocolLoggerWidget.cpp
HEADERS += \
    ../Imap/Model/ModelTest/modeltest.h \
    ComposeWidget.h \
    MessageView.h \
    SimplePartWidget.h \
    AbstractPartWidget.h \
    EmbeddedWebView.h \
    Window.h \
    Rfc822HeaderView.h \
    LoadablePartWidget.h \
    PartWidget.h \
    SettingsDialog.h \
    PartWidgetFactory.h \
    MsgListView.h \
    AttachmentView.h \
    ExternalElementsWidget.h \
    ProtocolLoggerWidget.h
FORMS += CreateMailboxDialog.ui \
    ComposeWidget.ui \
    SettingsImapPage.ui \
    SettingsCachePage.ui \
    SettingsOutgoingPage.ui \

RESOURCES += ../icons.qrc

trojita_libs = Imap MSA Streams qwwsmtpclient Common

myprefix = ../
include(../linking.pri)

GITVERSION_PREFIX = $$join(PWD,,,/../..)
include(../gitversion.pri)

XtConnect:DEFINES += XTUPLE_CONNECT
