/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Fake_OpenConnectionTask.h"

namespace Imap
{
namespace Mailbox
{

Fake_OpenConnectionTask::Fake_OpenConnectionTask(Imap::Mailbox::Model *model, Imap::Parser *parser_):
    OpenConnectionTask(model, 0)
{
    // We really want to call the protected constructor, otherwise the OpenConnectionTask
    // would create a socket itself, and we don't want to end up there
    parser = parser_;
    QTimer::singleShot(0, this, SLOT(slotPerform()));
}

void Fake_OpenConnectionTask::perform()
{
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    _completed();
}

bool Fake_OpenConnectionTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    // This is a fake task, and therefore we aren't interested in any responses.
    // We have to override OpenConnectionTask's implementation.
    Q_UNUSED(resp);
    return false;
}


}
}
