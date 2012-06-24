/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef MESSAGELISTWIDGET_H
#define MESSAGELISTWIDGET_H

#include <QWidget>

class QLineEdit;

namespace Gui {

class MsgListView;

/** @short Widget containing a list of messages along with quick filtering toolbar */
class MessageListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MessageListWidget(QWidget *parent = 0);

    QStringList searchConditions() const;

    // FIXME: consider making this private and moving the logic from Window with it
    MsgListView *tree;

signals:
    void requestingSearch(const QStringList &conditions);

protected slots:
    void slotApplySearch();
    void slotAutoEnableDisableSearch();

private:
    QLineEdit *m_quickSearchText;
};

}

#endif // MESSAGELISTWIDGET_H
