# -------------------------------------------------
# Project created by QtCreator 2009-12-30T21:03:09
# -------------------------------------------------
QT += core \
    gui \
    webkit \
    network
include(../../install.pri)
INCLUDEPATH += ../
DEPENDPATH += ../
DEFINES -= QT3_SUPPORT
TARGET = trojita
TEMPLATE = app
SOURCES += \
#    ../Imap/Model/ModelTest/modeltest.cpp \
    ../main.cpp \
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
    SettingsNames.cpp \
    ComposeWidget.cpp \
    MsgListView.cpp \
    ExternalElementsWidget.cpp
HEADERS += ComposeWidget.h \
    MessageView.h \
    SimplePartWidget.h \
    AbstractPartWidget.h \
    EmbeddedWebView.h \
    Window.h \
    Rfc822HeaderView.h \
    LoadablePartWidget.h \
    PartWidget.h \
    SettingsNames.h \
    SettingsDialog.h \
    PartWidgetFactory.h \
    MsgListView.h \
    AttachmentView.h \
    ExternalElementsWidget.h
FORMS += CreateMailboxDialog.ui \
    ComposeWidget.ui
RESOURCES += ../icons.qrc

trojita_libs = Imap MSA Streams iconloader qwwsmtpclient

myprefix = ../
include(../linking.pri)

GITVERSION_PREFIX = $$join(PWD,,,/../..)
include(../gitversion.pri)
