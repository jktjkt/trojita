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

#include "OfflineConnectionTask.h"
#include <QTimer>
#include "Common/ConnectionId.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/TaskPresentationModel.h"
#include "Streams/FakeSocket.h"

namespace Imap
{
namespace Mailbox
{

OfflineConnectionTask::OfflineConnectionTask(Model *model) : ImapTask(model)
{
    parser = new Parser(model, new Streams::FakeSocket(CONN_STATE_CONNECTED_PRETLS_PRECAPS), Common::ConnectionId::next());
    ParserState parserState(parser);
    parserState.connState = CONN_STATE_LOGOUT;
    model->m_parsers[parser] = parserState;
    model->m_taskModel->slotParserCreated(parser);
    markAsActiveTask();
    QTimer::singleShot(0, this, SLOT(slotPerform()));
}

/** @short A decorator for slottifying the perform() method */
void OfflineConnectionTask::slotPerform()
{
    perform();
}

void OfflineConnectionTask::perform()
{
    model->runReadyTasks();
    _failed("We're offline");
    QTimer::singleShot(0, this, SLOT(slotDie()));
}

/** @short A slot for the die() */
void OfflineConnectionTask::slotDie()
{
    die();
    deleteLater();
    model->killParser(parser, Model::PARSER_KILL_EXPECTED);
    model->m_parsers.remove(parser);
    model->m_taskModel->slotParserDeleted(parser);
}

/** @short This is an internal task */
QVariant OfflineConnectionTask::taskData(const int role) const
{
    Q_UNUSED(role);
    return QVariant();
}


}
}
