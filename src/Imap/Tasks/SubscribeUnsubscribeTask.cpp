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


SubscribeUnsubscribeTask::SubscribeUnsubscribeTask(Model *model, const QString &mailboxName, SubscribeUnsubscribeOperation operation):
    ImapTask(model), operation(operation), mailboxName(mailboxName)
{
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

SubscribeUnsubscribeTask::SubscribeUnsubscribeTask(Model *model, ImapTask *parentTask, const QString &mailboxName, SubscribeUnsubscribeOperation operation):
    ImapTask(model), conn(parentTask), operation(operation), mailboxName(mailboxName)
{
    conn->addDependentTask(this);
}

void SubscribeUnsubscribeTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    switch (operation) {
    case SUBSCRIBE:
        tag = parser->subscribe(mailboxName);
        break;
    case UNSUBSCRIBE:
        tag = parser->unSubscribe(mailboxName);
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
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(model->findMailboxByName(mailboxName));
            QString subscribed = QStringLiteral("\\SUBSCRIBED");
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
            if (mailbox) {
                auto index = mailbox->toIndex(model);
                emit model->dataChanged(index, index);
            }
            _completed();
        } else {
            _failed(tr("SUBSCRIBE/UNSUBSCRIBE has failed"));
            // FIXME: error handling
        }
        return true;
    } else {
        return false;
    }
}

QString SubscribeUnsubscribeTask::debugIdentification() const
{
    return QStringLiteral("Subscription update for %1").arg(mailboxName);
}

QVariant SubscribeUnsubscribeTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Updating subscription information")) : QVariant();
}

}
}
