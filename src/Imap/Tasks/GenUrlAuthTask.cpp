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


#include "GenUrlAuthTask.h"
#include <QUrl>
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "GetAnyConnectionTask.h"

namespace Imap
{
namespace Mailbox
{

GenUrlAuthTask::GenUrlAuthTask(Model *model, const QString &host, const QString &user, const QString &mailbox,
                               const uint uidValidity, const uint uid, const QString &part, const QString &access):
    ImapTask(model)
{
    req = QString::fromUtf8("imap://%1@%2/%3;UIDVALIDITY=%4/;UID=%5%6;urlauth=%7")
            .arg(user, host, QUrl::toPercentEncoding(mailbox), QString::number(uidValidity), QString::number(uid),
                 part.isEmpty() ? QString(): QString::fromUtf8("/;section=%1").arg(part),
                 access );
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

void GenUrlAuthTask::perform()
{
    parser = conn->parser;
    Q_ASSERT(parser);
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    tag = parser->genUrlAuth(req.toUtf8(), "INTERNAL");
}

bool GenUrlAuthTask::handleStateHelper(const Imap::Responses::State *const resp)
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

bool GenUrlAuthTask::handleGenUrlAuth(const Responses::GenUrlAuth *const resp)
{
    // FIXME: check whether the received URL matches what we expect and not aanything else; this is required for pipelining!
    emit gotAuth(resp->url);
    return true;
}

QVariant GenUrlAuthTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Obtaining authentication token")) : QVariant();
}

}
}
