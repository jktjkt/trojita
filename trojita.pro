# -------------------------------------------------
# Project created by QtCreator 2009-12-30T21:03:09
# -------------------------------------------------
QT += core \
    gui \
    webkit \
    network

DEFINES -= QT3_SUPPORT
DEFINES += LOG_AND_TRACE_ACTIVE
TARGET = trojita
TEMPLATE = app
INCLUDEPATH += ./Imap/Parser \
               ./Imap \
               ./Imap/Model \
#               ./Imap/Model/ModelTest \
               ./Imap/Parser/3rdparty \
               ./Gui \
               ./Imap/Network \
               ./Streams \
               ./MSA \
               ./qwwsmtpclient \
               ./iconloader
SOURCES +=  ./Imap/Parser/Parser.cpp \
    ./Imap/Parser/Command.cpp \
    ./Imap/Parser/Response.cpp \
    ./Imap/Parser/LowLevelParser.cpp \
    ./Imap/Parser/Data.cpp \
    ./Imap/Parser/Message.cpp \
    ./Imap/Parser/3rdparty/rfccodecs.cpp \
    ./Imap/Parser/3rdparty/kcodecs.cpp \
    ./Imap/Encoders.cpp \
    ./Imap/Parser/3rdparty/qmailcodec.cpp \
    ./Imap/Exceptions.cpp \
    ./Imap/Model/Model.cpp \
    ./Imap/Model/MailboxMetadata.cpp \
    ./Imap/Model/UnauthenticatedHandler.cpp \
    ./Imap/Model/AuthenticatedHandler.cpp \
    ./Imap/Model/SelectedHandler.cpp \
    ./Imap/Model/SelectingHandler.cpp \
    ./Imap/Model/MailboxModel.cpp \
    ./Imap/Model/PrettyMailboxModel.cpp \
    ./Imap/Model/MsgListModel.cpp \
    ./Imap/Model/MailboxTree.cpp \
    ./Imap/Model/MemoryCache.cpp \
    ./Imap/Model/Utils.cpp \
    ./Imap/Model/ModelWatcher.cpp \
    ./Gui/MessageView.cpp \
    ./Gui/EmbeddedWebView.cpp \
    ./Gui/PartWidgetFactory.cpp \
    ./Gui/PartWidget.cpp \
    ./Gui/SimplePartWidget.cpp \
    ./Gui/Rfc822HeaderView.cpp \
    ./Gui/AttachmentView.cpp \
    ./Gui/LoadablePartWidget.cpp \
    ./Imap/Network/MsgPartNetworkReply.cpp \
    ./Imap/Network/ForbiddenReply.cpp \
    ./Imap/Network/FormattingReply.cpp \
    ./Imap/Network/MsgPartNetAccessManager.cpp \
    ./Imap/Network/AuxiliaryReply.cpp \
#    ./Imap/Model/ModelTest/modeltest.cpp \
#    ./Imap/Model/ModelTest/ModelTestMoc.cpp \
    ./Streams/SocketFactory.cpp \
    ./Streams/IODeviceSocket.cpp \
    ./MSA/AbstractMSA.cpp \
    ./MSA/Sendmail.cpp \
    ./MSA/SMTP.cpp \
    ./main.cpp \
    ./Gui/Window.cpp \
    ./Gui/SettingsDialog.cpp \
    ./Gui/SettingsNames.cpp \
    ./Gui/ComposeWidget.cpp \
    ./Gui/MsgListView.cpp \
    ./qwwsmtpclient/qwwsmtpclient.cpp \
    ./iconloader/qticonloader.cpp

HEADERS += ./Streams/SocketFactory.h \
./Streams/IODeviceSocket.h\
./Streams/Socket.h\
./MSA/SMTP.h\
./MSA/Sendmail.h\
./MSA/AbstractMSA.h\
./Imap/Network/ForbiddenReply.h\
./Imap/Network/AuxiliaryReply.h\
./Imap/Network/MsgPartNetAccessManager.h\
./Imap/Network/MsgPartNetworkReply.h\
./Imap/Network/FormattingReply.h\
./Imap/Exceptions.h\
./Imap/Encoders.h\
./Imap/Model/MsgListModel.h\
./Imap/Model/SelectingHandler.h\
./Imap/Model/MailboxTree.h\
./Imap/Model/PrettyMailboxModel.h\
./Imap/Model/MailboxMetadata.h\
#./Imap/Model/ModelTest/modeltest.h\
./Imap/Model/ModelWatcher.h\
./Imap/Model/UnauthenticatedHandler.h\
./Imap/Model/AuthenticatedHandler.h\
./Imap/Model/MemoryCache.h\
./Imap/Model/Utils.h\
./Imap/Model/MailboxModel.h\
./Imap/Model/Cache.h\
./Imap/Model/SelectedHandler.h\
./Imap/Model/Model.h\
./Imap/Parser/Command.h\
./Imap/Parser/Parser.h\
./Imap/Parser/LowLevelParser.h\
./Imap/Parser/Message.h\
./Imap/Parser/Data.h\
./Imap/Parser/3rdparty/rfccodecs.h\
./Imap/Parser/3rdparty/qmailcodec.h\
./Imap/Parser/3rdparty/kcodecs.h\
./Imap/Parser/Response.h\
./qwwsmtpclient/qwwsmtpclient.h\
./iconloader/qticonloader.h\
#./tests/test_rfccodecs.h\
#./tests/test_Imap_Parser_parse.h\
#./tests/test_Imap_Responses.h\
#./tests/test_Imap_Message.h\
#./tests/test_Imap_LowLevelParser.h\
#./tests/qtest_kde.h\
./Gui/ComposeWidget.h\
./Gui/MessageView.h\
./Gui/SimplePartWidget.h\
./Gui/AbstractPartWidget.h\
./Gui/EmbeddedWebView.h\
./Gui/Window.h\
./Gui/Rfc822HeaderView.h\
./Gui/LoadablePartWidget.h\
./Gui/PartWidget.h\
./Gui/SettingsNames.h\
./Gui/SettingsDialog.h\
./Gui/PartWidgetFactory.h\
./Gui/MsgListView.h\
./Gui/AttachmentView.h



FORMS += ./Gui/CreateMailboxDialog.ui \
          ./Gui/ComposeWidget.ui 
RESOURCES +=  ./icons.qrc

