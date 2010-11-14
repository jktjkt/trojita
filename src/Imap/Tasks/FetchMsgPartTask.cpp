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


#include "FetchMsgPartTask.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {

FetchMsgPartTask::FetchMsgPartTask( Model* _model,
                                    TreeItemMailbox* mailbox, TreeItemPart* part ) :
    ImapTask( _model )
{
    QModelIndex mailboxIndex = model->createIndex( mailbox->row(), 0, mailbox );
    conn = model->findTaskResponsibleFor( mailboxIndex );
    conn->addDependentTask( this );
    if ( TreeItemModifiedPart* modifiedPart = dynamic_cast<TreeItemModifiedPart*>( part ) ) {
        // A special case, we're dealing with irregular layout
        index = model->createIndex( part->row(), static_cast<int>( modifiedPart->kind() ), part );
    } else {
        // Normal parts without fancy modifiers
        index = model->createIndex( part->row(), 0, part );
    }
    Q_ASSERT( index.isValid() );
}

void FetchMsgPartTask::perform()
{
    parser = conn->parser;
    Q_ASSERT( parser );
    model->_parsers[ parser ].activeTasks.append( this );

    if ( ! index.isValid() ) {
        // FIXME: add proper fix
        qDebug() << "Message got removed before we could have fetched it";
        _completed();
        return;
    }

    TreeItemPart* part = dynamic_cast<TreeItemPart*>( static_cast<TreeItem*>( index.internalPointer() ) );
    Q_ASSERT( part );
    tag = parser->fetch( Sequence( part->message()->row() + 1 ),
                         QStringList() << QString::fromAscii("BODY.PEEK[%1]").arg(
                                 part->mimeType() == QLatin1String("message/rfc822") ?
                                 QString::fromAscii("%1.HEADER").arg( part->partId() ) :
                                 part->partId()
                                 ) );
    model->_parsers[ parser ].commandMap[ tag ] = Model::Task( Model::Task::FETCH_PART, 0 );
    emit model->activityHappening( true );
}

bool FetchMsgPartTask::handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp )
{
    model->_genericHandleFetch( ptr, resp );
    return true;
}

bool FetchMsgPartTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( resp->tag.isEmpty() )
        return false;

    if ( resp->tag == tag ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( tag, Model::Task::FETCH_PART );

        if ( index.isValid() ) {
            TreeItemPart* part = dynamic_cast<TreeItemPart*>( static_cast<TreeItem*>( index.internalPointer() ) );
            Q_ASSERT( part );

            if ( resp->kind == Responses::OK ) {
                model->_finalizeFetchPart( ptr, part );
            } else {
                // FIXME: error handling
            }
        } else {
            // FIXME: error handling
            qDebug() << Q_FUNC_INFO << "Message part no longer available -- weird timing?";
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
