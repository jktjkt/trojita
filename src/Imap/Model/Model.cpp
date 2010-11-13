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
#include "ModelUpdaters.h"
#include "GetAnyConnectionTask.h"
#include "KeepMailboxOpenTask.h"
#include <QAbstractProxyModel>
#include <QAuthenticator>
#include <QCoreApplication>
#include <QDebug>
#include <QtAlgorithms>

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

bool uidComparator( const TreeItem* const a, const TreeItem* const b )
{
    const TreeItemMessage* const messageA = dynamic_cast<const TreeItemMessage* const>( a );
    const TreeItemMessage* const messageB = dynamic_cast<const TreeItemMessage* const>( b );
    return messageA->uid() < messageB->uid();
}

}

Model::Model( QObject* parent, AbstractCache* cache, SocketFactoryPtr socketFactory, TaskFactoryPtr taskFactory, bool offline ):
    // parent
    QAbstractItemModel( parent ),
    // our tools
    _cache(cache), _socketFactory(socketFactory), _taskFactory(taskFactory),
    _maxParsers(4), _mailboxes(0), _netPolicy( NETWORK_ONLINE ),
    _authenticator(0), lastParserId(0)
{
    _cache->setParent(this);
    _startTls = _socketFactory->startTlsRequired();

    _mailboxes = new TreeItemMailbox( 0 );

    _onlineMessageFetch << "ENVELOPE" << "BODYSTRUCTURE" << "RFC822.SIZE" << "UID" << "FLAGS";

    if ( offline ) {
        _netPolicy = NETWORK_OFFLINE;
        QTimer::singleShot( 0, this, SLOT(setNetworkOffline()) );
    } else {
        QTimer::singleShot( 0, this, SLOT( setNetworkOnline() ) );
    }
}

Model::~Model()
{
    delete _mailboxes;
    delete _authenticator;
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

            // Try various tasks, perhaps it's their response. Also check if they're already finished and remove them.
            for ( QList<ImapTask*>::iterator taskIt = taskSnapshot.begin(); taskIt != taskSnapshot.end(); ++taskIt ) {
                if ( ! handled )
                    handled = resp->plug( it->parser, *taskIt );

                if ( (*taskIt)->isFinished() )
                    deletedTasks << *taskIt;
            }

            removeDeletedTasks( deletedTasks, it->activeTasks );

            runReadyTasks();

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
            case Task::LOGIN:
            case Task::NONE:
            case Task::LIST:
            case Task::LIST_AFTER_CREATE:
            case Task::STATUS:
            case Task::SELECT:
            case Task::FETCH_WITH_FLAGS:
            case Task::FETCH_PART:
            case Task::NOOP:
            case Task::IDLE:
            case Task::CAPABILITY:
            case Task::STORE:
            case Task::NAMESPACE:
            case Task::EXPUNGE:
            case Task::COPY:
            case Task::CREATE:
            case Task::DELETE:
                throw CantHappen( "[port-in-progress]: should be handled elsewhere", *resp );
                break;
            case Task::FETCH_MESSAGE_METADATA:
                // Either we were fetching just UID & FLAGS, or that and stuff like BODYSTRUCTURE.
                // In any case, we don't have to do anything here, besides updating message status
                // FIXME: this should probably go when the Task migration's done, as Tasks themselves could be made responsible for state updates
                changeConnectionState( ptr, CONN_STATE_SELECTED );
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
        // FIXME: we should probably just eat them and don't bother, as untagged OK/NO could be rather common...
        switch ( resp->kind ) {
        case BYE:
            killParser( ptr );
            parsersMightBeIdling();
            break;
        default:
            throw UnexpectedResponseReceived( "[Port-in-progress] Unhhandled untagged response, sorry", *resp );
        }
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

void Model::_finalizeIncrementalList( Parser* parser, const QString& parentMailboxName )
{
    TreeItemMailbox* parentMbox = findParentMailboxByName( parentMailboxName );
    if ( ! parentMbox ) {
        qDebug() << "Weird, no idea where to put the newly created mailbox" << parentMailboxName;
        return;
    }

    QList<TreeItem*> mailboxes;

    QList<Responses::List>& listResponses = _parsers[ parser ].listResponses;
    for ( QList<Responses::List>::iterator it = listResponses.begin();
            it != listResponses.end(); /* nothing */ ) {
        if ( it->mailbox == parentMailboxName ) {
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

void Model::emitMessageCountChanged( TreeItemMailbox* const mailbox )
{
    TreeItemMsgList* list = static_cast<TreeItemMsgList*>( mailbox->_children[ 0 ] );
    QModelIndex msgListIndex = createIndex( list->row(), 0, list );
    emit dataChanged( msgListIndex, msgListIndex );
    emit messageCountPossiblyChanged( createIndex( mailbox->row(), 0, mailbox ) );
}

/** @short Retrieval of a message part has completed */
void Model::_finalizeFetchPart( Parser* parser, TreeItemPart* const part )
{
    if ( part->loading() ) {
        // basically, there's nothing to do if the FETCH targetted a message part and not the message as a whole
        qDebug() << "Imap::Model::_finalizeFetch(): didn't receive anything about message" <<
            part->message()->row() << "part" << part->partId() << "in mailbox" <<
            _parsers[ parser ].mailbox->mailbox();
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

    if ( mailbox /* FIXME: Tasks API port && _parsers[ parser ].responseHandler == selectedHandler */ ) {
        // the mailbox was already synced (?)
        mailbox->_children[0]->_fetchStatus = TreeItem::DONE;
        cache()->setMailboxSyncState( mailbox->mailbox(), _parsers[ parser ].mailbox->syncState );
        TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[0] );
        Q_ASSERT( list );
        saveUidMap( list );
        if ( command->kind == Task::FETCH_WITH_FLAGS ) {
            list->recalcUnreadMessageCount();
            list->_numberFetchingStatus = TreeItem::DONE;
            changeConnectionState( parser, CONN_STATE_SELECTED );
        }
        emitMessageCountChanged( mailbox );
    }
}

void Model::handleCapability( Imap::Parser* ptr, const Imap::Responses::Capability* const resp )
{
    updateCapabilities( ptr, resp->capabilities );
}

void Model::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    throw UnexpectedResponseReceived( "[Tasks API Port] Unhandled NumberResponse", *resp );
}

void Model::handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp )
{
    _parsers[ ptr ].listResponses << *resp;
}

void Model::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    throw UnexpectedResponseReceived( "[Tasks API Port] Unhandled Flags", *resp );
}

void Model::handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp )
{
    throw UnexpectedResponseReceived( "[Tasks API Port] Unhandled Search", *resp );
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
    throw UnexpectedResponseReceived( "[Tasks API Port] Unhandled Fetch", *resp );
}

void Model::handleNamespace( Imap::Parser* ptr, const Imap::Responses::Namespace* const resp )
{
    return; // because it's broken and won't fly
    Q_UNUSED(ptr); Q_UNUSED(resp);
}

void Model::handleSort(Imap::Parser *ptr, const Imap::Responses::Sort *const resp)
{
    throw UnexpectedResponseReceived( "[Tasks API Port] Unhandled Sort", *resp );
}

void Model::handleThread(Imap::Parser *ptr, const Imap::Responses::Thread *const resp)
{
    throw UnexpectedResponseReceived( "[Tasks API Port] Unhandled Thread", *resp );
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

    // Deal with the possibility of an "irregular shape" of our model here.
    // The issue is that some items have child items not only in column #0
    // and in specified number of rows, but also in row #0 and various columns.
    if ( column != 0 ) {
        TreeItem* item = parentItem->specialColumnPtr( row, column );
        if ( item )
            return QAbstractItemModel::createIndex( row, column, item );
        else
            return QModelIndex();
    }

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
    TreeItem* node = static_cast<TreeItem*>( index.internalPointer() );
    if ( !node ) {
        node = _mailboxes;
    }
    Q_ASSERT(node);
    return node->columnCount();
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
        _taskFactory->createListChildMailboxesTask( this, createIndex( item->row(), 0, item) );
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
        findTaskResponsibleFor( createIndex( mailboxPtr->row(), 0, mailboxPtr ) );
        // and that's all -- the task will detect following replies and sync automatically
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
        _taskFactory->createNumberOfMessagesTask( this, createIndex( mailboxPtr->row(), 0, mailboxPtr ) );
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
            item->_fetchStatus = TreeItem::LOADING;
            _taskFactory->createFetchMsgMetadataTask( this, QModelIndexList() << createIndex( item->row(), 0, item ) );
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
                _taskFactory->createFetchMsgMetadataTask( this, items );
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
        _taskFactory->createFetchMsgPartTask( this, mailboxPtr, item );
    }
}

void Model::resyncMailbox( const QModelIndex& mbox )
{
    findTaskResponsibleFor( mbox )->resynchronizeMailbox();
}

void Model::setNetworkPolicy( const NetworkPolicy policy )
{
    // If we're connecting after being offline, we should ask for an updated list of mailboxes
    // The main reason is that this happens after entering wrong password and going back online
    bool shouldReloadMailboxes = _netPolicy == NETWORK_OFFLINE && policy != NETWORK_OFFLINE;
    switch ( policy ) {
        case NETWORK_OFFLINE:
            for ( QMap<Parser*,ParserState>::iterator it = _parsers.begin(); it != _parsers.end(); ++it ) {
                Q_ASSERT( it->parser );
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
            emit networkPolicyExpensive();
            break;
        case NETWORK_ONLINE:
            _netPolicy = NETWORK_ONLINE;
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

void Model::switchToMailbox( const QModelIndex& mbox )
{
    if ( ! mbox.isValid() )
        return;

    if ( _netPolicy == NETWORK_OFFLINE )
        return;

    findTaskResponsibleFor( mbox );
}

void Model::updateCapabilities( Parser* parser, const QStringList capabilities )
{
    QStringList uppercaseCaps;
    Q_FOREACH( const QString& str, capabilities )
            uppercaseCaps << str.toUpper();
    _parsers[ parser ].capabilities = uppercaseCaps;
    _parsers[ parser ].capabilitiesFresh = true;
    parser->enableLiteralPlus( uppercaseCaps.contains( QLatin1String( "LITERAL+" ) ) );
}

void Model::markMessageDeleted( TreeItemMessage* msg, bool marked )
{
    _taskFactory->createUpdateFlagsTask( this, QModelIndexList() << createIndex( msg->row(), 0, msg ),
                         marked ? QLatin1String("+FLAGS") : QLatin1String("-FLAGS"),
                         QLatin1String("(\\Deleted)") );
}

void Model::markMessageRead( TreeItemMessage* msg, bool marked )
{
    _taskFactory->createUpdateFlagsTask( this, QModelIndexList() << createIndex( msg->row(), 0, msg ),
                         marked ? QLatin1String("+FLAGS") : QLatin1String("-FLAGS"),
                         QLatin1String("(\\Seen)") );
}

void Model::copyMoveMessages( TreeItemMailbox* sourceMbox, const QString& destMailboxName, QList<uint> uids, const CopyMoveOperation op )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        // FIXME: error signalling
        return;
    }

    Q_ASSERT( sourceMbox );
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( sourceMbox->child( 0, 0 ) );
    Q_ASSERT( list );

    qSort( uids );

    QModelIndexList messages;
    Sequence seq;
    QList<TreeItem*>::const_iterator it = list->_children.begin();
    // qBinaryFind is not designed to operate on a value of a different kind than stored in the container
    // so we can't really compare TreeItem* with uint, even though our LessThan supports that
    // (it keeps calling it both via LowerThan(*it, value) and LowerThan(value, *it).
    TreeItemMessage fakeMessage(0);
    uint lastUid = 0;
    Q_FOREACH( const uint& uid, uids ) {
        if ( lastUid == uid ) {
            // we have to filter out duplicates
            continue;
        }
        lastUid = uid;
        fakeMessage._uid = uid;
        it = qBinaryFind( it, list->_children.constEnd(), &fakeMessage, uidComparator );
        if ( it != list->_children.end() ) {
            messages << createIndex( (*it)->row(), 0, *it );
            seq.add( uid );
        } else {
            qDebug() << "Can't find UID" << uid << "when copying messages";
        }
    }

    _taskFactory->createCopyMoveMessagesTask( this, messages, destMailboxName, op );
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

    _taskFactory->createExpungeMailboxTask( this, createIndex( mbox->row(), 0, mbox ) );
}

void Model::createMailbox( const QString& name )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Can't create mailboxes while offline";
        return;
    }

    _taskFactory->createCreateMailboxTask( this, name );
}

void Model::deleteMailbox( const QString& name )
{
    if ( _netPolicy == NETWORK_OFFLINE ) {
        qDebug() << "Can't delete mailboxes while offline";
        return;
    }

    _taskFactory->createDeleteMailboxTask( this, name );
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
    while ( const QAbstractProxyModel* proxy = qobject_cast<const QAbstractProxyModel*>( index.model() ) ) {
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

CommandHandle Model::performAuthentication( Imap::Parser* ptr )
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
        return CommandHandle();
    } else {
        CommandHandle cmd = ptr->login( _authenticator->user(), _authenticator->password() );
        _parsers[ ptr ].commandMap[ cmd ] = Task( Task::LOGIN, 0 );
        emit activityHappening( true );
        return cmd;
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
    Q_FOREACH( ImapTask* task, _parsers[ parser ].activeTasks ) {
        task->die();
        task->deleteLater();
    }

    for ( QMap<CommandHandle, Task>::const_iterator it = _parsers[ parser ].commandMap.begin();
            it != _parsers[ parser ].commandMap.end(); ++it ) {
        // FIXME: fail the command, perform cleanup,...
    }
    _parsers[ parser ].commandMap.clear();
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

void Model::runReadyTasks()
{
    for ( QMap<Parser*,ParserState>::iterator parserIt = _parsers.begin(); parserIt != _parsers.end(); ++parserIt ) {
        bool runSomething = false;
        do {
            runSomething = false;
            // See responseReceived() for more details about why we do need to iterate over a copy here.
            // Basically, calls to ImapTask::perform could invalidate our precious iterators.
            QList<ImapTask*> origList = parserIt->activeTasks;
            QList<ImapTask*> deletedList;
            for ( QList<ImapTask*>::iterator taskIt = origList.begin(); taskIt != origList.end(); ++taskIt ) {
                if ( (*taskIt)->isReadyToRun() ) {
                    (*taskIt)->perform();
                    runSomething = true;
                }
                if ( (*taskIt)->isFinished() ) {
                    deletedList << *taskIt;
                }
            }
            removeDeletedTasks( deletedList, parserIt->activeTasks );
        } while ( runSomething );
    }
}

void Model::removeDeletedTasks( const QList<ImapTask*>& deletedTasks, QList<ImapTask*>& activeTasks )
{
    // Remove the finished commands
    for ( QList<ImapTask*>::const_iterator deletedIt = deletedTasks.begin(); deletedIt != deletedTasks.end(); ++deletedIt ) {
        (*deletedIt)->deleteLater();
        activeTasks.removeOne( *deletedIt );
    }
}

KeepMailboxOpenTask* Model::findTaskResponsibleFor( const QModelIndex& mailbox )
{
    Q_ASSERT( mailbox.isValid() );
    QModelIndex translatedIndex;
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( realTreeItem( mailbox, 0, &translatedIndex ) );
    Q_ASSERT( mailboxPtr );

    bool canCreateConn = _parsers.isEmpty(); // FIXME: multiple connections

    if ( mailboxPtr->maintainingTask ) {
        // The requested mailbox already has the maintaining task associated
        return mailboxPtr->maintainingTask;
    } else if ( canCreateConn ) {
        // The mailbox is not being maintained, but we can create a new connection
        return _taskFactory->createKeepMailboxOpenTask( this, translatedIndex, 0 );
    } else {
        // Too bad, we have to re-use an existing parser. That will probably lead to
        // stealing it from some mailbox, but there's no other way.
        Q_ASSERT( ! _parsers.isEmpty() );
        return _taskFactory->createKeepMailboxOpenTask( this, translatedIndex, _parsers.begin().key() );
    }
}
void Model::_genericHandleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    TreeItemMailbox* mailbox = _parsers[ ptr ].mailbox;
    if ( ! mailbox )
        throw UnexpectedResponseReceived( "Received FETCH reply, but AFAIK we haven't selected any mailbox yet", *resp );

    TreeItemPart* changedPart = 0;
    TreeItemMessage* changedMessage = 0;
    mailbox->handleFetchResponse( this, *resp, &changedPart, &changedMessage );
    if ( changedPart ) {
        QModelIndex index = createIndex( changedPart->row(), 0, changedPart );
        emit dataChanged( index, index );
    }
    if ( changedMessage ) {
        QModelIndex index = createIndex( changedMessage->row(), 0, changedMessage );
        emit dataChanged( index, index );
    }
}

QModelIndex Model::findMailboxForItems( const QModelIndexList& items )
{
    TreeItemMailbox* mailbox = 0;
    Q_FOREACH( const QModelIndex& index, items ) {
        TreeItemMailbox* currentMailbox = 0;

        TreeItem* item = static_cast<TreeItem*>( index.internalPointer() );
        Q_ASSERT(item);

        TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( item );
        if ( ! message ) {
            if ( TreeItemPart* part = dynamic_cast<TreeItemPart*>( item ) ) {
                message = part->message();
            } else {
                throw CantHappen( "findMailboxForItems() called on strange items");
            }
        }
        Q_ASSERT(message);
        TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( message->parent() );
        Q_ASSERT(list);
        currentMailbox = dynamic_cast<TreeItemMailbox*>( list->parent() );
        Q_ASSERT(currentMailbox);
        if ( ! mailbox ) {
            mailbox = currentMailbox;
        } else if ( mailbox != currentMailbox ) {
            throw CantHappen( "Messages from several mailboxes");
        }
    }
    return createIndex( mailbox->row(), 0, mailbox );
}

}
}
