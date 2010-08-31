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

#include "TaskFactory.h"
#include "OpenConnectionTask.h"

namespace Imap {
namespace Mailbox {

#define CREATE_FAKE_TASK(X) class Fake_##X: public X { \
public: virtual void perform() {} Fake_##X() { QTimer::singleShot( 0, this, SLOT(slotSucceed()) ); } \
};

CREATE_FAKE_TASK(OpenConnectionTask)

#undef CREATE_FAKE_TASK

TaskFactory::~TaskFactory()
{
}

OpenConnectionTask* TaskFactory::createOpenConnectionTask( Model* _model )
{
    return new OpenConnectionTask( _model );
}

TestingTaskFactory::TestingTaskFactory(): TaskFactory(), fakeOpenConnectionTask(false)
{
}

OpenConnectionTask* TestingTaskFactory::createOpenConnectionTask( Model *_model )
{
    if ( fakeOpenConnectionTask )
        return new Fake_OpenConnectionTask();
    else
        TaskFactory::createOpenConnectionTask( _model );
}

}
}
