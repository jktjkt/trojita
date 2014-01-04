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


#ifndef IMAP_MAILBOX_TASKPRESENTATIONMODEL_H
#define IMAP_MAILBOX_TASKPRESENTATIONMODEL_H

#include <QAbstractItemModel>

namespace Imap
{
class Parser;

namespace Mailbox
{

class ImapTask;
class Model;

/** @short Model providing a tree view on all tasks which belong to a particular model

This class provides a standard Qt model which provides a tree hierarchy of all tasks which are somehow registered with its parent
model.  The tasks are organized in a tree with active tasks being all children of the root item.  Tasks which are waiting for
completion of another task are positioned as children of the item they're blocking at.
*/
class TaskPresentationModel : public QAbstractItemModel
{
    Q_OBJECT

    QModelIndex indexForTask(ImapTask *const task) const;

public:
    explicit TaskPresentationModel(Model *model);

    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;
    virtual QModelIndex parent(const QModelIndex &child) const;
    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

public slots:
    void slotTaskDestroyed(const ImapTask *const task);
    void slotTaskGotReparented(const ImapTask *const task);
    void slotTaskMighHaveChanged(ImapTask *task);

    void slotParserCreated(Parser *parser);
    void slotParserDeleted(Parser *parser);

private:
    Model *m_model;

    friend class Model; // needs to be able to call reset() on us
};

/** @short Debug: dump the model in a tree-like manner */
void dumpModelContents(QAbstractItemModel *model, QModelIndex index = QModelIndex(), int offset = 0);

}
}

#endif // IMAP_MAILBOX_TASKPRESENTATIONMODEL_H
