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

trojita_libs = Imap/Model Imap/Parser Imap/Network MSA Streams iconloader qwwsmtpclient

myprefix = ../
for(what, trojita_libs) {
    mylib = $$replace(what,/,)
    unix {
        mypath = $$join(what,,$${myprefix},)
    }
    win32 {
        CONFIG( debug, debug|release ) {
            mypath = $$join(what,,$${myprefix},/debug)
        } else {
            mypath = $$join(what,,$${myprefix},/release)
        }
    }
    LIBS += $$join(mypath,,-L,)
    LIBS += $$join(mylib,,-l,)
    PRE_TARGETDEPS += $$join(mypath,,,$$join(mylib,,/lib,.a))
}
