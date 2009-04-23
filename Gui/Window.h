/* Copyright (C) 2007 - 2008 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef TROJITA_WINDOW_H
#define TROJITA_WINDOW_H

#include <QMainWindow>

#include "Imap/Cache.h"

class QAuthenticator;
class QTreeView;

namespace Imap {
namespace Mailbox {

class Model;
class MailboxModel;
class MsgListModel;

}
}

namespace Gui {

class MessageView;

class MainWindow: public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private slots:
    void showContextMenuMboxTree( const QPoint& position );
    void slotReloadMboxList();
    void slotResizeMsgListColumns();
    void alertReceived( const QString& message );
    void networkPolicyOffline();
    void networkPolicyExpensive();
    void networkPolicyOnline();
    void fullViewToggled( bool show );
    void slotShowSettings();
    void connectionError( const QString& message );
    void authenticationRequested( QAuthenticator* auth );

private:
    void createMenus();
    void createActions();
    void createWidgets();
    void setupModels();

    Imap::Mailbox::CachePtr cache;

    Imap::Mailbox::Model* model;
    Imap::Mailbox::MailboxModel* mboxModel;
    Imap::Mailbox::MsgListModel* msgListModel;

    QTreeView* mboxTree;
    QTreeView* msgListTree;
    QTreeView* allTree;
    MessageView* msgView;
    QDockWidget* allDock;

    QAction* reloadMboxList;
    QAction* reloadAllMailboxes;
    QAction* netOffline;
    QAction* netExpensive;
    QAction* netOnline;
    QAction* exitAction;
    QAction* showFullView;
    QAction* configSettings;
};

}

#endif
