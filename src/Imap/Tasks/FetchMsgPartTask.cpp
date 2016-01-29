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

FetchMsgPartTask::FetchMsgPartTask(Model *model, const QModelIndex &mailbox, const Uids &uids, const QList<QByteArray> &parts):
    ImapTask(model), uids(uids), parts(parts), mailboxIndex(mailbox)
{
    Q_ASSERT(!uids.isEmpty());
    conn = model->findTaskResponsibleFor(mailboxIndex);
    conn->addDependentTask(this);
    connect(this, &ImapTask::failed, this, &FetchMsgPartTask::markPendingItemsUnavailable);
}

void FetchMsgPartTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    Sequence seq = Sequence::fromVector(uids);
    tag = parser->uidFetch(seq, parts);
}

bool FetchMsgPartTask::handleFetch(const Imap::Responses::Fetch *const resp)
{
    if (!mailboxIndex.isValid()) {
        _failed(tr("Mailbox disappeared"));
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
        _failed(tr("Mailbox disappeared"));
        return false;
    }

    if (resp->tag == tag) {
        markPendingItemsUnavailable();
        if (resp->kind == Responses::OK) {
            log(QStringLiteral("Fetched parts"), Common::LOG_MESSAGES);
            model->changeConnectionState(parser, CONN_STATE_SELECTED);
            _completed();
        } else {
            _failed(tr("Part fetch failed"));
        }
        return true;
    } else {
        return false;
    }
}

QString FetchMsgPartTask::debugIdentification() const
{
    if (!mailboxIndex.isValid())
        return QStringLiteral("[invalid mailbox]");

    Q_ASSERT(!uids.isEmpty());
    QStringList buf;
    Q_FOREACH(const QByteArray &item, parts) {
        buf << QString::fromUtf8(item);
    }
    return QStringLiteral("%1: parts %2 for UIDs %3")
           .arg(mailboxIndex.data(RoleMailboxName).toString(), buf.join(QStringLiteral(", ")),
                QString::fromUtf8(Sequence::fromVector(uids).toByteArray()));
}

QVariant FetchMsgPartTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Downloading messages")) : QVariant();
}

/** @short We're dead, the data which hasn't arrived so far won't arrive in future unless a reset happens */
void FetchMsgPartTask::markPendingItemsUnavailable()
{
    if (!mailboxIndex.isValid())
        return;

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    QList<TreeItemMessage *> messages = model->findMessagesByUids(mailbox, uids);
    Q_FOREACH(TreeItemMessage *message, messages) {
        Q_FOREACH(const QByteArray &partId, parts) {
            if (model->finalizeFetchPart(mailbox, message->row() + 1, partId)) {
                log(QLatin1String("Fetched part ") + QString::fromUtf8(partId), Common::LOG_MESSAGES);
            } else {
                log(QLatin1String("Received no data for part ") + QString::fromUtf8(partId), Common::LOG_MESSAGES);
            }
        }
    }
}

}
}
