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
            Imap::Parser *parser = static_cast<Imap::Parser*>(parent.internalPointer());
            Model::ParserState &parserState = m_model->accessParser(parser);
            if (row >= parserState.activeTasks.size()) {
                return QModelIndex();
            } else {
                return createIndex(row, 0, parserState.activeTasks.at(row));
            }
        } else {
            // The parent is a regular ImapTask
            ImapTask *task = static_cast<ImapTask*>(parent.internalPointer());
            Q_ASSERT(task);
            if (row >= task->dependentTasks.size()) {
                return QModelIndex();
            } else {
                return createIndex(row, 0, task->dependentTasks.at(row));
            }
        }
    } else {
        // So this is about a ParserState -- fair enough
        if (row >= m_model->_parsers.size()) {
            return QModelIndex();
        } else {
            return createIndex(row, 0, m_model->_parsers.keys().at(row));
        }
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

    // The child is definitely an ImapTask
    ImapTask *task = static_cast<ImapTask*>(child.internalPointer());
    Q_ASSERT(task);
    if (task->parentTask) {
        // And the child says that it has a prent task. The parent of this childis therefore an ImapTask, too
        int index = task->parentTask->dependentTasks.indexOf(task);
        Q_ASSERT(index != -1);
        return createIndex(index, 0, task->parentTask);
    } else {
        // The child has no parent, so the child is apparently the top-level task for a given parser,
        // and hence the parent is obviously the ParserState
        int index = m_model->_parsers.keys().indexOf(task->parser);
        Q_ASSERT(index != -1);
        return createIndex(index, 0, task->parser);
    }
}

int TaskPresentationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        // This is where it starts to get complicated -- we're somewhere inside the tree
        if (parent.data(RoleTaskIsParserState).toBool()) {
            // A child of the top level item, ie. a ParserState object
            Imap::Parser *parser = static_cast<Imap::Parser*>(parent.internalPointer());
            Model::ParserState &parserState = m_model->accessParser(parser);
            return parserState.activeTasks.size();
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
    return (rowCount(parent) > 0) ? 1 : 0;
}

QVariant TaskPresentationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    bool isParserState = m_model->_parsers.find(static_cast<Imap::Parser*>(index.internalPointer())) != m_model->_parsers.end();

    switch (role) {
    case RoleTaskIsParserState:
        return isParserState;
    case Qt::DisplayRole:
        if (isParserState) {
            Imap::Parser *parser = static_cast<Imap::Parser*>(index.internalPointer());
            return tr("Parser %1").arg(QString::number(parser->parserId()));
        } else {
            ImapTask *task = static_cast<ImapTask*>(index.internalPointer());
            return task->debugIdentification();
        }
    default:
        return QVariant();
    }
}

/** @short Called when a new task is created and registered with the model */
void TaskPresentationModel::slotTaskCreated(const ImapTask *const task)
{
    qDebug() << Q_FUNC_INFO << task;
    reset();
}

/** @short Called when a particular task ceases to exist

The ImapTask might be in various stages of destruction at this point, so it is not advisable to access its contents from
this function.
*/
void TaskPresentationModel::slotTaskDestroyed(const ImapTask *const task)
{
    qDebug() << Q_FUNC_INFO << task;
    reset();
}

/** @short The ImapTask has been moved to an active state, that is, it will be processing the IMAP protocol chat from now.

*/
void TaskPresentationModel::slotTaskActivated(const ImapTask *const task)
{
    reset();
}

/** @short A new parser just got created

We don't bother with proper fine-grained signals here.
*/
void TaskPresentationModel::slotParserCreated(Parser *parser)
{
    reset();
}

/** @short A parser has just been deleted

We don't bother with proper fine-grained signals here.
*/
void TaskPresentationModel::slotParserDeleted(Parser *parser)
{
    reset();
}

}
}
