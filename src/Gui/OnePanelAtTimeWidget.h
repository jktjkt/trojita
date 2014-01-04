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
#ifndef GUI_ONEPANELATTIME_H
#define GUI_ONEPANELATTIME_H

#include <QStackedWidget>

class QAction;
class QMainWindow;
class QToolBar;

namespace Gui
{

class CompleteMessageWidget;
class MailBoxTreeView;
class MessageListWidget;

/** @short Implementation of the "show one panel at a time" mode */
class OnePanelAtTimeWidget: public QStackedWidget
{
    Q_OBJECT
public:
    OnePanelAtTimeWidget(QMainWindow *mainWindow, MailBoxTreeView *mboxTree, MessageListWidget *msgListWidget,
                         CompleteMessageWidget *messageWidget, QToolBar *toolbar, QAction* toolbarActions);
    virtual ~OnePanelAtTimeWidget();

public slots:
    void slotOneAtTimeGoBack();
    void slotOneAtTimeGoDeeper();

private:
    QMainWindow *m_mainWindow;
    MailBoxTreeView *m_mboxTree;
    MessageListWidget *m_msgListWidget;
    CompleteMessageWidget *m_messageWidget;

    QToolBar *m_toolbar;
    QAction *m_actionGoBack;
};

}

#endif // GUI_ONEPANELATTIME_H
