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

#ifndef TROJITA_WINDOW_H
#define TROJITA_WINDOW_H

#include <QMainWindow>
#include <QModelIndex>
#include <QPointer>
#include <QSystemTrayIcon>

#include "Composer/Recipients.h"
#include "Imap/ConnectionState.h"
#include "Imap/Model/Cache.h"

class QAuthenticator;
class QCloseEvent;
class QItemSelection;
class QModelIndex;
class QScrollArea;
class QSettings;
class QSplitter;
class QSslCertificate;
class QSslError;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace BE {
class Contacts;
}

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
class NetworkWatcher;
class PrettyMailboxModel;
class ThreadingMsgListModel;
class PrettyMsgListModel;

}
}

namespace Plugins
{
class PluginManager;
}

namespace Gui
{

class AbstractAddressbook;
class CompleteMessageWidget;
class ComposeWidget;
class MailBoxTreeView;
class MessageListWidget;
class ProtocolLoggerWidget;
class TaskProgressIndicator;

class MainWindow: public QMainWindow
{
    Q_OBJECT
    typedef QList<QPair<Composer::RecipientKind,QString> > RecipientsType;

    typedef enum { LAYOUT_COMPACT, LAYOUT_WIDE, LAYOUT_ONE_AT_TIME } LayoutMode;
public:
    MainWindow(QSettings *settings);
    ComposeWidget *invokeComposeDialog(const QString &subject = QString(), const QString &body = QString(),
                                       const RecipientsType &recipients = RecipientsType(),
                                       const QList<QByteArray> &inReplyTo = QList<QByteArray>(),
                                       const QList<QByteArray> &references = QList<QByteArray>(),
                                       const QModelIndex &replyingToMessage = QModelIndex());
    QSize sizeHint() const;

    Imap::Mailbox::Model *imapModel() const;

    const AbstractAddressbook *addressBook() const { return m_addressBook; }
    Composer::SenderIdentitiesModel *senderIdentitiesModel() { return m_senderIdentities; }
    Plugins::PluginManager *pluginManager() { return m_pluginManager; }
protected:
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *o, QEvent *e);
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
    void requireStartTlsInFuture();
    void slotComposeMailUrl(const QUrl &url);
    void slotManageContact(const QUrl &url);
    void slotComposeMail();
    void slotEditDraft();
    void slotReplyTo();
    void slotReplyAllButMe();
    void slotReplyAll();
    void slotReplyList();
    void slotReplyGuess();
    void slotUpdateMessageActions();
    void handleMarkAsRead(bool);
    void handleMarkAsDeleted(bool);
    void slotNextUnread();
    void slotPreviousUnread();
    void msgListClicked(const QModelIndex &);
    void msgListDoubleClicked(const QModelIndex &);
    void slotCreateMailboxBelowCurrent();
    void slotMarkCurrentMailboxRead();
    void slotCreateTopMailbox();
    void slotDeleteCurrentMailbox();
    void handleTrayIconChange();
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
    void fillMatchingAbookEntries(const QString &mail, QStringList &displayNames);
    void slotShowAboutTrojita();
    void slotDonateToTrojita();

    void slotSaveCurrentMessageBody();
    void slotViewMsgSource();
    void slotViewMsgHeaders();
    void slotThreadMsgList();
    void slotHideRead();
    void slotSortingPreferenceChanged();
    void slotSortingConfirmed(int column, Qt::SortOrder order);
    void slotSearchRequested(const QStringList &searchConditions);
    void slotCapabilitiesUpdated(const QStringList &capabilities);

    void slotMailboxDeleteFailed(const QString &mailbox, const QString &msg);
    void slotMailboxCreateFailed(const QString &mailbox, const QString &msg);
    void slotMailboxChanged(const QModelIndex &mailbox);

    void slotDownloadTransferError(const QString &errorString);
    void slotDownloadMessageFileNameRequested(QString *fileName);
    void slotScrollToUnseenMessage(const QModelIndex &mailbox, const QModelIndex &message);
    void slotUpdateWindowTitle();

    void slotLayoutCompact();
    void slotLayoutWide();
    void slotLayoutOneAtTime();
    void saveSizesAndState();
    void saveRawStateSetting(bool enabled);
    void possiblyLoadMessageOnSplittersChanged();

    void desktopGeometryChanged();

    void slotIconActivated(const QSystemTrayIcon::ActivationReason reason);
    void slotToggleSysTray();
    void invokeContactEditor();

protected:
    void resizeEvent(QResizeEvent *);

private:
    void defineActions();
    void createMenus();
    void createActions();
    void createWidgets();
    void setupModels();

    void nukeModels();
    void connectModelActions();

    void createMailboxBelow(const QModelIndex &index);

    void updateActionsOnlineOffline(bool online);

    void applySizesAndState();
    QString settingsKeyForLayout(const LayoutMode layout);

    void recoverDrafts();
    void createSysTray();
    void removeSysTray();

    Imap::Mailbox::Model *model;
    Imap::Mailbox::MailboxModel *mboxModel;
    Imap::Mailbox::PrettyMailboxModel *prettyMboxModel;
    Imap::Mailbox::MsgListModel *msgListModel;
    Imap::Mailbox::NetworkWatcher *m_networkWatcher;
    Imap::Mailbox::ThreadingMsgListModel *threadingMsgListModel;
    Imap::Mailbox::PrettyMsgListModel *prettyMsgListModel;
    Composer::SenderIdentitiesModel *m_senderIdentities;

    MailBoxTreeView *mboxTree;
    MessageListWidget *msgListWidget;
    QTreeView *allTree;
    QDockWidget *allDock;
    QTreeView *taskTree;
    QDockWidget *taskDock;

    CompleteMessageWidget *m_messageWidget;

    ProtocolLoggerWidget *imapLogger;
    QDockWidget *imapLoggerDock;

    QPointer<QSplitter> m_mainHSplitter;
    QPointer<QSplitter> m_mainVSplitter;
    QPointer<QStackedWidget> m_mainStack;

    LayoutMode m_layoutMode;
    bool m_skipSavingOfUI;
    QTimer *m_delayedStateSaving;

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
    QAction *m_oneAtTimeGoBack;
    QAction *composeMail;
    QAction *m_editDraft;
    QAction *m_replyPrivate;
    QAction *m_replyAllButMe;
    QAction *m_replyAll;
    QAction *m_replyList;
    QAction *m_replyGuess;
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
    QAction *viewMsgSource;
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
    QAction *m_actionLayoutOneAtTime;
    QAction *m_actionMarkMailboxAsRead;

    QAction *m_actionSubscribeMailbox;
    QAction *m_actionShowOnlySubscribed;

    QToolBar *m_mainToolbar;
    QToolButton *m_replyButton;
    QMenu *m_replyMenu;
    QToolButton *m_composeButton;
    QMenu *m_composeMenu;

    TaskProgressIndicator *busyParsersIndicator;
    QToolButton *networkIndicator;
    QToolButton *menuShow;

    bool m_ignoreStoredPassword;

    QSettings *m_settings;
    Plugins::PluginManager *m_pluginManager;

    AbstractAddressbook *m_addressBook;
    QPointer<BE::Contacts> m_contactsWidget;

    MainWindow(const MainWindow &); // don't implement
    MainWindow &operator=(const MainWindow &); // don't implement

    QSystemTrayIcon *m_trayIcon;
    QPoint m_headerDragStart;
};

}

#endif
