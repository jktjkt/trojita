/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_SUBSCRIBEUNSUBSCRIBETASK_H
#define IMAP_SUBSCRIBEUNSUBSCRIBETASK_H

#include <QPersistentModelIndex>
#include "ImapTask.h"
#include "SubscribeUnSubscribeOperation.h"

namespace Imap
{
namespace Mailbox
{

/** @short Ask for number of messages in a certain mailbox */
class SubscribeUnsubscribeTask : public ImapTask
{
    Q_OBJECT
public:
    SubscribeUnsubscribeTask(Model *model, const QModelIndex &mailbox, SubscribeUnsubscribeOperation operation);
    virtual void perform();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);

    virtual QString debugIdentification() const;
    virtual QVariant taskData(const int role) const;
    virtual bool needsMailbox() const {return false;}
private:
    CommandHandle tag;
    ImapTask *conn;
    SubscribeUnsubscribeOperation operation;
    QPersistentModelIndex mailboxIndex;
};

}
}

#endif // IMAP_SUBSCRIBEUNSUBSCRIBETASK_H
