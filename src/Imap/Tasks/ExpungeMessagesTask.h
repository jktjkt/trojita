/* Copyright (C) 2007 - 2011 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_EXPUNGEMESSAGES_TASK_H
#define IMAP_EXPUNGEMESSAGES_TASK_H

#include <QPersistentModelIndex>
#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Expunge messages marked as \\Deleted and matching the list of UIDs from one mailbox

This task will fail if the IMAP server doesn't support the UIDPLUS extension.
*/
class ExpungeMessagesTask : public ImapTask
{
    Q_OBJECT
public:
    ExpungeMessagesTask(Model *model, ImapTask *parentTask, const QList<QPersistentModelIndex> &messages);
    virtual void perform();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual QVariant taskData(const int role) const;
private:
    CommandHandle tag;
    ImapTask *conn;
    QList<QPersistentModelIndex> messages;
};

}
}

#endif // IMAP_EXPUNGEMESSAGES_TASK_H
