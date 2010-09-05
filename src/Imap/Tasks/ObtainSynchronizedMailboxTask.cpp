/* Copyright (C) 2007 - 2010 Jan Kundrát <jkt@flaska.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "ObtainSynchronizedMailboxTask.h"
#include <QTimer>
#include "GetAnyConnectionTask.h"
#include "OpenConnectionTask.h"
#include "MailboxTree.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

ObtainSynchronizedMailboxTask::ObtainSynchronizedMailboxTask( Model* _model, const QModelIndex& _mailboxIndex, bool forceResync ) :
    ImapTask( _model ), conn(0), mailboxIndex(_mailboxIndex), status(STATE_WAIT_FOR_CONN)
{
    // FIXME: find out if the mailbox is already selected; this crude hack is just a temporary placeholder
    // FIXME: this *WILL* break once we use more than one mailbox
    bool alreadySynced = ! model->_parsers.isEmpty();
    if ( alreadySynced ) {
        conn = model->_taskFactory->createGetAnyConnectionTask( model );
        conn->addDependentTask( this );
        // FIXME: here should go the code for honoring the forceResync flag
    } else {
        conn = model->_taskFactory->createOpenConnectionTask( model );
        conn->addDependentTask( this );
    }
}

void ObtainSynchronizedMailboxTask::perform()
{
    parser = conn->parser;
    Q_ASSERT( parser );
    model->_parsers[ parser ].activeTasks.append( this );

    if ( ! mailboxIndex.isValid() ) {
        // FIXME: proper error handling
        qDebug() << "The mailbox went missing, sorry";
        _completed();
        return;
    }

    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ));
    Q_ASSERT(mailbox);
    TreeItemMsgList* msgList = dynamic_cast<TreeItemMsgList*>( mailbox->_children[0] );
    Q_ASSERT( msgList );

    msgList->_fetchStatus = TreeItem::LOADING;

    QMap<Parser*,Model::ParserState>::iterator it = model->_parsers.find( parser );

    selectCmd = parser->select( mailbox->mailbox() );
    it->commandMap[ selectCmd ] = Model::Task( Model::Task::SELECT, 0 );
    it->mailbox = mailbox;
    ++it->selectingAnother;
    it->currentMbox = mailbox;
    model->_parsers[ parser ].currentMbox->syncState = SyncState();
    status = STATE_SELECTING;
    qDebug() << "STATE_SELECTING";
}

bool ObtainSynchronizedMailboxTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( handleResponseCodeInsideState( resp ) )
        return true;

    if ( resp->tag.isEmpty() )
        return false;

    if ( resp->tag == selectCmd ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( selectCmd, Model::Task::SELECT );

        if ( resp->kind == Responses::OK ) {
            qDebug() << "received OK for selectCmd";
            Q_ASSERT( status == STATE_SELECTING );
            _finalizeSelect();
        } else {
            // FIXME: Tasks API error handling
        }
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else if ( resp->tag == uidSyncingCmd ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( uidSyncingCmd, Model::Task::SEARCH_UIDS );

        if ( resp->kind == Responses::OK ) {
            qDebug() << "received OK for uidSyncingCmd";
            Q_ASSERT( status == STATE_SYNCING_UIDS );
            Q_ASSERT( mailboxIndex.isValid() ); // FIXME
            TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ));
            Q_ASSERT( mailbox );
            syncFlags( mailbox );
        } else {
            // FIXME: error handling
        }
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else if ( resp->tag == flagsCmd ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( flagsCmd, Model::Task::FETCH_FLAGS );

        if ( resp->kind == Responses::OK ) {
            qDebug() << "received OK for flagsCmd";
            Q_ASSERT( status == STATE_SYNCING_FLAGS );
            Q_ASSERT( mailboxIndex.isValid() ); // FIXME
            TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ));
            Q_ASSERT( mailbox );
        } else {
            // FIXME: error handling
        }
        _completed();
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else {
        return false;
    }
}

void ObtainSynchronizedMailboxTask::_finalizeSelect()
{
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ));
    Q_ASSERT(mailbox);
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[ 0 ] );
    Q_ASSERT( list );

    model->_parsers[ parser ].currentMbox = mailbox;
    model->_parsers[ parser ].responseHandler = model->selectedHandler;
    model->changeConnectionState( parser, CONN_STATE_SELECTED );
    const SyncState& syncState = model->_parsers[ parser ].currentMbox->syncState;
    const SyncState& oldState = model->cache()->mailboxSyncState( mailbox->mailbox() );
    list->_totalMessageCount = syncState.exists();
    // Note: syncState.unSeen() is the NUMBER of the first unseen message, not their count!

    const QList<uint>& seqToUid = model->cache()->uidMapping( mailbox->mailbox() );

    if ( static_cast<uint>( seqToUid.size() ) != oldState.exists() ||
         oldState.exists() != static_cast<uint>( list->_children.size() ) ) {

        qDebug() << "Inconsistent cache data, falling back to full sync (" <<
                seqToUid.size() << "in UID map," << oldState.exists() <<
                "EXIST before," << list->_children.size() << "nodes)";
        _fullMboxSync( mailbox, list, syncState );
    } else {
        if ( syncState.isUsableForSyncing() && oldState.isUsableForSyncing() && syncState.uidValidity() == oldState.uidValidity() ) {
            // Perform a nice re-sync

            if ( syncState.uidNext() == oldState.uidNext() ) {
                // No new messages

                if ( syncState.exists() == oldState.exists() ) {
                    // No deletions, either, so we resync only flag changes
                    _syncNoNewNoDeletions( mailbox, list, syncState, seqToUid );
                } else {
                    // Some messages got deleted, but there have been no additions
                    _syncOnlyDeletions( mailbox, list, syncState );
                }

            } else {
                // Some new messages were delivered since we checked the last time.
                // There's no guarantee they are still present, though.

                if ( syncState.uidNext() - oldState.uidNext() == syncState.exists() - oldState.exists() ) {
                    // Only some new arrivals, no deletions
                    _syncOnlyAdditions( mailbox, list, syncState, oldState );
                } else {
                    // Generic case; we don't know anything about which messages were deleted and which added
                    _syncGeneric( mailbox, list, syncState );
                }
            }
        } else {
            // Forget everything, do a dumb sync
            model->cache()->clearAllMessages( mailbox->mailbox() );
            _fullMboxSync( mailbox, list, syncState );
        }
    }
    model->cache()->setMailboxSyncState( mailbox->mailbox(), syncState );
}

void ObtainSynchronizedMailboxTask::_fullMboxSync( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState )
{
    model->cache()->clearUidMapping( mailbox->mailbox() );

    QModelIndex parent = model->createIndex( 0, 0, list );
    if ( ! list->_children.isEmpty() ) {
        model->beginRemoveRows( parent, 0, list->_children.size() - 1 );
        qDeleteAll( list->_children );
        list->_children.clear();
        model->endRemoveRows();
    }
    if ( syncState.exists() ) {
        bool willLoad = model->networkPolicy() == Model::NETWORK_ONLINE && syncState.exists() <= Model::StructureFetchLimit;
        model->beginInsertRows( parent, 0, syncState.exists() - 1 );
        for ( uint i = 0; i < syncState.exists(); ++i ) {
            TreeItemMessage* message = new TreeItemMessage( list );
            message->_offset = i;
            list->_children << message;
            // FIXME: re-evaluate this one when Task migration's done
            //if ( willLoad )
            //    message->_fetchStatus = TreeItem::LOADING;
        }
        model->endInsertRows();
        list->_fetchStatus = TreeItem::DONE;

        Q_ASSERT( ! model->_parsers[ parser ].selectingAnother );

        syncUids( mailbox );

        // FIXME: re-enable optimization (merge UID and FLAGS syncing) when Task migration's done
        list->_numberFetchingStatus = TreeItem::LOADING;
        list->_unreadMessageCount = 0;
    } else {
        list->_totalMessageCount = 0;
        list->_unreadMessageCount = 0;
        list->_numberFetchingStatus = TreeItem::DONE;
        list->_fetchStatus = TreeItem::DONE;
        model->cache()->setMailboxSyncState( mailbox->mailbox(), syncState );
        model->saveUidMap( list );

        // The remote mailbox is empty -> we're done now
        status = STATE_DONE;
        qDebug() << "STATE_DONE";
        _completed();
    }
    model->emitMessageCountChanged( mailbox );
}

void ObtainSynchronizedMailboxTask::_syncNoNewNoDeletions( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState, const QList<uint>& seqToUid )
{
    if ( syncState.exists() ) {
        // Verify that we indeed have all UIDs and not need them anymore
        bool uidsOk = true;
        for ( int i = 0; i < list->_children.size(); ++i ) {
            if ( ! static_cast<TreeItemMessage*>( list->_children[i] )->uid() ) {
                uidsOk = false;
                break;
            }
        }
        Q_ASSERT( uidsOk );
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
    model->cache()->setMailboxSyncState( mailbox->mailbox(), syncState );
    model->saveUidMap( list );

    if ( syncState.exists() ) {
        syncFlags( mailbox );
    } else {
        status = STATE_DONE;
        qDebug() << "STATE_DONE";
        _completed();
    }
}

void ObtainSynchronizedMailboxTask::_syncOnlyDeletions( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState )
{
    list->_numberFetchingStatus = TreeItem::LOADING;
    list->_unreadMessageCount = 0;
    QList<uint>& uidMap = model->_parsers[ parser ].uidMap;
    uidMap.clear();
    model->_parsers[ parser ].syncingFlags.clear();
    for ( uint i = 0; i < syncState.exists(); ++i )
        uidMap << 0;
    model->cache()->clearUidMapping( mailbox->mailbox() );
    syncUids( mailbox );
}

void ObtainSynchronizedMailboxTask::_syncOnlyAdditions( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState, const SyncState& oldState )
{
    for ( uint i = 0; i < syncState.uidNext() - oldState.uidNext(); ++i ) {
        TreeItemMessage* msg = new TreeItemMessage( list );
        msg->_offset = i + oldState.exists();
        list->_children << msg;
    }

    /*// FIXME: re-enable the optimization when Task migration is done
    QStringList items = ( false && model->networkPolicy() == Model::NETWORK_ONLINE &&
                          syncState.uidNext() - oldState.uidNext() <= model->StructureFetchLimit ) ?
                        model->_onlineMessageFetch : QStringList() << "UID" << "FLAGS";
    CommandHandle cmd = parser->fetch( Sequence( oldState.exists() + 1, syncState.exists() ),
                                       items );
    model->_parsers[ parser ].commandMap[ cmd ] = Model::Task( Model::Task::FETCH_WITH_FLAGS, mailbox );
    emit model->activityHappening( true );*/
    list->_numberFetchingStatus = TreeItem::LOADING;
    list->_fetchStatus = TreeItem::DONE;
    list->_unreadMessageCount = 0;
    model->cache()->clearUidMapping( mailbox->mailbox() );
    syncUids( mailbox );
}

void ObtainSynchronizedMailboxTask::_syncGeneric( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState )
{
    // FIXME: might be possible to optimize here...

    /*// At first, let's ask for UID numbers and FLAGS for all messages
    CommandHandle cmd = parser->fetch( Sequence( 1, syncState.exists() ),
                                       QStringList() << "UID" << "FLAGS" );
    model->_parsers[ parser ].commandMap[ cmd ] = Model::Task( Model::Task::FETCH_WITH_FLAGS, mailbox );
    emit model->activityHappening( true );
    model->_parsers[ parser ].responseHandler = model->selectingHandler;*/
    list->_numberFetchingStatus = TreeItem::LOADING;
    list->_unreadMessageCount = 0;
    QList<uint>& uidMap = model->_parsers[ parser ].uidMap;
    uidMap.clear();
    model->_parsers[ parser ].syncingFlags.clear();
    for ( uint i = 0; i < syncState.exists(); ++i )
        uidMap << 0;
    model->cache()->clearUidMapping( mailbox->mailbox() );
    syncUids( mailbox );
}

void ObtainSynchronizedMailboxTask::syncUids( TreeItemMailbox* mailbox, const uint lastKnownUid )
{
    QStringList criteria;
    if ( lastKnownUid == 0 ) {
        criteria << QLatin1String("ALL");
    } else {
        criteria << QString::fromAscii("UID %1:*").arg( lastKnownUid );
    }
    uidSyncingCmd = parser->uidSearch( criteria );
    model->_parsers[ parser ].commandMap[ uidSyncingCmd ] = Model::Task( Model::Task::SEARCH_UIDS, 0 );
    emit model->activityHappening( true );
    model->cache()->clearUidMapping( mailbox->mailbox() );
    status = STATE_SYNCING_UIDS;
    qDebug() << "STATE_SYNCING_UIDS";
}

void ObtainSynchronizedMailboxTask::syncFlags( TreeItemMailbox *mailbox )
{
    TreeItemMsgList* list = dynamic_cast<TreeItemMsgList*>( mailbox->_children[ 0 ] );
    Q_ASSERT( list );

    flagsCmd = parser->fetch( Sequence( 1, mailbox->syncState.exists() ), QStringList() << QLatin1String("FLAGS") );
    model->_parsers[ parser ].commandMap[ flagsCmd ] = Model::Task( Model::Task::FETCH_FLAGS, 0 );
    emit model->activityHappening( true );
    list->_numberFetchingStatus = TreeItem::LOADING;
    list->_unreadMessageCount = 0;
    status = STATE_SYNCING_FLAGS;
    qDebug() << "STATE_SYNCING_FLAGS";
}

bool ObtainSynchronizedMailboxTask::handleResponseCodeInsideState( const Imap::Responses::State* const resp )
{
    bool res = false;
    switch ( resp->respCode ) {
        case Responses::UNSEEN:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num ) {
                model->_parsers[ parser ].currentMbox->syncState.setUnSeen( num->data );
                res = true;
            } else {
                throw CantHappen( "State response has invalid UNSEEN respCodeData", *resp );
            }
            break;
        }
        case Responses::PERMANENTFLAGS:
        {
            const Responses::RespData<QStringList>* const num = dynamic_cast<const Responses::RespData<QStringList>* const>( resp->respCodeData.data() );
            if ( num ) {
                model->_parsers[ parser ].currentMbox->syncState.setPermanentFlags( num->data );
                res = true;
            } else {
                throw CantHappen( "State response has invalid PERMANENTFLAGS respCodeData", *resp );
            }
            break;
        }
        case Responses::UIDNEXT:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num ) {
                model->_parsers[ parser ].currentMbox->syncState.setUidNext( num->data );
                res = true;
            } else {
                throw CantHappen( "State response has invalid UIDNEXT respCodeData", *resp );
            }
            break;
        }
        case Responses::UIDVALIDITY:
        {
            const Responses::RespData<uint>* const num = dynamic_cast<const Responses::RespData<uint>* const>( resp->respCodeData.data() );
            if ( num ) {
                model->_parsers[ parser ].currentMbox->syncState.setUidValidity( num->data );
                res = true;
            } else {
                throw CantHappen( "State response has invalid UIDVALIDITY respCodeData", *resp );
            }
            break;
        }
            break;
        default:
            break;
    }
    return res;
}

bool ObtainSynchronizedMailboxTask::handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp )
{
    Q_ASSERT( ptr == parser );
    switch ( resp->kind ) {
        case Imap::Responses::EXISTS:
            model->_parsers[ ptr ].currentMbox->syncState.setExists( resp->number );
            return true;
            break;
        case Imap::Responses::EXPUNGE:
            // must be handled elsewhere
            break;
        case Imap::Responses::RECENT:
            model->_parsers[ ptr ].currentMbox->syncState.setRecent( resp->number );
            return true;
            break;
        default:
            throw CantHappen( "Got a NumberResponse of invalid kind. This is supposed to be handled in its constructor!", *resp );
    }
    return false;
}

bool ObtainSynchronizedMailboxTask::handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp )
{
    Q_ASSERT( ptr == parser );
    model->_parsers[ ptr ].currentMbox->syncState.setFlags( resp->flags );
    return true;
}


}
}
