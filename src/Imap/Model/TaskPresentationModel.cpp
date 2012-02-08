/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "TaskPresentationModel.h"
#include "ImapTask.h"
#include "ItemRoles.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

TaskPresentationModel::TaskPresentationModel(Model *model) :
    QAbstractItemModel(model), m_model(model)
{
}

QModelIndex TaskPresentationModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0)
        return QModelIndex();
    if (row < 0)
        return QModelIndex();

    if (parent.isValid()) {
        // Parent is a valid index, so the child is definitely an ImapTask. The parent could still be a ParserState, though.
        if (parent.data(RoleTaskIsParserState).toBool()) {
            // The parent is a ParserState
            Model::ParserState *parserState = static_cast<Model::ParserState*>(parent.internalPointer());
            Q_ASSERT(parserState);
            if (row >= parserState->activeTasks.size()) {
                return QModelIndex();
            } else {
                return createIndex(row, 0, parserState->activeTasks.at(row));
            }
        } else {
            // The parent is a regular ImapTask
            ImapTask *task = static_cast<ImapTask*>(parent.internalPointer());
            Q_ASSERT(task);


        }
    } else {
        // FIXME
    }
}

QModelIndex TaskPresentationModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    if (child.data(RoleTaskIsParserState).toBool()) {
        // A parent of the parser state is always the root item
        return QModelIndex();
    }

    // So it's definitely an ImapTask
    ImapTask *task = static_cast<ImapTask*>(child.internalPointer());
    Q_ASSERT(task);
    if (task->parentTask) {
        int index = task->parentTask->dependentTasks.indexOf(task);
        Q_ASSERT(index != -1);
        return createIndex(index, 0, task);
    } else {
        // no parent, so it's apparently the top-levle task for a given parser
        Model::ParserState &parserState = m_model->accessParser(task->parser);
        int index = parserState.activeTasks.indexOf(task);
        Q_ASSERT(index != -1);
        return createIndex(index, 0, task);
    }
}

int TaskPresentationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        // This is where it starts to get complicated -- we're somewhere inside the tree
        if (parent.data(RoleTaskIsParserState).toBool()) {
            // A child of the top level item, ie. a ParserState object
            Model::ParserState *parserState = static_cast<Model::ParserState*>(parent.internalPointer());
            Q_ASSERT(parserState);
            return parserState->activeTasks.size();
        } else {
            // It's a regular ImapTask
            ImapTask *task = static_cast<ImapTask*>(parent.internalPointer());
            Q_ASSERT(task);
            return task->dependentTasks.size();
        }
    } else {
        // The top-level stuff children represent the list of active connections
        return m_model->_parsers.size();
    }
}

int TaskPresentationModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    } else {
        return 1;
    }
}

QVariant TaskPresentationModel::data(const QModelIndex &index, int role) const
{
}

/** @short Called when a new task is created and registered with the model */
void TaskPresentationModel::slotTaskCreated(const ImapTask *const task)
{
    qDebug() << Q_FUNC_INFO << task;
}

/** @short Called when a particular task ceases to exist

The ImapTask might be in various stages of destruction at this point, so it is not advisable to access its contents from
this function.
*/
void TaskPresentationModel::slotTaskDestroyed(const ImapTask *const task)
{
    qDebug() << Q_FUNC_INFO << task;
}

/** @short The ImapTask has been moved to an active state, that is, it will be processing the IMAP protocol chat from now.

*/
void TaskPresentationModel::slotTaskActivated(const ImapTask *const task)
{
}

}
}
