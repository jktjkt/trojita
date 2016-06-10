/* Copyright (C) 2012 Thomas Gahr <thomas.gahr@physik.uni-muenchen.de>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#ifndef GUI_MAILBOXTREEVIEW_H
#define GUI_MAILBOXTREEVIEW_H

#include <QTreeView>

namespace Imap {
namespace Mailbox {
class MailboxFinder;
}
}

namespace Gui {

/** @short Show mailboxes in a tree view */
class MailBoxTreeView : public QTreeView
{
    Q_OBJECT
public:
    explicit MailBoxTreeView(QWidget *parent = nullptr);
    void setDesiredExpansion(const QStringList &mailboxNames);
    void setModel(QAbstractItemModel *model) override;
signals:
    /** @short User has changed their mind about the expanded/collapsed state of the mailbox tree

    Stuff which gets reported here might refer to mailboxes which do not even exist. At the same time,
    the code will not forget about those mailboxes which "aren't there yet".
    */
    void mailboxExpansionChanged(const QStringList &mailboxNames);
protected:
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void resetWatchedMailboxes();
private:
    Imap::Mailbox::MailboxFinder *m_mailboxFinder;
    QSet<QString> m_desiredExpansionState;
};
}

#endif // GUI_MAILBOXTREEVIEW_H
