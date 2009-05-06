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
#include <QAuthenticator>
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

IdleLauncher::IdleLauncher( Model* model, ParserPtr ptr ):
        m(model), parser(ptr), _idling(false)
{
    timer = new QTimer( this );
    timer->setSingleShot( true );
    timer->setInterval( 1000 );
    connect( timer, SIGNAL(timeout()), this, SLOT(perform()) );
}

void IdleLauncher::perform()
{
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


Model::Model( QObject* parent, CachePtr cache, SocketFactoryPtr socketFactory ):
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

    // FIXME: socket error handling
    ParserPtr parser( new Imap::Parser( this, _socketFactory->create() ) );
    _parsers[ parser.get() ] = ParserState( parser, 0, ReadOnly, CONN_STATE_ESTABLISHED, unauthHandler );
    connect( parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
    connect( parser.get(), SIGNAL( disconnected( const QString ) ), this, SLOT( slotParserDisconnected( const QString ) ) );
    if ( _startTls ) {
        CommandHandle cmd = parser->startTls();
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::STARTTLS, 0 );
    }
    _mailboxes = new TreeItemMailbox( 0 );
    QTimer::singleShot( 0, this, SLOT( setNetworkOnline() ) );

    _onlineMessageFetch << "ENVELOPE" << "BODYSTRUCTURE" << "RFC822.SIZE" << "UID" << "FLAGS";

    noopTimer = new QTimer( this );
    connect( noopTimer, SIGNAL(timeout()), this, SLOT(performNoop()) );
    noopTimer->start( PollingPeriod ); // FIXME: polling is ugly, even if done just once a minute
}

Model::~Model()
{
    for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
        disconnect( it.key(), SIGNAL( disconnected( const QString ) ), this, SLOT( slotParserDisconnected( const QString ) ) );
        disconnect( it.key(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
    }
    delete _mailboxes;
}

void Model::responseReceived()
{
    QMap<Parser*,ParserState>::iterator it = _parsers.find( qobject_cast<Imap::Parser*>( sender() ));
    Q_ASSERT( it != _parsers.end() );

    while ( it.value().parser->hasResponse() ) {
        std::tr1::shared_ptr<Imap::Responses::AbstractResponse> resp = it.value().parser->getResponse();
        Q_ASSERT( resp );
        resp->plug( it.value().parser, this );
    }
}

void Model::handleState( Imap::ParserPtr ptr, const Imap::Responses::State* const resp )
{
    // OK/NO/BAD/PREAUTH/BYE
    using namespace Imap::Responses;

    const QString& tag = resp->tag;

    if ( ! tag.isEmpty() ) {
        QMap<CommandHandle, Task>::iterator command = _parsers[ ptr.get() ].commandMap.find( tag );
        if ( command == _parsers[ ptr.get() ].commandMap.end() ) {
            qDebug() << "This command is not valid anymore" << tag;
            return;
        }

        // FIXME: distinguish among OK/NO/BAD here
        switch ( command->kind ) {
            case Task::STARTTLS:
                _parsers[ ptr.get() ].capabilitiesFresh = false;
                if ( resp->kind == Responses::OK ) {
                    QAuthenticator auth;
                    emit authRequested( &auth );
                    if ( auth.isNull() ) {
                        emit connectionError( tr("Can't login without user/password data") );
                    } else {
                        CommandHandle cmd = ptr->login( auth.user(), auth.password() );
                        _parsers[ ptr.get() ].commandMap[ cmd ] = Task( Task::LOGIN, 0 );
                    }
                } else {
                    emit connectionError( tr("Can't establish a secure connection to the server (STARTTLS failed). Refusing to proceed.") );
                }
                break;
            case Task::LOGIN:
                if ( resp->kind == Responses::OK ) {
                    _parsers[ ptr.get() ].connState = CONN_STATE_AUTH;
                    _parsers[ ptr.get() ].responseHandler = authenticatedHandler;
                    if ( ! _parsers[ ptr.get() ].capabilitiesFresh ) {
                        CommandHandle cmd = ptr->capability();
                        _parsers[ ptr.get() ].commandMap[ cmd ] = Task( Task::CAPABILITY, 0 );
                    }
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
                _finalizeStatus( ptr, command );
                break;
            case Task::SELECT:
                --_parsers[ ptr.get() ].selectingAnother;
                if ( resp->kind == Responses::OK ) {
                    _finalizeSelect( ptr, command );
                } else {
                    if ( _parsers[ ptr.get() ].connState == CONN_STATE_SELECTED )
                        _parsers[ ptr.get() ].connState = CONN_STATE_AUTH;
                    // FIXME: error handling
                }
                break;
            case Task::FETCH:
                _finalizeFetch( ptr, command );
                break;
            case Task::NOOP:
            case Task::CAPABILITY:
                // We don't have to do anything here
                break;
            case Task::STORE:
                // FIXME: check for errors
                break;
        }

        _parsers[ ptr.get() ].commandMap.erase( command );

    } else {
        // untagged response
        if ( _parsers[ ptr.get() ].responseHandler )
            _parsers[ ptr.get() ].responseHandler->handleState( ptr, resp );
    }
}

void Model::_finalizeList( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( command->what );
    QList<TreeItem*> mailboxes;

    QList<Responses::List>& listResponses = _parsers[ parser.get() ].listResponses;
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

    replaceChildMailboxes( parser, mailboxPtr, mailboxes );
}

void Model::replaceChildMailboxes( ParserPtr parser, TreeItemMailbox* mailboxPtr, const QList<TreeItem*> mailboxes )
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
}

void Model::_finalizeStatus( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    throw CantHappen( "Got unexpected STATUS response -- we don't issue any STATUS commands anymore!" );
}

void Model::_finalizeSelect( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( command->what );
    Q_ASSERT( mailbox );
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[ 0 ] );
    Q_ASSERT( list );
    _parsers[ parser.get() ].currentMbox = mailbox;
    _parsers[ parser.get() ].responseHandler = selectedHandler;
    list->_fetchStatus = TreeItem::DONE;

    const SyncState& syncState = _parsers[ parser.get() ].syncState;
    const SyncState& oldState = _cache->mailboxSyncState( mailbox->mailbox() );
    if ( syncState.isUsableForSyncing() && oldState.isUsableForSyncing() && syncState.uidValidity() == oldState.uidValidity() ) {
        // Perform a nice re-sync

        if ( syncState.uidNext() == oldState.uidNext() ) {
            // No new messages

            if ( syncState.exists() == oldState.exists() ) {
                // No deletions, either, so we resync only flag changes

                if ( syncState.exists() ) {
                    CommandHandle cmd = parser->fetch( Sequence::startingAt( 1 ),
                                                       QStringList( "FLAGS" ) );
                    _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, mailbox );
                }

                if ( list->_children.isEmpty() ) {
                    QList<TreeItem*> messages;
                    for ( uint i = 0; i < syncState.exists(); ++i )
                        messages << new TreeItemMessage( list );
                    list->setChildren( messages );

                } else {
                    if ( syncState.exists() != static_cast<uint>( list->_children.size() ) ) {
                        throw CantHappen( "TreeItemMsgList has wrong number of "
                                          "children, even though no change of "
                                          "message count occured" );
                    }
                }
                emit messageCountPossiblyChanged( createIndex( 0, 0, mailbox ) );

            } else {
                // Some messages got deleted, but there have been no additions

                CommandHandle cmd = parser->fetch( Sequence::startingAt( 1 ),
                                                   QStringList() << "UID" << "FLAGS" );
                _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, mailbox );
                // selecting handler should do the rest
                _parsers[ parser.get() ].responseHandler = selectingHandler;
                QList<uint>& uidMap = _parsers[ parser.get() ].uidMap;
                uidMap.clear();
                _parsers[ parser.get() ].syncingFlags.clear();
                for ( uint i = 0; i < syncState.exists(); ++i )
                    uidMap << 0;
            }

        } else {
            // Some new messages were delivered since we checked the last time.
            // There's no guarantee they are still present, though.

            if ( syncState.uidNext() - oldState.uidNext() == syncState.exists() - oldState.exists() ) {
                // Only some new arrivals, no deletions

                for ( uint i = 0; i < syncState.uidNext() - oldState.uidNext(); ++i ) {
                    list->_children << new TreeItemMessage( list );
                }

                QStringList items = ( networkPolicy() == NETWORK_ONLINE ) ?
                                    _onlineMessageFetch : QStringList() << "UID" << "FLAGS";
                CommandHandle cmd = parser->fetch( Sequence::startingAt( oldState.exists() + 1 ),
                                                   items );
                _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, mailbox );

            } else {
                // Generic case; we don't know anything about which messages were deleted and which added
                // FIXME: might be possible to optimize here...

                // At first, let's ask for UID numbers and FLAGS for all messages
                CommandHandle cmd = parser->fetch( Sequence::startingAt( 1 ),
                                                   QStringList() << "UID" << "FLAGS" );
                _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, mailbox );
                _parsers[ parser.get() ].responseHandler = selectingHandler;
                QList<uint>& uidMap = _parsers[ parser.get() ].uidMap;
                uidMap.clear();
                _parsers[ parser.get() ].syncingFlags.clear();
                for ( uint i = 0; i < syncState.exists(); ++i )
                    uidMap << 0;
            }
        }
    } else {
        // Forget everything, do a dumb sync
        // FIXME: wipe cache

        QModelIndex parent = createIndex( 0, 0, list );
        if ( ! list->_children.isEmpty() ) {
            beginRemoveRows( parent, 0, list->_children.size() - 1 );
            qDeleteAll( list->_children );
            list->_children.clear();
            endRemoveRows();
            emit messageCountPossiblyChanged( parent.parent() );
        }
        if ( syncState.exists() ) {
            beginInsertRows( parent, 0, syncState.exists() );
            for ( uint i = 0; i < syncState.exists(); ++i ) {
                list->_children << new TreeItemMessage( list );
            }
            endInsertRows();

            QStringList items = ( networkPolicy() == NETWORK_ONLINE ) ?
                                _onlineMessageFetch : QStringList() << "UID" << "FLAGS";
            CommandHandle cmd = parser->fetch( Sequence::startingAt( 1 ), items );
            _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, mailbox );
            emit messageCountPossiblyChanged( parent.parent() );
        }
    }
    _cache->setMailboxSyncState( mailbox->mailbox(), syncState ); // FIXME: only after everything's been done?
}

void Model::_finalizeFetch( ParserPtr parser, const QMap<CommandHandle, Task>::const_iterator command )
{
    TreeItemPart* part = dynamic_cast<TreeItemPart*>( command.value().what );
    if ( part && part->loading() ) {
        qDebug() << "Imap::Model::_finalizeFetch(): didn't receive anything about message" <<
            part->message()->row() << "part" << part->partId() << "in mailbox" <<
            _parsers[ parser.get() ].mailbox->mailbox();
        part->_fetchStatus = TreeItem::DONE;
    }
    if ( dynamic_cast<TreeItemMailbox*>( command.value().what ) &&
            _parsers[ parser.get() ].responseHandler == selectingHandler ) {
        _parsers[ parser.get() ].responseHandler = selectedHandler;

        QList<uint>& uidMap = _parsers[ parser.get() ].uidMap;
        TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( command.value().what->_children[0] );
        Q_ASSERT( list );

        QModelIndex parent = createIndex( 0, 0, list );
        if ( uidMap.isEmpty() ) {
            beginRemoveRows( parent, 0, list->_children.size() - 1 );
            qDeleteAll( list->setChildren( QList<TreeItem*>() ) );
            endRemoveRows();
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
                        if ( dynamic_cast<TreeItemMessage*>( list->_children[pos] )->_uid != uidMap[ i ] ) {
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
                for ( int i = uidMap.size(); i < list->_children.size(); ++i )
                    delete list->_children.takeAt( i );
                endRemoveRows();
            }
        }

        uidMap.clear();

        for ( QList<TreeItem*>::const_iterator it = list->_children.begin();
              it != list->_children.end(); ++it ) {
            TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( *it );
            Q_ASSERT( message );
            if ( message->_uid == 0 ) {
                qDebug() << "Message with unknown UID";
            } else {
                message->_flags = _parsers[ parser.get() ].syncingFlags[ message->_uid ];
                QModelIndex index = createIndex( message->row(), 0, message );
                emit dataChanged( index, index );
            }
        }
        _parsers[ parser.get() ].syncingFlags.clear();

        emit messageCountPossiblyChanged( parent.parent() );
    }
}

void Model::handleCapability( Imap::ParserPtr ptr, const Imap::Responses::Capability* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleCapability( ptr, resp );
}

void Model::handleNumberResponse( Imap::ParserPtr ptr, const Imap::Responses::NumberResponse* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleNumberResponse( ptr, resp );
}

void Model::handleList( Imap::ParserPtr ptr, const Imap::Responses::List* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleList( ptr, resp );
}

void Model::handleFlags( Imap::ParserPtr ptr, const Imap::Responses::Flags* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleFlags( ptr, resp );
}

void Model::handleSearch( Imap::ParserPtr ptr, const Imap::Responses::Search* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleSearch( ptr, resp );
}

void Model::handleStatus( Imap::ParserPtr ptr, const Imap::Responses::Status* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleStatus( ptr, resp );
}

void Model::handleFetch( Imap::ParserPtr ptr, const Imap::Responses::Fetch* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleFetch( ptr, resp );
}

void Model::handleNamespace( Imap::ParserPtr ptr, const Imap::Responses::Namespace* const resp )
{
    if ( _parsers[ ptr.get() ].responseHandler )
        _parsers[ ptr.get() ].responseHandler->handleNamespace( ptr, resp );
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
        ParserPtr parser = _getParser( 0, ReadOnly );
        TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item );
        Q_ASSERT( mailboxPtr );
        item->_fetchStatus = TreeItem::DONE;
        replaceChildMailboxes( parser, item, mailboxes );
    } else if ( networkPolicy() == NETWORK_OFFLINE ) {
        // No cached data, no network -> fail
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        // We have to go to the network
        ParserPtr parser = _getParser( 0, ReadOnly );
        CommandHandle cmd = parser->list( "", mailbox );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::LIST, item );
    }
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

    if ( networkPolicy() == NETWORK_OFFLINE ) {
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        _getParser( mailboxPtr, ReadOnly );
        // and that's all -- we will detect following replies and sync automatically
    }
}

void Model::_askForMsgMetadata( TreeItemMessage* item )
{
    Q_ASSERT( item->parent() ); // TreeItemMsgList
    Q_ASSERT( item->parent()->parent() ); // TreeItemMailbox
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->parent()->parent() );
    Q_ASSERT( mailboxPtr );

    int order = item->row();

    if ( networkPolicy() == NETWORK_OFFLINE ) {
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        ParserPtr parser = _getParser( mailboxPtr, ReadOnly );
        CommandHandle cmd = parser->fetch( Sequence( order + 1 ), _onlineMessageFetch );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, item );
    }
}

void Model::_askForMsgPart( TreeItemPart* item )
{
    // FIXME: fetch parts in chunks, not at once
    Q_ASSERT( item->message() ); // TreeItemMessage
    Q_ASSERT( item->message()->parent() ); // TreeItemMsgList
    Q_ASSERT( item->message()->parent()->parent() ); // TreeItemMailbox
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( item->message()->parent()->parent() );
    Q_ASSERT( mailboxPtr );

    if ( networkPolicy() == NETWORK_OFFLINE ) {
        item->_fetchStatus = TreeItem::UNAVAILABLE;
    } else {
        ParserPtr parser = _getParser( mailboxPtr, ReadOnly );
        CommandHandle cmd = parser->fetch( Sequence( item->message()->row() + 1 ),
                QStringList() << QString::fromAscii("BODY.PEEK[%1]").arg( item->partId() ) );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::FETCH, item );
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

ParserPtr Model::_getParser( TreeItemMailbox* mailbox, const RWMode mode, const bool reSync ) const
{
    if ( ! mailbox ) {
        return _parsers.begin().value().parser;
    } else {
        for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
            if ( it->mailbox == mailbox ) {
                if ( mode == ReadOnly || it->mode == mode ) {
                    if ( reSync ) {
                        CommandHandle cmd = ( it->mode == ReadWrite ) ?
                                            it->parser->select( mailbox->mailbox() ) :
                                            it->parser->examine( mailbox->mailbox() );
                        _parsers[ it->parser.get() ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
                        ++_parsers[ it->parser.get() ].selectingAnother;
                    }
                    return it->parser;
                } else {
                    it->mode = ReadWrite;
                    CommandHandle cmd = it->parser->select( mailbox->mailbox() );
                    _parsers[ it->parser.get() ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
                    ++_parsers[ it->parser.get() ].selectingAnother;
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
        _parsers[ parser.parser.get() ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
        ++_parsers[ parser.parser.get() ].selectingAnother;
        return parser.parser;
    } else {
        // we can create one more
        // FIXME: socket error handling
        ParserPtr parser( new Parser( const_cast<Model*>( this ), _socketFactory->create() ) );
        _parsers[ parser.get() ] = ParserState( parser, mailbox, mode, CONN_STATE_ESTABLISHED, unauthHandler );
        connect( parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
        connect( parser.get(), SIGNAL( disconnected() ), this, SLOT( slotParserDisconnected() ) );
        CommandHandle cmd;
        if ( _startTls ) {
            cmd = parser->startTls();
            _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::STARTTLS, 0 );
        }
        if ( mode == ReadWrite )
            cmd = parser->select( mailbox->mailbox() );
        else
            cmd = parser->examine( mailbox->mailbox() );
        _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::SELECT, mailbox );
        ++_parsers[ parser.get() ].selectingAnother;
        return parser;
    }
}

void Model::setNetworkPolicy( const NetworkPolicy policy )
{
    switch ( policy ) {
        case NETWORK_OFFLINE:
            noopTimer->stop();
            emit networkPolicyOffline();
            break;
        case NETWORK_EXPENSIVE:
            noopTimer->stop();
            emit networkPolicyExpensive();
            break;
        case NETWORK_ONLINE:
            noopTimer->start( PollingPeriod );
            emit networkPolicyOnline();
            break;
    }
    _netPolicy = policy;
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
}

void Model::idleTerminated()
{
    QMap<Parser*,ParserState>::iterator it = _parsers.find( qobject_cast<Imap::Parser*>( sender() ));
    if ( it == _parsers.end() )
        return;
    else {
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
        it->statusResponses.clear();
    }
    _mailboxes = new TreeItemMailbox( 0 );
    reset();
}

void Model::switchToMailbox( const QModelIndex& mbox )
{
    if ( ! mbox.isValid() )
        return;

    if ( TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>(
                 static_cast<TreeItem*>( mbox.internalPointer() ) ) ) {
        ParserPtr ptr = _getParser( mailbox, ReadOnly );
        if ( _parsers[ ptr.get() ].capabilitiesFresh &&
             _parsers[ ptr.get() ].capabilities.contains( QLatin1String( "IDLE" ) ) ) {
            if ( ! _parsers[ ptr.get() ].idleLauncher ) {
                _parsers[ ptr.get() ].idleLauncher = new IdleLauncher( this, ptr );
                connect( ptr.get(), SIGNAL( idleTerminated() ), this, SLOT( idleTerminated() ) );
            }
            if ( ! _parsers[ ptr.get() ].idleLauncher->idling() ) {
                _parsers[ ptr.get() ].idleLauncher->restart();
            }
        }
    }
}

void Model::enterIdle( ParserPtr parser )
{
    noopTimer->stop();
    CommandHandle cmd = parser->idle();
    _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::NOOP, 0 );
}

void Model::updateCapabilities( ParserPtr parser, const QStringList capabilities )
{
    parser->enableLiteralPlus( capabilities.contains( QLatin1String( "LITERAL+" ) ) );
}

void Model::updateFlags( TreeItemMessage* message, const QString& flagOperation, const QString& flags )
{
    ParserPtr parser = _getParser( dynamic_cast<TreeItemMailbox*>(
            static_cast<TreeItem*>( message->parent()->parent() ) ), ReadWrite );
    if ( message->_uid == 0 ) {
        qDebug() << "Error: attempted to work with message with UID 0";
        return;
    }
    CommandHandle cmd = parser->uidStore( Sequence( message->_uid ), flagOperation, flags );
    _parsers[ parser.get() ].commandMap[ cmd ] = Task( Task::STORE, message );
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

}
}

#include "Model.moc"
