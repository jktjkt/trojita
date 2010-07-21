/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "Model.h"
#include "MailboxTree.h"
#include "UnauthenticatedHandler.h"
#include "AuthenticatedHandler.h"
#include "CreateConnectionTask.h"
#include "SelectedHandler.h"
#include "SelectingHandler.h"
#include "ModelUpdaters.h"
#include "IdleLauncher.h"
#include "ImapTask.h"
#include "FetchMsgPartTask.h"
#include "UpdateFlagsTask.h"
#include "ListChildMailboxesTask.h"
#include "NumberOfMessagesTask.h"
#include "FetchMsgMetadataTask.h"
#include "ExpungeMailboxTask.h"
#include <QAbstractProxyModel>
#include <QAuthenticator>
#include <QCoreApplication>
#include <QDebug>

namespace Imap {
namespace Mailbox {

namespace {

bool MailboxNamesEqual( const TreeItem* const a, const TreeItem* const b )
{
    const TreeItemMailbox* const mailboxA = dynamic_cast<const TreeItemMailbox* const>(a);
    const TreeItemMailbox* const mailboxB = dynamic_cast<const TreeItemMailbox* const>(b);

    return mailboxA && mailboxB && mailboxA->mailbox() == mailboxB->mailbox();
}

bool MailboxNameComparator( const TreeItem* const a, const TreeItem* const b )
{
    const TreeItemMailbox* const mailboxA = dynamic_cast<const TreeItemMailbox* const>(a);
    const TreeItemMailbox* const mailboxB = dynamic_cast<const TreeItemMailbox* const>(b);

    if ( mailboxA->mailbox() == QLatin1String( "INBOX" ) )
        return true;
    if ( mailboxB->mailbox() == QLatin1String( "INBOX" ) )
        return false;
    return mailboxA->mailbox().compare( mailboxB->mailbox(), Qt::CaseInsensitive ) < 1;
}

}

ModelStateHandler::ModelStateHandler( Model* _m ): QObject(_m), m(_m)
{
}

Model::Model( QObject* parent, AbstractCache* cache, SocketFactoryPtr socketFactory, bool offline ):
    // parent
    QAbstractItemModel( parent ),
    // our tools
    _cache(cache), _socketFactory(socketFactory),
    _maxParsers(4), _mailboxes(0), _netPolicy( NETWORK_ONLINE ),
    _authenticator(0)
{
    _cache->setParent(this);
    _startTls = _socketFactory->startTlsRequired();

    unauthHandler = new UnauthenticatedHandler( this );
    authenticatedHandler = new AuthenticatedHandler( this );
    selectedHandler = new SelectedHandler( this );
    selectingHandler = new SelectingHandler( this );

    _mailboxes = new TreeItemMailbox( 0 );

    _onlineMessageFetch << "ENVELOPE" << "BODYSTRUCTURE" << "RFC822.SIZE" << "UID" << "FLAGS";

    noopTimer = new QTimer( this );
    noopTimer->setObjectName( QString::fromAscii("noopTimer-%1").arg( objectName() ) );
    connect( noopTimer, SIGNAL(timeout()), this, SLOT(performNoop()) );

    if ( offline ) {
        _netPolicy = NETWORK_OFFLINE;
        QTimer::singleShot( 0, this, SLOT(setNetworkOffline()) );
    } else {
        QTimer::singleShot( 0, this, SLOT( setNetworkOnline() ) );
        noopTimer->start( PollingPeriod );
    }
}

Model::~Model()
{
    delete _mailboxes;
}

void Model::responseReceived()
{
    QMap<Parser*,ParserState>::iterator it = _parsers.find( qobject_cast<Imap::Parser*>( sender() ));
    Q_ASSERT( it != _parsers.end() );

    while ( it.value().parser->hasResponse() ) {
        QSharedPointer<Imap::Responses::AbstractResponse> resp = it.value().parser->getResponse();
        Q_ASSERT( resp );
        try {
            /* At this point, we want to iterate over all active tasks and try them
            for processing the server's responses (the plug() method). However, this
            is rather complex -- this call to plug() could result in signals being
            emited, and certain slots connected to those signals might in turn want
            to queue more Tasks. Therefore, it->activeTasks could be modified, some
            items could be appended to it uwing the QList::append, which in turn could
            cause a realloc to happen, happily invalidating our iterators, and that
            kind of sucks.

            So, we have to iterate over a copy of the original list and istead of
            deleting Tasks, we store them into a temporary list. When we're done with
            processing, we walk the original list once again and simply remove all
            "deleted" items for real.

            This took me 3+ hours to track it down to what the hell was happening here,
            even though the underlying reason is simple -- QList::append() could invalidate
            existing iterators.
            */

            bool handled = false;
            QList<ImapTask*> taskSnapshot = it->activeTasks;
            QList<ImapTask*> deletedTasks;

            // Try various tasks, perhaps it's their response
            for ( QList<ImapTask*>::iterator taskIt = taskSnapshot.begin(); taskIt != taskSnapshot.end(); ++taskIt ) {
                bool handledNow = resp->plug( it->parser, *taskIt );

                if ( (*taskIt)->isFinished() )
                    deletedTasks << *taskIt;

                handled |= handledNow;
                if ( handled )
                    break;
            }
            // And now remove the finished commands
            for ( QList<ImapTask*>::iterator deletedIt = deletedTasks.begin(); deletedIt != deletedTasks.end(); ++deletedIt ) {
                (*deletedIt)->deleteLater();
                it->activeTasks.removeOne( *deletedIt );
            }
            if ( ! handled )
                resp->plug( it.value().parser, this );
        } catch ( Imap::ImapException& e ) {
            uint parserId = it->parser->parserId();
            killParser( it->parser );
            _parsers.erase( it );
            broadcastParseError( parserId, QString::fromStdString( e.exceptionClass() ), e.what(), e.line(), e.offset() );
            parsersMightBeIdling();
            return;
        }
        if ( ! it.value().parser ) {
            // it got deleted
            _parsers.erase( it );
            break;
        }
    }
    it->idleLauncher->postponeIdleIfActive();
}

void Model::handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    // OK/NO/BAD/PREAUTH/BYE
    using namespace Imap::Responses;

    const QString& tag = resp->tag;

    if ( ! tag.isEmpty() ) {
        QMap<CommandHandle, Task>::iterator command = _parsers[ ptr ].commandMap.find( tag );
        if ( command == _parsers[ ptr ].commandMap.end() ) {
            qDebug() << "This command is not valid anymore" << tag;
            return;
        }

        // FIXME: distinguish among OK/NO/BAD here
        switch ( command->kind ) {
            case Task::STARTTLS:
                _parsers[ ptr ].capabilitiesFresh = false;
                if ( resp->kind == Responses::OK ) {
                    // The connection is secured -> we can login
                    performAuthentication( ptr );
                } else {
                    emit connectionError( tr("Can't establish a secure connection to the server (STARTTLS failed). Refusing to proceed.") );
                }
                break;
            case Task::LOGIN:
                if ( resp->kind == Responses::OK ) {
                    changeConnectionState( ptr, CONN_STATE_AUTHENTICATED );
                    _parsers[ ptr ].responseHandler = authenticatedHandler;
                    if ( ! _parsers[ ptr ].capabilitiesFresh ) {
                        CommandHandle cmd = ptr->capability();
                        _parsers[ ptr ].commandMap[ cmd ] = Task( Task::CAPABILITY, 0 );
                        emit activityHappening( true );
                    }
                    //CommandHandle cmd = ptr->namespaceCommand();
                    //_parsers[ ptr ].commandMap[ cmd ] = Task( Task::NAMESPACE, 0 );
                    ptr->authStateReached();
                } else {
                    if ( _authenticator )
                        delete _authenticator;
                    _authenticator = 0;
                    // FIXME: handle this in a sane way
                    changeConnectionState( ptr, CONN_STATE_LOGIN_FAILED );
                    emit connectionError( tr("Login Failed: %1").arg( resp->message ) );
                }
                break;
            case Task::NONE:
                throw CantHappen( "Internal Error: command that is supposed to do nothing?", *resp );
                break;
            case Task::LIST:
                throw CantHappen( "The Task::LIST should've been handled by the ListChildMailboxesTask", *resp );
                break;
            case Task::LIST_AFTER_CREATE:
                if ( resp->kind == Responses::OK ) {
                    _finalizeIncrementalList( ptr, command );
                } else {
                    // FIXME
                }
                break;
            case Task::STATUS:
                throw CantHappen( "The Task::STATUS should've been handled by the NumberOfMessagesTask", *resp );
                break;
            case Task::SELECT:
                --_parsers[ ptr ].selectingAnother;
                if ( resp->kind == Responses::OK ) {
                    _finalizeSelect( ptr, command );
                } else {
                    if ( _parsers[ ptr ].connState == CONN_STATE_SELECTED )
                        changeConnectionState( ptr, CONN_STATE_AUTHENTICATED);
                    _parsers[ ptr ].currentMbox = 0;
                    // FIXME: error handling
                }
                break;
            case Task::FETCH_MESSAGE_METADATA:
                // Either we were fetching just UID & FLAGS, or that and stuff like BODYSTRUCTURE.
                // In any case, we don't have to do anything here, besides updating message status
                // FIXME: this should probably go when the Task migration's done, as Tasks themselves could be made responsible for state updates
                changeConnectionState( ptr, CONN_STATE_SELECTED );
                break;
            case Task::FETCH_WITH_FLAGS:
                _finalizeFetch( ptr, command );
                Q_FOREACH( ImapTask* task, _parsers[ ptr ].activeTasks ) {
                    CreateConnectionTask* conn = dynamic_cast<CreateConnectionTask*>( task );
                    if ( ! conn )
                        continue;
                    conn->_completed();
                }
                break;
            case Task::FETCH_PART:
                throw CantHappen( "The Task::FETCH_PART should've been handled by the FetchMsgPartTask", *resp );
                break;
            case Task::NOOP:
            case Task::IDLE:
                // We don't have to do anything here
                break;
            case Task::CAPABILITY:
                if ( _parsers[ ptr ].connState < CONN_STATE_AUTHENTICATED ) {
                    // This CAPABILITY is crucial for LOGIN
                    if ( _parsers[ ptr ].capabilities.contains( QLatin1String("LOGINDISABLED") ) ) {
                        qDebug() << "Can't login yet, trying STARTTLS";
                        // ... and we are forbidden from logging in, so we have to try the STARTTLS
                        CommandHandle cmd = ptr->startTls();
                        _parsers[ ptr ].commandMap[ cmd ] = Model::Task( Model::Task::STARTTLS, 0 );
                        emit activityHappening( true );
                    } else {
                        // Apparently no need for STARTTLS and we are free to login
                        performAuthentication( ptr );
                    }
                }
                break;
            case Task::STORE:
                // FIXME: check for errors
                break;
            case Task::NAMESPACE:
                // FIXME: what to do
                break;
            case Task::EXPUNGE:
                throw CantHappen( "The Task::EXPUNGE should've been handled by the ExpungeMailboxTask", *resp );
                break;
            case Task::COPY:
                // FIXME
                break;
            case Task::CREATE:
                _finalizeCreate( ptr, command, resp );
                break;
            case Task::DELETE:
                _finalizeDelete( ptr, command, resp );
                break;
            case Task::LOGOUT:
                // we are inside while loop in responseReceived(), so we can't delete current parser just yet
                killParser( ptr );
                parsersMightBeIdling();
                return;
        }

        // We have to verify that the command is still registered in the
        // commandMap, as it might have been removed in the meanwhile, for
        // example by the replaceChildMailboxes()
        if ( _parsers[ ptr ].commandMap.find( tag ) != _parsers[ ptr ].commandMap.end() )
            _parsers[ ptr ].commandMap.erase( command );
        else
            qDebug() << "This command is not valid anymore at the end of the loop" << tag;

        parsersMightBeIdling();

    } else {
        // untagged response
        if ( _parsers[ ptr ].responseHandler )
            _parsers[ ptr ].responseHandler->handleState( ptr, resp );
    }
}

void Model::_finalizeList( Parser* parser, TreeItemMailbox* mailboxPtr )
{
    QList<TreeItem*> mailboxes;

    QList<Responses::List>& listResponses = _parsers[ parser ].listResponses;
    const QString prefix = mailboxPtr->mailbox() + mailboxPtr->separator();
    for ( QList<Responses::List>::iterator it = listResponses.begin();
            it != listResponses.end(); /* nothing */ ) {
        if ( it->mailbox == mailboxPtr->mailbox() || it->mailbox == prefix ) {
            // rubbish, ignore
            it = listResponses.erase( it );
        } else if ( it->mailbox.startsWith( prefix ) ) {
            mailboxes << new TreeItemMailbox( mailboxPtr, *it );
            it = listResponses.erase( it );
        } else {
            // it clearly is someone else's LIST response
            ++it;
        }
    }
    qSort( mailboxes.begin(), mailboxes.end(), MailboxNameComparator );

    // Remove duplicates; would be great if this could be done in a STLish way,
    // but unfortunately std::unique won't help here (the "duped" part of the
    // sequence contains undefined items)
    if ( mailboxes.size() > 1 ) {
        QList<TreeItem*>::iterator it = mailboxes.begin();
        ++it;
        while ( it != mailboxes.end() ) {
            if ( MailboxNamesEqual( it[-1], *it ) ) {
                delete *it;
                it = mailboxes.erase( it );
            } else {
                ++it;
            }
        }
    }

    QList<MailboxMetadata> metadataToCache;
    QList<TreeItemMailbox*> mailboxesWithoutChildren;
    for ( QList<TreeItem*>::const_iterator it = mailboxes.begin(); it != mailboxes.end(); ++it ) {
        TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( *it );
        Q_ASSERT( mailbox );
        metadataToCache.append( mailbox->mailboxMetadata() );
        if ( mailbox->hasNoChildMaliboxesAlreadyKnown() ) {
            mailboxesWithoutChildren << mailbox;
        }
    }
    cache()->setChildMailboxes( mailboxPtr->mailbox(), metadataToCache );
    for ( QList<TreeItemMailbox*>::const_iterator it = mailboxesWithoutChildren.begin(); it != mailboxesWithoutChildren.end(); ++it )
        cache()->setChildMailboxes( (*it)->mailbox(), QList<MailboxMetadata>() );
    replaceChildMailboxes( mailboxPtr, mailboxes );
}

void Model::_finalizeIncrementalList( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemMailbox* parentMbox = findParentMailboxByName( command->str );
    if ( ! parentMbox ) {
        qDebug() << "Weird, no idea where to put the newly created mailbox" << command->str;
        return;
    }

    QList<TreeItem*> mailboxes;

    QList<Responses::List>& listResponses = _parsers[ parser ].listResponses;
    for ( QList<Responses::List>::iterator it = listResponses.begin();
            it != listResponses.end(); /* nothing */ ) {
        if ( it->mailbox == command->str ) {
            mailboxes << new TreeItemMailbox( parentMbox, *it );
            it = listResponses.erase( it );
        } else {
            // it clearly is someone else's LIST response
            ++it;
        }
    }
    qSort( mailboxes.begin(), mailboxes.end(), MailboxNameComparator );

    if ( mailboxes.size() == 0) {
        qDebug() << "Weird, no matching LIST response for our prompt after CREATE";
        qDeleteAll( mailboxes );
        return;
    } else if ( mailboxes.size() > 1 ) {
        qDebug() << "Weird, too many LIST responses for our prompt after CREATE";
        qDeleteAll( mailboxes );
        return;
    }

    QList<TreeItem*>::iterator it = parentMbox->_children.begin();
    Q_ASSERT( it != parentMbox->_children.end() );
    ++it;
    while ( it != parentMbox->_children.end() && MailboxNameComparator( *it, mailboxes[0] ) )
        ++it;
    QModelIndex parentIdx = parentMbox == _mailboxes ? QModelIndex() : QAbstractItemModel::createIndex( parentMbox->row(), 0, parentMbox );
    if ( it == parentMbox->_children.end() )
        beginInsertRows( parentIdx, parentMbox->_children.size(), parentMbox->_children.size() );
    else
        beginInsertRows( parentIdx, (*it)->row(), (*it)->row() );
    parentMbox->_children.insert( it, mailboxes[0] );
    endInsertRows();
}

void Model::replaceChildMailboxes( TreeItemMailbox* mailboxPtr, const QList<TreeItem*> mailboxes )
{
    /* It would be nice to avoid calling layoutAboutToBeChanged(), but
       unfortunately it seems that it is neccessary for QTreeView to work
       correctly (at least in Qt 4.5).

       Additionally, the fine-frained signals aren't popagated via
       QSortFilterProxyModel, on which we rely for detailed models like
       MailboxModel.

       Some observations are at http://lists.trolltech.com/qt-interest/2006-01/thread00099-0.html

       Trolltech's QFileSystemModel works in the same way:
            rowsAboutToBeInserted( QModelIndex(0,0,0x9bbf290,QFileSystemModel(0x9bac530) )  0 16 )
            rowsInserted( QModelIndex(0,0,0x9bbf290,QFileSystemModel(0x9bac530) )  0 16 )
            layoutAboutToBeChanged()
            layoutChanged()

       This applies to other handlers in this file which update model layout as
       well.
    */

    QModelIndex parent = mailboxPtr == _mailboxes ? QModelIndex() : QAbstractItemModel::createIndex( mailboxPtr->row(), 0, mailboxPtr );

    if ( mailboxPtr->_children.size() != 1 ) {
        // There's something besides the TreeItemMsgList and we're going to
        // overwrite them, so we have to delete them right now
        int count = mailboxPtr->rowCount( this );
        beginRemoveRows( parent, 1, count - 1 );
        QList<TreeItem*> oldItems = mailboxPtr->setChildren( QList<TreeItem*>() );
        endRemoveRows();

        // FIXME: this should be less drastical (ie cancel only what is reqlly really required to be cancelled
        for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
            it->commandMap.clear();
            it->mailbox = 0;
            it->currentMbox = 0;
            it->responseHandler = authenticatedHandler;
        }
        qDeleteAll( oldItems );
    }

    if ( ! mailboxes.isEmpty() ) {
        emit layoutAboutToBeChanged();
        beginInsertRows( parent, 1, mailboxes.size() );
        QList<TreeItem*> dummy = mailboxPtr->setChildren( mailboxes );
        endInsertRows();
        emit layoutChanged();
        Q_ASSERT( dummy.isEmpty() );
    } else {
        QList<TreeItem*> dummy = mailboxPtr->setChildren( mailboxes );
        Q_ASSERT( dummy.isEmpty() );
    }
    emit dataChanged( parent, parent );
}

void Model::_finalizeSelect( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    bool tasksNeedMoreWork = false;
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( command->what );
    Q_ASSERT( mailbox );
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[ 0 ] );
    Q_ASSERT( list );
    _parsers[ parser ].currentMbox = mailbox;
    _parsers[ parser ].responseHandler = selectedHandler;
    changeConnectionState( parser, CONN_STATE_SELECTED );

    const SyncState& syncState = _parsers[ parser ].syncState;
    const SyncState& oldState = cache()->mailboxSyncState( mailbox->mailbox() );

    list->_totalMessageCount = syncState.exists();
    // Note: syncState.unSeen() is the NUMBER of the first unseen message, not their count!

    if ( _parsers[ parser ].selectingAnother ) {
        emitMessageCountChanged( mailbox );
        // We have already queued a command that switches to another mailbox
        // Asking the parser to switch back would only make the situation worse,
        // so we can't do anything better than exit right now
        qDebug() << "FIXME FIXME: too many mailboxes in the queue; this breaks the Tasks API!!!";
        return;
    }

    QList<uint> seqToUid = cache()->uidMapping( mailbox->mailbox() );

    if ( static_cast<uint>( seqToUid.size() ) != oldState.exists() ||
         oldState.exists() != static_cast<uint>( list->_children.size() ) ) {

        qDebug() << "Inconsistent cache data, falling back to full sync (" <<
                seqToUid.size() << "in UID map," << oldState.exists() <<
                "EXIST before," << list->_children.size() << "nodes)";
        _fullMboxSync( mailbox, list, parser, syncState );
        return;
    }

    if ( syncState.isUsableForSyncing() && oldState.isUsableForSyncing() && syncState.uidValidity() == oldState.uidValidity() ) {
        // Perform a nice re-sync

        if ( syncState.uidNext() == oldState.uidNext() ) {
            // No new messages

            if ( syncState.exists() == oldState.exists() ) {
                // No deletions, either, so we resync only flag changes

                if ( syncState.exists() ) {
                    // Verify that we indeed have all UIDs and not need them anymore
                    bool uidsOk = true;
                    for ( int i = 0; i < list->_children.size(); ++i ) {
                        if ( ! static_cast<TreeItemMessage*>( list->_children[i] )->uid() ) {
                            uidsOk = false;
                            break;
                        }
                    }
                    QStringList items = QStringList( "FLAGS" );
                    if ( ! uidsOk )
                        items << "UID";
                    CommandHandle cmd = parser->fetch( Sequence( 1, syncState.exists() ),
                                                       items );
                    _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
                    tasksNeedMoreWork = true;
                    emit activityHappening( true );
                    list->_numberFetchingStatus = TreeItem::LOADING;
                    list->_unreadMessageCount = 0;
                } else {
                    list->_unreadMessageCount = 0;
                    list->_totalMessageCount = 0;
                    list->_numberFetchingStatus = TreeItem::DONE;
                }

                if ( list->_children.isEmpty() ) {
                    QList<TreeItem*> messages;
                    for ( uint i = 0; i < syncState.exists(); ++i ) {
                        TreeItemMessage* msg = new TreeItemMessage( list );
                        msg->_offset = i;
                        msg->_uid = seqToUid[ i ];
                        messages << msg;
                    }
                    list->setChildren( messages );

                } else {
                    if ( syncState.exists() != static_cast<uint>( list->_children.size() ) ) {
                        throw CantHappen( "TreeItemMsgList has wrong number of "
                                          "children, even though no change of "
                                          "message count occured" );
                    }
                }

                list->_fetchStatus = TreeItem::DONE;
                cache()->setMailboxSyncState( mailbox->mailbox(), syncState );
                saveUidMap( list );

            } else {
                // Some messages got deleted, but there have been no additions

                CommandHandle cmd = parser->fetch( Sequence( 1, syncState.exists() ),
                                                   QStringList() << "UID" << "FLAGS" );
                _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
                tasksNeedMoreWork = true;
                emit activityHappening( true );
                list->_numberFetchingStatus = TreeItem::LOADING;
                list->_unreadMessageCount = 0;
                // selecting handler should do the rest
                _parsers[ parser ].responseHandler = selectingHandler;
                QList<uint>& uidMap = _parsers[ parser ].uidMap;
                uidMap.clear();
                _parsers[ parser ].syncingFlags.clear();
                for ( uint i = 0; i < syncState.exists(); ++i )
                    uidMap << 0;
                cache()->clearUidMapping( mailbox->mailbox() );
            }

        } else {
            // Some new messages were delivered since we checked the last time.
            // There's no guarantee they are still present, though.

            if ( syncState.uidNext() - oldState.uidNext() == syncState.exists() - oldState.exists() ) {
                // Only some new arrivals, no deletions

                for ( uint i = 0; i < syncState.uidNext() - oldState.uidNext(); ++i ) {
                    TreeItemMessage* msg = new TreeItemMessage( list );
                    msg->_offset = i + oldState.exists();
                    list->_children << msg;
                }

                // FIXME: re-enable the optimization when Task migration is done
                QStringList items = ( false && networkPolicy() == NETWORK_ONLINE &&
                                      syncState.uidNext() - oldState.uidNext() <= StructureFetchLimit ) ?
                                    _onlineMessageFetch : QStringList() << "UID" << "FLAGS";
                CommandHandle cmd = parser->fetch( Sequence( oldState.exists() + 1, syncState.exists() ),
                                                   items );
                _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
                tasksNeedMoreWork = true;
                emit activityHappening( true );
                list->_numberFetchingStatus = TreeItem::LOADING;
                list->_fetchStatus = TreeItem::DONE;
                list->_unreadMessageCount = 0;
                cache()->clearUidMapping( mailbox->mailbox() );

            } else {
                // Generic case; we don't know anything about which messages were deleted and which added
                // FIXME: might be possible to optimize here...

                // At first, let's ask for UID numbers and FLAGS for all messages
                CommandHandle cmd = parser->fetch( Sequence( 1, syncState.exists() ),
                                                   QStringList() << "UID" << "FLAGS" );
                _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
                tasksNeedMoreWork = true;
                emit activityHappening( true );
                _parsers[ parser ].responseHandler = selectingHandler;
                list->_numberFetchingStatus = TreeItem::LOADING;
                list->_unreadMessageCount = 0;
                QList<uint>& uidMap = _parsers[ parser ].uidMap;
                uidMap.clear();
                _parsers[ parser ].syncingFlags.clear();
                for ( uint i = 0; i < syncState.exists(); ++i )
                    uidMap << 0;
                cache()->clearUidMapping( mailbox->mailbox() );
            }
        }
    } else {
        // Forget everything, do a dumb sync
        cache()->clearAllMessages( mailbox->mailbox() );
        _fullMboxSync( mailbox, list, parser, syncState );
        tasksNeedMoreWork = true;
    }
    emitMessageCountChanged( mailbox );
    if ( ! tasksNeedMoreWork ) {
        Q_FOREACH( ImapTask* task, _parsers[ parser ].activeTasks ) {
            CreateConnectionTask* conn = dynamic_cast<CreateConnectionTask*>( task );
            if ( ! conn )
                continue;
            conn->_completed();
        }
    }
}

void Model::emitMessageCountChanged( TreeItemMailbox* const mailbox )
{
    TreeItemMsgList* list = static_cast<TreeItemMsgList*>( mailbox->_children[ 0 ] );
    QModelIndex msgListIndex = createIndex( list->row(), 0, list );
    emit dataChanged( msgListIndex, msgListIndex );
    emit messageCountPossiblyChanged( createIndex( mailbox->row(), 0, mailbox ) );
}

void Model::_fullMboxSync( TreeItemMailbox* mailbox, TreeItemMsgList* list, Parser* parser, const SyncState& syncState )
{
    bool tasksNeedMoreWork = false;
    cache()->clearUidMapping( mailbox->mailbox() );

    QModelIndex parent = createIndex( 0, 0, list );
    if ( ! list->_children.isEmpty() ) {
        beginRemoveRows( parent, 0, list->_children.size() - 1 );
        qDeleteAll( list->_children );
        list->_children.clear();
        endRemoveRows();
    }
    if ( syncState.exists() ) {
        bool willLoad = networkPolicy() == NETWORK_ONLINE && syncState.exists() <= StructureFetchLimit;
        beginInsertRows( parent, 0, syncState.exists() - 1 );
        for ( uint i = 0; i < syncState.exists(); ++i ) {
            TreeItemMessage* message = new TreeItemMessage( list );
            message->_offset = i;
            list->_children << message;
            // FIXME: re-evaluate this one when Task migration's done
            //if ( willLoad )
            //    message->_fetchStatus = TreeItem::LOADING;
        }
        endInsertRows();
        list->_fetchStatus = TreeItem::DONE;

        Q_ASSERT( ! _parsers[ parser ].selectingAnother );

        // FIXME: re-enable optimization when Task migration's done
        QStringList items = false && willLoad ? _onlineMessageFetch : QStringList() << "UID" << "FLAGS";
        CommandHandle cmd = parser->fetch( Sequence( 1, syncState.exists() ), items );
        _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
        tasksNeedMoreWork = true;
        emit activityHappening( true );
        list->_numberFetchingStatus = TreeItem::LOADING;
        list->_unreadMessageCount = 0;
    } else {
        list->_totalMessageCount = 0;
        list->_unreadMessageCount = 0;
        list->_numberFetchingStatus = TreeItem::DONE;
        list->_fetchStatus = TreeItem::DONE;
        cache()->setMailboxSyncState( mailbox->mailbox(), syncState );
        saveUidMap( list );
    }
    emitMessageCountChanged( mailbox );
    if ( ! tasksNeedMoreWork ) {
        Q_FOREACH( ImapTask* task, _parsers[ parser ].activeTasks ) {
            CreateConnectionTask* conn = dynamic_cast<CreateConnectionTask*>( task );
            if ( ! conn )
                continue;
            conn->_completed();
        }
    }
}

/** @short Retrieval of a message part has completed */
void Model::_finalizeFetchPart( Parser* parser, TreeItemPart* const part )
{
    if ( part->loading() ) {
        // basically, there's nothing to do if the FETCH targetted a message part and not the message as a whole
        qDebug() << "Imap::Model::_finalizeFetch(): didn't receive anything about message" <<
            part->message()->row() << "part" << part->partId() << "in mailbox" <<
            _parsers[ parser ].currentMbox->mailbox();
        part->_fetchStatus = TreeItem::DONE;
    }
    changeConnectionState( parser, CONN_STATE_SELECTED );
}

/** @short A FETCH command has completed

This function is triggered when the remote server indicates that the FETCH command has been completed
and that it already sent all the data for the FETCH command.
*/
void Model::_finalizeFetch( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( command.value().what );

    if ( mailbox && _parsers[ parser ].responseHandler == selectedHandler ) {
        // the mailbox was already synced (?)
        mailbox->_children[0]->_fetchStatus = TreeItem::DONE;
        cache()->setMailboxSyncState( mailbox->mailbox(), _parsers[ parser ].syncState );
        TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[0] );
        Q_ASSERT( list );
        saveUidMap( list );
        if ( command->kind == Task::FETCH_WITH_FLAGS ) {
            list->recalcUnreadMessageCount();
            list->_numberFetchingStatus = TreeItem::DONE;
            changeConnectionState( parser, CONN_STATE_SELECTED );
        }
        emitMessageCountChanged( mailbox );
    } else if ( mailbox && _parsers[ parser ].responseHandler == selectingHandler ) {
        // the synchronization was still in progress
        _parsers[ parser ].responseHandler = selectedHandler;
        changeConnectionState( parser, CONN_STATE_SELECTED );
        cache()->setMailboxSyncState( mailbox->mailbox(), _parsers[ parser ].syncState );

        QList<uint>& uidMap = _parsers[ parser ].uidMap;
        TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[0] );
        Q_ASSERT( list );
        list->_fetchStatus = TreeItem::DONE;

        QModelIndex parent = createIndex( 0, 0, list );

        if ( uidMap.isEmpty() ) {
            // the mailbox is empty
            if ( list->_children.size() > 0 ) {
                beginRemoveRows( parent, 0, list->_children.size() - 1 );
                qDeleteAll( list->setChildren( QList<TreeItem*>() ) );
                endRemoveRows();
                cache()->clearAllMessages( mailbox->mailbox() );
            }
        } else {
            // some messages are present *now*; they might or might not have been there before
            int pos = 0;
            for ( int i = 0; i < uidMap.size(); ++i ) {
                if ( i >= list->_children.size() ) {
                    // now we're just adding new messages to the end of the list
                    beginInsertRows( parent, i, i );
                    TreeItemMessage * msg = new TreeItemMessage( list );
                    msg->_offset = i;
                    msg->_uid = uidMap[ i ];
                    list->_children << msg;
                    endInsertRows();
                } else if ( dynamic_cast<TreeItemMessage*>( list->_children[pos] )->_uid == uidMap[ i ] ) {
                    // current message has correct UID
                    dynamic_cast<TreeItemMessage*>( list->_children[pos] )->_offset = i;
                    continue;
                } else {
                    // Traverse the messages we have in the cache, checking their UIDs. The idea here
                    // is that we should be deleting all messages with UIDs different from the current
                    // "supposed UID" (ie. uidMap[i]); this is a valid behavior per the IMAP standard.
                    int pos = i;
                    bool found = false;
                    while ( pos < list->_children.size() ) {
                        TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( list->_children[pos] );
                        if ( message->_uid != uidMap[ i ] ) {
                            // this message is free to go
                            cache()->clearMessage( mailbox->mailbox(), message->uid() );
                            beginRemoveRows( parent, pos, pos );
                            delete list->_children.takeAt( pos );
                            // the _offset of all subsequent messages will be updated later
                            endRemoveRows();
                        } else {
                            // this message is the correct one -> keep it, go to the next UID
                            found = true;
                            message->_offset = i;
                            break;
                        }
                    }
                    if ( ! found ) {
                        // Add one message, continue the loop
                        Q_ASSERT( pos == list->_children.size() ); // we're at the end of the list
                        beginInsertRows( parent, i, i );
                        TreeItemMessage * msg = new TreeItemMessage( list );
                        msg->_uid = uidMap[ i ];
                        msg->_offset = i;
                        list->_children << msg;
                        endInsertRows();
                    }
                }
            }
            if ( uidMap.size() != list->_children.size() ) {
                // remove items at the end
                beginRemoveRows( parent, uidMap.size(), list->_children.size() - 1 );
                for ( int i = uidMap.size(); i < list->_children.size(); ++i ) {
                    TreeItemMessage* message = static_cast<TreeItemMessage*>( list->_children.takeAt( i ) );
                    cache()->clearMessage( mailbox->mailbox(), message->uid() );
                    delete message;
                }
                endRemoveRows();
            }
        }

        uidMap.clear();

        int unSeenCount = 0;
        for ( QList<TreeItem*>::const_iterator it = list->_children.begin();
              it != list->_children.end(); ++it ) {
            TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( *it );
            Q_ASSERT( message );
            if ( message->_uid == 0 ) {
                qDebug() << "Message with unknown UID";
            } else {
                message->_flags = _parsers[ parser ].syncingFlags[ message->_uid ];
                if ( message->uid() )
                    cache()->setMsgFlags( mailbox->mailbox(), message->uid(), message->_flags );
                if ( ! message->isMarkedAsRead() )
                    ++unSeenCount;
                message->_flagsHandled = true;
                QModelIndex index = createIndex( message->row(), 0, message );
                emit dataChanged( index, index );
            }
        }
        list->_totalMessageCount = list->_children.size();
        list->_unreadMessageCount = unSeenCount;
        list->_numberFetchingStatus = TreeItem::DONE;
        saveUidMap( list );
        _parsers[ parser ].syncingFlags.clear();
        emitMessageCountChanged( mailbox );
    }
}

void Model::_finalizeCreate( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command,  const Imap::Responses::State* const resp )
{
    if ( resp->kind == Responses::OK ) {
        emit mailboxCreationSucceded( command->str );
        CommandHandle cmd = parser->list( QLatin1String(""), command->str );
        _parsers[ parser ].commandMap[ cmd ] = Task( Task::LIST_AFTER_CREATE, command->str );
        emit activityHappening( true );
    } else {
        emit mailboxCreationFailed( command->str, resp->message );
    }
}

void Model::_finalizeDelete( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command,  const Imap::Responses::State* const resp )
{
    Q_UNUSED(parser);
    if ( resp->kind == Responses::OK ) {
        TreeItemMailbox* mailboxPtr = findMailboxByName( command->str );
        if ( mailboxPtr ) {
            TreeItem* parentPtr = mailboxPtr->parent();
            QModelIndex parentIndex = parentPtr == _mailboxes ? QModelIndex() : QAbstractItemModel::createIndex( parentPtr->row(), 0, parentPtr );
            beginRemoveRows( parentIndex, mailboxPtr->row(), mailboxPtr->row() );
            mailboxPtr->parent()->_children.removeAt( mailboxPtr->row() );
            endRemoveRows();
            delete mailboxPtr;
        } else {
            qDebug() << "The IMAP server just told us that it succeded to delete mailbox named" <<
                    command->str << ", yet wo don't know of any such mailbox. Message from the server:" <<
                    resp->message;
        }
        emit mailboxDeletionSucceded( command->str );
    } else {
        emit mailboxDeletionFailed( command->str, resp->message );
    }
}

void Model::handleCapability( Imap::Parser* ptr, const Imap::Responses::Capability* const resp )
{
    _parsers[ ptr ].capabilities = resp->capabilities;
    _parsers[ ptr ].capabilitiesFresh = true;
    updateCapabilities( ptr, resp->capabilities );
}

void Model::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    if ( _parsers[ ptr ].responseHandler )
        _parsers[ ptr ].responseHandler->handleNumberResponse( ptr, resp );
}

void Model::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    if ( _parsers[ ptr ].responseHandler )
        _parsers[ ptr ].responseHandler->handleList( ptr, resp );
}

void Model::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    if ( _parsers[ ptr ].responseHandler )
        _parsers[ ptr ].responseHandler->handleFlags( ptr, resp );
}

void Model::handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp )
{
    if ( _parsers[ ptr ].responseHandler )
        _parsers[ ptr ].responseHandler->handleSearch( ptr, resp );
}

void Model::handleStatus( Imap::Parser* ptr, const Imap::Responses::Status* const resp )
{
    Q_UNUSED( ptr );
    TreeItemMailbox* mailbox = findMailboxByName( resp->mailbox );
    if ( ! mailbox ) {
        qDebug() << "Couldn't find out which mailbox is" << resp->mailbox << "when parsing a STATUS reply";
        return;
    }
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[0] );
    Q_ASSERT( list );
    if ( resp->states.contains( Imap::Responses::Status::MESSAGES ) )
        list->_totalMessageCount = resp->states[ Imap::Responses::Status::MESSAGES ];
    if ( resp->states.contains( Imap::Responses::Status::UNSEEN ) )
        list->_unreadMessageCount = resp->states[ Imap::Responses::Status::UNSEEN ];
    list->_numberFetchingStatus = TreeItem::DONE;
    emitMessageCountChanged( mailbox );
}

void Model::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    if ( _parsers[ ptr ].responseHandler )
        _parsers[ ptr ].responseHandler->handleFetch( ptr, resp );
}

void Model::handleNamespace( Imap::Parser* ptr, const Imap::Responses::Namespace* const resp )
{
    return; // because it's broken and won't fly
    Q_UNUSED(ptr); Q_UNUSED(resp);
}

void Model::handleSort(Imap::Parser *ptr, const Imap::Responses::Sort *const resp)
{
    if ( _parsers[ ptr ].responseHandler )
        _parsers[ ptr ].responseHandler->handleSort( ptr, resp );
}

void Model::handleThread(Imap::Parser *ptr, const Imap::Responses::Thread *const resp)
{
    if ( _parsers[ ptr ].responseHandler )
        _parsers[ ptr ].responseHandler->handleThread( ptr, resp );
}

TreeItem* Model::translatePtr( const QModelIndex& index ) const
{
    return index.internalPointer() ? static_cast<TreeItem*>( index.internalPointer() ) : _mailboxes;
}

QVariant Model::data(const QModelIndex& index, int role ) const
{
    return translatePtr( index )->data( const_cast<Model*>( this ), role );
}

QModelIndex Model::index(int row, int column, const QModelIndex& parent ) const
{
    TreeItem* parentItem = translatePtr( parent );

    if ( column != 0 )
        return QModelIndex();

    TreeItem* child = parentItem->child( row, const_cast<Model*>( this ) );

    return child ? QAbstractItemModel::createIndex( row, column, child ) : QModelIndex();
}

QModelIndex Model::parent(const QModelIndex& index ) const
{
    if ( !index.isValid() )
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parent();

    if ( ! parentItem || parentItem == _mailboxes )
        return QModelIndex();

    return QAbstractItemModel::createIndex( parentItem->row(), 0, parentItem );
}

int Model::rowCount(const QModelIndex& index ) const
{
    TreeItem* node = static_cast<TreeItem*>( index.internalPointer() );
    if ( !node ) {
        node = _mailboxes;
    }
    Q_ASSERT(node);
    return node->rowCount( const_cast<Model*>( this ) );
}

int Model::columnCount(const QModelIndex& index ) const
{
    Q_UNUSED( index );
    return 1;
}

bool Model::hasChildren( const QModelIndex& parent ) const
{
    TreeItem* node = translatePtr( parent );

    if ( node )
        return node->hasChildren( const_cast<Model*>( this ) );
    else
        return false;
}

void Model::_askForChildrenOfMailbox( TreeItemMailbox* item )
{
    if ( networkPolicy() != NETWORK_ONLINE && cache()->childMailboxesFresh( item->mailbox() ) ) {
        // We aren't online and the permanent cache contains relevant data
        QList<MailboxMetadata> metadata = cache()->childMailboxes( item->mailbox() );
        QList<TreeItem*> mailboxes;
        for ( QList<MailboxMetadata>::const_iterator it = metadata.begin(); it != metadata.end(); ++it ) {
            mailboxes << TreeItemMailbox::fromMetadata( item, *it );
        }
        TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item );
        Q_ASSERT( mailboxPtr );
        // We can't call replaceChildMailboxes() here directly, as we're likely invoked from inside GUI
        _MailboxListUpdater* updater = new _MailboxListUpdater( this, mailboxPtr, mailboxes );
        QTimer::singleShot( 0, updater, SLOT(perform()) );
    } else if ( networkPolicy() == NETWORK_OFFLINE ) {
        // No cached data, no network -> fail
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        // We have to go to the network
        new ListChildMailboxesTask( this, createIndex( item->row(), 0, item) );
    }
    QModelIndex idx = createIndex( item->row(), 0, item );
    emit dataChanged( idx, idx );
}

void Model::reloadMailboxList()
{
    _mailboxes->rescanForChildMailboxes( this );
}

void Model::_askForMessagesInMailbox( TreeItemMsgList* item )
{
    Q_ASSERT( item->parent() );
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->parent() );
    Q_ASSERT( mailboxPtr );

    QString mailbox = mailboxPtr->mailbox();

    Q_ASSERT( item->_children.size() == 0 );

    bool cacheOk = false;
    QList<uint> uidMapping = cache()->uidMapping( mailbox );
    if ( networkPolicy() == NETWORK_OFFLINE && uidMapping.size() != item->_totalMessageCount ) {
        qDebug() << "UID cache stale for mailbox" << mailbox <<
                "(" << uidMapping.size() << "in UID cache vs." <<
                item->_totalMessageCount << "as totalMessageCount)";;
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else if ( uidMapping.size() ) {
        QModelIndex listIndex = createIndex( item->row(), 0, item );
        beginInsertRows( listIndex, 0, uidMapping.size() - 1 );
        for ( uint seq = 0; seq < static_cast<uint>( uidMapping.size() ); ++seq ) {
            TreeItemMessage* message = new TreeItemMessage( item );
            message->_offset = seq;
            message->_uid = uidMapping[ seq ];
            item->_children << message;
        }
        endInsertRows();
        cacheOk = true;
        item->_fetchStatus = TreeItem::DONE; // required for FETCH processing later on
    }

    if ( networkPolicy() != NETWORK_OFFLINE ) {
        new CreateConnectionTask( this, mailboxPtr );
        // and that's all -- we will detect following replies and sync automatically
    }
}

void Model::_askForNumberOfMessages( TreeItemMsgList* item )
{
    Q_ASSERT( item->parent() );
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->parent() );
    Q_ASSERT( mailboxPtr );

    if ( networkPolicy() == NETWORK_OFFLINE ) {
        Imap::Mailbox::SyncState syncState = cache()->mailboxSyncState( mailboxPtr->mailbox() );
        if ( syncState.isComplete() ) {
            item->_unreadMessageCount = 0;
            item->_totalMessageCount = syncState.exists();
            item->_numberFetchingStatus = TreeItem::DONE;
            // We're most likely invoked from deep inside the GUI, so we have to delay the update
            _NumberOfMessagesUpdater* updater = new _NumberOfMessagesUpdater( this, mailboxPtr );
            QTimer::singleShot( 0, updater, SLOT(perform()) );
        } else {
            item->_numberFetchingStatus = TreeItem::UNAVAILABLE;
        }
    } else {
        new NumberOfMessagesTask( this, createIndex( mailboxPtr->row(), 0, mailboxPtr ) );
    }
}

void Model::_askForMsgMetadata( TreeItemMessage* item )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( item->parent() );
    Q_ASSERT( list );
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( list->parent() );
    Q_ASSERT( mailboxPtr );

    if ( item->uid() ) {
        AbstractCache::MessageDataBundle data = cache()->messageMetadata( mailboxPtr->mailbox(), item->uid() );
        if ( data.uid == item->uid() ) {
            item->_envelope = data.envelope;
            item->_flags = cache()->msgFlags( mailboxPtr->mailbox(), item->uid() );
            item->_size = data.size;
            QDataStream stream( &data.serializedBodyStructure, QIODevice::ReadOnly );
            QVariantList unserialized;
            stream >> unserialized;
            QSharedPointer<Message::AbstractMessage> abstractMessage;
            try {
                abstractMessage = Message::AbstractMessage::fromList( unserialized, QByteArray(), 0 );
            } catch ( Imap::ParserException& e ) {
                qDebug() << "Error when parsing cached BODYSTRUCTURE" << e.what();
            }
            if ( ! abstractMessage ) {
                item->_fetchStatus = TreeItem::UNAVAILABLE;
            } else {
                QList<TreeItem*> newChildren = abstractMessage->createTreeItems( item );
                QList<TreeItem*> oldChildren = item->setChildren( newChildren );
                Q_ASSERT( oldChildren.size() == 0 );
                item->_fetchStatus = TreeItem::DONE;
            }
        }
    }

    if ( item->fetched() ) {
        // Nothing to do here
        return;
    }

    int order = item->row();

    switch ( networkPolicy() ) {
        case NETWORK_OFFLINE:
            break;
        case NETWORK_EXPENSIVE:
            {
                item->_fetchStatus = TreeItem::LOADING;
                new FetchMsgMetadataTask( this, QModelIndexList() << createIndex( item->row(), 0, item ) );
            }
            break;
        case NETWORK_ONLINE:
            {
                // preload
                QModelIndexList items;
                for ( int i = qMax( 0, order - StructurePreload );
                      i < qMin( list->_children.size(), order + StructurePreload );
                      ++i ) {
                    TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( list->_children[i] );
                    Q_ASSERT( message );
                    if ( item == message || ( ! message->fetched() && ! message->loading() ) ) {
                        message->_fetchStatus = TreeItem::LOADING;
                        items << createIndex( message->row(), 0, message );
                    }
                }
                new FetchMsgMetadataTask( this, items );
            }
            break;
    }
}

void Model::_askForMsgPart( TreeItemPart* item, bool onlyFromCache )
{
    // FIXME: fetch parts in chunks, not at once
    Q_ASSERT( item->message() ); // TreeItemMessage
    Q_ASSERT( item->message()->parent() ); // TreeItemMsgList
    Q_ASSERT( item->message()->parent()->parent() ); // TreeItemMailbox
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->message()->parent()->parent() );
    Q_ASSERT( mailboxPtr );

    uint uid = static_cast<TreeItemMessage*>( item->message() )->uid();
    if ( uid ) {
        const QByteArray& data = cache()->messagePart( mailboxPtr->mailbox(), uid, item->partId() );
        if ( ! data.isNull() ) {
            item->_data = data;
            item->_fetchStatus = TreeItem::DONE;
        }
    }

    if ( networkPolicy() == NETWORK_OFFLINE ) {
        if ( item->_fetchStatus != TreeItem::DONE )
            item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else if ( ! onlyFromCache ) {
        new FetchMsgPartTask( this, mailboxPtr, item );
    }
}

void Model::resyncMailbox( TreeItemMailbox* mbox )
{
    _getParser( mbox, ReadOnly, true );
}

void Model::performNoop()
{
    for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
        CommandHandle cmd = it->parser->noop();
        it->commandMap[ cmd ] = Task( Task::NOOP, 0 );
    }
}

Parser* Model::_getParser( TreeItemMailbox* mailbox, const RWMode mode, const bool reSync )
{
    static uint lastParserId = 0;

    Q_ASSERT( _netPolicy != NETWORK_OFFLINE );

    if ( ! mailbox && ! _parsers.isEmpty() ) {
        // Request does not specify a target mailbox
        return _parsers.begin().value().parser;
    } else {
        // Got to make sure we already have a mailbox selected
        for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
            if ( it->mailbox == mailbox ) {
                if ( mode == ReadOnly || it->mode == mode ) {
                    // The mailbox is already opened in a correct mode
                    if ( reSync ) {
                        CommandHandle cmd = ( it->mode == ReadWrite ) ?
                                            it->parser->select( mailbox->mailbox() ) :
                                            it->parser->examine( mailbox->mailbox() );
                        it->commandMap[ cmd ] = Task( Task::SELECT, mailbox );
                        it->mailbox = mailbox;
                        ++it->selectingAnother;
                    }
                    return it->parser;
                } else {
                    // Got to upgrade to R/W mode
                    it->mode = ReadWrite;
                    CommandHandle cmd = it->parser->select( mailbox->mailbox() );
                    it->commandMap[ cmd ] = Task( Task::SELECT, mailbox );
                    it->mailbox = mailbox;
                    ++it->selectingAnother;
                    return it->parser;
                }
            }
        }
    }
    // At this point, we're sure that there was no parser which already had opened the correct mailbox
    if ( _parsers.size() >= _maxParsers ) {
        // We can't create more parsers
        ParserState& parser = _parsers.begin().value();
        parser.mode = mode;
        parser.mailbox = mailbox;
        CommandHandle cmd;
        if ( mode == ReadWrite )
            cmd = parser.parser->select( mailbox->mailbox() );
        else
            cmd = parser.parser->examine( mailbox->mailbox() );
        parser.commandMap[ cmd ] = Task( Task::SELECT, mailbox );
        emit const_cast<Model*>(this)->activityHappening( true );
        ++parser.selectingAnother;
        return parser.parser;
    } else {
        // We can create one more, but we should try to find one which already exists,
        // but doesn't have an opened mailbox yet

        for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
            if ( ! it->mailbox ) {
                // ... and that's the one!
                CommandHandle cmd;
                if ( mode == ReadWrite )
                    cmd = it->parser->select( mailbox->mailbox() );
                else
                    cmd = it->parser->examine( mailbox->mailbox() );
                it->commandMap[ cmd ] = Task( Task::SELECT, mailbox );
                it->mailbox = mailbox;
                it->mode = mode;
                ++it->selectingAnother;
                return it->parser;
            }
        }

        // Now we can be sure that all the already-existing parsers are occupied and we can create one more
        // -> go for it.

        Parser* parser( new Parser( const_cast<Model*>( this ), _socketFactory->create(), ++lastParserId ) );
        _parsers[ parser ] = ParserState( parser, mailbox, mode, CONN_STATE_NONE, unauthHandler );
        _parsers[ parser ].idleLauncher = new IdleLauncher( this, parser );
        connect( parser, SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
        connect( parser, SIGNAL( disconnected( const QString ) ), this, SLOT( slotParserDisconnected( const QString ) ) );
        connect( parser, SIGNAL(connectionStateChanged(Imap::ConnectionState)), this, SLOT(handleSocketStateChanged(Imap::ConnectionState)) );
        connect( parser, SIGNAL(sendingCommand(QString)), this, SLOT(parserIsSendingCommand(QString)) );
        connect( parser, SIGNAL(parseError(QString,QString,QByteArray,int)), this, SLOT(slotParseError(QString,QString,QByteArray,int)) );
        connect( parser, SIGNAL(lineReceived(QByteArray)), this, SLOT(slotParserLineReceived(QByteArray)) );
        connect( parser, SIGNAL(lineSent(QByteArray)), this, SLOT(slotParserLineSent(QByteArray)) );
        connect( parser, SIGNAL( idleTerminated() ), this, SLOT( idleTerminated() ) );
        CommandHandle cmd;
        if ( _startTls ) {
            cmd = parser->startTls();
            _parsers[ parser ].commandMap[ cmd ] = Task( Task::STARTTLS, 0 );
            emit const_cast<Model*>(this)->activityHappening( true );
        }
        if ( mailbox ) {
            if ( mode == ReadWrite )
                cmd = parser->select( mailbox->mailbox() );
            else
                cmd = parser->examine( mailbox->mailbox() );
            _parsers[ parser ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
            emit const_cast<Model*>(this)->activityHappening( true );
            _parsers[ parser ].mailbox = mailbox;
            _parsers[ parser ].mode = mode;
            ++_parsers[ parser ].selectingAnother;
        }
        return parser;
    }
}

void Model::setNetworkPolicy( const NetworkPolicy policy )
{
    // If we're connecting after being offline, we should ask for an updated list of mailboxes
    // The main reason is that this happens after entering wrong password and going back online
    bool shouldReloadMailboxes = _netPolicy == NETWORK_OFFLINE && policy != NETWORK_OFFLINE;
    switch ( policy ) {
        case NETWORK_OFFLINE:
            noopTimer->stop();
            for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
                CommandHandle cmd = it->parser->logout();
                _parsers[ it.key() ].commandMap[ cmd ] = Task( Task::LOGOUT, 0 );
                emit activityHappening( true );
            }
            emit networkPolicyOffline();
            _netPolicy = NETWORK_OFFLINE;
            // FIXME: kill the connection
            break;
        case NETWORK_EXPENSIVE:
            _netPolicy = NETWORK_EXPENSIVE;
            noopTimer->stop();
            emit networkPolicyExpensive();
            break;
        case NETWORK_ONLINE:
            _netPolicy = NETWORK_ONLINE;
            noopTimer->start( PollingPeriod );
            emit networkPolicyOnline();
            break;
    }
    if ( shouldReloadMailboxes )
        reloadMailboxList();
}

void Model::slotParserDisconnected( const QString msg )
{
    emit connectionError( msg );

    Parser* which = qobject_cast<Parser*>( sender() );
    if ( ! which )
        return;

    // This function is *not* called from inside the responseReceived(), so we have to remove the parser from the list, too
    killParser( which );
    _parsers.remove( which );
    parsersMightBeIdling();
}

void Model::broadcastParseError( const uint parser, const QString& exceptionClass, const QString& errorMessage, const QByteArray& line, int position )
{
    emit logParserFatalError( parser, exceptionClass, errorMessage, line, position );
    QByteArray details = ( position == -1 ) ? QByteArray() : QByteArray( position, ' ' ) + QByteArray("^ here");
    emit connectionError( trUtf8( "<p>The IMAP server sent us a reply which we could not parse. "
                                  "This might either mean that there's a bug in Trojiá's code, or "
                                  "that the IMAP server you are connected to is broken. Please "
                                  "report this as a bug anyway. Here are the details:</p>"
                                  "<p><b>%1</b>: %2</p>"
                                  "<pre>%3\n%4</pre>"
                                  ).arg( exceptionClass, errorMessage, line, details ) );
}

void Model::slotParseError( const QString& exceptionClass, const QString& errorMessage, const QByteArray& line, int position )
{
    Parser* which = qobject_cast<Parser*>( sender() );
    Q_ASSERT( which );

    broadcastParseError( which->parserId(), exceptionClass, errorMessage, line, position );

    // This function is *not* called from inside the responseReceived(), so we have to remove the parser from the list, too
    killParser( which );
    _parsers.remove( which );
    parsersMightBeIdling();
}

void Model::idleTerminated()
{
    QMap<Parser*,ParserState>::iterator it = _parsers.find( qobject_cast<Imap::Parser*>( sender() ));
    if ( it == _parsers.end() ) {
        return;
    } else {
        Q_ASSERT( it->idleLauncher );
        it->idleLauncher->idlingTerminated();
    }
}

void Model::switchToMailbox( const QModelIndex& mbox, const RWMode mode )
{
    if ( ! mbox.isValid() )
        return;

    if ( _netPolicy == NETWORK_OFFLINE )
        return;

    if ( TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>(
                 realTreeItem( mbox ) ) ) {
        Parser* ptr = _getParser( mailbox, mode );
        if ( _parsers[ ptr ].capabilitiesFresh &&
             _parsers[ ptr ].capabilities.contains( QLatin1String( "IDLE" ) ) ) {
            _parsers[ ptr ].idleLauncher->enterIdleLater();
        }
    }
}

void Model::enterIdle( Parser* parser )
{
    noopTimer->stop();
    CommandHandle cmd = parser->idle();
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::IDLE, 0 );
}

void Model::updateCapabilities( Parser* parser, const QStringList capabilities )
{
    parser->enableLiteralPlus( capabilities.contains( QLatin1String( "LITERAL+" ) ) );
}

void Model::markMessageDeleted( TreeItemMessage* msg, bool marked )
{
    new UpdateFlagsTask( this, QModelIndexList() << createIndex( msg->row(), 0, msg ),
                         marked ? QLatin1String("+FLAGS") : QLatin1String("-FLAGS"),
                         QLatin1String("(\\Deleted)") );
}

void Model::markMessageRead( TreeItemMessage* msg, bool marked )
{
    new UpdateFlagsTask( this, QModelIndexList() << createIndex( msg->row(), 0, msg ),
                         marked ? QLatin1String("+FLAGS") : QLatin1String("-FLAGS"),
                         QLatin1String("(\\Seen)") );
}

void Model::copyMessages( TreeItemMailbox* sourceMbox, const QString& destMailboxName, const Sequence& seq )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        // FIXME: error signalling
        return;
    }

    Q_ASSERT( sourceMbox );
    Parser* parser = _getParser( sourceMbox, ReadOnly );
    CommandHandle cmd = parser->uidCopy( seq, destMailboxName );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::COPY, sourceMbox );
    emit activityHappening( true );
}

void Model::markUidsDeleted( TreeItemMailbox* mbox, const Sequence& messages )
{
    Q_ASSERT( mbox );
    Parser* parser = _getParser( mbox, ReadWrite );
    CommandHandle cmd = parser->uidStore( messages, QLatin1String("+FLAGS"), QLatin1String("\\Deleted") );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::STORE, mbox );
    emit activityHappening( true );
}

TreeItemMailbox* Model::findMailboxByName( const QString& name ) const
{
    return findMailboxByName( name, _mailboxes );
}

TreeItemMailbox* Model::findMailboxByName( const QString& name,
                                           const TreeItemMailbox* const root ) const
{
    Q_ASSERT( ! root->_children.isEmpty() );
    // FIXME: names are sorted, so linear search is not required
    for ( int i = 1; i < root->_children.size(); ++i ) {
        TreeItemMailbox* mailbox = static_cast<TreeItemMailbox*>( root->_children[i] );
        if ( name == mailbox->mailbox() )
            return mailbox;
        else if ( name.startsWith( mailbox->mailbox() + mailbox->separator() ) )
            return findMailboxByName( name, mailbox );
    }
    return 0;
}

TreeItemMailbox* Model::findParentMailboxByName( const QString& name ) const
{
    TreeItemMailbox* root = _mailboxes;
    while ( true ) {
        if ( root->_children.size() == 1 ) {
            break;
        }
        bool found = false;
        for ( int i = 1; ! found && i < root->_children.size(); ++i ) {
            TreeItemMailbox* const item = dynamic_cast<TreeItemMailbox*>( root->_children[i] );
            Q_ASSERT( item );
            if ( name.startsWith( item->mailbox() + item->separator() ) ) {
                root = item;
                found = true;
            }
        }
        if ( ! found ) {
            return root;
        }
    }
    return root;
}


void Model::expungeMailbox( TreeItemMailbox* mbox )
{
    if ( ! mbox )
        return;

    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Can't expunge while offline";
        return;
    }

    new ExpungeMailboxTask( this, createIndex( mbox->row(), 0, mbox ) );
}

void Model::createMailbox( const QString& name )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Can't create mailboxes while offline";
        return;
    }

    Parser* parser = _getParser( 0, ReadOnly );
    CommandHandle cmd = parser->create( name );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::CREATE, name );
    emit activityHappening( true );
}

void Model::deleteMailbox( const QString& name )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Can't delete mailboxes while offline";
        return;
    }

    Parser* parser = _getParser( 0, ReadOnly );
    CommandHandle cmd = parser->deleteMailbox( name );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::DELETE, name );
    emit activityHappening( true );
}

void Model::saveUidMap( TreeItemMsgList* list )
{
    QList<uint> seqToUid;
    for ( int i = 0; i < list->_children.size(); ++i )
        seqToUid << static_cast<TreeItemMessage*>( list->_children[ i ] )->uid();
    cache()->setUidMapping( static_cast<TreeItemMailbox*>( list->parent() )->mailbox(), seqToUid );
}


TreeItem* Model::realTreeItem( QModelIndex index, const Model** whichModel, QModelIndex* translatedIndex )
{
    const QAbstractProxyModel* proxy = qobject_cast<const QAbstractProxyModel*>( index.model() );
    while ( proxy ) {
        index = proxy->mapToSource( index );
        proxy = qobject_cast<const QAbstractProxyModel*>( index.model() );
    }
    const Model* model = qobject_cast<const Model*>( index.model() );
    if ( ! model ) {
        qDebug() << index << "does not belong to Imap::Mailbox::Model";
        return 0;
    }
    if ( whichModel )
        *whichModel = model;
    if ( translatedIndex )
        *translatedIndex = index;
    return static_cast<TreeItem*>( index.internalPointer() );
}

void Model::performAuthentication( Imap::Parser* ptr )
{
    // The LOGINDISABLED capability is checked elsewhere
    if ( ! _authenticator ) {
        _authenticator = new QAuthenticator();
        emit authRequested( _authenticator );
    }

    if ( _authenticator->isNull() ) {
        delete _authenticator;
        _authenticator = 0;
        emit connectionError( tr("Can't login without user/password data") );
    } else {
        CommandHandle cmd = ptr->login( _authenticator->user(), _authenticator->password() );
        _parsers[ ptr ].commandMap[ cmd ] = Task( Task::LOGIN, 0 );
        emit activityHappening( true );
    }
}

void Model::changeConnectionState(Parser *parser, ConnectionState state)
{
    _parsers[ parser ].connState = state;
    emit connectionStateChanged( parser, state );
}

void Model::handleSocketStateChanged(Imap::ConnectionState state)
{
    Parser* ptr = qobject_cast<Parser*>( sender() );
    Q_ASSERT(ptr);
    if ( _parsers[ ptr ].connState < state ) {
        changeConnectionState( ptr, state );
    }
}

void Model::parserIsSendingCommand( const QString& tag)
{
    Parser* ptr = qobject_cast<Parser*>( sender() );
    Q_ASSERT(ptr);
    QMap<CommandHandle, Task>::const_iterator it = _parsers[ ptr ].commandMap.find( tag );
    if ( it == _parsers[ ptr ].commandMap.end() ) {
        qDebug() << "Dunno anything about command" << tag;
        return;
    }

    switch ( it->kind ) {
        case Task::NONE: // invalid
        case Task::STARTTLS: // handled elsewhere
        case Task::NAMESPACE: // FIXME: not needed yet
        case Task::LOGOUT: // not worth the effort
            break;
        case Task::LOGIN:
            changeConnectionState( ptr, CONN_STATE_LOGIN );
            break;
        case Task::SELECT:
            changeConnectionState( ptr, CONN_STATE_SELECTING );
            break;
        case Task::FETCH_WITH_FLAGS:
            changeConnectionState( ptr, CONN_STATE_SYNCING );
            break;
        case Task::FETCH_PART:
            changeConnectionState( ptr, CONN_STATE_FETCHING_PART );
            break;
        case Task::FETCH_MESSAGE_METADATA:
            changeConnectionState( ptr, CONN_STATE_FETCHING_MSG_METADATA );
            break;
        case Task::NOOP:
        case Task::IDLE:
            // do nothing
            break;
    }
}

void Model::parsersMightBeIdling()
{
    bool someParserBusy = false;
    Q_FOREACH( const ParserState& p, _parsers ) {
        if ( p.commandMap.isEmpty() )
            continue;
        Q_FOREACH( const Task& t, p.commandMap ) {
            if ( t.kind != Task::IDLE ) {
                someParserBusy = true;
                break;
            }
        }
    }
    emit activityHappening( someParserBusy );
}

void Model::killParser(Parser *parser)
{
    for ( QMap<CommandHandle, Task>::const_iterator it = _parsers[ parser ].commandMap.begin();
            it != _parsers[ parser ].commandMap.end(); ++it ) {
        // FIXME: fail the command, perform cleanup,...
    }
    _parsers[ parser ].commandMap.clear();
    _parsers[ parser ].idleLauncher->die();
    _parsers[ parser ].idleLauncher->deleteLater();
    _parsers[ parser ].idleLauncher = 0;
    noopTimer->stop();
    parser->disconnect();
    parser->deleteLater();
    _parsers[ parser ].parser = 0;
}

void Model::slotParserLineReceived( const QByteArray& line )
{
    Parser* parser = qobject_cast<Parser*>( sender() );
    Q_ASSERT( parser );
    Q_ASSERT( _parsers.contains( parser ) );
    emit logParserLineReceived( parser->parserId(), line );
}

void Model::slotParserLineSent( const QByteArray& line )
{
    Parser* parser = qobject_cast<Parser*>( sender() );
    Q_ASSERT( parser );
    Q_ASSERT( _parsers.contains( parser ) );
    emit logParserLineSent( parser->parserId(), line );
}

void Model::setCache( AbstractCache* cache )
{
    if ( _cache )
        _cache->deleteLater();
    _cache = cache;
    _cache->setParent( this );
}

}
}
