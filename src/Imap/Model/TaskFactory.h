/* Copyright (C) 2007 - 2010 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_MODEL_TASKFACTORY_H
#define IMAP_MODEL_TASKFACTORY_H

#include <memory>
#include <QMap>
#include <QModelIndex>

namespace Imap {
class Parser;
namespace Mailbox {

class OpenConnectionTask;
class GetAnyConnectionTask;
class ListChildMailboxesTask;
class Model;

class TaskFactory
{
public:
    virtual OpenConnectionTask* createOpenConnectionTask( Model* _model );
    virtual GetAnyConnectionTask* createGetAnyConnectionTask( Model* _model );
    virtual ListChildMailboxesTask* createListChildMailboxesTask( Model* _model, const QModelIndex& mailbox );
    virtual ~TaskFactory();
};

class TestingTaskFactory: public TaskFactory
{
public:
    TestingTaskFactory();
    virtual OpenConnectionTask* createOpenConnectionTask( Model* _model );
    virtual ListChildMailboxesTask* createListChildMailboxesTask( Model* _model, const QModelIndex& mailbox );
    bool fakeOpenConnectionTask;
    bool fakeListChildMailboxes;
    QMap<QString,QStringList> fakeListChildMailboxesMap;
private:
    Parser* newParser( Model* model );
};

typedef std::auto_ptr<TaskFactory> TaskFactoryPtr;

}
}

#endif // IMAP_MODEL_TASKFACTORY_H
