/* Copyright (C) 2013 Yasser Aziza <yasser.aziza@gmail.com>

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

#ifndef IMAP_UPDATEFLAGSOFALLMESSAGES_TASK_H
#define IMAP_UPDATEFLAGSOFALLMESSAGES_TASK_H


#include <QPersistentModelIndex>
#include "Imap/Model/FlagsOperation.h"
#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Update messages flags for all messages in a given mailbox

The purpose of this task is to make sure the IMAP flags for all messages of
a given mailbox are changed.
*/
class UpdateFlagsOfAllMessagesTask : public ImapTask
{
    Q_OBJECT
public:

    UpdateFlagsOfAllMessagesTask(Model *model, const QModelIndex &mailboxIndex, const FlagsOperation flagOperation,
                                 const QString &flags);

    virtual void perform();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual QVariant taskData(const int role) const;
    virtual bool needsMailbox() const {return true;}
private:
    CommandHandle tag;
    ImapTask *conn;
    FlagsOperation flagOperation;
    QString flags;
    QModelIndex mailboxIndex;
};

}
}

#endif // IMAP_UPDATEFLAGSOFALLMESSAGES_TASK_H
