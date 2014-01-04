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

#ifndef IMAP_FETCHMSGPARTTASK_H
#define IMAP_FETCHMSGPARTTASK_H

#include <QPersistentModelIndex>
#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Fetch a message part */
class FetchMsgPartTask : public ImapTask
{
    Q_OBJECT
public:
    FetchMsgPartTask(Model *model, const QModelIndex &mailbox, const QList<uint> &uids, const QStringList &parts);
    virtual void perform();

    virtual bool handleFetch(const Imap::Responses::Fetch *const resp);
    virtual bool handleStateHelper(const Imap::Responses::State *const resp);

    virtual QString debugIdentification() const;
    virtual QVariant taskData(const int role) const;
    virtual bool needsMailbox() const {return true;}
private:
    CommandHandle tag;
    ImapTask *conn;
    QList<uint> uids;
    QStringList parts;
    QPersistentModelIndex mailboxIndex;
};

}
}

#endif // IMAP_FETCHMSGPARTTASK_H
