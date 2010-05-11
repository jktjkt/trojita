QT += core network
CONFIG += staticlib
DEFINES -= QT3_SUPPORT
INCLUDEPATH += ../ ../..
DEPENDPATH += ../ ../..
TARGET = ImapModel
TEMPLATE = lib
SOURCES += Model.cpp \
    MailboxMetadata.cpp \
    UnauthenticatedHandler.cpp \
    AuthenticatedHandler.cpp \
    SelectedHandler.cpp \
    SelectingHandler.cpp \
    MailboxModel.cpp \
    PrettyMailboxModel.cpp \
    MsgListModel.cpp \
    MailboxTree.cpp \
    MemoryCache.cpp \
    Utils.cpp \
    ModelWatcher.cpp
HEADERS += Model.h \
    MailboxMetadata.h \
    UnauthenticatedHandler.h \
    AuthenticatedHandler.h \
    SelectedHandler.h \
    SelectingHandler.h \
    MailboxModel.h \
    PrettyMailboxModel.h \
    MsgListModel.h \
    MailboxTree.h \
    MemoryCache.h \
    Utils.h \
    ModelWatcher.h
