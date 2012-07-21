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


#include "AppendTask.h"
#include "GetAnyConnectionTask.h"
#include "ItemRoles.h"
#include "Model.h"

namespace Imap
{
namespace Mailbox
{

AppendTask::AppendTask(Model *model, const QString &targetMailbox, const QByteArray &rawMessageData, const QStringList &flags,
                       const QDateTime &timestamp):
    ImapTask(model), targetMailbox(targetMailbox), rawMessageData(rawMessageData), flags(flags), timestamp(timestamp)
{
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

AppendTask::AppendTask(Model *model, const QString &targetMailbox, const QList<CatenatePair> &data, const QStringList &flags,
                       const QDateTime &timestamp):
    ImapTask(model), targetMailbox(targetMailbox), data(data), flags(flags), timestamp(timestamp)
{
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

void AppendTask::perform()
{
    parser = conn->parser;
    Q_ASSERT(parser);
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    if (data.isEmpty()) {
        tag = parser->append(targetMailbox, rawMessageData, flags, timestamp);
    } else {
        tag = parser->appendCatenate(targetMailbox, data, flags, timestamp);
    }
}

bool AppendTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (resp->kind == Responses::OK) {
            if (resp->respCode == Responses::APPENDUID) {
                const Responses::RespData<QPair<uint, Sequence> > *const respData =
                        dynamic_cast<const Responses::RespData<QPair<uint, Sequence> >* const>(resp->respCodeData.data());
                Q_ASSERT(respData);
                QList<uint> uids = respData->data.second.toList();
                if (uids.size() != 1) {
                    log("APPENDUID: mallformed data, cannot extract a single UID");
                } else {
                    emit appendUid(respData->data.first, uids.front());
                }
            }
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

QVariant AppendTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Uploading message")) : QVariant();
}

}
}
