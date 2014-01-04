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


#include "FetchMsgMetadataTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "KeepMailboxOpenTask.h"

namespace Imap
{
namespace Mailbox
{

FetchMsgMetadataTask::FetchMsgMetadataTask(Model *model, const QModelIndex &mailbox, const QList<uint> &uids) :
    ImapTask(model), mailbox(mailbox), uids(uids)
{
    Q_ASSERT(!uids.isEmpty());
    conn = model->findTaskResponsibleFor(mailbox);
    conn->addDependentTask(this);
}

void FetchMsgMetadataTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    Sequence seq = Sequence::fromList(uids);

    // we do not want to use _onlineMessageFetch because it contains UID and FLAGS
    tag = parser->uidFetch(seq, QStringList() << QLatin1String("ENVELOPE") << QLatin1String("INTERNALDATE") <<
                           QLatin1String("BODYSTRUCTURE") << QLatin1String("RFC822.SIZE") <<
                           QLatin1String("BODY.PEEK[HEADER.FIELDS (References List-Post)]"));
}

bool FetchMsgMetadataTask::handleFetch(const Imap::Responses::Fetch *const resp)
{
    if (! mailbox.isValid()) {
        _failed("handleFetch: mailbox disappeared");
        // FIXME: nice error handling
        return false;
    }
    TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItemMailbox *>(mailbox.internalPointer()));
    Q_ASSERT(mailboxPtr);
    model->genericHandleFetch(mailboxPtr, resp);
    return true;
}

bool FetchMsgMetadataTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (resp->kind == Responses::OK) {
            _completed();
        } else {
            _failed("UID FETCH failed");
            // FIXME: error handling
        }
        return true;
    } else {
        return false;
    }
}

QString FetchMsgMetadataTask::debugIdentification() const
{
    if (!mailbox.isValid())
        return QLatin1String("[invalid mailbox]");

    Q_ASSERT(!uids.isEmpty());
    return QString::fromUtf8("%1: UIDs %2").arg(mailbox.data(RoleMailboxName).toString(),
                                                QString::fromUtf8(Sequence::fromList(uids).toByteArray()));
}

QVariant FetchMsgMetadataTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Downloading headers")) : QVariant();
}

}
}
