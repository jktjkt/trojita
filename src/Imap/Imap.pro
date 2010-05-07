QT += core network
CONFIG += staticlib
DEFINES -= QT3_SUPPORT
INCLUDEPATH += ../
TARGET = Imap
TEMPLATE = lib
SOURCES += Parser/Parser.cpp \
    Parser/Command.cpp \
    Parser/Response.cpp \
    Parser/LowLevelParser.cpp \
    Parser/Data.cpp \
    Parser/Message.cpp \
    Parser/3rdparty/rfccodecs.cpp \
    Parser/3rdparty/kcodecs.cpp \
    Parser/3rdparty/qmailcodec.cpp \
    Encoders.cpp \
    Exceptions.cpp \
    Model/Model.cpp \
    Model/MailboxMetadata.cpp \
    Model/UnauthenticatedHandler.cpp \
    Model/AuthenticatedHandler.cpp \
    Model/SelectedHandler.cpp \
    Model/SelectingHandler.cpp \
    Model/MailboxModel.cpp \
    Model/PrettyMailboxModel.cpp \
    Model/MsgListModel.cpp \
    Model/MailboxTree.cpp \
    Model/MemoryCache.cpp \
    Model/Utils.cpp \
    Model/ModelWatcher.cpp
HEADERS += Parser/Parser.h \
    Parser/Command.h \
    Parser/Response.h \
    Parser/LowLevelParser.h \
    Parser/Data.h \
    Parser/Message.h \
    Parser/3rdparty/rfccodecs.h \
    Parser/3rdparty/kcodecs.h \
    Parser/3rdparty/qmailcodec.h \
    Encoders.h \
    Exceptions.h \
    Model/Model.h \
    Model/MailboxMetadata.h \
    Model/UnauthenticatedHandler.h \
    Model/AuthenticatedHandler.h \
    Model/SelectedHandler.h \
    Model/SelectingHandler.h \
    Model/MailboxModel.h \
    Model/PrettyMailboxModel.h \
    Model/MsgListModel.h \
    Model/MailboxTree.h \
    Model/MemoryCache.h \
    Model/Utils.h \
    Model/ModelWatcher.h
