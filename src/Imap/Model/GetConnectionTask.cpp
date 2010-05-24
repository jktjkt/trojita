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

#include "GetConnectionTask.h"
#include <QTimer>

namespace Imap {
namespace Mailbox {

GetConnectionTask::GetConnectionTask( Model* _model,
                                      TreeItemMailbox* _what, Model::RWMode _mode,
                                      const CommandHandle& _parentTask ) :
    ImapTask(_model, 0, _parentTask)
{
    // FIXME: later change zeros to _what and _mode
    parser = model->_getParser( 0, Model::ReadOnly, false );
    QTimer::singleShot( 0, this, SIGNAL(completed()) );
}

void GetConnectionTask::perform()
{
    // nothing to do here
}

void GetConnectionTask::abort()
{
    // can't really abort
}

void GetConnectionTask::upstreamFailed()
{
    // no upstream
}

}
}
