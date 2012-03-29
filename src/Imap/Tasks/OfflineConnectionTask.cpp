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

#include "OfflineConnectionTask.h"
#include <QTimer>
#include "Streams/FakeSocket.h"
#include "TaskPresentationModel.h"

namespace Imap
{
namespace Mailbox
{

OfflineConnectionTask::OfflineConnectionTask(Model *model) : ImapTask(model)
{
    parser = new Parser(model, new FakeSocket(), ++model->lastParserId);
    ParserState parserState(parser);
    parserState.connState = CONN_STATE_LOGOUT;
    model->_parsers[parser] = parserState;
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
    model->_parsers.remove(parser);
    model->m_taskModel->slotParserDeleted(parser);
}

}
}
