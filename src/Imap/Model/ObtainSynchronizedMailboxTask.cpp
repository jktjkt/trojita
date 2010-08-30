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
#include "CreateConnectionTask.h"
#include "MailboxTree.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

ObtainSynchronizedMailboxTask::ObtainSynchronizedMailboxTask( Model* _model, const QModelIndex& _mailboxIndex ) :
    ImapTask( _model ), parser(0), createConn(0), mailboxIndex(_mailboxIndex)
{
    // FIXME: find out if the mailbox is already selected
    bool alreadySynced = false;
    if ( alreadySynced ) {
        QTimer::singleShot( 0, this, SLOT(slotPerform()) );
        //parser = something;
        // FIXME: somehow pass the Parser*
    } else {
        createConn = new CreateConnectionTask( _model, 0 );
        createConn->addDependentTask( this );
    }
}

void ObtainSynchronizedMailboxTask::slotPerform()
{
    perform();
}

void ObtainSynchronizedMailboxTask::perform()
{
    if ( ! mailboxIndex.isValid() ) {
        // FIXME: proper error handling
        qDebug() << "The mailbox went missing, sorry";
        _completed();
        return;
    }

    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ));
    Q_ASSERT(mailbox);

    if ( ! parser ) {
        Q_ASSERT(createConn);
        parser = createConn->parser;
    }

    QMap<Parser*,Model::ParserState>::iterator it = model->_parsers.find( parser );

    selectCmd = parser->select( mailbox->mailbox() );
    it->commandMap[ selectCmd ] = Model::Task( Model::Task::SELECT, 0 );
    it->mailbox = mailbox;
    ++it->selectingAnother;
    it->currentMbox = mailbox;
}

bool ObtainSynchronizedMailboxTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( resp->tag == selectCmd ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( selectCmd, Model::Task::SELECT );

        if ( resp->kind == Responses::OK ) {
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

        } else {
            // FIXME: Tasks API error handling
        }
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else {
        return false;
    }
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

        // FIXME: re-enable optimization when Task migration's done
        QStringList items = false && willLoad ? model->_onlineMessageFetch : QStringList() << "UID" << "FLAGS";
        uidSyncingCmd = parser->fetch( Sequence( 1, syncState.exists() ), items );
        model->_parsers[ parser ].commandMap[ uidSyncingCmd ] = Model::Task( Model::Task::FETCH_WITH_FLAGS, mailbox );
        emit model->activityHappening( true );
        list->_numberFetchingStatus = TreeItem::LOADING;
        list->_unreadMessageCount = 0;
    } else {
        list->_totalMessageCount = 0;
        list->_unreadMessageCount = 0;
        list->_numberFetchingStatus = TreeItem::DONE;
        list->_fetchStatus = TreeItem::DONE;
        model->cache()->setMailboxSyncState( mailbox->mailbox(), syncState );
        model->saveUidMap( list );
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
        QStringList items = QStringList( "FLAGS" );
        if ( ! uidsOk )
            items << "UID";
        CommandHandle cmd = parser->fetch( Sequence( 1, syncState.exists() ),
                                           items );
        model->_parsers[ parser ].commandMap[ cmd ] = Model::Task( Model::Task::FETCH_WITH_FLAGS, mailbox );
        emit model->activityHappening( true );
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
    model->cache()->setMailboxSyncState( mailbox->mailbox(), syncState );
    model->saveUidMap( list );
}

void ObtainSynchronizedMailboxTask::_syncOnlyDeletions( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState )
{
    CommandHandle cmd = parser->fetch( Sequence( 1, syncState.exists() ),
                                       QStringList() << "UID" << "FLAGS" );
    model->_parsers[ parser ].commandMap[ cmd ] = Model::Task( Model::Task::FETCH_WITH_FLAGS, mailbox );
    emit model->activityHappening( true );
    list->_numberFetchingStatus = TreeItem::LOADING;
    list->_unreadMessageCount = 0;
    // selecting handler should do the rest
    model->_parsers[ parser ].responseHandler = model->selectingHandler;
    QList<uint>& uidMap = model->_parsers[ parser ].uidMap;
    uidMap.clear();
    model->_parsers[ parser ].syncingFlags.clear();
    for ( uint i = 0; i < syncState.exists(); ++i )
        uidMap << 0;
    model->cache()->clearUidMapping( mailbox->mailbox() );
}

void ObtainSynchronizedMailboxTask::_syncOnlyAdditions( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState, const SyncState& oldState )
{
    for ( uint i = 0; i < syncState.uidNext() - oldState.uidNext(); ++i ) {
        TreeItemMessage* msg = new TreeItemMessage( list );
        msg->_offset = i + oldState.exists();
        list->_children << msg;
    }

    // FIXME: re-enable the optimization when Task migration is done
    QStringList items = ( false && model->networkPolicy() == Model::NETWORK_ONLINE &&
                          syncState.uidNext() - oldState.uidNext() <= model->StructureFetchLimit ) ?
                        model->_onlineMessageFetch : QStringList() << "UID" << "FLAGS";
    CommandHandle cmd = parser->fetch( Sequence( oldState.exists() + 1, syncState.exists() ),
                                       items );
    model->_parsers[ parser ].commandMap[ cmd ] = Model::Task( Model::Task::FETCH_WITH_FLAGS, mailbox );
    emit model->activityHappening( true );
    list->_numberFetchingStatus = TreeItem::LOADING;
    list->_fetchStatus = TreeItem::DONE;
    list->_unreadMessageCount = 0;
    model->cache()->clearUidMapping( mailbox->mailbox() );
}

void ObtainSynchronizedMailboxTask::_syncGeneric( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState )
{
    // FIXME: might be possible to optimize here...

    // At first, let's ask for UID numbers and FLAGS for all messages
    CommandHandle cmd = parser->fetch( Sequence( 1, syncState.exists() ),
                                       QStringList() << "UID" << "FLAGS" );
    model->_parsers[ parser ].commandMap[ cmd ] = Model::Task( Model::Task::FETCH_WITH_FLAGS, mailbox );
    emit model->activityHappening( true );
    model->_parsers[ parser ].responseHandler = model->selectingHandler;
    list->_numberFetchingStatus = TreeItem::LOADING;
    list->_unreadMessageCount = 0;
    QList<uint>& uidMap = model->_parsers[ parser ].uidMap;
    uidMap.clear();
    model->_parsers[ parser ].syncingFlags.clear();
    for ( uint i = 0; i < syncState.exists(); ++i )
        uidMap << 0;
    model->cache()->clearUidMapping( mailbox->mailbox() );
}



}
}
