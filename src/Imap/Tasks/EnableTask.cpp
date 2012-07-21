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


#include "EnableTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap
{
namespace Mailbox
{

EnableTask::EnableTask(Model *model, ImapTask *parentTask, const QList<QByteArray> &extensions) :
    ImapTask(model)
{
    Q_FOREACH(const QByteArray &item, extensions) {
        this->extensions << item.toUpper();
    }
    parentTask->addDependentTask(this);
}

void EnableTask::perform()
{
    parser = parentTask->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    tag = parser->enable(extensions);
}

bool EnableTask::handleEnabled(const Responses::Enabled *const resp)
{
    Q_FOREACH(const QByteArray &anExtension, resp->extensions) {
        // as soon as at least one of them was requested, let's declare that we've eaten this response
        if (extensions.contains(anExtension.toUpper()))
            return true;
    }
    return false;
}

bool EnableTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (resp->kind == Responses::OK) {
            // nothing should be needed here
            _completed();
        } else {
            _failed("ENABLE failed, strange");
            // FIXME: error handling
        }
        return true;
    } else {
        return false;
    }
}

QVariant EnableTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Enabling IMAP extensions")) : QVariant();
}


}
}
