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


#include "DeleteMailboxTask.h"
#include "Common/InvokeMethod.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "GetAnyConnectionTask.h"

namespace Imap
{
namespace Mailbox
{


DeleteMailboxTask::DeleteMailboxTask(Model *model, const QString &mailbox):
    ImapTask(model), mailbox(mailbox)
{
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

void DeleteMailboxTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    tag = parser->deleteMailbox(mailbox);
}

bool DeleteMailboxTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (resp->kind == Responses::OK) {
            TreeItemMailbox *mailboxPtr = model->findMailboxByName(mailbox);
            if (mailboxPtr) {
                TreeItem *parentPtr = mailboxPtr->parent();
                QModelIndex parentIndex = parentPtr == model->m_mailboxes ? QModelIndex() : parentPtr->toIndex(model);
                model->beginRemoveRows(parentIndex, mailboxPtr->row(), mailboxPtr->row());
                mailboxPtr->parent()->m_children.erase(mailboxPtr->parent()->m_children.begin() + mailboxPtr->row());
                model->endRemoveRows();
                delete mailboxPtr;
            } else {
                QString buf;
                QDebug dbg(&buf);
                dbg << "The IMAP server just told us that it succeeded to delete mailbox named" <<
                    mailbox << ", yet we don't know of any such mailbox. Message from the server:" <<
                    resp->message;
                log(buf);
            }
            EMIT_LATER(model, mailboxDeletionSucceded, Q_ARG(QString, mailbox));
            _completed();
        } else {
            EMIT_LATER(model, mailboxDeletionFailed, Q_ARG(QString, mailbox), Q_ARG(QString, resp->message));
            _failed("Couldn't delete mailbox");
        }
        return true;
    } else {
        return false;
    }
}

QVariant DeleteMailboxTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Deleting mailbox")) : QVariant();
}

}
}
