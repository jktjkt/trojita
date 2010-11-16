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

#include "GetAnyConnectionTask.h"
#include <QTimer>
#include "KeepMailboxOpenTask.h"
#include "MailboxTree.h"
#include "OpenConnectionTask.h"

namespace Imap {
namespace Mailbox {

GetAnyConnectionTask::GetAnyConnectionTask( Model* _model ) :
    ImapTask( _model ), newConn(0)
{
    if ( model->_parsers.isEmpty() ) {
        // We're creating a completely new connection
        newConn = model->_taskFactory->createOpenConnectionTask( model );
        newConn->addDependentTask( this );
    } else {
        // There's an existing parser, so let's reuse it
        QMap<Parser*,Model::ParserState>::iterator it = model->_parsers.begin();
        parser = it.key();
        Q_ASSERT( parser );

        if ( it->mailbox && it->mailbox->maintainingTask ) {
            // It has already some mailbox and KeepMailboxOpenTask associated, so we have
            // to be nice and inform the maintainingTask about our presence, otherwise we'd
            // risk breaking an existing IDLE, which is fatal.
            qDebug() << this << "Registering as dependent stuff";
            newConn = it->mailbox->maintainingTask;
            it->mailbox->maintainingTask->addDependentTask( this );
        } else {
            // The parser doesn't have anything associated with it, so we can go ahead and
            // register ourselves
            qDebug() << this << "Being bold, going ahead.";
            it->activeTasks.append( this );
            QTimer::singleShot( 0, model, SLOT(runReadyTasks()) );
        }
    }
}

void GetAnyConnectionTask::perform()
{
    // This is special from most ImapTasks' perform(), because the activeTasks could have already been updated
    if ( newConn ) {
        // We're "dependent" on some connection, so we should update our parser (even though
        // it could be already set), and also register ourselves with the Model
        parser = newConn->parser;
        Q_ASSERT( parser );
        model->_parsers[ parser ].activeTasks.append( this );
    }
    // ... we don't really have to do any work here, just declare ourselves completed
    _completed();
}

bool GetAnyConnectionTask::isReadyToRun() const
{
    return ! isFinished() && ! newConn;
}


}
}
