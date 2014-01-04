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


#include "SubscribeUnsubscribeTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "GetAnyConnectionTask.h"

namespace Imap
{
namespace Mailbox
{


SubscribeUnsubscribeTask::SubscribeUnsubscribeTask(Model *model, const QModelIndex &mailbox, SubscribeUnsubscribeOperation operation):
    ImapTask(model), operation(operation), mailboxIndex(mailbox)
{
    Q_ASSERT(dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailbox.internalPointer())));
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

void SubscribeUnsubscribeTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    if (! mailboxIndex.isValid()) {
        // FIXME: add proper fix
        log("Mailbox vanished before we could ask for number of messages inside");
        _completed();
        return;
    }
    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);

    switch (operation) {
    case SUBSCRIBE:
        tag = parser->subscribe(mailbox->mailbox());
        break;
    case UNSUBSCRIBE:
        tag = parser->unSubscribe(mailbox->mailbox());
        break;
    default:
        Q_ASSERT(false);
    }
}

bool SubscribeUnsubscribeTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {
        if (resp->kind == Responses::OK) {
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
            QString subscribed = QLatin1String("\\SUBSCRIBED");
            switch (operation) {
            case SUBSCRIBE:
                if (mailbox && !mailbox->m_metadata.flags.contains(subscribed)) {
                    mailbox->m_metadata.flags.append(subscribed);
                }
                break;
            case UNSUBSCRIBE:
                if (mailbox) {
                    mailbox->m_metadata.flags.removeOne(subscribed);
                }
            }
            _completed();
        } else {
            _failed("SUBSCRIBE/UNSUBSCRIBE has failed");
            // FIXME: error handling
        }
        return true;
    } else {
        return false;
    }
}

QString SubscribeUnsubscribeTask::debugIdentification() const
{
    if (! mailboxIndex.isValid())
        return QLatin1String("[invalid mailboxIndex]");

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    return QString::fromUtf8("attached to %1").arg(mailbox->mailbox());
}

QVariant SubscribeUnsubscribeTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Looking for messages")) : QVariant();
}

}
}
