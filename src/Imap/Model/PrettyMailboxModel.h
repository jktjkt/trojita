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
#ifndef PRETTYMAILBOXMODEL_H
#define PRETTYMAILBOXMODEL_H

#include <QSortFilterProxyModel>
#include "Imap/Model/MailboxModel.h"

namespace Imap
{

namespace Mailbox
{

/** @short A pretty proxy model which increases sexiness of the MailboxModel */
class PrettyMailboxModel: public QSortFilterProxyModel
{
    Q_OBJECT
public:
    PrettyMailboxModel(QObject *parent, MailboxModel *mailboxModel);
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const;
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    /** @short Override in order to prevent needless LIST commands */
    virtual bool hasChildren(const QModelIndex &parent=QModelIndex()) const;
#ifdef XTUPLE_CONNECT
    void xtConnectStatusChanged(QModelIndex index);
#endif

public slots:
    void setShowOnlySubscribed(bool filterUnsubscribed);

private:
    bool m_showOnlySubscribed;
};

}

}

#endif // PRETTYMAILBOXMODEL_H
