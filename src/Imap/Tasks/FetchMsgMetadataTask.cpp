/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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


#include "FetchMsgMetadataTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

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
                           QLatin1String("BODYSTRUCTURE") << QLatin1String("RFC822.SIZE"));
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
    return QString::fromAscii("%1: UIDs %2").arg(mailbox.data(RoleMailboxName).toString(), Sequence::fromList(uids).toString());
}

QVariant FetchMsgMetadataTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Downloading headers")) : QVariant();
}

}
}
