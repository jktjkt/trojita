# An experimental .pro file for debugging of Nokia's Remote Build
# This is pretty much an experiment and doesn't work any better than the
# original trojita.pro yet.


QT += core \
    gui \
    webkit \
    network \
    sql

DEFINES -= QT3_SUPPORT
TARGET = trojita
TEMPLATE = app


INCLUDEPATH += src src/Gui
DEPENDPATH += src src/Gui
SOURCES += \
    src/main.cpp \
    src/Gui/MessageView.cpp \
    src/Gui/EmbeddedWebView.cpp \
    src/Gui/PartWidgetFactory.cpp \
    src/Gui/PartWidget.cpp \
    src/Gui/SimplePartWidget.cpp \
    src/Gui/Rfc822HeaderView.cpp \
    src/Gui/AttachmentView.cpp \
    src/Gui/LoadablePartWidget.cpp \
    src/Gui/Window.cpp \
    src/Gui/SettingsDialog.cpp \
    src/Gui/SettingsNames.cpp \
    src/Gui/ComposeWidget.cpp \
    src/Gui/MsgListView.cpp \
    src/Gui/ExternalElementsWidget.cpp \
    src/Gui/ProtocolLoggerWidget.cpp \
    src/Imap/Model/ModelTest/modeltest.cpp
HEADERS += src/Gui/ComposeWidget.h \
    src/Gui/MessageView.h \
    src/Gui/SimplePartWidget.h \
    src/Gui/AbstractPartWidget.h \
    src/Gui/EmbeddedWebView.h \
    src/Gui/Window.h \
    src/Gui/Rfc822HeaderView.h \
    src/Gui/LoadablePartWidget.h \
    src/Gui/PartWidget.h \
    src/Gui/SettingsNames.h \
    src/Gui/SettingsDialog.h \
    src/Gui/PartWidgetFactory.h \
    src/Gui/MsgListView.h \
    src/Gui/AttachmentView.h \
    src/Gui/ExternalElementsWidget.h \
    src/Gui/ProtocolLoggerWidget.h \
    src/Imap/Model/ModelTest/modeltest.h
FORMS += src/Gui/CreateMailboxDialog.ui \
    src/Gui/ComposeWidget.ui
RESOURCES += src/icons.qrc

INCLUDEPATH += src/Imap src/Imap/Parser src/Imap/Model src/Imap/Network
DEPENDPATH += src/Imap src/Imap/Parser src/Imap/Model src/Imap/Network
SOURCES += src/Imap/Parser/Parser.cpp \
    src/Imap/Parser/Command.cpp \
    src/Imap/Parser/Response.cpp \
    src/Imap/Parser/LowLevelParser.cpp \
    src/Imap/Parser/Data.cpp \
    src/Imap/Parser/Message.cpp \
    src/Imap/Parser/3rdparty/rfccodecs.cpp \
    src/Imap/Parser/3rdparty/kcodecs.cpp \
    src/Imap/Parser/3rdparty/qmailcodec.cpp \
    src/Imap/Encoders.cpp \
    src/Imap/Exceptions.cpp \
    src/Imap/Model/Model.cpp \
    src/Imap/Model/MailboxMetadata.cpp \
    src/Imap/Model/UnauthenticatedHandler.cpp \
    src/Imap/Model/AuthenticatedHandler.cpp \
    src/Imap/Model/SelectedHandler.cpp \
    src/Imap/Model/SelectingHandler.cpp \
    src/Imap/Model/ModelUpdaters.cpp \
    src/Imap/Model/MailboxModel.cpp \
    src/Imap/Model/PrettyMailboxModel.cpp \
    src/Imap/Model/MsgListModel.cpp \
    src/Imap/Model/MailboxTree.cpp \
    src/Imap/Model/MemoryCache.cpp \
    src/Imap/Model/SQLCache.cpp \
    src/Imap/Model/DiskPartCache.cpp \
    src/Imap/Model/CombinedCache.cpp \
    src/Imap/Model/Utils.cpp \
    src/Imap/Model/IdleLauncher.cpp \
    src/Imap/Model/ModelWatcher.cpp \
    src/Imap/Network/MsgPartNetworkReply.cpp \
    src/Imap/Network/ForbiddenReply.cpp \
    src/Imap/Network/FormattingReply.cpp \
    src/Imap/Network/MsgPartNetAccessManager.cpp \
    src/Imap/Network/AuxiliaryReply.cpp
HEADERS += src/Imap/Parser/Parser.h \
    src/Imap/Parser/Command.h \
    src/Imap/Parser/Response.h \
    src/Imap/Parser/LowLevelParser.h \
    src/Imap/Parser/Data.h \
    src/Imap/Parser/Message.h \
    src/Imap/Parser/3rdparty/rfccodecs.h \
    src/Imap/Parser/3rdparty/kcodecs.h \
    src/Imap/Parser/3rdparty/qmailcodec.h \
    src/Imap/Encoders.h \
    src/Imap/Exceptions.h \
    src/Imap/ConnectionState.h \
    src/Imap/Model/Model.h \
    src/Imap/Model/MailboxMetadata.h \
    src/Imap/Model/UnauthenticatedHandler.h \
    src/Imap/Model/AuthenticatedHandler.h \
    src/Imap/Model/SelectedHandler.h \
    src/Imap/Model/SelectingHandler.h \
    src/Imap/Model/ModelUpdaters.h \
    src/Imap/Model/MailboxModel.h \
    src/Imap/Model/PrettyMailboxModel.h \
    src/Imap/Model/MsgListModel.h \
    src/Imap/Model/MailboxTree.h \
    src/Imap/Model/MemoryCache.h \
    src/Imap/Model/SQLCache.h \
    src/Imap/Model/DiskPartCache.h \
    src/Imap/Model/CombinedCache.h \
    src/Imap/Model/Cache.h \
    src/Imap/Model/Utils.h \
    src/Imap/Model/IdleLauncher.h \
    src/Imap/Model/ModelWatcher.h \
    src/Imap/Network/ForbiddenReply.h \
    src/Imap/Network/AuxiliaryReply.h \
    src/Imap/Network/MsgPartNetAccessManager.h \
    src/Imap/Network/MsgPartNetworkReply.h \
    src/Imap/Network/FormattingReply.h

INCLUDEPATH += src/MSA
DEPENDPATH += src/MSA
SOURCES += src/MSA/AbstractMSA.cpp \
    src/MSA/SMTP.cpp \
    src/MSA/Sendmail.cpp
HEADERS += src/MSA/AbstractMSA.h \
    src/MSA/SMTP.h \
    src/MSA/Sendmail.h

INCLUDEPATH += src/Streams
DEPENDPATH += src/Streams
SOURCES += src/Streams/SocketFactory.cpp \
    src/Streams/IODeviceSocket.cpp \
    src/Streams/DeletionWatcher.cpp \
    src/Streams/FakeSocket.cpp
HEADERS += src/Streams/Socket.h \
    src/Streams/SocketFactory.h \
    src/Streams/IODeviceSocket.h \
    src/Streams/DeletionWatcher.h \
    src/Streams/FakeSocket.h


INCLUDEPATH += src/qwwsmtpclient
DEPENDPATH += src/qwwsmtpclient
SOURCES += src/qwwsmtpclient/qwwsmtpclient.cpp
HEADERS += src/qwwsmtpclient/qwwsmtpclient.h

INCLUDEPATH += src/qticonloader
DEPENDPATH += src/qticonloader
SOURCES += iconloader/qticonloader.cpp
HEADERS += iconloader/qticonloader.h
