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


#include "UpdateFlagsTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "CopyMoveMessagesTask.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

UpdateFlagsTask::UpdateFlagsTask(Model *model, const QModelIndexList &messages_, const FlagsOperation flagOperation, const QString &flags):
    ImapTask(model), copyMove(0), flagOperation(flagOperation), flags(flags)
{
    if (messages_.isEmpty()) {
        throw CantHappen("UpdateFlagsTask called with empty message set");
    }
    Q_FOREACH(const QModelIndex& index, messages_) {
        messages << index;
    }
    QModelIndex mailboxIndex = model->findMailboxForItems(messages_);
    conn = model->findTaskResponsibleFor(mailboxIndex);
    conn->addDependentTask(this);
}

UpdateFlagsTask::UpdateFlagsTask(Model *model, CopyMoveMessagesTask *copyTask, const QList<QPersistentModelIndex> &messages,
                                 const FlagsOperation flagOperation, const QString &flags):
    ImapTask(model), conn(0), copyMove(copyTask), messages(messages), flagOperation(flagOperation), flags(flags)
{
    copyTask->addDependentTask(this);
}

void UpdateFlagsTask::perform()
{
    Q_ASSERT(conn || copyMove);
    if (conn)
        parser = conn->parser;
    else if (copyMove)
        parser = copyMove->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    Sequence seq;
    bool first = true;

    Q_FOREACH(const QPersistentModelIndex& index, messages) {
        if (!index.isValid()) {
            log("Some message got removed before we could update its flags", Common::LOG_MESSAGES);
        } else {
            TreeItem *item = static_cast<TreeItem *>(index.internalPointer());
            Q_ASSERT(item);
            TreeItemMessage *message = dynamic_cast<TreeItemMessage *>(item);
            Q_ASSERT(message);
            if (first) {
                seq = Sequence(message->uid());
                first = false;
            } else {
                seq.add(message->uid());
            }
            switch (flagOperation) {
            case FLAG_ADD:
            case FLAG_REMOVE:
            case FLAG_USE_THESE:
                // we aren't supposed to update them ourselves; the IMAP server will tell us
                break;
            case FLAG_REMOVE_SILENT:
            {
                TreeItemMsgList *list = dynamic_cast<TreeItemMsgList*>(message->parent());
                Q_ASSERT(list);
                QStringList newFlags = message->m_flags;
                newFlags.removeOne(flags);
                message->setFlags(list, newFlags , false);
                // we don't have to either re-sort or call Model::normalizeFlags again from this context;
                // this will change when the model starts de-duplicating whole lists
                model->cache()->setMsgFlags(static_cast<TreeItemMailbox*>(list->parent())->mailbox(), message->uid(), newFlags);
                break;
            }
            case FLAG_ADD_SILENT:
            {
                TreeItemMsgList *list = dynamic_cast<TreeItemMsgList*>(message->parent());
                Q_ASSERT(list);
                QStringList newFlags = message->m_flags;
                if (!newFlags.contains(flags)) {
                    newFlags << flags;
                    message->setFlags(list, model->normalizeFlags(newFlags), false);
                    model->cache()->setMsgFlags(static_cast<TreeItemMailbox*>(list->parent())->mailbox(), message->uid(), newFlags);
                }
                break;
            }
            }
        }
    }

    if (first) {
        // No valid messages
        _failed("All messages got removed before we could've updated their flags");
        return;
    }
    tag = parser->uidStore(seq, toImapString(flagOperation), flags);
}

bool UpdateFlagsTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (resp->kind == Responses::OK) {
            // nothing should be needed here
            _completed();
        } else {
            _failed("Failed to update FLAGS");
            // FIXME: error handling
        }
        return true;
    }
    return false;
}

QVariant UpdateFlagsTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Saving message state")) : QVariant();
}


}
}
