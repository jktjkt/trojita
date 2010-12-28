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


#include "ThreadTask.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap {
namespace Mailbox {


ThreadTask::ThreadTask( Model* _model, const QModelIndex& mailbox, const QString &_algorithm, const QStringList &_searchCriteria ):
    ImapTask( _model ), mailboxIndex(mailbox), algorithm(_algorithm), searchCriteria(_searchCriteria)
{
    conn = model->findTaskResponsibleFor( mailbox );
    conn->addDependentTask( this );
}

void ThreadTask::perform()
{
    parser = conn->parser;
    Q_ASSERT( parser );
    model->accessParser( parser ).activeTasks.append( this );

    if ( ! mailboxIndex.isValid() ) {
        // FIXME: add proper fix
        qDebug() << "Mailbox vanished before we could ask for threading info";
        _completed();
        return;
    }

    tag = parser->uidThread( algorithm, QLatin1String("utf-8"), searchCriteria );
    model->accessParser( parser ).commandMap[ tag ] = Model::Task( Model::Task::THREAD, 0 );
    emit model->activityHappening( true );
}

bool ThreadTask::handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp )
{
    if ( resp->tag.isEmpty() )
        return false;

    if ( resp->tag == tag ) {
        IMAP_TASK_ENSURE_VALID_COMMAND( tag, Model::Task::THREAD );
        // FIXME: we should probably care about how the command ended here...
        _completed();
        emit model->threadingAvailable( mailboxIndex, algorithm, searchCriteria, mapping );
        mapping.clear();
        IMAP_TASK_CLEANUP_COMMAND;
        return true;
    } else {
        return false;
    }
}

bool ThreadTask::handleThread( Imap::Parser *ptr, const Imap::Responses::Thread *const resp )
{
    Q_UNUSED(ptr);

    QList<Imap::Responses::Thread::Node> queue = resp->rootItems;
    while ( ! queue.isEmpty() ) {
        Imap::Responses::Thread::Node node = queue.takeFirst();
        queue.append( node.children );
        QList<uint> numbers;
        Q_FOREACH( const Imap::Responses::Thread::Node &child, node.children ) {
            numbers.append( child.num );
        }
        mapping[ node.num ] = numbers;
    }
    qDebug() << mapping;
    return true;
}

}
}
