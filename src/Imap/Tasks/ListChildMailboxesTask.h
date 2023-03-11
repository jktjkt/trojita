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

#ifndef IMAP_LISTCHILDMAILBOXES_TASK_H
#define IMAP_LISTCHILDMAILBOXES_TASK_H

#include <QPersistentModelIndex>
#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Ask for child mailboxes below a certain mailbox */
class ListChildMailboxesTask : public ImapTask
{
    Q_OBJECT
public:
    ListChildMailboxesTask(Model *_model, const QModelIndex &mailbox);
    ~ListChildMailboxesTask();
    void perform() override;

    bool handleStateHelper(const Imap::Responses::State *const resp) override;
    bool handleStatus(const Imap::Responses::Status *const resp) override;

    QString debugIdentification() const override;
    QVariant taskData(const int role) const override;
    bool needsMailbox() const override {return false;}
protected:
    void applyCachedStatus();
    void _failed(const QString &errorMessage) override;

    CommandHandle tag;
    ImapTask *conn;
    QPersistentModelIndex mailboxIndex;
    bool mailboxIsRootMailbox;
    QList<Imap::Responses::Status*> m_pendingStatusResponses;
};

}
}

#endif // IMAP_LISTCHILDMAILBOXES_TASK_H
