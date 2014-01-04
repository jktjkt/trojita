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


#include "FetchMsgPartTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

FetchMsgPartTask::FetchMsgPartTask(Model *model, const QModelIndex &mailbox, const QList<uint> &uids, const QStringList &parts):
    ImapTask(model), uids(uids), parts(parts), mailboxIndex(mailbox)
{
    Q_ASSERT(!uids.isEmpty());
    conn = model->findTaskResponsibleFor(mailboxIndex);
    conn->addDependentTask(this);
}

void FetchMsgPartTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    Sequence seq = Sequence::fromList(uids);
    tag = parser->uidFetch(seq, parts);
}

bool FetchMsgPartTask::handleFetch(const Imap::Responses::Fetch *const resp)
{
    if (!mailboxIndex.isValid()) {
        _failed("Mailbox disappeared");
        return false;
    }

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    model->genericHandleFetch(mailbox, resp);
    return true;
}

bool FetchMsgPartTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (!mailboxIndex.isValid()) {
        _failed("Mailbox disappeared");
        return false;
    }

    if (resp->tag == tag) {
        if (resp->kind == Responses::OK) {
            log("Fetched parts", Common::LOG_MESSAGES);
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
            Q_ASSERT(mailbox);
            QList<TreeItemMessage *> messages = model->findMessagesByUids(mailbox, uids);
            Q_FOREACH(TreeItemMessage *message, messages) {
                Q_FOREACH(const QString &partId, parts) {
                    log("Fetched part" + partId, Common::LOG_MESSAGES);
                    model->finalizeFetchPart(mailbox, message->row() + 1, partId);
                }
            }
            model->changeConnectionState(parser, CONN_STATE_SELECTED);
            _completed();
        } else {
            // FIXME: error handling
            _failed("Part fetch failed");
        }
        return true;
    } else {
        return false;
    }
}

QString FetchMsgPartTask::debugIdentification() const
{
    if (!mailboxIndex.isValid())
        return QLatin1String("[invalid mailbox]");

    Q_ASSERT(!uids.isEmpty());
    return QString::fromUtf8("%1: parts %2 for UIDs %3")
           .arg(mailboxIndex.data(RoleMailboxName).toString(), parts.join(QLatin1String(", ")),
                Sequence::fromList(uids).toByteArray());
}

QVariant FetchMsgPartTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Downloading messages")) : QVariant();
}

}
}
