# -------------------------------------------------
# Project created by QtCreator 2009-12-30T21:03:09
# -------------------------------------------------
QT += core \
    gui \
    webkit \
    network
include(../../install.pri)
INCLUDEPATH += ../
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
HEADERS += \
# ./tests/test_rfccodecs.h\
# ./tests/test_Imap_Parser_parse.h\
# ./tests/test_Imap_Responses.h\
# ./tests/test_Imap_Message.h\
# ./tests/test_Imap_LowLevelParser.h\
# ./tests/qtest_kde.h\
    ComposeWidget.h \
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

LIBS += -L../Imap/Parser -L../Imap/Model -L../Imap/Network -L../MSA -L../Streams -L../iconloader -L../qwwsmtpclient
LIBS += -lImapModel -lImapParser -lImapNetwork -lMSA -lStreams -liconloader -lqwwsmtpclient

PRE_TARGETDEPS += ../Imap/Model/libImapModel.a \
    ../Imap/Parser/libImapParser.a \
    ../Imap/Network/libImapNetwork.a \
    ../MSA/libMSA.a \
    ../Streams/libStreams.a \
    ../iconloader/libiconloader.a \
    ../qwwsmtpclient/libqwwsmtpclient.a
