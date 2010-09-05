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
#include "OpenConnectionTask.h"

namespace Imap {
namespace Mailbox {

GetAnyConnectionTask::GetAnyConnectionTask( Model* _model ) :
    ImapTask( _model ), newConn(0)
{
    if ( model->_parsers.isEmpty() ) {
        newConn = model->_taskFactory->createOpenConnectionTask( model );
        newConn->addDependentTask( this );
    } else {
        model->_parsers.begin()->activeTasks.append( this );
        parser = model->_parsers.begin().key();
        QTimer::singleShot( 0, model, SLOT(runReadyTasks()) );
    }
}

void GetAnyConnectionTask::perform()
{
    if ( newConn ) {
        parser = newConn->parser;
        model->_parsers[ parser ].activeTasks.append( this );
    }
    _completed();
}

bool GetAnyConnectionTask::isReadyToRun() const
{
    return ! isFinished() && ! newConn;
}


}
}
