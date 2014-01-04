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


#include "Common/Application.h"
#include "IdTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/Utils.h"
#include "GetAnyConnectionTask.h"

namespace Imap
{
namespace Mailbox
{

IdTask::IdTask(Model *_model, ImapTask *dependingTask) : ImapTask(_model)
{
    dependingTask->addDependentTask(this);
    parser = dependingTask->parser;
}

void IdTask::perform()
{
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    if (model->property("trojita-imap-enable-id").toBool()) {
        QMap<QByteArray,QByteArray> identification;
        identification["name"] = "Trojita";
        identification["version"] = Common::Application::version.toUtf8();
        identification["os"] = systemPlatformVersion().toUtf8();
        tag = parser->idCommand(identification);
    } else {
        tag = parser->idCommand();
    }
}

bool IdTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {
        if (resp->kind == Responses::OK) {
            // nothing should be needed here
            _completed();
        } else {
            _failed("ID failed, strange");
            // But hey, we can just ignore this one
        }
        return true;
    } else {
        return false;
    }
}

bool IdTask::handleId(const Responses::Id *const resp)
{
    model->m_idResult = resp->data;
    return true;
}

QVariant IdTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Identifying server")) : QVariant();
}

}
}
