# -------------------------------------------------
# Project created by QtCreator 2009-12-30T21:03:09
# -------------------------------------------------
QT += core \
    gui \
    webkit \
    network \
    sql
include(../../install.pri)
include(../../configh.pri)
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
    ProtocolLoggerWidget.cpp \
    CheckForUpdates.cpp \
    Util.cpp \
    AutoCompletion.cpp \
    TaskProgressIndicator.cpp \
    TagListWidget.cpp \
    FlowLayout.cpp \
    TagWidget.cpp \
    UserAgentWebPage.cpp
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
    ProtocolLoggerWidget.h \
    CheckForUpdates.h \
    IconLoader.h \
    Util.h \
    AutoCompletion.h \
    TaskProgressIndicator.h \
    TagListWidget.h \
    FlowLayout.h \
    TagWidget.h \
    UserAgentWebPage.h
FORMS += CreateMailboxDialog.ui \
    ComposeWidget.ui \
    SettingsImapPage.ui \
    SettingsCachePage.ui \
    SettingsOutgoingPage.ui \

RESOURCES += ../icons.qrc

trojita_libs = Imap MSA Streams qwwsmtpclient Common

myprefix = ../
include(../linking.pri)

XtConnect:DEFINES += XTUPLE_CONNECT
XtConnect:RESOURCES += ../xtconnect-icons.qrc

unix:!trojita_harmattan {
    INSTALLS += desktop iconsvg icon32

    desktop.path = $$DATADIR/applications
    desktop.files += trojita.desktop

    iconsvg.path = $$DATADIR/icons/hicolor/scalable/apps
    iconsvg.files += ../icons/trojita.svg

    icon32.path = $$DATADIR/icons/hicolor/32x32/apps
    icon32.files += ../icons/trojita.png
}

trojita_harmattan {
    desktopfile.files = trojita.desktop
    desktopfile.path = /usr/share/applications
    INSTALLS += desktopfile
}

trojita_harmattan {
    icon.files = ../icons/trojita.png
    icon.path = /usr/share/icons/hicolor/64x64/apps
    INSTALLS += icon
}
