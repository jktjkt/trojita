/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef TROJITA_WINDOW_H
#define TROJITA_WINDOW_H

#include <QMainWindow>

#include "Imap/ConnectionState.h"
#include "Imap/Model/Cache.h"
#include "Imap/Model/MessageComposer.h"

class QAuthenticator;
class QItemSelection;
class QModelIndex;
class QScrollArea;
class QSplitter;
class QSslCertificate;
class QSslError;
class QToolButton;
class QTreeView;

namespace Composer
{
class SenderIdentitiesModel;
}

namespace Imap
{
namespace Mailbox
{

class Model;
class MailboxModel;
class MsgListModel;
class PrettyMailboxModel;
class ThreadingMsgListModel;
class PrettyMsgListModel;

}
}

namespace Gui
{

class AbstractAddressbook;
class MailBoxTreeView;
class MessageView;
class MessageListWidget;
class ProtocolLoggerWidget;
class TaskProgressIndicator;

class MainWindow: public QMainWindow
{
    Q_OBJECT
    typedef QList<QPair<Composer::RecipientKind,QString> > RecipientsType;
public:
    MainWindow();
    void invokeComposeDialog(const QString &subject = QString(), const QString &body = QString(),
                             const RecipientsType &recipients = RecipientsType(),
                             const QList<QByteArray> &inReplyTo = QList<QByteArray>(),
                             const QList<QByteArray> &references = QList<QByteArray>(),
                             const QModelIndex &replyingToMessage = QModelIndex());
    QSize sizeHint() const;

    Imap::Mailbox::Model *imapModel() const;
    bool isCatenateSupported() const;
    bool isGenUrlAuthSupported() const;
    bool isImapSubmissionSupported() const;

    const AbstractAddressbook *addressBook() const { return m_addressBook; }
    Composer::SenderIdentitiesModel *senderIdentitiesModel() { return m_senderIdentities; }

private slots:
    void showContextMenuMboxTree(const QPoint &position);
    void showContextMenuMsgListTree(const QPoint &position);
    void slotReloadMboxList();
    void slotResyncMbox();
    void alertReceived(const QString &message);
    void networkPolicyOffline();
    void networkPolicyExpensive();
    void networkPolicyOnline();
    void slotShowSettings();
    void slotShowImapInfo();
    void slotExpunge();
    void connectionError(const QString &message);
    void cacheError(const QString &message);
    void authenticationRequested();
    void authenticationFailed(const QString &message);
    void sslErrors(const QList<QSslCertificate> &certificateChain, const QList<QSslError> &errors);
    void slotComposeMailUrl(const QUrl &url);
    void slotComposeMail();
    void slotReplyTo();
    void slotReplyAll();
    void handleMarkAsRead(bool);
    void handleMarkAsDeleted(bool);
    void slotNextUnread();
    void slotPreviousUnread();
    void msgListActivated(const QModelIndex &);
    void msgListClicked(const QModelIndex &);
    void msgListDoubleClicked(const QModelIndex &);
    void msgListSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void slotCreateMailboxBelowCurrent();
    void slotCreateTopMailbox();
    void slotDeleteCurrentMailbox();
#ifdef XTUPLE_CONNECT
    void slotXtSyncCurrentMailbox();
#endif
    void slotSubscribeCurrentMailbox();
    void slotShowOnlySubscribed();
    void updateMessageFlags();
    void updateMessageFlags(const QModelIndex &index);
    void scrollMessageUp();
    void showConnectionStatus(QObject *parser, Imap::ConnectionState state);
    void slotShowLinkTarget(const QString &link);
    void slotShowAboutTrojita();
    void slotDonateToTrojita();

    void slotSaveCurrentMessageBody();
    void slotViewMsgHeaders();
    void slotThreadMsgList();
    void slotHideRead();
    void slotSortingPreferenceChanged();
    void slotSortingConfirmed(int column, Qt::SortOrder order);
    void slotSearchRequested(const QStringList &searchConditions);
    void slotCapabilitiesUpdated(const QStringList &capabilities);

    void slotMailboxDeleteFailed(const QString &mailbox, const QString &msg);
    void slotMailboxCreateFailed(const QString &mailbox, const QString &msg);

    void slotDownloadMessageTransferError(const QString &errorString);
    void slotDownloadMessageFileNameRequested(QString *fileName);
    void slotScrollToUnseenMessage(const QModelIndex &mailbox, const QModelIndex &message);
    void slotUpdateWindowTitle();

    void slotLayoutCompact();
    void slotLayoutWide();

private:
    void createMenus();
    void createActions();
    void createWidgets();
    void setupModels();

    void nukeModels();
    void connectModelActions();

    void createMailboxBelow(const QModelIndex &index);

    void updateActionsOnlineOffline(bool online);

    void migrateSettings();

    Imap::Mailbox::Model *model;
    Imap::Mailbox::MailboxModel *mboxModel;
    Imap::Mailbox::PrettyMailboxModel *prettyMboxModel;
    Imap::Mailbox::MsgListModel *msgListModel;
    Imap::Mailbox::ThreadingMsgListModel *threadingMsgListModel;
    Imap::Mailbox::PrettyMsgListModel *prettyMsgListModel;
    Composer::SenderIdentitiesModel *m_senderIdentities;

    MailBoxTreeView *mboxTree;
    MessageListWidget *msgListWidget;
    QTreeView *allTree;
    MessageView *msgView;
    QDockWidget *allDock;
    QTreeView *taskTree;
    QDockWidget *taskDock;

    QScrollArea *area;

    ProtocolLoggerWidget *imapLogger;
    QDockWidget *imapLoggerDock;

    QSplitter *m_mainHSplitter;
    QSplitter *m_mainVSplitter;

    QAction *reloadMboxList;
    QAction *reloadAllMailboxes;
    QAction *resyncMbox;
    QAction *netOffline;
    QAction *netExpensive;
    QAction *netOnline;
    QAction *exitAction;
    QAction *showFullView;
    QAction *showTaskView;
    QAction *showImapLogger;
    QAction *logPersistent;
    QAction *showImapCapabilities;
    QAction *showMenuBar;
    QAction *showToolBar;
    QAction *configSettings;
    QAction *composeMail;
    QAction *replyTo;
    QAction *replyAll;
    QAction *expunge;
    QAction *createChildMailbox;
    QAction *createTopMailbox;
    QAction *deleteCurrentMailbox;
#ifdef XTUPLE_CONNECT
    QAction *xtIncludeMailboxInSync;
#endif
    QAction *aboutTrojita;
    QAction *donateToTrojita;

    QAction *markAsRead;
    QAction *markAsDeleted;
    QAction *saveWholeMessage;
    QAction *viewMsgHeaders;
    QAction *m_nextMessage;
    QAction *m_previousMessage;

    QAction *actionThreadMsgList;
    QAction *actionHideRead;
    QAction *m_actionSortByArrival;
    QAction *m_actionSortByCc;
    QAction *m_actionSortByDate;
    QAction *m_actionSortByFrom;
    QAction *m_actionSortBySize;
    QAction *m_actionSortBySubject;
    QAction *m_actionSortByTo;
    QAction *m_actionSortThreading;
    QAction *m_actionSortNone;
    QAction *m_actionSortAscending;
    QAction *m_actionSortDescending;
    QAction *m_actionLayoutCompact;
    QAction *m_actionLayoutWide;

    QAction *m_actionSubscribeMailbox;
    QAction *m_actionShowOnlySubscribed;

    QToolBar *m_mainToolbar;

    TaskProgressIndicator *busyParsersIndicator;
    QToolButton *networkIndicator;

    bool m_ignoreStoredPassword;
    bool m_supportsCatenate;
    bool m_supportsGenUrlAuth;
    bool m_supportsImapSubmission;

    AbstractAddressbook *m_addressBook;

    MainWindow(const MainWindow &); // don't implement
    MainWindow &operator=(const MainWindow &); // don't implement
};

}

#endif
