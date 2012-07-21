/* Copyright (C) 2007 - 2012 Jan Kundrát <jkt@flaska.net>

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


#include "UidSubmitTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "MailboxTree.h"
#include "Model.h"

namespace Imap
{
namespace Mailbox
{

UidSubmitTask::UidSubmitTask(Model *model, const QString &mailbox, const uint uidValidity, const uint uid,
                             const UidSubmitOptionsList &submitOptions):
    ImapTask(model), m_uidValidity(uidValidity), m_uid(uid), m_options(submitOptions)
{
    m_mailboxIndex = model->findMailboxByName(mailbox)->toIndex(model);
    conn = model->findTaskResponsibleFor(m_mailboxIndex);
    conn->addDependentTask(this);
}

void UidSubmitTask::perform()
{
    parser = conn->parser;
    Q_ASSERT(parser);
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    Q_ASSERT(m_mailboxIndex.isValid());
    const uint realUidValidity = m_mailboxIndex.data(RoleMailboxUidValidity).toUInt();
    if (realUidValidity != m_uidValidity) {
        _failed(tr("UIDVALIDITY mismatch: expected %1, got %2")
                .arg(QString::number(realUidValidity), QString::number(m_uidValidity)));
        return;
    }

    tag = parser->uidSendmail(m_uid, m_options);
}

bool UidSubmitTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (resp->kind == Responses::OK) {
            // nothing should be needed here
            _completed();
        } else {
            _failed(resp->message);
        }
        return true;
    } else {
        return false;
    }
}

QVariant UidSubmitTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Sending mail")) : QVariant();
}

}
}
