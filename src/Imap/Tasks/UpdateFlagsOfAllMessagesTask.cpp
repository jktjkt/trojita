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


#include "UpdateFlagsOfAllMessagesTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

UpdateFlagsOfAllMessagesTask::UpdateFlagsOfAllMessagesTask(Model *model, const QModelIndex &mailboxIndex,
                                                           const FlagsOperation flagOperation, const QString &flags):
    ImapTask(model), flagOperation(flagOperation), flags(flags), mailboxIndex(mailboxIndex)
{
    Q_ASSERT(mailboxIndex.isValid());
    Q_ASSERT(flagOperation == Imap::Mailbox::FLAG_ADD || flagOperation == Imap::Mailbox::FLAG_ADD_SILENT);
    conn = model->findTaskResponsibleFor(mailboxIndex);
    conn->addDependentTask(this);
}

void UpdateFlagsOfAllMessagesTask::perform()
{
    Q_ASSERT(conn);
    parser = conn->parser;

    markAsActiveTask();
    IMAP_TASK_CHECK_ABORT_DIE;

    Sequence seq = Sequence::startingAt(1);
    tag = parser->store(seq, toImapString(flagOperation), flags);
}

bool UpdateFlagsOfAllMessagesTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {
        if (resp->kind == Responses::OK) {
            TreeItemMailbox *mailbox = model->mailboxForSomeItem(mailboxIndex);
            if (!mailbox) {
                // There isn't much to be done here -- let's assume that the index has disappeared.
                // The flags will be resynced the next time we open that mailbox.
                _failed(QLatin1String("Mailbox is gone"));
                return true;
            }
            Q_ASSERT(mailbox);
            TreeItemMsgList *list = dynamic_cast<TreeItemMsgList*>(mailbox->m_children [0]);
            Q_ASSERT(list);

            Q_FOREACH (TreeItem *item, list->m_children) {
                TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(item);
                Q_ASSERT(message);

                Q_ASSERT(flagOperation == Imap::Mailbox::FLAG_ADD || flagOperation == Imap::Mailbox::FLAG_ADD_SILENT);
                QStringList newFlags = message->m_flags;
                if (!newFlags.contains(flags)) {
                    newFlags << flags;
                    message->setFlags(list, model->normalizeFlags(newFlags), true);
                    model->cache()->setMsgFlags(mailbox->mailbox(), message->uid(), newFlags);
                    QModelIndex messageIndex = model->createIndex(message->m_offset, 0, message);

                    // emitting dataChanged() separately for each message in the mailbox:
                    // Trojita model assmues that dataChanged is emitted individually
                    model->dataChanged(messageIndex, messageIndex);
                }
            }
            model->dataChanged(mailboxIndex, mailboxIndex);
            list->fetchNumbers(model);
            _completed();
        } else {
            _failed("Failed to update Mailbox FLAGS");
        }
        return true;
    }
    return false;
}

QVariant UpdateFlagsOfAllMessagesTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Saving mailbox state")) : QVariant();
}

}
}
