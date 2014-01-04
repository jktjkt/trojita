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

#ifndef IMAP_MODEL_VISIBLETASKSMODEL_H
#define IMAP_MODEL_VISIBLETASKSMODEL_H

#include <QSortFilterProxyModel>

class KDescendantsProxyModel;

namespace Imap
{
namespace Mailbox
{

/** @short Proxy model showing a list of tasks that are active or pending

In contrast to the full tree model, this proxy will show only those ImapTasks that somehow correspond to an activity requested by
user.  This means that auxiliary tasks like GetAnyConnectionTask, KeepMailboxOpenTask etc are not shown.

The goal is to have a way of showing an activity indication whenever the IMAP connection is "doing something".
*/
class VisibleTasksModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(bool hasVisibleTasks READ hasVisibleTasks NOTIFY hasVisibleTasksChanged)
public:
    explicit VisibleTasksModel(QObject *parent, QAbstractItemModel *taskModel);
    bool hasVisibleTasks() const;

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QHash<int, QByteArray> trojitaProxyRoleNames() const;
#else
    virtual QHash<int, QByteArray> roleNames() const;
#endif

signals:
    void hasVisibleTasksChanged();
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
private:
    KDescendantsProxyModel *m_flatteningModel;
};

}
}

#endif // IMAP_MODEL_VISIBLETASKSMODEL_H
