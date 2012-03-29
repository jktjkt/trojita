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
#include "GetAnyConnectionTask.h"
#include "ItemRoles.h"
#include "KeepMailboxOpenTask.h"
#include "Model.h"
#include "NoopTask.h"
#include "OpenConnectionTask.h"
#include "UnSelectTask.h"

namespace Imap
{
namespace Mailbox
{

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
            Imap::Parser *parser = static_cast<Imap::Parser *>(parent.internalPointer());
            ParserState &parserState = m_model->accessParser(parser);
            if (row >= parserState.activeTasks.size()) {
                return QModelIndex();
            } else {
                return createIndex(row, 0, parserState.activeTasks.at(row));
            }
        } else {
            // The parent is a regular ImapTask
            ImapTask *task = static_cast<ImapTask *>(parent.internalPointer());
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
    return indexForTask(static_cast<ImapTask *>(child.internalPointer()));
}

/** @short Return a QModelIndex for the specified ImapTask* */
QModelIndex TaskPresentationModel::indexForTask(const ImapTask *const task) const
{
    Q_ASSERT(task);
    if (task->parentTask) {
        // And the child says that it has a prent task. The parent of this childis therefore an ImapTask, too.
        // The question is, what's the parent of our parent?
        int index = -1;
        if (task->parentTask->parentTask) {
            // The parent has an ImapTask as a parent.
            index = task->parentTask->parentTask->dependentTasks.indexOf(task->parentTask);
        } else {
            // Our grandparent is a ParserState
            index = m_model->accessParser(task->parentTask->parser).activeTasks.indexOf(task->parentTask);
        }
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
            Imap::Parser *parser = static_cast<Imap::Parser *>(parent.internalPointer());
            ParserState &parserState = m_model->accessParser(parser);
            return parserState.activeTasks.size();
        } else {
            // It's a regular ImapTask
            ImapTask *task = static_cast<ImapTask *>(parent.internalPointer());
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

    bool isParserState = m_model->_parsers.find(static_cast<Imap::Parser *>(index.internalPointer())) != m_model->_parsers.end();

    switch (role) {
    case RoleTaskIsParserState:
        return isParserState;
    case RoleTaskIsVisible: {
        if (isParserState) {
            // That's not a task at all
            return false;
        }

        ImapTask *task = static_cast<ImapTask *>(index.internalPointer());
        if (dynamic_cast<KeepMailboxOpenTask *>(task) || dynamic_cast<GetAnyConnectionTask *>(task) ||
            dynamic_cast<UnSelectTask *>(task)) {
            // Internal, auxiliary tasks
            return false;
        } else {
            return true;
        }
    }
    case Qt::DisplayRole:
        if (isParserState) {
            Imap::Parser *parser = static_cast<Imap::Parser *>(index.internalPointer());
            return tr("Parser %1").arg(QString::number(parser->parserId()));
        } else {
            ImapTask *task = static_cast<ImapTask *>(index.internalPointer());
            QString className = QLatin1String(task->metaObject()->className());
            className.remove(QLatin1String("Imap::Mailbox::"));
            return tr("%1: %2").arg(className, task->debugIdentification());
        }
    default:
        return QVariant();
    }
}

/** @short Called when a particular task ceases to exist

The ImapTask might be in various stages of destruction at this point, so it is not advisable to access its contents from
this function.
*/
void TaskPresentationModel::slotTaskDestroyed(const ImapTask *const task)
{
    Q_UNUSED(task);
    reset();
}

/** @short A new parser just got created

We don't bother with proper fine-grained signals here.
*/
void TaskPresentationModel::slotParserCreated(Parser *parser)
{
    Q_UNUSED(parser);
    reset();
}

/** @short A parser has just been deleted

We don't bother with proper fine-grained signals here.
*/
void TaskPresentationModel::slotParserDeleted(Parser *parser)
{
    Q_UNUSED(parser);
    reset();
}

/** @short A parent of the given Imaptask has just changed

The task might or might not have been present in the model before.  We don't know.
*/
void TaskPresentationModel::slotTaskGotReparented(const ImapTask *const task)
{
    reset();
    connect(task, SIGNAL(completed(ImapTask *)), this, SLOT(slotTaskMighHaveChanged(ImapTask *)));
}

/** @short The textual description, the state or something else related to this task might have changed */
void TaskPresentationModel::slotTaskMighHaveChanged(ImapTask *task)
{
    QModelIndex index = indexForTask(task);
    Q_ASSERT(index.isValid());
    emit dataChanged(index, index);
}

void dumpModelContents(QAbstractItemModel *model, QModelIndex index, QByteArray offset)
{
    qDebug() << offset << index.data(Qt::DisplayRole).toString();
    for (int i=0; i < model->rowCount(index); ++i) {
        QModelIndex child = model->index(i, 0, index);
        if (!child.isValid()) {
            qDebug() << "FAIL: " << index << child << i << model->rowCount(index);
        }
        Q_ASSERT(child.isValid());
        dumpModelContents(model, child, offset+QByteArray(" "));
    }
}


}
}
