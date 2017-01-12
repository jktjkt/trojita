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

#include <algorithm>
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
    connect(this, &ImapTask::completed, this, &FetchMsgPartTask::markPendingItemsUnavailable);
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
        if (resp->respCode == Responses::UNKNOWN_CTE) {
            doForAllParts([this](TreeItemPart *part, const QByteArray &partId, const uint uid) {
                handleUnknownCTE(part, partId, uid);
            });
            // clean up so that these won't get marked UNAVAILABLE
            uids.clear();
            parts.clear();
            _failed(QStringLiteral("BINARY fetch failed: server reported [UNKNOWN-CTE]"));
        } else if (resp->kind == Responses::OK) {
            log(QStringLiteral("Fetched parts"), Common::LOG_MESSAGES);
            model->changeConnectionState(parser, CONN_STATE_SELECTED);
            _completed();
        } else {
            _failed(QStringLiteral("Part fetch failed"));
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

    if (uids.isEmpty())
        return QStringLiteral("[no items to fetch]");

    Q_ASSERT(!uids.isEmpty());
    QStringList buf;
    std::transform(parts.begin(), parts.end(), std::back_inserter(buf),
                   [](const QByteArray &x) {return QString::fromUtf8(x);}
                   );
    return QStringLiteral("%1: parts %2 for UIDs %3")
           .arg(mailboxIndex.data(RoleMailboxName).toString(), buf.join(QStringLiteral(", ")),
                QString::fromUtf8(Sequence::fromVector(uids).toByteArray()));
}

QVariant FetchMsgPartTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Downloading messages")) : QVariant();
}

void FetchMsgPartTask::doForAllParts(const std::function<void(TreeItemPart *, const QByteArray &, const uint)> &f)
{
    if (!mailboxIndex.isValid())
        return;

    Q_ASSERT(model);
    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    const auto messages = model->findMessagesByUids(mailbox, uids);
    for(auto message: messages) {
        for (const auto &partId: parts) {
            auto part = mailbox->partIdToPtr(model, static_cast<TreeItemMessage *>(message), partId);
            f(part, partId, message->uid());
        }
    }
}

/** @short We're dead, the data which hasn't arrived so far won't arrive in future unless a reset happens */
void FetchMsgPartTask::markPendingItemsUnavailable()
{
    doForAllParts([this](TreeItemPart *part, const QByteArray &partId, const uint uid) {
        if (!part) {
            log(QStringLiteral("FETCH: Cannot find part %1 for UID %2 in the tree")
                .arg(QString::fromUtf8(partId), QString::number(uid)), Common::LOG_MESSAGES);
            return;
        }
        if (part->loading()) {
            log(QStringLiteral("Received no data for part %1 UID %2").arg(QString::fromUtf8(partId), QString::number(uid)),
                Common::LOG_MESSAGES);
            markPartUnavailable(part);
        } else {
            log(QStringLiteral("Fetched part %1 for UID %2").arg(QString::fromUtf8(partId), QString::number(uid)),
                Common::LOG_MESSAGES);
        }
    });
}

/** @short Give up fetching attempts for this part */
void FetchMsgPartTask::markPartUnavailable(TreeItemPart *part)
{
    part->setFetchStatus(TreeItem::UNAVAILABLE);
    QModelIndex idx = part->toIndex(model);
    emit model->dataChanged(idx, idx);
}

void FetchMsgPartTask::handleUnknownCTE(TreeItemPart *part, const QByteArray &partId, const uint uid)
{
    if (!part) {
        log(QStringLiteral("FETCH: Cannot find part %1 of UID %2 in the tree")
            .arg(QString::fromUtf8(partId), QString::number(uid)), Common::LOG_MESSAGES);
        return;
    }
    if (part->fetched()) {
        log(QStringLiteral("Fetched part %1 of for UID %2").arg(QString::fromUtf8(partId), QString::number(uid)),
            Common::LOG_MESSAGES);
        return;
    }
    if (part->m_binaryCTEFailed) {
        // This is nasty -- how come that we issued something which ended up in an [UNKNOWN-CTE] for a second time?
        // Better prevent infinite loops here.
        log(QStringLiteral("FETCH BINARY: retry apparently also failed for part %1 UID %2")
            .arg(QString::fromUtf8(partId), QString::number(uid)));
        markPartUnavailable(part);
        return;
    } else {
        log(QStringLiteral("FETCH BINARY: got UNKNOWN-CTE for part %1 of UID %2, will fetch using the old-school way")
            .arg(QString::fromUtf8(partId), QString::number(uid)));
        part->m_binaryCTEFailed = true;
        model->askForMsgPart(part);
    }
}

}
}
