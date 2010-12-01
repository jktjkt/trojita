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


#include "FetchMsgMetadataTask.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {

FetchMsgMetadataTask::FetchMsgMetadataTask( Model* _model, const QModelIndexList& _messages ) :
    ImapTask( _model )
{
    if ( _messages.isEmpty() ) {
        throw CantHappen( "FetchMsgMetadataTask called with empty message set");
    }
    Q_FOREACH( const QModelIndex& index, _messages ) {
        messages << index;
    }
    QModelIndex mailboxIndex = model->findMailboxForItems( _messages );
    conn = model->findTaskResponsibleFor( mailboxIndex );
    conn->addDependentTask( this );
}

void FetchMsgMetadataTask::perform()
{
    parser = conn->parser;
    Q_ASSERT( parser );
    model->_parsers[ parser ].activeTasks.append( this );

    Sequence seq;
    bool first = true;

    Q_FOREACH( const QPersistentModelIndex& index, messages ) {
        if ( ! index.isValid() ) {
            // FIXME: add proper fix
            qDebug() << "Some message got removed before we could ask for their metadata";
        } else {
            TreeItem* item = static_cast<TreeItem*>( index.internalPointer() );
            Q_ASSERT(item);
            TreeItemMessage* message = dynamic_cast<TreeItemMessage*>( item );
            Q_ASSERT(message);
            if ( first ) {
                seq = Sequence( message->uid() );
                first = false;
            } else {
                seq.add( message->uid() );
            }
        }
    }

    if ( first ) {
        // No valid messages
        qDebug() << "All messages got removed before we could ask for their metadata";
        _completed();
        return;
    }

    // we do not want to use _onlineMessageFetch because it contains UID and FLAGS
    tag = parser->uidFetch( seq, QStringList() << QLatin1String("ENVELOPE") << QLatin1String("BODYSTRUCTURE") << QLatin1String("RFC822.SIZE") );
    model->_parsers[ parser ].commandMap[ tag ] = Model::Task( Model::Task::FETCH_MESSAGE_METADATA, 0 );
    emit model->activityHappening( true );
}

bool FetchMsgMetadataTask::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    Q_ASSERT( ! messages.isEmpty() );
    TreeItemMailbox* mailbox = Model::mailboxForSomeItem( messages.first() );
    model->_genericHandleFetch( mailbox, resp );
    return true;
}

bool FetchMsgMetadataTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( resp->tag.isEmpty() )
        return false;

    if ( resp->tag == tag ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( tag, Model::Task::FETCH_MESSAGE_METADATA );

        if ( resp->kind == Responses::OK ) {
            // nothing should be needed here
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

}
}
