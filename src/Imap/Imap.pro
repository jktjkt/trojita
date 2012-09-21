include(../../configh.pri)
include(../Streams/ZlibLinking.pri)
QT += network \
    sql
CONFIG += staticlib
INCLUDEPATH += ../ \
    ../.. \
    . \
    Model \
    Tasks
DEPENDPATH += ../ \
    ../.. \
    . \
    Model \
    Tasks
TARGET = Imap
TEMPLATE = lib
SOURCES += Parser/Parser.cpp \
    Parser/Command.cpp \
    Parser/Response.cpp \
    Parser/Sequence.cpp \
    Parser/LowLevelParser.cpp \
    Parser/Data.cpp \
    Parser/Message.cpp \
    Parser/ThreadingNode.cpp \
    Parser/3rdparty/rfccodecs.cpp \
    Parser/3rdparty/kcodecs.cpp \
    Parser/3rdparty/qmailcodec.cpp \
    Encoders.cpp \
    Exceptions.cpp \
    ConnectionState.cpp \
    Model/Model.cpp \
    Model/MailboxMetadata.cpp \
    Model/MailboxModel.cpp \
    Model/PrettyMailboxModel.cpp \
    Model/MsgListModel.cpp \
    Model/ThreadingMsgListModel.cpp \
    Model/PrettyMsgListModel.cpp \
    Model/MailboxTree.cpp \
    Model/MemoryCache.cpp \
    Model/SQLCache.cpp \
    Model/DiskPartCache.cpp \
    Model/CombinedCache.cpp \
    Model/Utils.cpp \
    Model/TaskFactory.cpp \
    Model/DelayedPopulation.cpp \
    Model/ParserState.cpp \
    Tasks/ImapTask.cpp \
    Tasks/FetchMsgPartTask.cpp \
    Tasks/FetchMsgMetadataTask.cpp \
    Tasks/UpdateFlagsTask.cpp \
    Tasks/ListChildMailboxesTask.cpp \
    Tasks/NumberOfMessagesTask.cpp \
    Tasks/ExpungeMailboxTask.cpp \
    Tasks/ExpungeMessagesTask.cpp \
    Tasks/CreateMailboxTask.cpp \
    Tasks/DeleteMailboxTask.cpp \
    Tasks/CopyMoveMessagesTask.cpp \
    Tasks/ObtainSynchronizedMailboxTask.cpp \
    Tasks/KeepMailboxOpenTask.cpp \
    Tasks/OpenConnectionTask.cpp \
    Tasks/GetAnyConnectionTask.cpp \
    Tasks/Fake_ListChildMailboxesTask.cpp \
    Tasks/Fake_OpenConnectionTask.cpp \
    Tasks/NoopTask.cpp \
    Tasks/IdTask.cpp \
    Tasks/IdleLauncher.cpp \
    Tasks/ThreadTask.cpp \
    Tasks/UnSelectTask.cpp \
    Tasks/OfflineConnectionTask.cpp \
    Tasks/EnableTask.cpp \
    Tasks/SortTask.cpp \
    Tasks/AppendTask.cpp \
    Tasks/SubscribeUnsubscribeTask.cpp \
    Tasks/GenUrlAuthTask.cpp \
    Tasks/UidSubmitTask.cpp \
    Model/ModelWatcher.cpp \
    Network/MsgPartNetworkReply.cpp \
    Network/ForbiddenReply.cpp \
    Network/MsgPartNetAccessManager.cpp \
    Network/FileDownloadManager.cpp \
    Model/TaskPresentationModel.cpp \
    \ #Model/kdeui-itemviews/kdescendantsproxymodel.cpp \
    \ #Model/VisibleTasksModel.cpp \
    Model/SubtreeModel.cpp \
    Model/OneMessageModel.cpp \
    Model/FindInterestingPart.cpp \
    Model/MessageComposer.cpp \
    Model/ComposerAttachments.cpp
HEADERS += Parser/Parser.h \
    Parser/Command.h \
    Parser/Response.h \
    Parser/Sequence.h \
    Parser/LowLevelParser.h \
    Parser/Data.h \
    Parser/Message.h \
    Parser/ThreadingNode.h \
    Parser/3rdparty/rfccodecs.h \
    Parser/3rdparty/kcodecs.h \
    Parser/3rdparty/qmailcodec.h \
    Encoders.h \
    Exceptions.h \
    ConnectionState.h \
    Model/Model.h \
    Model/MailboxMetadata.h \
    Model/MailboxModel.h \
    Model/PrettyMailboxModel.h \
    Model/MsgListModel.h \
    Model/ThreadingMsgListModel.h \
    Model/PrettyMsgListModel.h \
    Model/MailboxTree.h \
    Model/MemoryCache.h \
    Model/SQLCache.h \
    Model/DiskPartCache.h \
    Model/CombinedCache.h \
    Model/Cache.h \
    Model/Utils.h \
    Model/TaskFactory.h \
    Model/DelayedPopulation.h \
    Model/CopyMoveOperation.h \
    Model/FlagsOperation.h \
    Model/SubscribeUnSubscribeOperation.h \
    Model/ItemRoles.h \
    Model/RingBuffer.h \
    Model/Logging.h \
    Model/ParserState.h \
    Tasks/ImapTask.h \
    Tasks/FetchMsgPartTask.h \
    Tasks/FetchMsgMetadataTask.h \
    Tasks/UpdateFlagsTask.h \
    Tasks/ListChildMailboxesTask.h \
    Tasks/NumberOfMessagesTask.h \
    Tasks/ExpungeMailboxTask.h \
    Tasks/ExpungeMessagesTask.h \
    Tasks/CreateMailboxTask.h \
    Tasks/DeleteMailboxTask.h \
    Tasks/CopyMoveMessagesTask.h \
    Tasks/ObtainSynchronizedMailboxTask.h \
    Tasks/KeepMailboxOpenTask.h \
    Tasks/OpenConnectionTask.h \
    Tasks/GetAnyConnectionTask.h \
    Tasks/Fake_ListChildMailboxesTask.h \
    Tasks/Fake_OpenConnectionTask.h \
    Tasks/NoopTask.h \
    Tasks/IdTask.h \
    Tasks/IdleLauncher.h \
    Tasks/ThreadTask.h \
    Tasks/UnSelectTask.h \
    Tasks/OfflineConnectionTask.h \
    Tasks/EnableTask.h \
    Tasks/SortTask.h \
    Tasks/AppendTask.h \
    Tasks/SubscribeUnsubscribeTask.h \
    Tasks/GenUrlAuthTask.h \
    Tasks/UidSubmitTask.h \
    Model/ModelWatcher.h \
    Network/ForbiddenReply.h \
    Network/MsgPartNetAccessManager.h \
    Network/MsgPartNetworkReply.h \
    Network/FileDownloadManager.h \
    Model/TaskPresentationModel.h \
    \ #kdeui-itemviews/kdescendantsproxymodel.h \
    kdeui-itemviews/kbihash_p.h \
    \ #Model/VisibleTasksModel.h \
    Model/SubtreeModel.h \
    Model/OneMessageModel.h \
    Model/FindInterestingPart.h \
    Model/MessageComposer.h \
    Model/ComposerAttachments.h \
    Model/CatenateData.h \
    Model/UidSubmitData.h

XtConnect:DEFINES += XTUPLE_CONNECT
