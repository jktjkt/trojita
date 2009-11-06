/* Copyright (C) 2007 - 2008 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "Model.h"
#include "MailboxTree.h"
#include "UnauthenticatedHandler.h"
#include "AuthenticatedHandler.h"
#include "SelectedHandler.h"
#include "SelectingHandler.h"
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

ModelStateHandler::ModelStateHandler( Model* _m ): m(_m)
{
}

IdleLauncher::IdleLauncher( Model* model, Parser* ptr ):
        m(model), parser(ptr), _idling(false)
{
    timer = new QTimer( this );
    timer->setSingleShot( true );
    timer->setInterval( 1000 );
    connect( timer, SIGNAL(timeout()), this, SLOT(perform()) );
}

void IdleLauncher::perform()
{
    if ( ! parser ) {
        timer->stop();
        return;
    }
    m->enterIdle( parser );
    _idling = true;
}

void IdleLauncher::idlingTerminated()
{
    _idling = false;
}

void IdleLauncher::restart()
{
    timer->start();
}

bool IdleLauncher::idling()
{
    return _idling;
}


Model::Model( QObject* parent, CachePtr cache, SocketFactoryPtr socketFactory, bool offline ):
    // parent
    QAbstractItemModel( parent ),
    // our tools
    _cache(cache), _socketFactory(socketFactory),
    _maxParsers(1), _mailboxes(0), _netPolicy( NETWORK_ONLINE )
{
    _startTls = _socketFactory->startTlsRequired();

    unauthHandler = new UnauthenticatedHandler( this );
    authenticatedHandler = new AuthenticatedHandler( this );
    selectedHandler = new SelectedHandler( this );
    selectingHandler = new SelectingHandler( this );

    _mailboxes = new TreeItemMailbox( 0 );

    _onlineMessageFetch << "ENVELOPE" << "BODYSTRUCTURE" << "RFC822.SIZE" << "UID" << "FLAGS";

    noopTimer = new QTimer( this );
    connect( noopTimer, SIGNAL(timeout()), this, SLOT(performNoop()) );

    if ( offline ) {
        _netPolicy = NETWORK_OFFLINE;
        QTimer::singleShot( 0, this, SLOT(setNetworkOffline()) );
    } else {
        // FIXME: socket error handling
        Parser* parser( new Imap::Parser( this, _socketFactory->create() ) );
        _parsers[ parser ] = ParserState( parser, 0, ReadOnly, CONN_STATE_ESTABLISHED, unauthHandler );
        connect( parser, SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
        connect( parser, SIGNAL( disconnected( const QString ) ), this, SLOT( slotParserDisconnected( const QString ) ) );
        if ( _startTls ) {
            CommandHandle cmd = parser->startTls();
            _parsers[ parser ].commandMap[ cmd ] = Task( Task::STARTTLS, 0 );
        }
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

    int i = 0;

    while ( it.value().parser->hasResponse() ) {
        QSharedPointer<Imap::Responses::AbstractResponse> resp = it.value().parser->getResponse();
        Q_ASSERT( resp );
        resp->plug( it.value().parser, this );
        if ( ! it.value().parser ) {
            // it got deleted
            _parsers.erase( it );
            break;
        }
        ++i;
        if ( i == 100 ) {
            qApp->processEvents();
            i = 0;
        }
    }
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
                    QAuthenticator auth;
                    emit authRequested( &auth );
                    if ( auth.isNull() ) {
                        emit connectionError( tr("Can't login without user/password data") );
                    } else {
                        CommandHandle cmd = ptr->login( auth.user(), auth.password() );
                        _parsers[ ptr ].commandMap[ cmd ] = Task( Task::LOGIN, 0 );
                    }
                } else {
                    emit connectionError( tr("Can't establish a secure connection to the server (STARTTLS failed). Refusing to proceed.") );
                }
                break;
            case Task::LOGIN:
                if ( resp->kind == Responses::OK ) {
                    _parsers[ ptr ].connState = CONN_STATE_AUTH;
                    _parsers[ ptr ].responseHandler = authenticatedHandler;
                    if ( ! _parsers[ ptr ].capabilitiesFresh ) {
                        CommandHandle cmd = ptr->capability();
                        _parsers[ ptr ].commandMap[ cmd ] = Task( Task::CAPABILITY, 0 );
                    }
                    CommandHandle cmd = ptr->namespaceCommand();
                    _parsers[ ptr ].commandMap[ cmd ] = Task( Task::NAMESPACE, 0 );
                    completelyReset();
                } else {
                    // FIXME: handle this in a sane way
                    emit connectionError( tr("Login Failed") );
                }
                break;
            case Task::NONE:
                throw CantHappen( "Internal Error: command that is supposed to do nothing?", *resp );
                break;
            case Task::LIST:
                if ( resp->kind == Responses::OK ) {
                    _finalizeList( ptr, command );
                } else {
                    // FIXME
                }
                break;
            case Task::STATUS:
                // FIXME
                break;
            case Task::SELECT:
                --_parsers[ ptr ].selectingAnother;
                if ( resp->kind == Responses::OK ) {
                    _finalizeSelect( ptr, command );
                } else {
                    if ( _parsers[ ptr ].connState == CONN_STATE_SELECTED )
                        _parsers[ ptr ].connState = CONN_STATE_AUTH;
                    // FIXME: error handling
                }
                break;
            case Task::FETCH:
            case Task::FETCH_WITH_FLAGS:
                _finalizeFetch( ptr, command );
                break;
            case Task::NOOP:
            case Task::CAPABILITY:
                // We don't have to do anything here
                break;
            case Task::STORE:
                // FIXME: check for errors
                break;
            case Task::NAMESPACE:
                // FIXME: what to do
                break;
            case Task::EXPUNGE:
                // FIXME
                break;
            case Task::COPY:
                // FIXME
                break;
            case Task::CREATE:
                // FIXME
                break;
            case Task::DELETE:
                // FIXME: error reporting...
                break;
            case Task::LOGOUT:
                // we are inside while loop in responseReceived(), so we can't delete current parser just yet
                disconnect( ptr, SIGNAL( disconnected( const QString& ) ), this, SLOT( slotParserDisconnected( const QString& ) ) );
                ptr->deleteLater();
                _parsers[ ptr ].parser = 0; // because sometimes it isn't deleted yet
                return;
        }

        Q_ASSERT( _parsers.contains( ptr ) );
        Q_ASSERT( _parsers[ ptr ].commandMap.contains( command.key() ) );
        _parsers[ ptr ].commandMap.erase( command );

    } else {
        // untagged response
        if ( _parsers[ ptr ].responseHandler )
            _parsers[ ptr ].responseHandler->handleState( ptr, resp );
    }
}

void Model::_finalizeList( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( command->what );
    QList<TreeItem*> mailboxes;

    QList<Responses::List>& listResponses = _parsers[ parser ].listResponses;
    for ( QList<Responses::List>::const_iterator it = listResponses.begin();
            it != listResponses.end(); ++it ) {
        if ( it->mailbox != mailboxPtr->mailbox() + mailboxPtr->separator() )
            mailboxes << new TreeItemMailbox( command->what, *it );
    }
    listResponses.clear();
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
    for ( QList<TreeItem*>::const_iterator it = mailboxes.begin(); it != mailboxes.end(); ++it ) {
        metadataToCache.append( dynamic_cast<TreeItemMailbox*>( *it )->mailboxMetadata() );
    }
    _cache->setChildMailboxes( mailboxPtr->mailbox(), metadataToCache );

    replaceChildMailboxes( mailboxPtr, mailboxes );
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
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( command->what );
    Q_ASSERT( mailbox );
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[ 0 ] );
    Q_ASSERT( list );
    _parsers[ parser ].currentMbox = mailbox;
    _parsers[ parser ].responseHandler = selectedHandler;

    const SyncState& syncState = _parsers[ parser ].syncState;
    const SyncState& oldState = _cache->mailboxSyncState( mailbox->mailbox() );

    list->_totalMessageCount = syncState.exists();
    // Note: syncState.unSeen() is the NUMBER of the first unseen message, not their count!

    if ( _parsers[ parser ].selectingAnother ) {
        emitMessageCountChanged( mailbox );
        // We have already queued a command that switches to another mailbox
        // Asking the parser to switch back would only make the situation worse,
        // so we can't do anything better than exit right now
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
                    CommandHandle cmd = parser->fetch( Sequence::startingAt( 1 ),
                                                       items );
                    _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
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
                _cache->setMailboxSyncState( mailbox->mailbox(), syncState );
                saveUidMap( list );

            } else {
                // Some messages got deleted, but there have been no additions

                CommandHandle cmd = parser->fetch( Sequence::startingAt( 1 ),
                                                   QStringList() << "UID" << "FLAGS" );
                _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
                list->_numberFetchingStatus = TreeItem::LOADING;
                list->_unreadMessageCount = 0;
                // selecting handler should do the rest
                _parsers[ parser ].responseHandler = selectingHandler;
                QList<uint>& uidMap = _parsers[ parser ].uidMap;
                uidMap.clear();
                _parsers[ parser ].syncingFlags.clear();
                for ( uint i = 0; i < syncState.exists(); ++i )
                    uidMap << 0;
                _cache->clearUidMapping( mailbox->mailbox() );
            }

        } else {
            // Some new messages were delivered since we checked the last time.
            // There's no guarantee they are still present, though.

            if ( syncState.uidNext() - oldState.uidNext() == syncState.exists() - oldState.exists() ) {
                // Only some new arrivals, no deletions

                for ( uint i = 0; i < syncState.uidNext() - oldState.uidNext(); ++i ) {
                    list->_children << new TreeItemMessage( list );
                }

                QStringList items = ( networkPolicy() == NETWORK_ONLINE &&
                                      syncState.uidNext() - oldState.uidNext() <= StructureFetchLimit ) ?
                                    _onlineMessageFetch : QStringList() << "UID" << "FLAGS";
                CommandHandle cmd = parser->fetch( Sequence::startingAt( oldState.exists() + 1 ),
                                                   items );
                _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
                list->_numberFetchingStatus = TreeItem::LOADING;
                list->_fetchStatus = TreeItem::DONE;
                list->_unreadMessageCount = 0;
                _cache->clearUidMapping( mailbox->mailbox() );

            } else {
                // Generic case; we don't know anything about which messages were deleted and which added
                // FIXME: might be possible to optimize here...

                // At first, let's ask for UID numbers and FLAGS for all messages
                CommandHandle cmd = parser->fetch( Sequence::startingAt( 1 ),
                                                   QStringList() << "UID" << "FLAGS" );
                _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
                _parsers[ parser ].responseHandler = selectingHandler;
                list->_numberFetchingStatus = TreeItem::LOADING;
                list->_unreadMessageCount = 0;
                QList<uint>& uidMap = _parsers[ parser ].uidMap;
                uidMap.clear();
                _parsers[ parser ].syncingFlags.clear();
                for ( uint i = 0; i < syncState.exists(); ++i )
                    uidMap << 0;
                _cache->clearUidMapping( mailbox->mailbox() );
            }
        }
    } else {
        // Forget everything, do a dumb sync
        _cache->clearAllMessages( mailbox->mailbox() );
        _fullMboxSync( mailbox, list, parser, syncState );
    }
    emitMessageCountChanged( mailbox );
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
    _cache->clearUidMapping( mailbox->mailbox() );

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
            list->_children << message;
            if ( willLoad )
                message->_fetchStatus = TreeItem::LOADING;
        }
        endInsertRows();
        list->_fetchStatus = TreeItem::DONE;

        Q_ASSERT( ! _parsers[ parser ].selectingAnother );

        QStringList items = willLoad ? _onlineMessageFetch : QStringList() << "UID" << "FLAGS";
        CommandHandle cmd = parser->fetch( Sequence::startingAt( 1 ), items );
        _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH_WITH_FLAGS, mailbox );
        list->_numberFetchingStatus = TreeItem::LOADING;
        list->_unreadMessageCount = 0;
    } else {
        list->_totalMessageCount = 0;
        list->_unreadMessageCount = 0;
        list->_numberFetchingStatus = TreeItem::DONE;
        list->_fetchStatus = TreeItem::DONE;
        _cache->setMailboxSyncState( mailbox->mailbox(), syncState );
        saveUidMap( list );
    }
    emitMessageCountChanged( mailbox );
}

void Model::_finalizeFetch( Parser* parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( command.value().what );
    TreeItemPart* part = dynamic_cast<TreeItemPart*>( command.value().what );
    if ( part && part->loading() ) {
        qDebug() << "Imap::Model::_finalizeFetch(): didn't receive anything about message" <<
            part->message()->row() << "part" << part->partId() << "in mailbox" <<
            _parsers[ parser ].mailbox->mailbox();
        part->_fetchStatus = TreeItem::DONE;
    }
    if ( mailbox && _parsers[ parser ].responseHandler == selectedHandler ) {
        mailbox->_children[0]->_fetchStatus = TreeItem::DONE;
        _cache->setMailboxSyncState( mailbox->mailbox(), _parsers[ parser ].syncState );
        TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[0] );
        Q_ASSERT( list );
        saveUidMap( list );
        if ( command->kind == Task::FETCH_WITH_FLAGS ) {
            list->recalcUnreadMessageCount();
            list->_numberFetchingStatus = TreeItem::DONE;
        }
        emitMessageCountChanged( mailbox );
    } else if ( mailbox && _parsers[ parser ].responseHandler == selectingHandler ) {
        _parsers[ parser ].responseHandler = selectedHandler;
        _cache->setMailboxSyncState( mailbox->mailbox(), _parsers[ parser ].syncState );

        QList<uint>& uidMap = _parsers[ parser ].uidMap;
        TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[0] );
        Q_ASSERT( list );
        list->_fetchStatus = TreeItem::DONE;

        QModelIndex parent = createIndex( 0, 0, list );
        if ( uidMap.isEmpty() ) {
            if ( list->_children.size() > 0 ) {
                beginRemoveRows( parent, 0, list->_children.size() - 1 );
                qDeleteAll( list->setChildren( QList<TreeItem*>() ) );
                endRemoveRows();
                cache()->clearAllMessages( mailbox->mailbox() );
            }
        } else {
            int pos = 0;
            for ( int i = 0; i < uidMap.size(); ++i ) {
                if ( i >= list->_children.size() ) {
                    beginInsertRows( parent, i, i );
                    TreeItemMessage * msg = new TreeItemMessage( list );
                    msg->_uid = uidMap[ i ];
                    list->_children << msg;
                    endInsertRows();
                } else if ( dynamic_cast<TreeItemMessage*>( list->_children[pos] )->_uid == uidMap[ i ] ) {
                    continue;
                } else {
                    int pos = i;
                    bool found = false;
                    while ( pos < list->_children.size() ) {
                        TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( list->_children[pos] );
                        if ( message->_uid != uidMap[ i ] ) {
                            cache()->clearMessage( mailbox->mailbox(), message->uid() );
                            beginRemoveRows( parent, pos, pos );
                            delete list->_children.takeAt( pos );
                            endRemoveRows();
                        } else {
                            found = true;
                            break;
                        }
                    }
                    if ( ! found ) {
                        Q_ASSERT( pos == list->_children.size() ); // we're at the end of the list
                        beginInsertRows( parent, i, i );
                        TreeItemMessage * msg = new TreeItemMessage( list );
                        msg->_uid = uidMap[ i ];
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

    if ( _netPolicy == NETWORK_OFFLINE )
        return;

    Parser* parser = _getParser( 0, ReadOnly );
    QList<Responses::NamespaceData>::const_iterator it;
    for ( it = resp->personal.begin(); it != resp->personal.end(); ++it )
        parser->list( QLatin1String(""),
                      ( it->prefix.isEmpty() ?
                            QLatin1String("") :
                            ( it->prefix + it->separator ) )
                      + QLatin1Char('%') );
    for ( it = resp->users.begin(); it != resp->users.end(); ++it )
        parser->list( QLatin1String(""),
                      ( it->prefix.isEmpty() ?
                            QLatin1String("") :
                            ( it->prefix + it->separator ) )
                      + QLatin1Char('%') );
    for ( it = resp->other.begin(); it != resp->other.end(); ++it )
        parser->list( QLatin1String(""),
                      ( it->prefix.isEmpty() ?
                            QLatin1String("") :
                            ( it->prefix + it->separator ) )
                      + QLatin1Char('%') );

    // FIXME: this will work only on servers that don't re-order commands
    CommandHandle cmd;
    cmd = parser->list( QLatin1String(""), QLatin1String("INBOX") );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::LIST, _mailboxes );
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
    QString mailbox = item->mailbox();

    if ( mailbox.isNull() )
        mailbox = "%";
    else
        mailbox = mailbox + item->separator() + QChar( '%' );

    if ( networkPolicy() != NETWORK_ONLINE && _cache->childMailboxesFresh( item->mailbox() ) ) {
        // We aren't online and the permanent cache contains relevant data
        QList<MailboxMetadata> metadata = _cache->childMailboxes( item->mailbox() );
        QList<TreeItem*> mailboxes;
        for ( QList<MailboxMetadata>::const_iterator it = metadata.begin(); it != metadata.end(); ++it ) {
            mailboxes << TreeItemMailbox::fromMetadata( item, *it );
        }
        TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item );
        Q_ASSERT( mailboxPtr );
        item->_fetchStatus = TreeItem::DONE;
        replaceChildMailboxes( item, mailboxes );
    } else if ( networkPolicy() == NETWORK_OFFLINE ) {
        // No cached data, no network -> fail
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        // We have to go to the network
        Parser* parser = _getParser( 0, ReadOnly );
        CommandHandle cmd = parser->list( "", mailbox );
        _parsers[ parser ].commandMap[ cmd ] = Task( Task::LIST, item );
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
            message->_uid = uidMapping[ seq ];
            item->_children << message;
        }
        endInsertRows();
        cacheOk = true;
        item->_fetchStatus = TreeItem::DONE; // required for FETCH processing later on
    }

    if ( networkPolicy() != NETWORK_OFFLINE ) {
        _getParser( mailboxPtr, ReadOnly );
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
            emitMessageCountChanged( mailboxPtr );
        } else {
            item->_numberFetchingStatus = TreeItem::UNAVAILABLE;
        }
    } else {
        Parser* parser = _getParser( 0, ReadOnly );
        CommandHandle cmd = parser->status( mailboxPtr->mailbox(),
                                            QStringList() << QLatin1String("MESSAGES") << QLatin1String("UNSEEN") );
        _parsers[ parser ].commandMap[ cmd ] = Task( Task::STATUS, item );
    }
}

void Model::_askForMsgMetadata( TreeItemMessage* item )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( item->parent() );
    Q_ASSERT( list );
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( list->parent() );
    Q_ASSERT( mailboxPtr );

    int order = item->row();

    if ( item->uid() ) {
        AbstractCache::MessageDataBundle data = cache()->messageMetadata( mailboxPtr->mailbox(), item->uid() );
        if ( data.uid == item->uid() ) {
            item->_envelope = data.envelope;
            item->_flags = data.flags;
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

    switch ( networkPolicy() ) {
        case NETWORK_OFFLINE:
            break;
        case NETWORK_EXPENSIVE:
            if ( ! item->fetched() ) {
                item->_fetchStatus = TreeItem::LOADING;
                Parser* parser = _getParser( mailboxPtr, ReadOnly );
                CommandHandle cmd = parser->fetch( Sequence( order + 1 ), _onlineMessageFetch );
                _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH, item );
            }
            break;
        case NETWORK_ONLINE:
            if ( ! item->fetched() ) {
                item->_fetchStatus = TreeItem::LOADING;
                // preload
                Sequence seq( order + 1 );
                for ( int i = qMax( 0, order - StructurePreload );
                      i < qMin( list->_children.size(), order + StructurePreload );
                      ++i ) {
                    TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( list->_children[i] );
                    Q_ASSERT( message );
                    if ( item != message && ! message->fetched() && ! message->loading() ) {
                        message->_fetchStatus = TreeItem::LOADING;
                        seq.add( message->row() + 1 );
                    }
                }
                Parser* parser = _getParser( mailboxPtr, ReadOnly );
                CommandHandle cmd = parser->fetch( seq, _onlineMessageFetch );
                _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH, item );
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
        Parser* parser = _getParser( mailboxPtr, ReadOnly );
        CommandHandle cmd = parser->fetch( Sequence( item->message()->row() + 1 ),
                QStringList() << QString::fromAscii("BODY.PEEK[%1]").arg(
                        item->mimeType() == QLatin1String("message/rfc822") ?
                            QString::fromAscii("%1.HEADER").arg( item->partId() ) :
                            item->partId()
                        ) );
        _parsers[ parser ].commandMap[ cmd ] = Task( Task::FETCH, item );
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

Parser* Model::_getParser( TreeItemMailbox* mailbox, const RWMode mode, const bool reSync ) const
{
    Q_ASSERT( _netPolicy != NETWORK_OFFLINE );

    if ( ! mailbox && ! _parsers.isEmpty() ) {
        return _parsers.begin().value().parser;
    } else {
        for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
            if ( it->mailbox == mailbox ) {
                if ( mode == ReadOnly || it->mode == mode ) {
                    if ( reSync ) {
                        CommandHandle cmd = ( it->mode == ReadWrite ) ?
                                            it->parser->select( mailbox->mailbox() ) :
                                            it->parser->examine( mailbox->mailbox() );
                        _parsers[ it->parser ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
                        ++_parsers[ it->parser ].selectingAnother;
                    }
                    return it->parser;
                } else {
                    it->mode = ReadWrite;
                    CommandHandle cmd = it->parser->select( mailbox->mailbox() );
                    _parsers[ it->parser ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
                    ++_parsers[ it->parser ].selectingAnother;
                    return it->parser;
                }
            }
        }
    }
    if ( _parsers.size() >= _maxParsers ) {
        ParserState& parser = _parsers.begin().value();
        parser.mode = mode;
        parser.mailbox = mailbox;
        CommandHandle cmd;
        if ( mode == ReadWrite )
            cmd = parser.parser->select( mailbox->mailbox() );
        else
            cmd = parser.parser->examine( mailbox->mailbox() );
        _parsers[ parser.parser ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
        ++_parsers[ parser.parser ].selectingAnother;
        return parser.parser;
    } else {
        // we can create one more
        // FIXME: socket error handling
        Parser* parser( new Parser( const_cast<Model*>( this ), _socketFactory->create() ) );
        _parsers[ parser ] = ParserState( parser, mailbox, mode, CONN_STATE_ESTABLISHED, unauthHandler );
        connect( parser, SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
        connect( parser, SIGNAL( disconnected( const QString ) ), this, SLOT( slotParserDisconnected( const QString ) ) );
        CommandHandle cmd;
        if ( _startTls ) {
            cmd = parser->startTls();
            _parsers[ parser ].commandMap[ cmd ] = Task( Task::STARTTLS, 0 );
        }
        if ( mailbox ) {
            if ( mode == ReadWrite )
                cmd = parser->select( mailbox->mailbox() );
            else
                cmd = parser->examine( mailbox->mailbox() );
            _parsers[ parser ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
            ++_parsers[ parser ].selectingAnother;
        }
        return parser;
    }
}

void Model::setNetworkPolicy( const NetworkPolicy policy )
{
    switch ( policy ) {
        case NETWORK_OFFLINE:
            noopTimer->stop();
            for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
                CommandHandle cmd = _parsers[ it.key() ].parser->logout();
                _parsers[ it.key() ].commandMap[ cmd ] = Task( Task::LOGOUT, 0 );
            }
            emit networkPolicyOffline();
            _netPolicy = NETWORK_OFFLINE;
            // FIXME: kill the connection
            break;
        case NETWORK_EXPENSIVE:
            _netPolicy = NETWORK_EXPENSIVE;
            noopTimer->stop();
            _getParser( 0, ReadOnly );
            emit networkPolicyExpensive();
            break;
        case NETWORK_ONLINE:
            _netPolicy = NETWORK_ONLINE;
            _getParser( 0, ReadOnly );
            noopTimer->start( PollingPeriod );
            emit networkPolicyOnline();
            break;
    }
}

void Model::slotParserDisconnected( const QString msg )
{
    Parser* which = qobject_cast<Parser*>( sender() );
    if ( ! which )
        return;
    for ( QMap<CommandHandle, Task>::const_iterator it = _parsers[ which ].commandMap.begin();
            it != _parsers[ which ].commandMap.end(); ++it ) {
        // FIXME: fail the command, perform cleanup,...
    }
    noopTimer->stop();
    emit connectionError( msg );
    disconnect( which, SIGNAL(responseReceived()), this, SLOT(responseReceived()) );
    disconnect( which, SIGNAL(disconnected(QString)), this, SLOT(slotParserDisconnected(QString)) );
    which->deleteLater();
    _parsers.remove( which );
}

void Model::idleTerminated()
{
    QMap<Parser*,ParserState>::iterator it = _parsers.find( qobject_cast<Imap::Parser*>( sender() ));
    if ( it == _parsers.end() ) {
        return;
    } else {
        Q_ASSERT( it->idleLauncher );
        it->idleLauncher->restart();
    }
}


void Model::completelyReset()
{
    // FIXME: some replies might be already flying on their way to the parser, so we might receive duplicate data...
    delete _mailboxes;
    for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
        it->commandMap.clear();
        it->listResponses.clear();
    }
    _mailboxes = new TreeItemMailbox( 0 );
    reset();
}

void Model::switchToMailbox( const QModelIndex& mbox )
{
    if ( ! mbox.isValid() )
        return;

    if ( _netPolicy == NETWORK_OFFLINE )
        return;

    if ( TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>(
                 realTreeItem( mbox ) ) ) {
        Parser* ptr = _getParser( mailbox, ReadWrite );
        if ( _parsers[ ptr ].capabilitiesFresh &&
             _parsers[ ptr ].capabilities.contains( QLatin1String( "IDLE" ) ) ) {
            if ( ! _parsers[ ptr ].idleLauncher ) {
                _parsers[ ptr ].idleLauncher = new IdleLauncher( this, ptr );
                connect( ptr, SIGNAL( idleTerminated() ), this, SLOT( idleTerminated() ) );
            }
            if ( ! _parsers[ ptr ].idleLauncher->idling() ) {
                _parsers[ ptr ].idleLauncher->restart();
            }
        }
    }
}

void Model::enterIdle( Parser* parser )
{
    noopTimer->stop();
    CommandHandle cmd = parser->idle();
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::NOOP, 0 );
}

void Model::updateCapabilities( Parser* parser, const QStringList capabilities )
{
    parser->enableLiteralPlus( capabilities.contains( QLatin1String( "LITERAL+" ) ) );
}

void Model::updateFlags( TreeItemMessage* message, const QString& flagOperation, const QString& flags )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Ignoring requests to modify message flags when OFFLINE";
        return;
    }
    Parser* parser = _getParser( dynamic_cast<TreeItemMailbox*>(
            static_cast<TreeItem*>( message->parent()->parent() ) ), ReadWrite );
    if ( message->_uid == 0 ) {
        qDebug() << "Error: attempted to work with message with UID 0";
        return;
    }
    CommandHandle cmd = parser->uidStore( Sequence( message->_uid ), flagOperation, flags );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::STORE, message );
}

void Model::markMessageDeleted( TreeItemMessage* msg, bool marked )
{
    updateFlags( msg, marked ? QLatin1String("+FLAGS") : QLatin1String("-FLAGS"),
                 QLatin1String("(\\Deleted)") );
}

void Model::markMessageRead( TreeItemMessage* msg, bool marked )
{
    updateFlags( msg, marked ? QLatin1String("+FLAGS") : QLatin1String("-FLAGS"),
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
}

void Model::markUidsDeleted( TreeItemMailbox* mbox, const Sequence& messages )
{
    Q_ASSERT( mbox );
    Parser* parser = _getParser( mbox, ReadWrite );
    CommandHandle cmd = parser->uidStore( messages, QLatin1String("+FLAGS"), QLatin1String("\\Deleted") );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::STORE, mbox );
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

void Model::expungeMailbox( TreeItemMailbox* mbox )
{
    if ( ! mbox )
        return;

    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Can't expunge while offline";
        return;
    }

    Parser* parser = _getParser( mbox, ReadWrite );
    CommandHandle cmd = parser->expunge(); // BIG FAT WARNING: what happens if the SELECT fails???
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::EXPUNGE, mbox );
}

void Model::createMailbox( const QString& name )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Can't create mailboxes while offline";
        return;
    }

    Parser* parser = _getParser( 0, ReadOnly );
    CommandHandle cmd = parser->create( name );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::CREATE, 0 );
    // FIXME: issue a LIST as well?
}

void Model::deleteMailbox( const QString& name )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Can't delete mailboxes while offline";
        return;
    }

    Parser* parser = _getParser( 0, ReadOnly );
    CommandHandle cmd = parser->deleteMailbox( name );
    _parsers[ parser ].commandMap[ cmd ] = Task( Task::DELETE, 0 );
    // FIXME: issue a LIST as well?
}

void Model::saveUidMap( TreeItemMsgList* list )
{
    QList<uint> seqToUid;
    for ( int i = 0; i < list->_children.size(); ++i )
        seqToUid << static_cast<TreeItemMessage*>( list->_children[ i ] )->uid();
    _cache->setUidMapping( static_cast<TreeItemMailbox*>( list->parent() )->mailbox(), seqToUid );
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

}
}

#include "Model.moc"
