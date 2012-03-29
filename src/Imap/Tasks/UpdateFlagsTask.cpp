/* Copyright (C) 2007 - 2011 Jan Kundrát <jkt@flaska.net>

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


#include "UpdateFlagsTask.h"
#include "KeepMailboxOpenTask.h"
#include "CopyMoveMessagesTask.h"
#include "Model.h"
#include "MailboxTree.h"

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

UpdateFlagsTask::UpdateFlagsTask(Model *_model, CopyMoveMessagesTask *copyTask, const QList<QPersistentModelIndex> &_messages,
                                 const FlagsOperation _flagOperation, const QString &_flags):
    ImapTask(_model), conn(0), copyMove(copyTask), messages(_messages), flagOperation(_flagOperation), flags(_flags)
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
            log("Some message got removed before we could update its flags", LOG_MESSAGES);
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
        }
    }

    if (first) {
        // No valid messages
        _failed("All messages got removed before we could've updated their flags");
        return;
    }

    QString op;
    switch (flagOperation) {
    case FLAG_ADD:
        op = QLatin1String("+FLAGS");
        break;
    case FLAG_REMOVE:
        op = QLatin1String("-FLAGS");
        break;
    }
    Q_ASSERT(!op.isEmpty());
    tag = parser->uidStore(seq, op, flags);
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
    } else {
        return false;
    }
}


}
}
