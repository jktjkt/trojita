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


#include "SortTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap
{
namespace Mailbox
{


SortTask::SortTask(Model *model, const QModelIndex &mailbox, const QStringList &sortCriteria):
    ImapTask(model), mailboxIndex(mailbox), sortCriteria(sortCriteria)
{
    conn = model->findTaskResponsibleFor(mailbox);
    conn->addDependentTask(this);
}

void SortTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    if (! mailboxIndex.isValid()) {
        _failed("Mailbox vanished before we could ask for threading info");
        return;
    }

    if (model->accessParser(parser).capabilitiesFresh &&
            model->accessParser(parser).capabilities.contains(QLatin1String("ESORT"))) {
        // FIXME: add support for sort contexts from RFC5267
        tag = parser->uidESort(sortCriteria, QLatin1String("utf-8"), QStringList() << QLatin1String("ALL"));
    } else {
        tag = parser->uidSort(sortCriteria, QLatin1String("utf-8"), QStringList() << QLatin1String("ALL"));
    }
}

bool SortTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {
        if (resp->kind == Responses::OK) {
            emit model->sortingAvailable(mailboxIndex, sortCriteria, sortResult);
            _completed();
        } else {
            _failed("Sorting command has failed");
        }
        return true;
    } else {
        return false;
    }
}

bool SortTask::handleSort(const Imap::Responses::Sort *const resp)
{
    sortResult = resp->numbers;
    return true;
}

bool SortTask::handleESearch(const Responses::ESearch *const resp)
{
    if (resp->tag != tag)
        return false;

    if (resp->seqOrUids != Imap::Responses::ESearch::UIDS)
        throw UnexpectedResponseReceived("ESEARCH response to a UID SORT command with matching tag uses "
                                         "sequence numbers instead of UIDs", *resp);

    if (resp->listData.contains("ALL"))
        sortResult = resp->listData["ALL"];
    else if (resp->numData.contains("ALL"))
        sortResult = QList<uint>() << resp->numData["ALL"];
    else
        throw UnexpectedResponseReceived("ESEARCH response to UID SORT doesn't contain the ALL result", *resp);

    return true;
}

QVariant SortTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Sorting mailbox")) : QVariant();
}

void SortTask::_failed(const QString &errorMessage)
{
    // FIXME: show this in the GUI
    emit model->sortingFailed(mailboxIndex, sortCriteria);
    ImapTask::_failed(errorMessage);
}

}
}
