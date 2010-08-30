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

#ifndef IMAP_KEEPMAILBOXOPENTASK_H
#define IMAP_KEEPMAILBOXOPENTASK_H

#include "ImapTask.h"
#include <QModelIndex>
#include <QSet>

namespace Imap {
namespace Mailbox {

class TreeItemMailbox;

class ObtainSynchronizedMailboxTask;

/** @short Maintain a connection to a mailbox

This Task shall maintain a connection to a remote mailbox, updating the mailbox
state while receiving various messages.

Essentially, this Task is responsible for processing stuff like EXPUNGE replies
while the mailbox is selected. It's a bit special task, because it will not emit
completed() unless something else wants to re-use its Parser instance.
*/
class KeepMailboxOpenTask : public ImapTask
{
Q_OBJECT
public:
    KeepMailboxOpenTask( Model* _model, const QModelIndex& mailbox );
    virtual void perform();

    /** @short Mark an ImapTask as a user of this task */
    void registerUsingTask( ImapTask* user );

    /** @short Queue a request for replacement of this task

      Upon receiving this request, the KeepMailboxOpenTask will _complete() itself
as soon as all the "using" tasks get completed.
*/
    void terminate();

private slots:
    void slotMailboxObtained();
    void slotTaskDeleted( QObject* object );

private:
    ObtainSynchronizedMailboxTask* obtainTask;
    QSet<ImapTask*> users;
    bool shouldExit;
};

}
}

#endif // IMAP_KEEPMAILBOXOPENTASK_H
