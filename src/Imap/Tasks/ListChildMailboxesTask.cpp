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


#include "ListChildMailboxesTask.h"
#include "CreateConnectionTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {


ListChildMailboxesTask::ListChildMailboxesTask( Model* _model, const QModelIndex& mailbox ):
    ImapTask( _model ), mailboxIndex(mailbox)
{
    TreeItemMailbox* mailboxPtr = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailbox.internalPointer() ) );
    Q_ASSERT( mailboxPtr );
    conn = new CreateConnectionTask( _model, 0 );
    conn->addDependentTask( this );
}

void ListChildMailboxesTask::perform()
{
    if ( ! mailboxIndex.isValid() ) {
        // FIXME: add proper fix
        qDebug() << "Mailbox vanished before we could ask for its children";
        _completed();
        return;
    }
    TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ) );
    Q_ASSERT( mailbox );
    parser = conn->parser;
    model->_parsers[ parser ].activeTasks.append( this );

    QString mailboxName = mailbox->mailbox();
    if ( mailboxName.isNull() )
        mailboxName = QString::fromAscii("%");
    else
        mailboxName += mailbox->separator() + QChar( '%' );
    tag = parser->list( "", mailboxName );
    model->_parsers[ parser ].commandMap[ tag ] = Model::Task( Model::Task::LIST, 0 );
    emit model->activityHappening( true );
}

bool ListChildMailboxesTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( resp->tag == tag ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( tag, Model::Task::LIST );

        if ( mailboxIndex.isValid() ) {
            TreeItemMailbox* mailbox = dynamic_cast<TreeItemMailbox*>( static_cast<TreeItem*>( mailboxIndex.internalPointer() ) );
            Q_ASSERT( mailbox );

            if ( resp->kind == Responses::OK ) {
                model->_finalizeList( ptr, mailbox );
            } else {
                // FIXME: error handling
            }
        } else {
            // FIXME: error handling
            qDebug() << Q_FUNC_INFO << "Mailbox no longer available -- weird timing?";
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
