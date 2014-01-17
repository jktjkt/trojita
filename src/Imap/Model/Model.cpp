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

#include <QAbstractProxyModel>
#include <QAuthenticator>
#include <QCoreApplication>
#include <QDebug>
#include <QtAlgorithms>
#include "Model.h"
#include "MailboxTree.h"
#include "QAIM_reset.h"
#include "SpecialFlagNames.h"
#include "TaskPresentationModel.h"
#include "Common/FindWithUnknown.h"
#include "Common/InvokeMethod.h"
#include "Imap/Encoders.h"
#include "Imap/Tasks/AppendTask.h"
#include "Imap/Tasks/GetAnyConnectionTask.h"
#include "Imap/Tasks/KeepMailboxOpenTask.h"
#include "Imap/Tasks/OpenConnectionTask.h"
#include "Imap/Tasks/UpdateFlagsTask.h"
#include "Streams/SocketFactory.h"

//#define DEBUG_PERIODICALLY_DUMP_TASKS
//#define DEBUG_TASK_ROUTING

namespace
{

using namespace Imap::Mailbox;

/** @short Return true iff the two mailboxes have the same name

It's an error to call this function on anything else but a mailbox.
*/
bool MailboxNamesEqual(const TreeItem *const a, const TreeItem *const b)
{
    const TreeItemMailbox *const mailboxA = dynamic_cast<const TreeItemMailbox *const>(a);
    const TreeItemMailbox *const mailboxB = dynamic_cast<const TreeItemMailbox *const>(b);
    Q_ASSERT(mailboxA);
    Q_ASSERT(mailboxB);

    return mailboxA->mailbox() == mailboxB->mailbox();
}

/** @short Mailbox name comparator to be used when sorting mailbox names

The special-case mailbox name, the "INBOX", is always sorted as the first one.
*/
bool MailboxNameComparator(const TreeItem *const a, const TreeItem *const b)
{
    const TreeItemMailbox *const mailboxA = dynamic_cast<const TreeItemMailbox *const>(a);
    const TreeItemMailbox *const mailboxB = dynamic_cast<const TreeItemMailbox *const>(b);

    if (mailboxA->mailbox() == QLatin1String("INBOX"))
        return true;
    if (mailboxB->mailbox() == QLatin1String("INBOX"))
        return false;
    return mailboxA->mailbox().compare(mailboxB->mailbox(), Qt::CaseInsensitive) < 1;
}

bool uidComparator(const TreeItem *const item, const uint uid)
{
    const TreeItemMessage *const message = static_cast<const TreeItemMessage *const>(item);
    uint messageUid = message->uid();
    Q_ASSERT(messageUid);
    return messageUid < uid;
}

bool messageHasUidZero(const TreeItem *const item)
{
    const TreeItemMessage *const message = static_cast<const TreeItemMessage *const>(item);
    return message->uid() == 0;
}

}

namespace Imap
{
namespace Mailbox
{

Model::Model(QObject *parent, AbstractCache *cache, SocketFactoryPtr socketFactory, TaskFactoryPtr taskFactory):
    // parent
    QAbstractItemModel(parent),
    // our tools
    m_cache(cache), m_socketFactory(std::move(socketFactory)), m_taskFactory(std::move(taskFactory)), m_maxParsers(4), m_mailboxes(0),
    m_netPolicy(NETWORK_OFFLINE),  m_taskModel(0), m_hasImapPassword(false)
{
    m_cache->setParent(this);
    m_startTls = m_socketFactory->startTlsRequired();

    m_mailboxes = new TreeItemMailbox(0);

    onlineMessageFetch << QLatin1String("ENVELOPE") << QLatin1String("BODYSTRUCTURE") << QLatin1String("RFC822.SIZE") <<
                          QLatin1String("UID") << QLatin1String("FLAGS");

    EMIT_LATER_NOARG(this, networkPolicyOffline);
    EMIT_LATER_NOARG(this, networkPolicyChanged);

#ifdef DEBUG_PERIODICALLY_DUMP_TASKS
    QTimer *periodicTaskDumper = new QTimer(this);
    periodicTaskDumper->setInterval(1000);
    connect(periodicTaskDumper, SIGNAL(timeout()), this, SLOT(slotTasksChanged()));
    periodicTaskDumper->start();
#endif

    m_taskModel = new TaskPresentationModel(this);

    m_specialFlagNames[QLatin1String("\\seen")] = FlagNames::seen;
    m_specialFlagNames[QLatin1String("\\deleted")] = FlagNames::deleted;
    m_specialFlagNames[QLatin1String("\\answered")] = FlagNames::answered;
    m_specialFlagNames[QLatin1String("\\recent")] = FlagNames::recent;
    m_specialFlagNames[QLatin1String("$forwarded")] = FlagNames::forwarded;

    m_periodicMailboxNumbersRefresh = new QTimer(this);
    // polling every five minutes
    m_periodicMailboxNumbersRefresh->setInterval(5 * 60 * 1000);
    connect(m_periodicMailboxNumbersRefresh, SIGNAL(timeout()), this, SLOT(invalidateAllMessageCounts()));
}

Model::~Model()
{
    delete m_mailboxes;
}

/** @short Process responses from all sockets */
void Model::responseReceived()
{
    for (QMap<Parser *,ParserState>::iterator it = m_parsers.begin(); it != m_parsers.end(); ++it) {
        responseReceived(it);
    }
}

/** @short Process responses from the specified parser */
void Model::responseReceived(Parser *parser)
{
    QMap<Parser *,ParserState>::iterator it = m_parsers.find(parser);
    if (it == m_parsers.end()) {
        // This is a queued signal, so it's perfectly possible that the sender is gone already
        return;
    }
    responseReceived(it);
}

/** @short Process responses from the specified parser */
void Model::responseReceived(const QMap<Parser *,ParserState>::iterator it)
{
    Q_ASSERT(it->parser);

    int counter = 0;
    while (it->parser && it->parser->hasResponse()) {
        QSharedPointer<Imap::Responses::AbstractResponse> resp = it->parser->getResponse();
        Q_ASSERT(resp);
        // Always log BAD responses from a central place. They're bad enough to warant an extra treatment.
        // FIXME: is it worth an UI popup?
        if (Responses::State *stateResponse = dynamic_cast<Responses::State *>(resp.data())) {
            if (stateResponse->kind == Responses::BAD) {
                QString buf;
                QTextStream s(&buf);
                s << *stateResponse;
                logTrace(it->parser->parserId(), Common::LOG_OTHER, QLatin1String("Model"), QString::fromUtf8("BAD response: %1").arg(buf));
                qDebug() << buf;
            }
        }
        try {
            /* At this point, we want to iterate over all active tasks and try them
            for processing the server's responses (the plug() method). However, this
            is rather complex -- this call to plug() could result in signals being
            emitted, and certain slots connected to those signals might in turn want
            to queue more Tasks. Therefore, it->activeTasks could be modified, some
            items could be appended to it using the QList::append, which in turn could
            cause a realloc to happen, happily invalidating our iterators, and that
            kind of sucks.

            So, we have to iterate over a copy of the original list and instead of
            deleting Tasks, we store them into a temporary list. When we're done with
            processing, we walk the original list once again and simply remove all
            "deleted" items for real.

            This took me 3+ hours to track it down to what the hell was happening here,
            even though the underlying reason is simple -- QList::append() could invalidate
            existing iterators.
            */

            bool handled = false;
            QList<ImapTask *> taskSnapshot = it->activeTasks;
            QList<ImapTask *> deletedTasks;
            QList<ImapTask *>::const_iterator taskEnd = taskSnapshot.constEnd();

            // Try various tasks, perhaps it's their response. Also check if they're already finished and remove them.
            for (QList<ImapTask *>::const_iterator taskIt = taskSnapshot.constBegin(); taskIt != taskEnd; ++taskIt) {
                if (! handled) {

#ifdef DEBUG_TASK_ROUTING
                    try {
                        logTrace(it->parser->parserId(), Common::LOG_TASKS, QString(),
                                 QString::fromAscii("Routing to %1 %2").arg((*taskIt)->metaObject()->className(),
                                                                            (*taskIt)->debugIdentification()));
#endif
                    handled = resp->plug(*taskIt);
#ifdef DEBUG_TASK_ROUTING
                        if (handled) {
                            logTrace(it->parser->parserId(), Common::LOG_TASKS, (*taskIt)->debugIdentification(), QLatin1String("Handled"));
                        }
                    } catch (std::exception &e) {
                        logTrace(it->parser->parserId(), Common::LOG_TASKS, (*taskIt)->debugIdentification(), QLatin1String("Got exception when handling"));
                        throw;
                    }
#endif
                }

                if ((*taskIt)->isFinished()) {
                    deletedTasks << *taskIt;
                }
            }

            removeDeletedTasks(deletedTasks, it->activeTasks);

            runReadyTasks();

            if (! handled) {
                resp->plug(it->parser, this);
#ifdef DEBUG_TASK_ROUTING
                if (it->parser) {
                    logTrace(it->parser->parserId(), Common::LOG_TASKS, QLatin1String("Model"), QLatin1String("Handled"));
                } else {
                    logTrace(0, Common::LOG_TASKS, QLatin1String("Model"), QLatin1String("Handled"));
                }
#endif
            }
        } catch (Imap::StartTlsFailed &e) {
            uint parserId = it->parser->parserId();
            killParser(it->parser, PARSER_KILL_HARD);
            logTrace(parserId, Common::LOG_PARSE_ERROR, QString::fromStdString(e.exceptionClass()), QLatin1String("STARTTLS has failed"));
            EMIT_LATER(this, connectionError, Q_ARG(QString, tr("<p>The server has refused to start the encryption through the STARTTLS command.</p>")));
            setNetworkPolicy(NETWORK_OFFLINE);
            break;
        } catch (Imap::ImapException &e) {
            uint parserId = it->parser->parserId();
            killParser(it->parser, PARSER_KILL_HARD);
            broadcastParseError(parserId, QString::fromStdString(e.exceptionClass()), e.what(), e.line(), e.offset());
            break;
        }

        // Return to the event loop every 100 messages to handle GUI events
        ++counter;
        if (counter == 100) {
            QTimer::singleShot(0, this, SLOT(responseReceived()));
            break;
        }
    }

    if (!it->parser) {
        // He's dead, Jim
        killParser(it.key(), PARSER_JUST_DELETE_LATER);
        m_parsers.erase(it);
        RESET_MODEL_2(m_taskModel);
    }
}

void Model::handleState(Imap::Parser *ptr, const Imap::Responses::State *const resp)
{
    // OK/NO/BAD/PREAUTH/BYE
    using namespace Imap::Responses;

    const QString &tag = resp->tag;

    if (!tag.isEmpty()) {
        if (tag == accessParser(ptr).logoutCmd) {
            // The LOGOUT is special, as it isn't associated with any task
            killParser(ptr, PARSER_KILL_EXPECTED);
        } else {
            if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
                return;
            // Unhandled command -- this is *extremely* weird
            throw CantHappen("The following command should have been handled elsewhere", *resp);
        }
    } else {
        // untagged response
        // FIXME: we should probably just eat them and don't bother, as untagged OK/NO could be rather common...
        switch (resp->kind) {
        case BYE:
            if (accessParser(ptr).logoutCmd.isEmpty()) {
                // The connection got closed but we haven't really requested that -- we better treat that as error, including
                // going offline...
                // ... but before that, expect that the connection will get closed soon
                accessParser(ptr).connState = CONN_STATE_LOGOUT;
                EMIT_LATER(this, connectionError, Q_ARG(QString, resp->message));
                setNetworkPolicy(NETWORK_OFFLINE);
            }
            if (accessParser(ptr).parser) {
                // previous block could enter the event loop and hence kill our parser; we shouldn't try to kill it twice
                killParser(ptr, PARSER_KILL_EXPECTED);
            }
            break;
        case OK:
            if (resp->respCode == NONE) {
                // This one probably should not be logged at all; dovecot sends these reponses to keep NATted connections alive
                break;
            } else {
                logTrace(ptr->parserId(), Common::LOG_OTHER, QString(), QLatin1String("Warning: unhandled untagged OK with a response code"));
                break;
            }
        case NO:
            logTrace(ptr->parserId(), Common::LOG_OTHER, QString(), QLatin1String("Warning: unhandled untagged NO..."));
            break;
        default:
            throw UnexpectedResponseReceived("Unhandled untagged response, sorry", *resp);
        }
    }
}

void Model::finalizeList(Parser *parser, TreeItemMailbox *mailboxPtr)
{
    TreeItemChildrenList mailboxes;

    QList<Responses::List> &listResponses = accessParser(parser).listResponses;
    const QString prefix = mailboxPtr->mailbox() + mailboxPtr->separator();
    for (QList<Responses::List>::iterator it = listResponses.begin(); it != listResponses.end(); /* nothing */) {
        if (it->mailbox == mailboxPtr->mailbox() || it->mailbox == prefix) {
            // rubbish, ignore
            it = listResponses.erase(it);
        } else if (it->mailbox.startsWith(prefix)) {
            mailboxes << new TreeItemMailbox(mailboxPtr, *it);
            it = listResponses.erase(it);
        } else {
            // it clearly is someone else's LIST response
            ++it;
        }
    }
    qSort(mailboxes.begin(), mailboxes.end(), MailboxNameComparator);

    // Remove duplicates; would be great if this could be done in a STLish way,
    // but unfortunately std::unique won't help here (the "duped" part of the
    // sequence contains undefined items)
    if (mailboxes.size() > 1) {
        auto it = mailboxes.begin();
        // We've got to ignore the first one, that's the message list
        ++it;
        while (it != mailboxes.end()) {
            if (MailboxNamesEqual(it[-1], *it)) {
                delete *it;
                it = mailboxes.erase(it);
            } else {
                ++it;
            }
        }
    }

    QList<MailboxMetadata> metadataToCache;
    TreeItemChildrenList mailboxesWithoutChildren;
    for (auto it = mailboxes.constBegin(); it != mailboxes.constEnd(); ++it) {
        TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(*it);
        Q_ASSERT(mailbox);
        metadataToCache.append(mailbox->mailboxMetadata());
        if (mailbox->hasNoChildMailboxesAlreadyKnown()) {
            mailboxesWithoutChildren << mailbox;
        }
    }
    cache()->setChildMailboxes(mailboxPtr->mailbox(), metadataToCache);
    for (auto it = mailboxesWithoutChildren.constBegin(); it != mailboxesWithoutChildren.constEnd(); ++it)
        cache()->setChildMailboxes(static_cast<TreeItemMailbox *>(*it)->mailbox(), QList<MailboxMetadata>());
    replaceChildMailboxes(mailboxPtr, mailboxes);
}

void Model::finalizeIncrementalList(Parser *parser, const QString &parentMailboxName)
{
    TreeItemMailbox *parentMbox = findParentMailboxByName(parentMailboxName);
    if (! parentMbox) {
        qDebug() << "Weird, no idea where to put the newly created mailbox" << parentMailboxName;
        return;
    }

    QList<TreeItem *> mailboxes;

    QList<Responses::List> &listResponses = accessParser(parser).listResponses;
    for (QList<Responses::List>::iterator it = listResponses.begin();
         it != listResponses.end(); /* nothing */) {
        if (it->mailbox == parentMailboxName) {
            mailboxes << new TreeItemMailbox(parentMbox, *it);
            it = listResponses.erase(it);
        } else {
            // it clearly is someone else's LIST response
            ++it;
        }
    }
    qSort(mailboxes.begin(), mailboxes.end(), MailboxNameComparator);

    if (mailboxes.size() == 0) {
        qDebug() << "Weird, no matching LIST response for our prompt after CREATE";
        qDeleteAll(mailboxes);
        return;
    } else if (mailboxes.size() > 1) {
        qDebug() << "Weird, too many LIST responses for our prompt after CREATE";
        qDeleteAll(mailboxes);
        return;
    }

    auto it = parentMbox->m_children.begin();
    Q_ASSERT(it != parentMbox->m_children.end());
    ++it;
    while (it != parentMbox->m_children.end() && MailboxNameComparator(*it, mailboxes[0]))
        ++it;
    QModelIndex parentIdx = parentMbox == m_mailboxes ? QModelIndex() : parentMbox->toIndex(this);
    if (it == parentMbox->m_children.end())
        beginInsertRows(parentIdx, parentMbox->m_children.size(), parentMbox->m_children.size());
    else
        beginInsertRows(parentIdx, (*it)->row(), (*it)->row());
    parentMbox->m_children.insert(it, mailboxes[0]);
    endInsertRows();
}

void Model::replaceChildMailboxes(TreeItemMailbox *mailboxPtr, const TreeItemChildrenList &mailboxes)
{
    /* Previously, we would call layoutAboutToBeChanged() and layoutChanged() here, but it
       resulted in invalid memory access in the attached QSortFilterProxyModels like this one:

    ==23294== Invalid read of size 4
    ==23294==    at 0x5EA34B1: QSortFilterProxyModelPrivate::index_to_iterator(QModelIndex const&) const (qsortfilterproxymodel.cpp:191)
    ==23294==    by 0x5E9F8A3: QSortFilterProxyModel::parent(QModelIndex const&) const (qsortfilterproxymodel.cpp:1654)
    ==23294==    by 0x5C5D45D: QModelIndex::parent() const (qabstractitemmodel.h:389)
    ==23294==    by 0x5E47C48: QTreeView::drawRow(QPainter*, QStyleOptionViewItem const&, QModelIndex const&) const (qtreeview.cpp:1479)
    ==23294==    by 0x5E479D9: QTreeView::drawTree(QPainter*, QRegion const&) const (qtreeview.cpp:1441)
    ==23294==    by 0x5E4703A: QTreeView::paintEvent(QPaintEvent*) (qtreeview.cpp:1274)
    ==23294==    by 0x5810C30: QWidget::event(QEvent*) (qwidget.cpp:8346)
    ==23294==    by 0x5C91D03: QFrame::event(QEvent*) (qframe.cpp:557)
    ==23294==    by 0x5D4259C: QAbstractScrollArea::viewportEvent(QEvent*) (qabstractscrollarea.cpp:1043)
    ==23294==    by 0x5DFFD6E: QAbstractItemView::viewportEvent(QEvent*) (qabstractitemview.cpp:1619)
    ==23294==    by 0x5E46EE0: QTreeView::viewportEvent(QEvent*) (qtreeview.cpp:1256)
    ==23294==    by 0x5D43110: QAbstractScrollAreaPrivate::viewportEvent(QEvent*) (qabstractscrollarea_p.h:100)
    ==23294==  Address 0x908dbec is 20 bytes inside a block of size 24 free'd
    ==23294==    at 0x4024D74: operator delete(void*) (vg_replace_malloc.c:346)
    ==23294==    by 0x5EA5236: void qDeleteAll<QHash<QModelIndex, QSortFilterProxyModelPrivate::Mapping*>::const_iterator>(QHash<QModelIndex, QSortFilterProxyModelPrivate::Mapping*>::const_iterator, QHash<QModelIndex, QSortFilterProxyModelPrivate::Mapping*>::const_iterator) (qalgorithms.h:322)
    ==23294==    by 0x5EA3C06: void qDeleteAll<QHash<QModelIndex, QSortFilterProxyModelPrivate::Mapping*> >(QHash<QModelIndex, QSortFilterProxyModelPrivate::Mapping*> const&) (qalgorithms.h:330)
    ==23294==    by 0x5E9E64B: QSortFilterProxyModelPrivate::_q_sourceLayoutChanged() (qsortfilterproxymodel.cpp:1249)
    ==23294==    by 0x5EA29EC: QSortFilterProxyModel::qt_metacall(QMetaObject::Call, int, void**) (moc_qsortfilterproxymodel.cpp:133)
    ==23294==    by 0x80EB205: Imap::Mailbox::PrettyMailboxModel::qt_metacall(QMetaObject::Call, int, void**) (moc_PrettyMailboxModel.cpp:64)
    ==23294==    by 0x65D3EAD: QMetaObject::metacall(QObject*, QMetaObject::Call, int, void**) (qmetaobject.cpp:237)
    ==23294==    by 0x65E8D7C: QMetaObject::activate(QObject*, QMetaObject const*, int, void**) (qobject.cpp:3272)
    ==23294==    by 0x664A7E8: QAbstractItemModel::layoutChanged() (moc_qabstractitemmodel.cpp:161)
    ==23294==    by 0x664A354: QAbstractItemModel::qt_metacall(QMetaObject::Call, int, void**) (moc_qabstractitemmodel.cpp:118)
    ==23294==    by 0x5E9A3A9: QAbstractProxyModel::qt_metacall(QMetaObject::Call, int, void**) (moc_qabstractproxymodel.cpp:67)
    ==23294==    by 0x80EAF3D: Imap::Mailbox::MailboxModel::qt_metacall(QMetaObject::Call, int, void**) (moc_MailboxModel.cpp:81)

       I have no idea why something like that happens -- layoutChanged() should be a hint that the indexes are gone now, which means
       that the code should *not* use tham after that point. That's just weird.
    */

    QModelIndex parent = mailboxPtr == m_mailboxes ? QModelIndex() : mailboxPtr->toIndex(this);

    if (mailboxPtr->m_children.size() != 1) {
        // There's something besides the TreeItemMsgList and we're going to
        // overwrite them, so we have to delete them right now
        int count = mailboxPtr->rowCount(this);
        beginRemoveRows(parent, 1, count - 1);
        auto oldItems = mailboxPtr->setChildren(TreeItemChildrenList());
        endRemoveRows();

        qDeleteAll(oldItems);
    }

    if (! mailboxes.isEmpty()) {
        beginInsertRows(parent, 1, mailboxes.size());
        auto dummy = mailboxPtr->setChildren(mailboxes);
        endInsertRows();
        Q_ASSERT(dummy.isEmpty());
    } else {
        auto dummy = mailboxPtr->setChildren(mailboxes);
        Q_ASSERT(dummy.isEmpty());
    }
    emit dataChanged(parent, parent);
}

void Model::emitMessageCountChanged(TreeItemMailbox *const mailbox)
{
    TreeItemMsgList *list = static_cast<TreeItemMsgList *>(mailbox->m_children[0]);
    QModelIndex msgListIndex = list->toIndex(this);
    emit dataChanged(msgListIndex, msgListIndex);
    emit messageCountPossiblyChanged(mailbox->toIndex(this));
}

/** @short Retrieval of a message part has completed */
void Model::finalizeFetchPart(TreeItemMailbox *const mailbox, const uint sequenceNo, const QString &partId)
{
    // At first, verify that the message itself is marked as loaded.
    // If it isn't, it's probably because of Model::releaseMessageData().
    TreeItem *item = mailbox->m_children[0]; // TreeItemMsgList
    item = item->child(sequenceNo - 1, this);   // TreeItemMessage
    Q_ASSERT(item);   // FIXME: or rather throw an exception?
    if (item->accessFetchStatus() == TreeItem::NONE) {
        // ...and it indeed got released, so let's just return and don't try to check anything
        return;
    }

    TreeItemPart *part = mailbox->partIdToPtr(this, static_cast<TreeItemMessage *>(item), partId);
    if (! part) {
        qDebug() << "Can't verify part fetching status: part is not here!";
        return;
    }
    if (part->loading()) {
        // basically, there's nothing to do if the FETCH targetted a message part and not the message as a whole
        qDebug() << "Imap::Model::_finalizeFetch(): didn't receive anything about message" <<
                 part->message()->row() << "part" << part->partId();
        part->setFetchStatus(TreeItem::DONE);
    }
}

void Model::handleCapability(Imap::Parser *ptr, const Imap::Responses::Capability *const resp)
{
    updateCapabilities(ptr, resp->capabilities);
}

void Model::handleNumberResponse(Imap::Parser *ptr, const Imap::Responses::NumberResponse *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("[Tasks API Port] Unhandled NumberResponse", *resp);
}

void Model::handleList(Imap::Parser *ptr, const Imap::Responses::List *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    accessParser(ptr).listResponses << *resp;
}

void Model::handleFlags(Imap::Parser *ptr, const Imap::Responses::Flags *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("[Tasks API Port] Unhandled Flags", *resp);
}

void Model::handleSearch(Imap::Parser *ptr, const Imap::Responses::Search *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("[Tasks API Port] Unhandled Search", *resp);
}

void Model::handleESearch(Imap::Parser *ptr, const Imap::Responses::ESearch *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("Unhandled ESEARCH", *resp);
}

void Model::handleStatus(Imap::Parser *ptr, const Imap::Responses::Status *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    Q_UNUSED(ptr);
    TreeItemMailbox *mailbox = findMailboxByName(resp->mailbox);
    if (! mailbox) {
        qDebug() << "Couldn't find out which mailbox is" << resp->mailbox << "when parsing a STATUS reply";
        return;
    }
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(mailbox->m_children[0]);
    Q_ASSERT(list);
    if (resp->states.contains(Imap::Responses::Status::MESSAGES))
        list->m_totalMessageCount = resp->states[ Imap::Responses::Status::MESSAGES ];
    if (resp->states.contains(Imap::Responses::Status::UNSEEN))
        list->m_unreadMessageCount = resp->states[ Imap::Responses::Status::UNSEEN ];
    if (resp->states.contains(Imap::Responses::Status::RECENT))
        list->m_recentMessageCount = resp->states[ Imap::Responses::Status::RECENT ];
    list->m_numberFetchingStatus = TreeItem::DONE;
    emitMessageCountChanged(mailbox);
}

void Model::handleFetch(Imap::Parser *ptr, const Imap::Responses::Fetch *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("[Tasks API Port] Unhandled Fetch", *resp);
}

void Model::handleNamespace(Imap::Parser *ptr, const Imap::Responses::Namespace *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    return; // because it's broken and won't fly
    Q_UNUSED(ptr);
    Q_UNUSED(resp);
}

void Model::handleSort(Imap::Parser *ptr, const Imap::Responses::Sort *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("[Tasks API Port] Unhandled Sort", *resp);
}

void Model::handleThread(Imap::Parser *ptr, const Imap::Responses::Thread *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("[Tasks API Port] Unhandled Thread", *resp);
}

void Model::handleId(Parser *ptr, const Responses::Id *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("Unhandled ID response", *resp);
}

void Model::handleEnabled(Parser *ptr, const Responses::Enabled *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("Unhandled ENABLED response", *resp);
}

void Model::handleVanished(Parser *ptr, const Responses::Vanished *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("Unhandled VANISHED response", *resp);
}

void Model::handleGenUrlAuth(Parser *ptr, const Responses::GenUrlAuth *const resp)
{
    if (accessParser(ptr).connState == CONN_STATE_LOGOUT)
        return;
    throw UnexpectedResponseReceived("Unhandled GENURLAUTH response", *resp);
}

void Model::handleSocketEncryptedResponse(Parser *ptr, const Responses::SocketEncryptedResponse *const resp)
{
    Q_UNUSED(resp);
    logTrace(ptr->parserId(), Common::LOG_IO_READ, "Model", "Information about the SSL state not handled by the upper layers -- disconnect?");
    killParser(ptr, PARSER_KILL_EXPECTED);
}

TreeItem *Model::translatePtr(const QModelIndex &index) const
{
    return index.internalPointer() ? static_cast<TreeItem *>(index.internalPointer()) : m_mailboxes;
}

QVariant Model::data(const QModelIndex &index, int role) const
{
    return translatePtr(index)->data(const_cast<Model *>(this), role);
}

QModelIndex Model::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid()) {
        Q_ASSERT(parent.model() == this);
    }

    TreeItem *parentItem = translatePtr(parent);

    // Deal with the possibility of an "irregular shape" of our model here.
    // The issue is that some items have child items not only in column #0
    // and in specified number of rows, but also in row #0 and various columns.
    if (column != 0) {
        TreeItem *item = parentItem->specialColumnPtr(row, column);
        if (item)
            return QAbstractItemModel::createIndex(row, column, item);
        else
            return QModelIndex();
    }

    TreeItem *child = parentItem->child(row, const_cast<Model *>(this));

    return child ? QAbstractItemModel::createIndex(row, column, child) : QModelIndex();
}

QModelIndex Model::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    Q_ASSERT(index.model() == this);

    TreeItem *childItem = static_cast<TreeItem *>(index.internalPointer());
    TreeItem *parentItem = childItem->parent();

    if (! parentItem || parentItem == m_mailboxes)
        return QModelIndex();

    return QAbstractItemModel::createIndex(parentItem->row(), 0, parentItem);
}

int Model::rowCount(const QModelIndex &index) const
{
    TreeItem *node = static_cast<TreeItem *>(index.internalPointer());
    if (!node) {
        node = m_mailboxes;
    } else {
        Q_ASSERT(index.model() == this);
    }
    Q_ASSERT(node);
    return node->rowCount(const_cast<Model *>(this));
}

int Model::columnCount(const QModelIndex &index) const
{
    TreeItem *node = static_cast<TreeItem *>(index.internalPointer());
    if (!node) {
        node = m_mailboxes;
    } else {
        Q_ASSERT(index.model() == this);
    }
    Q_ASSERT(node);
    return node->columnCount();
}

bool Model::hasChildren(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        Q_ASSERT(parent.model() == this);
    }

    TreeItem *node = translatePtr(parent);

    if (node)
        return node->hasChildren(const_cast<Model *>(this));
    else
        return false;
}

void Model::askForTopLevelChildren(const CacheLoadingMode cacheMode)
{
    askForChildrenOfMailbox(m_mailboxes, cacheMode == LOAD_FORCE_RELOAD);
}

void Model::askForChildrenOfMailbox(const QModelIndex &index, const CacheLoadingMode cacheMode)
{
    if (!index.isValid())
        return;
    Q_ASSERT(index.model() == this);
    auto mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(index.internalPointer()));
    Q_ASSERT(mailbox);
    askForChildrenOfMailbox(mailbox, cacheMode == LOAD_FORCE_RELOAD);
}

void Model::askForMessagesInMailbox(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    Q_ASSERT(index.model() == this);
    auto msgList = dynamic_cast<TreeItemMsgList *>(static_cast<TreeItem *>(index.internalPointer()));
    Q_ASSERT(msgList);
    askForMessagesInMailbox(msgList);
}

void Model::askForChildrenOfMailbox(TreeItemMailbox *item, bool forceReload)
{
    if (!forceReload && cache()->childMailboxesFresh(item->mailbox())) {
        // The permanent cache contains relevant data
        QList<MailboxMetadata> metadata = cache()->childMailboxes(item->mailbox());
        TreeItemChildrenList mailboxes;
        for (QList<MailboxMetadata>::const_iterator it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
            mailboxes << TreeItemMailbox::fromMetadata(item, *it);
        }
        TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(item);
        Q_ASSERT(mailboxPtr);
        replaceChildMailboxes(mailboxPtr, mailboxes);
    } else if (networkPolicy() == NETWORK_OFFLINE) {
        // No cached data, no network -> fail
        item->setFetchStatus(TreeItem::UNAVAILABLE);
        QModelIndex idx = item->toIndex(this);
        emit dataChanged(idx, idx);
        return;
    }

    // We shall ask the network
    m_taskFactory->createListChildMailboxesTask(this, item->toIndex(this));

    QModelIndex idx = item->toIndex(this);
    emit dataChanged(idx, idx);
}

void Model::reloadMailboxList()
{
    m_mailboxes->rescanForChildMailboxes(this);
}

void Model::askForMessagesInMailbox(TreeItemMsgList *item)
{
    Q_ASSERT(item->parent());
    TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(item->parent());
    Q_ASSERT(mailboxPtr);

    QString mailbox = mailboxPtr->mailbox();

    Q_ASSERT(item->m_children.size() == 0);

    QList<uint> uidMapping = cache()->uidMapping(mailbox);
    auto oldSyncState = cache()->mailboxSyncState(mailbox);
    if (networkPolicy() == NETWORK_OFFLINE && oldSyncState.isUsableForSyncing()
            && static_cast<uint>(uidMapping.size()) != oldSyncState.exists()) {
        // Problem with the cached data
        qDebug() << "UID cache stale for mailbox" << mailbox
                 << "(" << uidMapping.size() << "in UID cache vs." << oldSyncState.exists() << "in the sync state and"
                 << item->m_totalMessageCount << "as totalMessageCount (possibly updated by STATUS))";
        item->setFetchStatus(TreeItem::UNAVAILABLE);
    } else if (networkPolicy() == NETWORK_OFFLINE && !oldSyncState.isUsableForSyncing()) {
        // Nothing in the cache
        item->setFetchStatus(TreeItem::UNAVAILABLE);
    } else if (oldSyncState.isUsableForSyncing()) {
        // We can pre-populate the tree with data from cache
        Q_ASSERT(item->m_children.isEmpty());
        Q_ASSERT(item->accessFetchStatus() == TreeItem::LOADING);
        QModelIndex listIndex = item->toIndex(this);
        if (uidMapping.size()) {
            beginInsertRows(listIndex, 0, uidMapping.size() - 1);
            for (uint seq = 0; seq < static_cast<uint>(uidMapping.size()); ++seq) {
                TreeItemMessage *message = new TreeItemMessage(item);
                message->m_offset = seq;
                message->m_uid = uidMapping[seq];
                item->m_children << message;
                QStringList flags = cache()->msgFlags(mailbox, message->m_uid);
                flags.removeOne(QLatin1String("\\Recent"));
                message->m_flags = normalizeFlags(flags);
            }
            endInsertRows();
        }
        mailboxPtr->syncState = oldSyncState;
        item->setFetchStatus(TreeItem::DONE); // required for FETCH processing later on
        // The list of messages was satisfied from cache. Do the same for the message counts, if applicable
        item->recalcVariousMessageCounts(this);
    }

    if (networkPolicy() != NETWORK_OFFLINE) {
        findTaskResponsibleFor(mailboxPtr);
        // and that's all -- the task will detect following replies and sync automatically
    }
}

void Model::askForNumberOfMessages(TreeItemMsgList *item)
{
    Q_ASSERT(item->parent());
    TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(item->parent());
    Q_ASSERT(mailboxPtr);

    if (networkPolicy() == NETWORK_OFFLINE) {
        Imap::Mailbox::SyncState syncState = cache()->mailboxSyncState(mailboxPtr->mailbox());
        if (syncState.isUsableForNumbers()) {
            item->m_unreadMessageCount = syncState.unSeenCount();
            item->m_totalMessageCount = syncState.exists();
            item->m_recentMessageCount = syncState.recent();
            item->m_numberFetchingStatus = TreeItem::DONE;
            emitMessageCountChanged(mailboxPtr);
        } else {
            item->m_numberFetchingStatus = TreeItem::UNAVAILABLE;
        }
    } else {
        m_taskFactory->createNumberOfMessagesTask(this, mailboxPtr->toIndex(this));
    }
}

void Model::askForMsgMetadata(TreeItemMessage *item, const PreloadingMode preloadMode)
{
    Q_ASSERT(item->uid());
    Q_ASSERT(!item->fetched());
    TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(item->parent());
    Q_ASSERT(list);
    TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(list->parent());
    Q_ASSERT(mailboxPtr);

    if (item->uid()) {
        AbstractCache::MessageDataBundle data = cache()->messageMetadata(mailboxPtr->mailbox(), item->uid());
        if (data.uid == item->uid()) {
            item->data()->m_envelope = data.envelope;
            item->data()->m_size = data.size;
            item->data()->m_hdrReferences = data.hdrReferences;
            item->data()->m_hdrListPost = data.hdrListPost;
            item->data()->m_hdrListPostNo = data.hdrListPostNo;
            QDataStream stream(&data.serializedBodyStructure, QIODevice::ReadOnly);
            stream.setVersion(QDataStream::Qt_4_6);
            QVariantList unserialized;
            stream >> unserialized;
            QSharedPointer<Message::AbstractMessage> abstractMessage;
            try {
                abstractMessage = Message::AbstractMessage::fromList(unserialized, QByteArray(), 0);
            } catch (Imap::ParserException &e) {
                qDebug() << "Error when parsing cached BODYSTRUCTURE" << e.what();
            }
            if (! abstractMessage) {
                item->setFetchStatus(TreeItem::UNAVAILABLE);
            } else {
                auto newChildren = abstractMessage->createTreeItems(item);
                if (item->m_children.isEmpty()) {
                    TreeItemChildrenList oldChildren = item->setChildren(newChildren);
                    Q_ASSERT(oldChildren.size() == 0);
                } else {
                    // The following assert guards against that crazy signal emitting we had when various askFor*()
                    // functions were not delayed. If it gets hit, it means that someone tried to call this function
                    // on an item which was already loaded.
                    Q_ASSERT(item->m_children.isEmpty());
                    item->setChildren(newChildren);
                }
                item->setFetchStatus(TreeItem::DONE);
            }
        }
    }

    switch (networkPolicy()) {
    case NETWORK_OFFLINE:
        if (item->accessFetchStatus() != TreeItem::DONE)
            item->setFetchStatus(TreeItem::UNAVAILABLE);
        break;
    case NETWORK_EXPENSIVE:
        if (item->accessFetchStatus() != TreeItem::DONE) {
            item->setFetchStatus(TreeItem::LOADING);
            findTaskResponsibleFor(mailboxPtr)->requestEnvelopeDownload(item->uid());
        }
        break;
    case NETWORK_ONLINE:
    {
        if (item->accessFetchStatus() != TreeItem::DONE) {
            item->setFetchStatus(TreeItem::LOADING);
            findTaskResponsibleFor(mailboxPtr)->requestEnvelopeDownload(item->uid());
        }

        // preload
        if (preloadMode != PRELOAD_PER_POLICY)
            break;
        bool ok;
        int preload = property("trojita-imap-preload-msg-metadata").toInt(&ok);
        if (! ok)
            preload = 50;
        int order = item->row();
        for (int i = qMax(0, order - preload); i < qMin(list->m_children.size(), order + preload); ++i) {
            TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(list->m_children[i]);
            Q_ASSERT(message);
            if (item != message && !message->fetched() && !message->loading() && message->uid()) {
                message->setFetchStatus(TreeItem::LOADING);
                // cannot ask the KeepTask directly, that'd completely ignore the cache
                // but we absolutely have to block the preload :)
                askForMsgMetadata(message, PRELOAD_DISABLED);
            }
        }
    }
    break;
    }
}

void Model::askForMsgPart(TreeItemPart *item, bool onlyFromCache)
{
    Q_ASSERT(item->message());   // TreeItemMessage
    Q_ASSERT(item->message()->parent());   // TreeItemMsgList
    Q_ASSERT(item->message()->parent()->parent());   // TreeItemMailbox
    TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(item->message()->parent()->parent());
    Q_ASSERT(mailboxPtr);

    // We are asking for a message part, which means that the structure of a message is already known.
    // If the UID was zero at this point, it would mean that we are completely doomed.
    uint uid = static_cast<TreeItemMessage *>(item->message())->uid();
    Q_ASSERT(uid);

    // Check whether this is a request for fetching the special item representing the raw contents prior to any CTE undoing
    TreeItemPart *itemForFetchOperation = item;
    TreeItemModifiedPart *modifiedPart = dynamic_cast<TreeItemModifiedPart*>(item);
    bool isSpecialRawPart = modifiedPart && modifiedPart->kind() == TreeItem::OFFSET_RAW_CONTENTS;
    if (isSpecialRawPart) {
        itemForFetchOperation = dynamic_cast<TreeItemPart*>(item->parent());
        Q_ASSERT(itemForFetchOperation);
    }

    const QByteArray &data = cache()->messagePart(mailboxPtr->mailbox(), uid,
                                                  isSpecialRawPart ?
                                                      itemForFetchOperation->partId() + QLatin1String(".X-RAW")
                                                    : item->partId());
    if (! data.isNull()) {
        item->m_data = data;
        item->setFetchStatus(TreeItem::DONE);
        return;
    }

    if (!isSpecialRawPart) {
        const QByteArray &data = cache()->messagePart(mailboxPtr->mailbox(), uid,
                                                      itemForFetchOperation->partId() + QLatin1String(".X-RAW"));

        if (!data.isNull()) {
            Imap::decodeContentTransferEncoding(data, item->encoding(), item->dataPtr());
            item->setFetchStatus(TreeItem::DONE);
            return;
        }

        if (item->m_partRaw && item->m_partRaw->loading()) {
            // There's already a request for the raw data. Let's use it and don't queue an extra fetch here.
            item->setFetchStatus(TreeItem::LOADING);
            return;
        }
    }

    if (networkPolicy() == NETWORK_OFFLINE) {
        if (item->accessFetchStatus() != TreeItem::DONE)
            item->setFetchStatus(TreeItem::UNAVAILABLE);
    } else if (! onlyFromCache) {
        KeepMailboxOpenTask *keepTask = findTaskResponsibleFor(mailboxPtr);
        TreeItemPart::PartFetchingMode fetchingMode = TreeItemPart::FETCH_PART_IMAP;
        if (!isSpecialRawPart && keepTask->parser && accessParser(keepTask->parser).capabilitiesFresh &&
                accessParser(keepTask->parser).capabilities.contains(QLatin1String("BINARY"))) {
            if (!item->hasChildren(0)) {
                // The BINARY only actually makes sense on leaf MIME nodes
                fetchingMode = TreeItemPart::FETCH_PART_BINARY;
            }
        }
        keepTask->requestPartDownload(item->message()->m_uid, itemForFetchOperation->partIdForFetch(fetchingMode), item->octets());
    }
}

void Model::resyncMailbox(const QModelIndex &mbox)
{
    findTaskResponsibleFor(mbox)->resynchronizeMailbox();
}

void Model::setNetworkPolicy(const NetworkPolicy policy)
{
    bool networkReconnected = m_netPolicy == NETWORK_OFFLINE && policy != NETWORK_OFFLINE;
    switch (policy) {
    case NETWORK_OFFLINE:
        for (QMap<Parser *,ParserState>::iterator it = m_parsers.begin(); it != m_parsers.end(); ++it) {
            if (!it->parser || it->connState == CONN_STATE_LOGOUT) {
                // there's no point in sending LOGOUT over these
                continue;
            }
            Q_ASSERT(it->parser);
            if (it->maintainingTask) {
                // First of all, give the maintaining task a chance to finish its housekeeping
                it->maintainingTask->stopForLogout();
            }
            // Kill all tasks that are also using this connection
            Q_FOREACH(ImapTask *task, it->activeTasks) {
                task->die();
            }
            it->logoutCmd = it->parser->logout();
            it->connState = CONN_STATE_LOGOUT;
        }
        m_netPolicy = NETWORK_OFFLINE;
        m_periodicMailboxNumbersRefresh->stop();
        emit networkPolicyChanged();
        emit networkPolicyOffline();

        // FIXME: kill the connection
        break;
    case NETWORK_EXPENSIVE:
        m_netPolicy = NETWORK_EXPENSIVE;
        m_periodicMailboxNumbersRefresh->stop();
        emit networkPolicyChanged();
        emit networkPolicyExpensive();
        break;
    case NETWORK_ONLINE:
        m_netPolicy = NETWORK_ONLINE;
        m_periodicMailboxNumbersRefresh->start();
        emit networkPolicyChanged();
        emit networkPolicyOnline();
        break;
    }

    if (networkReconnected) {
        // We're connecting after being offline
        if (m_mailboxes->accessFetchStatus() != TreeItem::NONE) {
            // We should ask for an updated list of mailboxes
            // The main reason is that this happens after entering wrong password and going back online
            reloadMailboxList();
        }
    } else if (m_netPolicy == NETWORK_ONLINE) {
        // The connection is online after some time in a different mode. Let's use this opportunity to request
        // updated message counts from all visible mailboxes.
        invalidateAllMessageCounts();
    }
}

void Model::handleSocketDisconnectedResponse(Parser *ptr, const Responses::SocketDisconnectedResponse *const resp)
{
    if (!accessParser(ptr).logoutCmd.isEmpty() || accessParser(ptr).connState == CONN_STATE_LOGOUT) {
        // If we're already scheduled for logout, don't treat connection errors as, well, errors.
        // This branch can be reached by e.g. user selecting offline after a network change, with logout
        // already on the fly.

        // But we still absolutely want to clean up and kill the connection/Parser anyway
        killParser(ptr, PARSER_KILL_EXPECTED);
    } else {
        logTrace(ptr->parserId(), Common::LOG_PARSE_ERROR, QString(), resp->message);
        killParser(ptr, PARSER_KILL_EXPECTED);
        EMIT_LATER(this, connectionError, Q_ARG(QString, resp->message));
        setNetworkPolicy(NETWORK_OFFLINE);
    }
}

void Model::handleParseErrorResponse(Imap::Parser *ptr, const Imap::Responses::ParseErrorResponse *const resp)
{
    Q_ASSERT(ptr);
    broadcastParseError(ptr->parserId(), resp->exceptionClass, resp->message, resp->line, resp->offset);
    killParser(ptr, PARSER_KILL_HARD);
}

void Model::broadcastParseError(const uint parser, const QString &exceptionClass, const QString &errorMessage, const QByteArray &line, int position)
{
    emit logParserFatalError(parser, exceptionClass, errorMessage, line, position);
    QByteArray details = (position == -1) ? QByteArray() : QByteArray(position, ' ') + QByteArray("^ here");
    logTrace(parser, Common::LOG_PARSE_ERROR, exceptionClass, QString::fromUtf8("%1\n%2\n%3").arg(errorMessage, line, details));
    QString message;
    if (exceptionClass == QLatin1String("NotAnImapServerError")) {
        QString service;
        if (line.startsWith("+OK") || line.startsWith("-ERR")) {
            service = tr("<p>It appears that you are connecting to a POP3 server. That won't work here.</p>");
        } else if (line.startsWith("220 ") || line.startsWith("220-")) {
            service = tr("<p>It appears that you are connecting to an SMTP server. That won't work here.</p>");
        }
        message = trUtf8("<h2>This is not an IMAP server</h2>"
                         "%1"
                         "<p>Please check your settings to make sure you are connecting to the IMAP service. "
                         "A typical port number for IMAP is 143 or 993.</p>"
                         "<p>The server said:</p>"
                         "<pre>%2</pre>").arg(service, QString::fromUtf8(line.constData()));
    } else {
        message = trUtf8("<p>The IMAP server sent us a reply which we could not parse. "
                         "This might either mean that there's a bug in Trojitá's code, or "
                         "that the IMAP server you are connected to is broken. Please "
                         "report this as a bug anyway. Here are the details:</p>"
                         "<p><b>%1</b>: %2</p>"
                         "<pre>%3\n%4</pre>"
                         ).arg(exceptionClass, errorMessage, line, details);
    }
    EMIT_LATER(this, connectionError, Q_ARG(QString, message));
    setNetworkPolicy(NETWORK_OFFLINE);
}

void Model::switchToMailbox(const QModelIndex &mbox)
{
    if (! mbox.isValid())
        return;

    if (m_netPolicy == NETWORK_OFFLINE)
        return;

    findTaskResponsibleFor(mbox);
}

void Model::updateCapabilities(Parser *parser, const QStringList capabilities)
{
    Q_ASSERT(parser);
    QStringList uppercaseCaps;
    Q_FOREACH(const QString& str, capabilities) {
        QString cap = str.toUpper();
        if (m_capabilitiesBlacklist.contains(cap)) {
            logTrace(parser->parserId(), Common::LOG_OTHER, QLatin1String("Model"), QString::fromUtf8("Ignoring capability \"%1\"").arg(cap));
            continue;
        }
        uppercaseCaps << cap;
    }
    accessParser(parser).capabilities = uppercaseCaps;
    accessParser(parser).capabilitiesFresh = true;
    parser->enableLiteralPlus(uppercaseCaps.contains(QLatin1String("LITERAL+")));

    for (QMap<Parser *,ParserState>::const_iterator it = m_parsers.constBegin(); it != m_parsers.constEnd(); ++it) {
        if (it->connState == CONN_STATE_LOGOUT) {
            // Skip all parsers which are currently stuck in LOGOUT
            continue;
        } else {
            // The CAPABILITIES were received by a first "usable" parser; let's treat this one as the authoritative one
            emit capabilitiesUpdated(uppercaseCaps);
        }
    }

    if (!uppercaseCaps.contains(QLatin1String("IMAP4REV1"))) {
        changeConnectionState(parser, CONN_STATE_LOGOUT);
        accessParser(parser).logoutCmd = parser->logout();
        EMIT_LATER(this, connectionError, Q_ARG(QString, tr("We aren't talking to an IMAP4 server")));
        setNetworkPolicy(NETWORK_OFFLINE);
    }
}

ImapTask *Model::setMessageFlags(const QModelIndexList &messages, const QString flag, const FlagsOperation marked)
{
    Q_ASSERT(!messages.isEmpty());
    Q_ASSERT(messages.front().model() == this);
    return m_taskFactory->createUpdateFlagsTask(this, messages, marked, QString("(%1)").arg(flag));
}

void Model::markMessagesDeleted(const QModelIndexList &messages, const FlagsOperation marked)
{
    this->setMessageFlags(messages, "\\Deleted", marked);
}

void Model::markMailboxAsRead(const QModelIndex &mailbox)
{
    if (!mailbox.isValid())
        return;

    QModelIndex index;
    realTreeItem(mailbox, 0, &index);
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.model() == this);
    Q_ASSERT(dynamic_cast<TreeItemMailbox*>(static_cast<TreeItem*>(index.internalPointer())));
    m_taskFactory->createUpdateFlagsOfAllMessagesTask(this, index, Imap::Mailbox::FLAG_ADD_SILENT, QLatin1String("\\Seen"));
}

void Model::markMessagesRead(const QModelIndexList &messages, const FlagsOperation marked)
{
    this->setMessageFlags(messages, "\\Seen", marked);
}

void Model::copyMoveMessages(TreeItemMailbox *sourceMbox, const QString &destMailboxName, QList<uint> uids, const CopyMoveOperation op)
{
    if (m_netPolicy == NETWORK_OFFLINE) {
        // FIXME: error signalling
        return;
    }

    Q_ASSERT(sourceMbox);

    qSort(uids);

    QModelIndexList messages;
    Sequence seq;
    Q_FOREACH(TreeItemMessage* m, findMessagesByUids(sourceMbox, uids)) {
        messages << m->toIndex(this);
        seq.add(m->uid());
    }
    m_taskFactory->createCopyMoveMessagesTask(this, messages, destMailboxName, op);
}

/** @short Convert a list of UIDs to a list of pointers to the relevant message nodes */
QList<TreeItemMessage *> Model::findMessagesByUids(const TreeItemMailbox *const mailbox, const QList<uint> &uids)
{
    const TreeItemMsgList *const list = dynamic_cast<const TreeItemMsgList *const>(mailbox->m_children[0]);
    Q_ASSERT(list);
    QList<TreeItemMessage *> res;
    auto it = list->m_children.constBegin();
    uint lastUid = 0;
    Q_FOREACH(const uint& uid, uids) {
        if (lastUid == uid) {
            // we have to filter out duplicates
            continue;
        }
        lastUid = uid;
        it = Common::lowerBoundWithUnknownElements(it, list->m_children.constEnd(), uid, messageHasUidZero, uidComparator);
        if (it != list->m_children.end() && static_cast<TreeItemMessage *>(*it)->uid() == uid) {
            res << static_cast<TreeItemMessage *>(*it);
        } else {
            qDebug() << "Can't find UID" << uid;
        }
    }
    return res;
}

/** @short Find a message with UID that matches the passed key, handling those with UID zero correctly

If there's no such message, the next message with a valid UID is returned instead. If there are no such messages, the iterator can
point to a message with UID zero or to the end of the list.
*/
TreeItemChildrenList::iterator Model::findMessageOrNextOneByUid(TreeItemMsgList *list, const uint uid)
{
    return Common::lowerBoundWithUnknownElements(list->m_children.begin(), list->m_children.end(), uid, messageHasUidZero, uidComparator);
}

TreeItemMailbox *Model::findMailboxByName(const QString &name) const
{
    return findMailboxByName(name, m_mailboxes);
}

TreeItemMailbox *Model::findMailboxByName(const QString &name, const TreeItemMailbox *const root) const
{
    Q_ASSERT(!root->m_children.isEmpty());
    // Names are sorted, so linear search is not required. On the ohterhand, the mailbox sizes are typically small enough
    // so that this shouldn't matter at all, and linear search is simple enough.
    for (int i = 1; i < root->m_children.size(); ++i) {
        TreeItemMailbox *mailbox = static_cast<TreeItemMailbox *>(root->m_children[i]);
        if (name == mailbox->mailbox())
            return mailbox;
        else if (name.startsWith(mailbox->mailbox() + mailbox->separator()))
            return findMailboxByName(name, mailbox);
    }
    return 0;
}

/** @short Find a parent mailbox for the specified name */
TreeItemMailbox *Model::findParentMailboxByName(const QString &name) const
{
    TreeItemMailbox *root = m_mailboxes;
    while (true) {
        if (root->m_children.size() == 1) {
            break;
        }
        bool found = false;
        for (int i = 1; !found && i < root->m_children.size(); ++i) {
            TreeItemMailbox *const item = dynamic_cast<TreeItemMailbox *>(root->m_children[i]);
            Q_ASSERT(item);
            if (name.startsWith(item->mailbox() + item->separator())) {
                root = item;
                found = true;
            }
        }
        if (!found) {
            return root;
        }
    }
    return root;
}


void Model::expungeMailbox(const QModelIndex &mailbox)
{
    if (!mailbox.isValid())
        return;

    if (m_netPolicy == NETWORK_OFFLINE) {
        qDebug() << "Can't expunge while offline";
        return;
    }

    m_taskFactory->createExpungeMailboxTask(this, mailbox);
}

void Model::createMailbox(const QString &name)
{
    if (m_netPolicy == NETWORK_OFFLINE) {
        qDebug() << "Can't create mailboxes while offline";
        return;
    }

    m_taskFactory->createCreateMailboxTask(this, name);
}

void Model::deleteMailbox(const QString &name)
{
    if (m_netPolicy == NETWORK_OFFLINE) {
        qDebug() << "Can't delete mailboxes while offline";
        return;
    }

    m_taskFactory->createDeleteMailboxTask(this, name);
}

void Model::subscribeMailbox(const QString &name)
{
    if (m_netPolicy == NETWORK_OFFLINE) {
        qDebug() << "Can't manage subscriptions while offline";
        return;
    }

    TreeItemMailbox *mailbox = findMailboxByName(name);
    if (!mailbox) {
        qDebug() << "SUBSCRIBE: No such mailbox.";
        return;
    }
    QModelIndex index = mailbox->toIndex(this);
    Q_ASSERT(index.isValid());
    m_taskFactory->createSubscribeUnsubscribeTask(this, index, SUBSCRIBE);
}

void Model::unsubscribeMailbox(const QString &name)
{
    if (m_netPolicy == NETWORK_OFFLINE) {
        qDebug() << "Can't manage subscriptions while offline";
        return;
    }

    TreeItemMailbox *mailbox = findMailboxByName(name);
    if (!mailbox) {
        qDebug() << "UNSUBSCRIBE: No such mailbox.";
        return;
    }
    QModelIndex index = mailbox->toIndex(this);
    Q_ASSERT(index.isValid());
    m_taskFactory->createSubscribeUnsubscribeTask(this, index, UNSUBSCRIBE);
}

void Model::saveUidMap(TreeItemMsgList *list)
{
    QList<uint> seqToUid;
    for (int i = 0; i < list->m_children.size(); ++i)
        seqToUid << static_cast<TreeItemMessage *>(list->m_children[ i ])->uid();
    cache()->setUidMapping(static_cast<TreeItemMailbox *>(list->parent())->mailbox(), seqToUid);
}


TreeItem *Model::realTreeItem(QModelIndex index, const Model **whichModel, QModelIndex *translatedIndex)
{
    while (const QAbstractProxyModel *proxy = qobject_cast<const QAbstractProxyModel *>(index.model())) {
        index = proxy->mapToSource(index);
        proxy = qobject_cast<const QAbstractProxyModel *>(index.model());
    }
    const Model *model = qobject_cast<const Model *>(index.model());
    Q_ASSERT(model);
    if (whichModel)
        *whichModel = model;
    if (translatedIndex)
        *translatedIndex = index;
    return static_cast<TreeItem *>(index.internalPointer());
}

void Model::changeConnectionState(Parser *parser, ConnectionState state)
{
    accessParser(parser).connState = state;
    logTrace(parser->parserId(), Common::LOG_TASKS, QLatin1String("conn"), connectionStateToString(state));
    emit connectionStateChanged(parser, state);
}

void Model::handleSocketStateChanged(Parser *parser, Imap::ConnectionState state)
{
    Q_ASSERT(parser);
    if (accessParser(parser).connState < state) {
        changeConnectionState(parser, state);
    }
}

void Model::killParser(Parser *parser, ParserKillingMethod method)
{
    if (method == PARSER_JUST_DELETE_LATER) {
        Q_ASSERT(accessParser(parser).parser == 0);
        Q_FOREACH(ImapTask *task, accessParser(parser).activeTasks) {
            task->deleteLater();
        }
        parser->deleteLater();
        return;
    }

    Q_FOREACH(ImapTask *task, accessParser(parser).activeTasks) {
        task->die();
    }

    parser->disconnect();
    Q_ASSERT(accessParser(parser).parser);
    accessParser(parser).parser = 0;
    switch (method) {
    case PARSER_KILL_EXPECTED:
        logTrace(parser->parserId(), Common::LOG_IO_WRITTEN, QString(), "*** Connection closed.");
        return;
    case PARSER_KILL_HARD:
        logTrace(parser->parserId(), Common::LOG_IO_WRITTEN, QString(), "*** Connection killed.");
        return;
    case PARSER_JUST_DELETE_LATER:
        // already handled
        return;
    }
    Q_ASSERT(false);
}

void Model::slotParserLineReceived(Parser *parser, const QByteArray &line)
{
    logTrace(parser->parserId(), Common::LOG_IO_READ, QString(), line);
}

void Model::slotParserLineSent(Parser *parser, const QByteArray &line)
{
    logTrace(parser->parserId(), Common::LOG_IO_WRITTEN, QString(), line);
}

void Model::setCache(AbstractCache *cache)
{
    if (m_cache)
        m_cache->deleteLater();
    m_cache = cache;
    m_cache->setParent(this);
}

void Model::runReadyTasks()
{
    for (QMap<Parser *,ParserState>::iterator parserIt = m_parsers.begin(); parserIt != m_parsers.end(); ++parserIt) {
        bool runSomething = false;
        do {
            runSomething = false;
            // See responseReceived() for more details about why we do need to iterate over a copy here.
            // Basically, calls to ImapTask::perform could invalidate our precious iterators.
            QList<ImapTask *> origList = parserIt->activeTasks;
            QList<ImapTask *> deletedList;
            QList<ImapTask *>::const_iterator taskEnd = origList.constEnd();
            for (QList<ImapTask *>::const_iterator taskIt = origList.constBegin(); taskIt != taskEnd; ++taskIt) {
                ImapTask *task = *taskIt;
                if (task->isReadyToRun()) {
                    task->perform();
                    runSomething = true;
                }
                if (task->isFinished()) {
                    deletedList << task;
                }
            }
            removeDeletedTasks(deletedList, parserIt->activeTasks);
#ifdef TROJITA_DEBUG_TASK_TREE
            if (!deletedList.isEmpty())
                checkTaskTreeConsistency();
#endif
        } while (runSomething);
    }
}

void Model::removeDeletedTasks(const QList<ImapTask *> &deletedTasks, QList<ImapTask *> &activeTasks)
{
    // Remove the finished commands
    for (QList<ImapTask *>::const_iterator deletedIt = deletedTasks.begin(); deletedIt != deletedTasks.end(); ++deletedIt) {
        (*deletedIt)->deleteLater();
        activeTasks.removeOne(*deletedIt);
        // It isn't destroyed yet, but should be removed from the model nonetheless
        m_taskModel->slotTaskDestroyed(*deletedIt);
    }
}

KeepMailboxOpenTask *Model::findTaskResponsibleFor(const QModelIndex &mailbox)
{
    Q_ASSERT(mailbox.isValid());
    QModelIndex translatedIndex;
    TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(realTreeItem(mailbox, 0, &translatedIndex));
    return findTaskResponsibleFor(mailboxPtr);
}

KeepMailboxOpenTask *Model::findTaskResponsibleFor(TreeItemMailbox *mailboxPtr)
{
    Q_ASSERT(mailboxPtr);
    bool canCreateParallelConn = m_parsers.isEmpty(); // FIXME: multiple connections

    if (mailboxPtr->maintainingTask) {
        // The requested mailbox already has the maintaining task associated
        if (accessParser(mailboxPtr->maintainingTask->parser).connState == CONN_STATE_LOGOUT) {
            // The connection is currently getting closed, so we have to create another one
            return m_taskFactory->createKeepMailboxOpenTask(this, mailboxPtr->toIndex(this), 0);
        } else {
            // it's usable as-is
            return mailboxPtr->maintainingTask;
        }
    } else if (canCreateParallelConn) {
        // The mailbox is not being maintained, but we can create a new connection
        return m_taskFactory->createKeepMailboxOpenTask(this, mailboxPtr->toIndex(this), 0);
    } else {
        // Too bad, we have to re-use an existing parser. That will probably lead to
        // stealing it from some mailbox, but there's no other way.
        Q_ASSERT(!m_parsers.isEmpty());

        for (QMap<Parser *,ParserState>::const_iterator it = m_parsers.constBegin(); it != m_parsers.constEnd(); ++it) {
            if (it->connState == CONN_STATE_LOGOUT) {
                // this one is not usable
                continue;
            }
            return m_taskFactory->createKeepMailboxOpenTask(this, mailboxPtr->toIndex(this), it.key());
        }
        // At this point, we have no other choice than to create a new connection
        return m_taskFactory->createKeepMailboxOpenTask(this, mailboxPtr->toIndex(this), 0);
    }
}

void Model::genericHandleFetch(TreeItemMailbox *mailbox, const Imap::Responses::Fetch *const resp)
{
    Q_ASSERT(mailbox);
    QList<TreeItemPart *> changedParts;
    TreeItemMessage *changedMessage = 0;
    mailbox->handleFetchResponse(this, *resp, changedParts, changedMessage, false);
    if (! changedParts.isEmpty()) {
        Q_FOREACH(TreeItemPart* part, changedParts) {
            QModelIndex index = part->toIndex(this);
            emit dataChanged(index, index);
        }
    }
    if (changedMessage) {
        QModelIndex index = changedMessage->toIndex(this);
        emit dataChanged(index, index);
        emitMessageCountChanged(mailbox);
    }
}

QModelIndex Model::findMailboxForItems(const QModelIndexList &items)
{
    TreeItemMailbox *mailbox = 0;
    Q_FOREACH(const QModelIndex& index, items) {
        TreeItemMailbox *currentMailbox = 0;
        Q_ASSERT(index.model() == this);

        TreeItem *item = static_cast<TreeItem *>(index.internalPointer());
        Q_ASSERT(item);

        if ((currentMailbox = dynamic_cast<TreeItemMailbox *>(item))) {
            // yes, that's an assignment, not a comparison

            // This case is OK
        } else {
            // TreeItemMessage and TreeItemPart have to walk the tree, which is why they are lumped together in this branch
            TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(item);
            if (!message) {
                if (TreeItemPart *part = dynamic_cast<TreeItemPart *>(item)) {
                    message = part->message();
                } else {
                    throw CantHappen("findMailboxForItems() called on strange items");
                }
            }
            Q_ASSERT(message);
            TreeItemMsgList *list = dynamic_cast<TreeItemMsgList *>(message->parent());
            Q_ASSERT(list);
            currentMailbox = dynamic_cast<TreeItemMailbox *>(list->parent());
        }

        Q_ASSERT(currentMailbox);
        if (!mailbox) {
            mailbox = currentMailbox;
        } else if (mailbox != currentMailbox) {
            throw CantHappen("Messages from several mailboxes");
        }
    }
    return mailbox->toIndex(this);
}

void Model::slotTasksChanged()
{
    dumpModelContents(m_taskModel);
}

void Model::slotTaskDying(QObject *obj)
{
    ImapTask *task = static_cast<ImapTask *>(obj);
    for (QMap<Parser *,ParserState>::iterator it = m_parsers.begin(); it != m_parsers.end(); ++it) {
        it->activeTasks.removeOne(task);
    }
    m_taskModel->slotTaskDestroyed(task);
}

TreeItemMailbox *Model::mailboxForSomeItem(QModelIndex index)
{
    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(index.internalPointer()));
    while (index.isValid() && ! mailbox) {
        index = index.parent();
        mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(index.internalPointer()));
    }
    return mailbox;
}

ParserState &Model::accessParser(Parser *parser)
{
    Q_ASSERT(m_parsers.contains(parser));
    return m_parsers[ parser ];
}

QModelIndex Model::findMessageForItem(QModelIndex index)
{
    if (! index.isValid())
        return QModelIndex();

    if (! dynamic_cast<const Model *>(index.model()))
        return QModelIndex();

    TreeItem *item = static_cast<TreeItem *>(index.internalPointer());
    Q_ASSERT(item);
    while (item) {
        Q_ASSERT(index.internalPointer() == item);
        if (dynamic_cast<TreeItemMessage *>(item)) {
            return index;
        } else if (dynamic_cast<TreeItemPart *>(item)) {
            index = index.parent();
            item = item->parent();
        } else {
            return QModelIndex();
        }
    }
    return QModelIndex();
}

void Model::releaseMessageData(const QModelIndex &message)
{
    if (! message.isValid())
        return;

    const Model *whichModel = 0;
    QModelIndex realMessage;
    realTreeItem(message, &whichModel, &realMessage);
    Q_ASSERT(whichModel == this);

    TreeItemMessage *msg = dynamic_cast<TreeItemMessage *>(static_cast<TreeItem *>(realMessage.internalPointer()));
    if (! msg)
        return;

    msg->setFetchStatus(TreeItem::NONE);

#ifndef XTUPLE_CONNECT
    beginRemoveRows(realMessage, 0, msg->m_children.size() - 1);
#endif
    if (msg->data()->m_partHeader) {
        msg->data()->m_partHeader->silentlyReleaseMemoryRecursive();
        delete msg->data()->m_partHeader;
        msg->data()->m_partHeader = 0;
    }
    if (msg->data()->m_partText) {
        msg->data()->m_partText->silentlyReleaseMemoryRecursive();
        delete msg->data()->m_partText;
        msg->data()->m_partText = 0;
    }
    delete msg->m_data;
    msg->m_data = 0;
    Q_FOREACH(TreeItem *item, msg->m_children) {
        TreeItemPart *part = dynamic_cast<TreeItemPart *>(item);
        Q_ASSERT(part);
        part->silentlyReleaseMemoryRecursive();
        delete part;
    }
    msg->m_children.clear();
#ifndef XTUPLE_CONNECT
    endRemoveRows();
    emit dataChanged(realMessage, realMessage);
#endif
}

QStringList Model::capabilities() const
{
    if (m_parsers.isEmpty())
        return QStringList();

    if (m_parsers.constBegin()->capabilitiesFresh)
        return m_parsers.constBegin()->capabilities;

    return QStringList();
}

void Model::logTrace(uint parserId, const Common::LogKind kind, const QString &source, const QString &message)
{
    Common::LogMessage m(QDateTime::currentDateTime(), kind, source,  message, 0);
    emit logged(parserId, m);
}

/** @short Overloaded version which accepts a QModelIndex of an item which is somehow "related" to the logged message

The relevantIndex argument is used for finding out what parser to send the message to.
*/
void Model::logTrace(const QModelIndex &relevantIndex, const Common::LogKind kind, const QString &source, const QString &message)
{
    QModelIndex translatedIndex;
    realTreeItem(relevantIndex, 0, &translatedIndex);

    // It appears that it's OK to use 0 here; the attached loggers apparently deal with random parsers appearing just OK
    uint parserId = 0;

    if (translatedIndex.isValid()) {
        Q_ASSERT(translatedIndex.model() == this);
        QModelIndex mailboxIndex = findMailboxForItems(QModelIndexList() << translatedIndex);
        Q_ASSERT(mailboxIndex.isValid());
        TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
        Q_ASSERT(mailboxPtr);
        if (mailboxPtr->maintainingTask) {
            parserId = mailboxPtr->maintainingTask->parser->parserId();
        }
    }

    logTrace(parserId, kind, source, message);
}

QAbstractItemModel *Model::taskModel() const
{
    return m_taskModel;
}

QMap<QByteArray,QByteArray> Model::serverId() const
{
    return m_idResult;
}

/** @short Handle explicit sharing and case mapping for message flags

This function will try to minimize the amount of QString instances used for storage of individual message flags via Qt's implicit
sharing that is built into QString.

At the same time, some well-known flags are converted to their "canonical" form (like \\SEEN -> \\Seen etc).
*/
QStringList Model::normalizeFlags(const QStringList &source) const
{
    QStringList res;
#if QT_VERSION >= QT_VERSION_CHECK(4, 7, 0)
    res.reserve(source.size());
#endif
    for (QStringList::const_iterator flag = source.constBegin(); flag != source.constEnd(); ++flag) {
        // At first, perform a case-insensitive lookup in the (rather short) list of known special flags
        QString lowerCase = flag->toLower();
        QHash<QString,QString>::const_iterator known = m_specialFlagNames.constFind(lowerCase);
        if (known != m_specialFlagNames.constEnd()) {
            res.append(*known);
            continue;
        }

        // If it isn't a special flag, just check whether it's been encountered already
        QSet<QString>::const_iterator it = m_flagLiterals.constFind(*flag);
        if (it == m_flagLiterals.constEnd()) {
            // Not in cache, so add it and return an implicitly shared copy
            m_flagLiterals.insert(*flag);
            res.append(*flag);
        } else {
            // It's in the cache already, se let's QString share the data
            res.append(*it);
        }
    }
    // Always sort the flags when performing normalization to obtain reasonable results and be ready for possible future
    // deduplication of the actual QLists
    res.sort();
    return res;
}

/** @short Set the IMAP username */
void Model::setImapUser(const QString &imapUser)
{
    m_imapUser = imapUser;
}

/** @short Username to use for login */
QString Model::imapUser() const
{
    return m_imapUser;
}

/** @short Set the password that the user wants to use */
void Model::setImapPassword(const QString &password)
{
    m_imapPassword = password;
    m_hasImapPassword = true;
    informTasksAboutNewPassword();
}

/** @short Return the user's password, if cached */
QString Model::imapPassword() const
{
    return m_imapPassword;
}

/** @short Indicate that the user doesn't want to provide her password */
void Model::unsetImapPassword()
{
    m_imapPassword.clear();
    m_hasImapPassword = false;
    informTasksAboutNewPassword();
}

/** @short Tell all tasks which want to know about the availability of a password */
void Model::informTasksAboutNewPassword()
{
    Q_FOREACH(const ParserState &p, m_parsers) {
        Q_FOREACH(ImapTask *task, p.activeTasks) {
            OpenConnectionTask *openTask = dynamic_cast<OpenConnectionTask *>(task);
            if (!openTask)
                continue;
            openTask->authCredentialsNowAvailable();
        }
    }
}

/** @short Forward a policy decision about accepting or rejecting a SSL state */
void Model::setSslPolicy(const QList<QSslCertificate> &sslChain, const QList<QSslError> &sslErrors, bool proceed)
{
    m_sslErrorPolicy.prepend(qMakePair(qMakePair(sslChain, sslErrors), proceed));
     Q_FOREACH(const ParserState &p, m_parsers) {
        Q_FOREACH(ImapTask *task, p.activeTasks) {
            OpenConnectionTask *openTask = dynamic_cast<OpenConnectionTask *>(task);
            if (!openTask)
                continue;
            if (openTask->sslCertificateChain() == sslChain && openTask->sslErrors() == sslErrors) {
                openTask->sslConnectionPolicyDecided(proceed);
            }
        }
    }
}

void Model::processSslErrors(OpenConnectionTask *task)
{
    // Qt doesn't define either operator< or a qHash specialization for QList<QSslError> (what a surprise),
    // so we use a plain old QList. Given that there will be at most one different QList<QSslError> sequence for
    // each connection attempt (and more realistically, for each server at all), this O(n) complexity shall not matter
    // at all.
    QList<QPair<QPair<QList<QSslCertificate>, QList<QSslError> >, bool> >::const_iterator it = m_sslErrorPolicy.constBegin();
    while (it != m_sslErrorPolicy.constEnd()) {
        if (it->first.first == task->sslCertificateChain() && it->first.second == task->sslErrors()) {
            task->sslConnectionPolicyDecided(it->second);
            return;
        }
        ++it;
    }
    EMIT_LATER(this, needsSslDecision, Q_ARG(QList<QSslCertificate>, task->sslCertificateChain()), Q_ARG(QList<QSslError>, task->sslErrors()));
}

QModelIndex Model::messageIndexByUid(const QString &mailboxName, const uint uid)
{
    TreeItemMailbox *mailbox = findMailboxByName(mailboxName);
    Q_ASSERT(mailbox);
    QList<TreeItemMessage*> messages = findMessagesByUids(mailbox, QList<uint>() << uid);
    if (messages.isEmpty()) {
        return QModelIndex();
    } else {
        Q_ASSERT(messages.size() == 1);
        return messages.front()->toIndex(this);
    }
}

/** @short Forget any cached data about number of messages in all mailboxes */
void Model::invalidateAllMessageCounts()
{
    QList<TreeItemMailbox*> queue;
    queue.append(m_mailboxes);
    while (!queue.isEmpty()) {
        TreeItemMailbox *head = queue.takeFirst();
        // ignore first child, the TreeItemMsgList
        for (auto it = head->m_children.constBegin() + 1; it != head->m_children.constEnd(); ++it) {
            queue.append(static_cast<TreeItemMailbox*>(*it));
        }
        TreeItemMsgList *list = dynamic_cast<TreeItemMsgList*>(head->m_children[0]);

        if (list->m_numberFetchingStatus == TreeItem::DONE && !head->maintainingTask) {
            // Ask only for data which were previously available
            // Also don't mess with a mailbox which is already being kept up-to-date because it's selected.
            list->m_numberFetchingStatus = TreeItem::NONE;
            emitMessageCountChanged(head);
        }
    }
}

AppendTask *Model::appendIntoMailbox(const QString &mailbox, const QByteArray &rawMessageData, const QStringList &flags,
                                     const QDateTime &timestamp)
{
    return m_taskFactory->createAppendTask(this, mailbox, rawMessageData, flags, timestamp);
}

AppendTask *Model::appendIntoMailbox(const QString &mailbox, const QList<CatenatePair> &data, const QStringList &flags,
                                     const QDateTime &timestamp)
{
    return m_taskFactory->createAppendTask(this, mailbox, data, flags, timestamp);
}

GenUrlAuthTask *Model::generateUrlAuthForMessage(const QString &host, const QString &user, const QString &mailbox,
                                                 const uint uidValidity, const uint uid, const QString &part, const QString &access)
{
    return m_taskFactory->createGenUrlAuthTask(this, host, user, mailbox, uidValidity, uid, part, access);
}

UidSubmitTask *Model::sendMailViaUidSubmit(const QString &mailbox, const uint uidValidity, const uint uid,
                                           const UidSubmitOptionsList &options)
{
    return m_taskFactory->createUidSubmitTask(this, mailbox, uidValidity, uid, options);
}

#ifdef TROJITA_DEBUG_TASK_TREE
#define TROJITA_DEBUG_TASK_TREE_VERBOSE
void Model::checkTaskTreeConsistency()
{
    for (QMap<Parser *,ParserState>::const_iterator parserIt = m_parsers.constBegin(); parserIt != m_parsers.constEnd(); ++parserIt) {
#ifdef TROJITA_DEBUG_TASK_TREE_VERBOSE
        qDebug() << "\nParser" << parserIt.key() << "; all active tasks:";
        Q_FOREACH(ImapTask *activeTask, parserIt.value().activeTasks) {
            qDebug() << ' ' << activeTask << activeTask->debugIdentification() << activeTask->parser;
        }
#endif
        Q_FOREACH(ImapTask *activeTask, parserIt.value().activeTasks) {
#ifdef TROJITA_DEBUG_TASK_TREE_VERBOSE
            qDebug() << "Active task" << activeTask << activeTask->debugIdentification() << activeTask->parser;
#endif
            Q_ASSERT(activeTask->parser == parserIt.key());
            Q_ASSERT(!activeTask->parentTask);
            checkDependentTasksConsistency(parserIt.key(), activeTask, 0, 0);
        }

        // Make sure that no task is present twice in here
        QList<ImapTask*> taskQueue = parserIt.value().activeTasks;
        for (int i = 0; i < taskQueue.size(); ++i) {
            Q_FOREACH(ImapTask *yetAnotherTask, taskQueue[i]->dependentTasks) {
                Q_ASSERT(!taskQueue.contains(yetAnotherTask));
                taskQueue.push_back(yetAnotherTask);
            }
        }
    }
}

void Model::checkDependentTasksConsistency(Parser *parser, ImapTask *task, ImapTask *expectedParentTask, int depth)
{
#ifdef TROJITA_DEBUG_TASK_TREE_VERBOSE
    QByteArray prefix;
    prefix.fill(' ', depth);
    qDebug() << prefix.constData() << "Checking" << task << task->debugIdentification();
#endif
    Q_ASSERT(parser);
    Q_ASSERT(!task->parser || task->parser == parser);
    Q_ASSERT(task->parentTask == expectedParentTask);
    if (task->parentTask) {
        Q_ASSERT(task->parentTask->dependentTasks.contains(task));
        if (task->parentTask->parentTask) {
            Q_ASSERT(task->parentTask->parentTask->dependentTasks.contains(task->parentTask));
        } else {
            Q_ASSERT(task->parentTask->parser);
            Q_ASSERT(accessParser(task->parentTask->parser).activeTasks.contains(task->parentTask));
        }
    } else {
        Q_ASSERT(accessParser(parser).activeTasks.contains(task));
    }

    Q_FOREACH(ImapTask *childTask, task->dependentTasks) {
        checkDependentTasksConsistency(parser, childTask, task, depth + 1);
    }
}
#endif

void Model::setCapabilitiesBlacklist(const QStringList &blacklist)
{
    m_capabilitiesBlacklist = blacklist;
}

bool Model::isCatenateSupported() const
{
    return capabilities().contains(QLatin1String("CATENATE"));
}

bool Model::isGenUrlAuthSupported() const
{
    return capabilities().contains(QLatin1String("URLAUTH"));
}

bool Model::isImapSubmissionSupported() const
{
    QStringList caps = capabilities();
    return caps.contains(QLatin1String("UIDPLUS")) && caps.contains(QLatin1String("X-DRAFT-I01-SENDMAIL"));
}

}
}
