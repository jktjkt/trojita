/* Copyright (C) 2007 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_IDTASK_H
#define IMAP_IDTASK_H

#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Send an ID command to the server */
class IdTask : public ImapTask
{

public:
    IdTask(Model *model, ImapTask *dependingTask);
    virtual void perform();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual bool handleId(const Responses::Id *const resp);
    virtual QVariant taskData(const int role) const;
private:
    CommandHandle tag;
};

}
}

#endif // IMAP_IDTASK_H
