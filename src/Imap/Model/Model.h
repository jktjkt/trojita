/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

//#define TROJITA_DEBUG_TASK_TREE

#ifndef IMAP_MODEL_H
#define IMAP_MODEL_H

#include <QAbstractItemModel>
#include <QPointer>
#include <QTimer>
#include "Cache.h"
#include "../ConnectionState.h"
#include "../Parser/Parser.h"
#include "CacheLoadingMode.h"
#include "CopyMoveOperation.h"
#include "FlagsOperation.h"
#include "NetworkPolicy.h"
#include "ParserState.h"
#include "TaskFactory.h"

#include "Common/Logging.h"

class QAuthenticator;
class QNetworkSession;
class QSslError;

class FakeCapabilitiesInjector;
class ImapModelIdleTest;
class LibMailboxSync;

namespace Composer {
class ImapMessageAttachmentItem;
class ImapPartAttachmentItem;
class MessageComposer;
}

namespace Streams {
class SocketFactory;
}

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

class TreeItem;
class TreeItemMailbox;
class TreeItemMsgList;
class TreeItemMessage;
class TreeItemPart;
class MsgListModel;
class MailboxModel;
class NetworkWatcher;

class ImapTask;
class KeepMailboxOpenTask;
class TaskPresentationModel;
template <typename SourceModel> class SubtreeClassSpecificItem;
typedef std::unique_ptr<Streams::SocketFactory> SocketFactoryPtr;

/** @short Progress of mailbox synchronization with the IMAP server */
typedef enum { STATE_WAIT_FOR_CONN, /**< Waiting for connection to become active */
               STATE_SELECTING, /**< SELECT command in progress */
               STATE_SYNCING_UIDS, /**< UID syncing in progress */
               STATE_SYNCING_FLAGS, /**< Flag syncing in progress */
               STATE_DONE /**< Mailbox is fully synchronized, both UIDs and flags are up to date */
             } MailboxSyncingProgress;

/** @short A model implementing view of the whole IMAP server */
class Model: public QAbstractItemModel
{
    Q_OBJECT

    Q_PROPERTY(QString imapUser READ imapUser WRITE setImapUser)
    Q_PROPERTY(QString imapPassword READ imapPassword WRITE setImapPassword RESET unsetImapPassword)
    Q_PROPERTY(bool isNetworkAvailable READ isNetworkAvailable NOTIFY networkPolicyChanged)
    Q_PROPERTY(bool isNetworkOnline READ isNetworkOnline NOTIFY networkPolicyChanged)

    /** @short How to open a mailbox */
    enum RWMode {
        ReadOnly /**< @short Use EXAMINE or leave it in SELECTed mode*/,
        ReadWrite /**< @short Invoke SELECT if necessarry */
    };

    mutable AbstractCache *m_cache;
    mutable SocketFactoryPtr m_socketFactory;
    TaskFactoryPtr m_taskFactory;
    mutable QMap<Parser *,ParserState> m_parsers;
    int m_maxParsers;
    mutable TreeItemMailbox *m_mailboxes;
    mutable NetworkPolicy m_netPolicy;
    bool m_startTls;

    mutable QList<Imap::Responses::NamespaceData> m_personalNamespace, m_otherUsersNamespace, m_sharedNamespace;

    QList<QPair<QPair<QList<QSslCertificate>, QList<QSslError> >, bool> > m_sslErrorPolicy;


public:
    Model(QObject *parent, AbstractCache *cache, SocketFactoryPtr m_socketFactory, TaskFactoryPtr m_taskFactory);
    ~Model();

    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;
    virtual QModelIndex parent(const QModelIndex &index) const;
    virtual int rowCount(const QModelIndex &index) const;
    virtual int columnCount(const QModelIndex &index) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual bool hasChildren(const QModelIndex &parent = QModelIndex()) const;

    void handleState(Imap::Parser *ptr, const Imap::Responses::State *const resp);
    void handleCapability(Imap::Parser *ptr, const Imap::Responses::Capability *const resp);
    void handleNumberResponse(Imap::Parser *ptr, const Imap::Responses::NumberResponse *const resp);
    void handleList(Imap::Parser *ptr, const Imap::Responses::List *const resp);
    void handleFlags(Imap::Parser *ptr, const Imap::Responses::Flags *const resp);
    void handleSearch(Imap::Parser *ptr, const Imap::Responses::Search *const resp);
    void handleESearch(Imap::Parser *ptr, const Imap::Responses::ESearch *const resp);
    void handleStatus(Imap::Parser *ptr, const Imap::Responses::Status *const resp);
    void handleFetch(Imap::Parser *ptr, const Imap::Responses::Fetch *const resp);
    void handleNamespace(Imap::Parser *ptr, const Imap::Responses::Namespace *const resp);
    void handleSort(Imap::Parser *ptr, const Imap::Responses::Sort *const resp);
    void handleThread(Imap::Parser *ptr, const Imap::Responses::Thread *const resp);
    void handleId(Imap::Parser *ptr, const Imap::Responses::Id *const resp);
    void handleEnabled(Imap::Parser *ptr, const Imap::Responses::Enabled *const resp);
    void handleVanished(Imap::Parser *ptr, const Imap::Responses::Vanished *const resp);
    void handleGenUrlAuth(Imap::Parser *ptr, const Imap::Responses::GenUrlAuth *const resp);
    void handleSocketEncryptedResponse(Imap::Parser *ptr, const Imap::Responses::SocketEncryptedResponse *const resp);
    void handleSocketDisconnectedResponse(Imap::Parser *ptr, const Imap::Responses::SocketDisconnectedResponse *const resp);
    void handleParseErrorResponse(Imap::Parser *ptr, const Imap::Responses::ParseErrorResponse *const resp);

    AbstractCache *cache() const { return m_cache; }
    /** Throw away current cache implementation, replace it with the new one

    The old cache is automatically deleted.
    */
    void setCache(AbstractCache *cache);

    /** @short Force a SELECT / EXAMINE of a mailbox

    This command sends a SELECT or EXAMINE command to the remote server, even if the
    requested mailbox is currently selected. This has a side effect that we synchronize
    the list of messages, which is why this function exists in the first place.
    */
    void resyncMailbox(const QModelIndex &mbox);

    /** @short Mark all messages in a given mailbox as read */
    void markMailboxAsRead(const QModelIndex &mailbox);
    /** @short Add/Remove a flag for the indicated message */
    ImapTask *setMessageFlags(const QModelIndexList &messages, const QString flag, const FlagsOperation marked);
    /** @short Ask the server to set/unset the \\Deleted flag for the indicated messages */
    void markMessagesDeleted(const QModelIndexList &messages, const FlagsOperation marked);
    /** @short Ask the server to set/unset the \\Seen flag for the indicated messages */
    void markMessagesRead(const QModelIndexList &messages, const FlagsOperation marked);

    /** @short Run the EXPUNGE command in the specified mailbox */
    void expungeMailbox(const QModelIndex &mailbox);

    /** @short Copy or move a sequence of messages between two mailboxes */
    void copyMoveMessages(TreeItemMailbox *sourceMbox, const QString &destMboxName, QList<uint> uids, const CopyMoveOperation op);

    /** @short Create a new mailbox */
    void createMailbox(const QString &name);
    /** @short Delete an existing mailbox */
    void deleteMailbox(const QString &name);

    /** @short Subscribe a mailbox */
    void subscribeMailbox(const QString &name);
    /** @short Unsubscribe a mailbox */
    void unsubscribeMailbox(const QString &name);

    /** @short Save a message into a mailbox */
    AppendTask* appendIntoMailbox(const QString &mailbox, const QByteArray &rawMessageData, const QStringList &flags,
                                  const QDateTime &timestamp);

    /** @short Save a message into a mailbox using the CATENATE extension */
    AppendTask* appendIntoMailbox(const QString &mailbox, const QList<CatenatePair> &data, const QStringList &flags,
                                  const QDateTime &timestamp);

    /** @short Issue the GENURLAUTH command for a specified part/section */
    GenUrlAuthTask *generateUrlAuthForMessage(const QString &host, const QString &user, const QString &mailbox,
                                              const uint uidValidity, const uint uid, const QString &part, const QString &access);

    /** @short Send a mail through the UID SEND mechanism */
    UidSubmitTask *sendMailViaUidSubmit(const QString &mailbox, const uint uidValidity, const uint uid,
                                        const UidSubmitOptionsList &options);


    /** @short Returns true if we are allowed to access the network */
    bool isNetworkAvailable() const { return m_netPolicy != NETWORK_OFFLINE; }
    /** @short Returns true if the network access is cheap */
    bool isNetworkOnline() const { return m_netPolicy == NETWORK_ONLINE; }

    /** @short Return a TreeItem* for a specified index

    Certain proxy models implement their own indexes. These indexes typically won't
    share the internalPointer() with the original model.  Because we use this pointer
    quite often, a method is needed to automatically go through the list of all proxy
    models and return the appropriate raw pointer.

    Upon success, a valid pointer is returned, *whichModel is set to point to the
    corresponding Model instance and *translatedIndex contains the index in the real
    underlying model. If any of these pointers is NULL, it won't get changed, of course.

    Upon failure, this function returns 0 and doesn't touch any of @arg whichModel
    and @arg translatedIndex.
    */
    static TreeItem *realTreeItem(QModelIndex index, const Model **whichModel = 0, QModelIndex *translatedIndex = 0);

    /** @short Walks the index hierarchy up until it finds a message which owns this part/message */
    static QModelIndex findMessageForItem(QModelIndex index);

    /** @short Inform the model that data for this message won't likely be requested in near future

    Model will transform the corresponding TreeItemMessage into the state similar to how it would look
    right after a fresh mailbox synchronization. All TreeItemParts will be freed, envelope and body
    structure forgotten. This will substantially reduce Model's memory usage.

    The UID and flags are not affected by this operation. The cache and any data stored in there will
    also be left intact (and would indeed be consulted instead of the network when future requests for
    this message happen again.

    */
    void releaseMessageData(const QModelIndex &message);

    /** @short Return a list of capabilities which are supported by the server */
    QStringList capabilities() const;

    /** @short Log an IMAP-related message */
    void logTrace(uint parserId, const Common::LogKind kind, const QString &source, const QString &message);
    void logTrace(const QModelIndex &relevantIndex, const Common::LogKind kind, const QString &source, const QString &message);

    /** @short Return the server's response to the ID command

    When the server indicates that the ID command is available, Trojitá will always send the ID command.  The information sent to
    the server is controlled by the trojita-imap-enable-id property; if it is set, Trojitá will tell the truth and proudly present
    herself under her own name. If it isn't set, it will just send NIL on the wire.

    The command is always sent upon each connection (ie. one protocol session, not the mailbox session).  The result of this
    function will therefore not make much sense (like be empty) when the model has always been offline.  Each reconnection updates
    the cached result.  See RFC 2971 for the semantics of the ID command.
    */
    QMap<QByteArray,QByteArray> serverId() const;

    QStringList normalizeFlags(const QStringList &source) const;

    QString imapUser() const;
    void setImapUser(const QString &imapUser);
    QString imapPassword() const;
    void setImapPassword(const QString &imapPassword);
    void unsetImapPassword();

    /** @short Return an index for the message specified by the mailbox name and the message UID */
    QModelIndex messageIndexByUid(const QString &mailboxName, const uint uid);

    /** @short Provide a list of capabilities to not use

    All blacklisted capabilities must be provided in the upper case. Capabilities are filtered only as they are reported by the
    IMAP server. Calling this function while the connection is already open and kicking might have terrible consequences.
    */
    void setCapabilitiesBlacklist(const QStringList &blacklist);

    bool isCatenateSupported() const;
    bool isGenUrlAuthSupported() const;
    bool isImapSubmissionSupported() const;

public slots:
    /** @short Ask for an updated list of mailboxes on the server */
    void reloadMailboxList();

    /** @short Try to maintain a connection to the given mailbox

      This function informs the Model that the user is interested in receiving
      updates about the mailbox state, such as about the arrival of new messages.
      The usual response to such a hint is launching the IDLE command.
    */
    void switchToMailbox(const QModelIndex &mbox);

    /** @short Get a pointer to the model visualizing the state of the tasks

    The returned object still belongs to this Imap::Mailbox::Model, and its internal working is implementation-specific.  The only
    valid method of access is through the usual Qt Interview framework.
    */
    QAbstractItemModel *taskModel() const;

    void setSslPolicy(const QList<QSslCertificate> &sslChain, const QList<QSslError> &sslErrors, bool proceed);

    void invalidateAllMessageCounts();

private slots:
    /** @short Helper for low-level state change propagation */
    void handleSocketStateChanged(Imap::Parser *parser, Imap::ConnectionState state);

    /** @short The parser has received a full line */
    void slotParserLineReceived(Imap::Parser *parser, const QByteArray &line);

    /** @short The parser has sent a block of data */
    void slotParserLineSent(Imap::Parser *parser, const QByteArray &line);

    /** @short There's been a change in the state of various tasks */
    void slotTasksChanged();

    /** @short A maintaining task is about to die */
    void slotTaskDying(QObject *obj);

signals:
    /** @short This signal is emitted then the server sent us an ALERT response code */
    void alertReceived(const QString &message);
    /** @short The network went offline

      This signal is emitted if the network connection went offline for any reason.
    Common reasons are an explicit user action or a network error.
    */
    void networkPolicyOffline();
    /** @short The network access policy got changed to "expensive" */
    void networkPolicyExpensive();
    /** @short The network is available and cheap again */
    void networkPolicyOnline();

    /** @short The network policy has changed

    This signal is emitted whenever the network policy (might) got changed to any state and for any reason. No filtering for false
    positives is done, i.e. it might be emitted even when no change has actually taken place.
    */
    void networkPolicyChanged();

    /** @short A connection error has been encountered */
    void connectionError(const QString &message);
    /** @short The server requests the user to authenticate

    The user is expected to file username and password through setting the "imapUser" and "imapPassword" properties.  The imapUser
    shall be set at first (if needed); a change to imapPassword will trigger the authentication process.
    */
    void authRequested();

    /** @short The authentication attempt has failed

    Slots attached to his signal should display an appropriate message to the user and (if applicable) also invalidate
    the cached credentials.  The credentials be requested when the model decides to try logging in again via the usual
    authRequested() function.
    */
    void authAttemptFailed(const QString &message);

    /** @short Signal the need for a decision about accepting a particular SSL state

    This signal is emitted in case a conneciton has hit a series of SSL errors which has not been encountered before.  The rest
    of the code shall make a decision about whether the presented sequence of errors is safe to allow and call the setSslPolicy()
    with the passed list of errors and an instruction whether to continue or not.
    */
    void needsSslDecision(const QList<QSslCertificate> &certificates, const QList<QSslError> &sslErrors);

    /** @short Inform the user that it is advised to enable STARTTLS in future connection attempts */
    void requireStartTlsInFuture();

    /** @short The amount of messages in the indicated mailbox might have changed */
    void messageCountPossiblyChanged(const QModelIndex &mailbox);

    /** @short We've succeeded to create the given mailbox */
    void mailboxCreationSucceded(const QString &mailbox);
    /** @short The mailbox creation failed for some reason */
    void mailboxCreationFailed(const QString &mailbox, const QString &message);
    /** @short We've succeeded to delete a mailbox */
    void mailboxDeletionSucceded(const QString &mailbox);
    /** @short Mailbox deletion failed */
    void mailboxDeletionFailed(const QString &mailbox, const QString &message);

    /** @short Inform the GUI about the progress of a connection */
    void connectionStateChanged(QObject *parser, Imap::ConnectionState state);   // got to use fully qualified namespace here

    /** @short The parser has encountered a fatal error */
    void logParserFatalError(uint parser, const QString &exceptionClass, const QString &message, const QByteArray &line, int position);

    void mailboxSyncingProgress(const QModelIndex &mailbox, Imap::Mailbox::MailboxSyncingProgress state);

    void mailboxFirstUnseenMessage(const QModelIndex &maillbox, const QModelIndex &message);

    /** @short Threading has arrived */
    void threadingAvailable(const QModelIndex &mailbox, const QByteArray &algorithm,
                            const QStringList &searchCriteria, const QVector<Imap::Responses::ThreadingNode> &mapping);

    /** @short Failed to obtain threading information */
    void threadingFailed(const QModelIndex &mailbox, const QByteArray &algorithm, const QStringList &searchCriteria);

    void capabilitiesUpdated(const QStringList &capabilities);

    void logged(uint parserId, const Common::LogMessage &message);

private:
    Model &operator=(const Model &);  // don't implement
    Model(const Model &);  // don't implement


    friend class TreeItem;
    friend class TreeItemMailbox;
    friend class TreeItemMsgList;
    friend class TreeItemMessage;
    friend class TreeItemPart;
    friend class TreeItemModifiedPart; // needs access to createIndex()
    friend class MsgListModel; // needs access to createIndex()
    friend class MailboxModel; // needs access to createIndex()
    friend class ThreadingMsgListModel; // needs access to taskFactory
    friend class SubtreeClassSpecificItem<Model>; // needs access to createIndex()

    friend class IdleLauncher;

    friend class ImapTask;
    friend class FetchMsgPartTask;
    friend class UpdateFlagsTask;
    friend class UpdateFlagsOfAllMessagesTask;
    friend class ListChildMailboxesTask;
    friend class NumberOfMessagesTask;
    friend class FetchMsgMetadataTask;
    friend class ExpungeMailboxTask;
    friend class ExpungeMessagesTask;
    friend class CreateMailboxTask;
    friend class DeleteMailboxTask;
    friend class CopyMoveMessagesTask;
    friend class ObtainSynchronizedMailboxTask;
    friend class KeepMailboxOpenTask;
    friend class OpenConnectionTask;
    friend class GetAnyConnectionTask;
    friend class IdTask;
    friend class Fake_ListChildMailboxesTask;
    friend class Fake_OpenConnectionTask;
    friend class NoopTask;
    friend class ThreadTask;
    friend class UnSelectTask;
    friend class OfflineConnectionTask;
    friend class SortTask;
    friend class AppendTask;
    friend class SubscribeUnsubscribeTask;
    friend class GenUrlAuthTask;
    friend class UidSubmitTask;

    friend class TestingTaskFactory; // needs access to socketFactory
    friend class NetworkWatcher; // needs access to the network policy manipulation

    friend class ::FakeCapabilitiesInjector; // for injecting fake capabilities
    friend class ::ImapModelIdleTest; // needs access to findTaskResponsibleFor() for IDLE testing
    friend class TaskPresentationModel; // needs access to the ParserState
    friend class ::LibMailboxSync; // needs access to accessParser/ParserState

    friend class Composer::ImapMessageAttachmentItem; // needs access to findMailboxByName and findMessagesByUids
    friend class Composer::ImapPartAttachmentItem; // dtto
    friend class Composer::MessageComposer; // dtto

    void askForChildrenOfMailbox(TreeItemMailbox *item, bool forceReload);
    void askForMessagesInMailbox(TreeItemMsgList *item);
    void askForNumberOfMessages(TreeItemMsgList *item);

    typedef enum {PRELOAD_PER_POLICY, PRELOAD_DISABLED} PreloadingMode;

    void askForMsgMetadata(TreeItemMessage *item, PreloadingMode preloadMode);
    void askForMsgPart(TreeItemPart *item, bool onlyFromCache=false);

    void finalizeList(Parser *parser, TreeItemMailbox *const mailboxPtr);
    void finalizeIncrementalList(Parser *parser, const QString &parentMailboxName);
    void finalizeFetchPart(TreeItemMailbox *const mailbox, const uint sequenceNo, const QString &partId);
    void genericHandleFetch(TreeItemMailbox *mailbox, const Imap::Responses::Fetch *const resp);

    void replaceChildMailboxes(TreeItemMailbox *mailboxPtr, const TreeItemChildrenList &mailboxes);
    void updateCapabilities(Parser *parser, const QStringList capabilities);

    TreeItem *translatePtr(const QModelIndex &index) const;

    void emitMessageCountChanged(TreeItemMailbox *const mailbox);

    TreeItemMailbox *findMailboxByName(const QString &name) const;
    TreeItemMailbox *findMailboxByName(const QString &name, const TreeItemMailbox *const root) const;
    TreeItemMailbox *findParentMailboxByName(const QString &name) const;
    QList<TreeItemMessage *> findMessagesByUids(const TreeItemMailbox *const mailbox, const QList<uint> &uids);
    TreeItemChildrenList::iterator findMessageOrNextOneByUid(TreeItemMsgList *list, const uint uid);

    static TreeItemMailbox *mailboxForSomeItem(QModelIndex index);

    void saveUidMap(TreeItemMsgList *list);

    /** @short Return a corresponding KeepMailboxOpenTask for a given mailbox */
    KeepMailboxOpenTask *findTaskResponsibleFor(const QModelIndex &mailbox);
    KeepMailboxOpenTask *findTaskResponsibleFor(TreeItemMailbox *mailboxPtr);

    /** @short Find a mailbox which is expected to be common for all passed items

    The @arg items is expected to consists of message parts or messages themselves.
    If they belong to different mailboxes, an exception is thrown.
    */
    QModelIndex findMailboxForItems(const QModelIndexList &items);

    NetworkPolicy networkPolicy() const { return m_netPolicy; }
    void setNetworkPolicy(const NetworkPolicy policy);

    /** @short Helper function for changing connection state */
    void changeConnectionState(Parser *parser, ConnectionState state);

    void processSslErrors(OpenConnectionTask *task);

    /** @short Is the reason for killing the parser an expected one? */
    typedef enum {
        PARSER_KILL_EXPECTED, /**< @short Normal operation */
        PARSER_KILL_HARD, /**< @short Sudden, unexpected death */
        PARSER_JUST_DELETE_LATER /**< @short Just call deleteLater(), nothing else */
    } ParserKillingMethod;

    /** @short Dispose of the parser in a C++-safe way */
    void killParser(Parser *parser, ParserKillingMethod method=PARSER_KILL_HARD);

    ParserState &accessParser(Parser *parser);

    /** @short Helper for the slotParseError() */
    void broadcastParseError(const uint parser, const QString &exceptionClass, const QString &errorMessage, const QByteArray &line, int position);

    void responseReceived(const QMap<Parser *,ParserState>::iterator it);

    /** @short Remove deleted Tasks from the activeTasks list */
    void removeDeletedTasks(const QList<ImapTask *> &deletedTasks, QList<ImapTask *> &activeTasks);

    void informTasksAboutNewPassword();

    QStringList onlineMessageFetch;

    /** @short Model visualizing the state of the tasks */
    TaskPresentationModel *m_taskModel;

    QMap<QByteArray,QByteArray> m_idResult;

    QHash<QString,QString> m_specialFlagNames;
    mutable QSet<QString> m_flagLiterals;

    /** @short Username for login */
    QString m_imapUser;
    /** @short Cached copy of the IMAP password */
    QString m_imapPassword;
    /** @short Is the IMAP password cached in the Model already? */
    bool m_hasImapPassword;

    QTimer *m_periodicMailboxNumbersRefresh;

    QStringList m_capabilitiesBlacklist;

protected slots:
    void responseReceived();
    void responseReceived(Imap::Parser *parser);
    void askForChildrenOfMailbox(const QModelIndex &index, const Imap::Mailbox::CacheLoadingMode cacheMode);
    void askForTopLevelChildren(const Imap::Mailbox::CacheLoadingMode cacheMode);
    void askForMessagesInMailbox(const QModelIndex &index);

    void runReadyTasks();

#ifdef TROJITA_DEBUG_TASK_TREE
    void checkTaskTreeConsistency();
    void checkDependentTasksConsistency(Parser *parser, ImapTask *task, ImapTask *expectedParentTask, int depth);
#endif

};

}

}

#endif /* IMAP_MODEL_H */
