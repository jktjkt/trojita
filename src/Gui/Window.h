/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@kde.org>

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
class QMessageBox;
class QModelIndex;
class QScrollArea;
class QSettings;
class QSplitter;
class QSslCertificate;
class QSslError;
class QStackedWidget;
class QToolButton;
class QTreeView;

namespace Composer
{
class SenderIdentitiesModel;
}

namespace Imap
{

class ImapAccess;

namespace Mailbox
{

class Model;
class PrettyMailboxModel;
class ThreadingMsgListModel;
class PrettyMsgListModel;

}
}

namespace Plugins
{
class PluginManager;
}

namespace MSA
{
class MSAFactory;
}
namespace Gui
{

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
    void showMainWindow();
    QSize sizeHint() const;

    Imap::Mailbox::Model *imapModel() const;

    Composer::SenderIdentitiesModel *senderIdentitiesModel() { return m_senderIdentities; }
    Plugins::PluginManager *pluginManager() { return m_pluginManager; }
    QSettings *settings() const { return m_settings; }
    MSA::MSAFactory *msaFactory();

    // FIXME: this should be changed to some wrapper when support for multiple accounts is available
    Imap::ImapAccess *imapAccess() const;

    void enableLoggingToDisk();

    static QWidget *messageSourceWidget(const QModelIndex &message);

public slots:
    void slotComposeMailUrl(const QUrl &url);
    void slotComposeMail();
    void invokeContactEditor();
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
    void imapError(const QString &message);
    void networkError(const QString &message);
    void cacheError(const QString &message);
    void authenticationRequested();
    void authenticationContinue(const QString &pass, const QString &errorMessage = QString());
    void checkSslPolicy();
    void slotManageContact(const QUrl &url);
    void slotEditDraft();
    void slotReplyTo();
    void slotReplyAllButMe();
    void slotReplyAll();
    void slotReplyList();
    void slotReplyGuess();
    void slotForwardAsAttachment();
    void slotUpdateMessageActions();
    void handleMarkAsRead(bool);
    void handleMarkAsDeleted(bool);
    void handleMarkAsFlagged(const bool);
    void handleMarkAsJunk(const bool);
    void handleMarkAsNotJunk(const bool);
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
    void updateMessageFlagsOf(const QModelIndex &index);
    void scrollMessageUp();
    void slotMessageModelChanged(QAbstractItemModel *model);
    void showConnectionStatus(uint parserId, Imap::ConnectionState state);
    void slotShowLinkTarget(const QString &link);
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
    void slotMailboxSyncFailed(const QString &mailbox, const QString &msg);
    void slotMailboxChanged(const QModelIndex &mailbox);

    void slotDownloadTransferError(const QString &errorString);
    void slotDownloadMessageFileNameRequested(QString *fileName);
    void slotScrollToUnseenMessage();
    void slotScrollToCurrent();
    void slotUpdateWindowTitle();

    void slotLayoutCompact();
    void slotLayoutWide();
    void slotLayoutOneAtTime();
    void saveSizesAndState();
    void saveRawStateSetting(bool enabled);
    void possiblyLoadMessageOnSplittersChanged();
    void updateNetworkIndication();

    void desktopGeometryChanged();

    void slotIconActivated(const QSystemTrayIcon::ActivationReason reason);
    void slotToggleSysTray();

    void slotResetReconnectState();

    void slotPluginsChanged();

    void showStatusMessage(const QString &message);

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

    QModelIndexList translatedSelection() const;

    Imap::ImapAccess *m_imapAccess;

    Imap::Mailbox::PrettyMailboxModel *prettyMboxModel;
    Imap::Mailbox::PrettyMsgListModel *prettyMsgListModel;
    Composer::SenderIdentitiesModel *m_senderIdentities;

    MailBoxTreeView *mboxTree;
    MessageListWidget *msgListWidget;
    QTreeView *allTree;
    QDockWidget *allDock;
    QTreeView *taskTree;
    QDockWidget *taskDock;
    QTreeView *mailMimeTree;
    QDockWidget *mailMimeDock;

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
    QAction *m_netToolbarDefaultAction;
    QAction *exitAction;
    QAction *showFullView;
    QAction *showTaskView;
    QAction *showMimeView;
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
    QAction *m_forwardAsAttachment;
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
    QAction *markAsFlagged;
    QAction *markAsJunk;
    QAction *markAsNotJunk;
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

    QAction *m_actionContactEditor;

    QToolBar *m_mainToolbar;
    QToolButton *m_menuFromToolBar;
    QToolButton *m_replyButton;
    QMenu *m_replyMenu;
    QToolButton *m_composeButton;
    QMenu *m_composeMenu;

    TaskProgressIndicator *busyParsersIndicator;
    QToolButton *networkIndicator;

    bool m_ignoreStoredPassword;

    QSettings *m_settings;
    Plugins::PluginManager *m_pluginManager;

    QMessageBox *m_networkErrorMessageBox;

    MainWindow(const MainWindow &); // don't implement
    MainWindow &operator=(const MainWindow &); // don't implement

    QSystemTrayIcon *m_trayIcon;
    QPoint m_headerDragStart;
};

}

#endif
