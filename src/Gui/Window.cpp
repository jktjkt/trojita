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

#include <QAuthenticator>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QSplitter>
#include <QSslError>
#include <QSslKey>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>

#include "AbookAddressbook/AbookAddressbook.h"
#include "AbookAddressbook/be-contacts.h"
#include "Common/Application.h"
#include "Common/Paths.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/ModelWatcher.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/NetworkWatcher.h"
#include "Imap/Model/PrettyMailboxModel.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Model/Utils.h"
#include "Imap/Network/FileDownloadManager.h"
#include "MSA/Sendmail.h"
#include "MSA/SMTP.h"
#include "Plugins/PluginManager.h"
#include "CompleteMessageWidget.h"
#include "ComposeWidget.h"
#include "IconLoader.h"
#include "MailBoxTreeView.h"
#include "MessageListWidget.h"
#include "MessageView.h"
#include "MessageSourceWidget.h"
#include "MsgListView.h"
#include "OnePanelAtTimeWidget.h"
#include "PasswordDialog.h"
#include "ProtocolLoggerWidget.h"
#include "SettingsDialog.h"
#include "SimplePartWidget.h"
#include "Streams/SocketFactory.h"
#include "TaskProgressIndicator.h"
#include "Util.h"
#include "Window.h"
#include "ShortcutHandler/ShortcutHandler.h"

#include "ui_CreateMailboxDialog.h"
#include "ui_AboutDialog.h"

#include "Imap/Model/ModelTest/modeltest.h"

/** @short All user-facing widgets and related classes */
namespace Gui
{

enum {
    MINIMUM_WIDTH_NORMAL = 800,
    MINIMUM_WIDTH_WIDE = 1250
};

MainWindow::MainWindow(QSettings *settings): QMainWindow(), model(0),
    m_mainHSplitter(0), m_mainVSplitter(0), m_mainStack(0), m_layoutMode(LAYOUT_COMPACT), m_skipSavingOfUI(true),
    m_delayedStateSaving(0), m_actionSortNone(0), m_ignoreStoredPassword(false), m_settings(settings), m_pluginManager(0), m_trayIcon(0)
{
    // m_pluginManager must be created before calling createWidgets
    m_pluginManager = new Plugins::PluginManager(m_settings, Common::SettingsNames::addressbookPlugin, Common::SettingsNames::passwordPlugin, this);

    createWidgets();

    ShortcutHandler *shortcutHandler = new ShortcutHandler(this);
    shortcutHandler->setSettingsObject(m_settings);
    defineActions();
    shortcutHandler->readSettings(); // must happen after defineActions()

    Imap::migrateSettings(m_settings);

    m_senderIdentities = new Composer::SenderIdentitiesModel(this);
    m_senderIdentities->loadFromSettings(*m_settings);

    if (! m_settings->contains(Common::SettingsNames::imapMethodKey)) {
        QTimer::singleShot(0, this, SLOT(slotShowSettings()));
    }


    // TODO write more addressbook backends and make this configurable
    m_addressBook = new AbookAddressbook();

    setupModels();
    createActions();
    createMenus();
    slotToggleSysTray();

    // Please note that Qt 4.6.1 really requires passing the method signature this way, *not* using the SLOT() macro
    QDesktopServices::setUrlHandler(QLatin1String("mailto"), this, "slotComposeMailUrl");
    QDesktopServices::setUrlHandler(QLatin1String("x-trojita-manage-contact"), this, "slotManageContact");

    slotUpdateWindowTitle();

    recoverDrafts();

    if (m_actionLayoutWide->isEnabled() &&
            m_settings->value(Common::SettingsNames::guiMainWindowLayout) == Common::SettingsNames::guiMainWindowLayoutWide) {
        m_actionLayoutWide->trigger();
    } else if (m_settings->value(Common::SettingsNames::guiMainWindowLayout) == Common::SettingsNames::guiMainWindowLayoutOneAtTime) {
        m_actionLayoutOneAtTime->trigger();
    } else {
        m_actionLayoutCompact->trigger();
    }

    // Don't listen to QDesktopWidget::resized; that is emitted too early (when it gets fired, the screen size has changed, but
    // the workspace area is still the old one). Instead, listen to workAreaResized which gets emitted at an appropriate time.
    // The delay is still there to guarantee some smoothing; on jkt's box there are typically three events in a rapid sequence
    // (some of them most likely due to the fact that at first, the actual desktop gets resized, the plasma panel reacts
    // to that and only after the panel gets resized, the available size of "the rest" is correct again).
    // Which is why it makes sense to introduce some delay in there. The 0.5s delay is my best guess and "should work" (especially
    // because every change bumps the timer anyway, as Thomas pointed out).
    QTimer *delayedResize = new QTimer(this);
    delayedResize->setSingleShot(true);
    delayedResize->setInterval(500);
    connect(delayedResize, SIGNAL(timeout()), this, SLOT(desktopGeometryChanged()));
    connect(qApp->desktop(), SIGNAL(workAreaResized(int)), delayedResize, SLOT(start()));
    m_skipSavingOfUI = false;
}

void MainWindow::defineActions()
{
    ShortcutHandler *shortcutHandler = ShortcutHandler::instance();
    shortcutHandler->defineAction(QLatin1String("action_application_exit"), QLatin1String("application-exit"), tr("E&xit"), QKeySequence::Quit);
    shortcutHandler->defineAction(QLatin1String("action_compose_mail"), QLatin1String("document-edit"), tr("&New Message..."), QKeySequence::New);
    shortcutHandler->defineAction(QLatin1String("action_compose_draft"), QLatin1String("document-open-recent"), tr("&Edit Draft..."));
    shortcutHandler->defineAction(QLatin1String("action_show_menubar"), QLatin1String("view-list-text"), tr("Show Main Menu &Bar"), tr("Ctrl+M"));
    shortcutHandler->defineAction(QLatin1String("action_expunge"), QLatin1String("trash-empty"), tr("Exp&unge"), tr("Ctrl+E"));
    shortcutHandler->defineAction(QLatin1String("action_mark_as_read"), QLatin1String("mail-mark-read"), tr("Mark as &Read"), QLatin1String("M"));
    shortcutHandler->defineAction(QLatin1String("action_go_to_next_unread"), QLatin1String("arrow-right"), tr("&Next Unread Message"), QLatin1String("N"));
    shortcutHandler->defineAction(QLatin1String("action_go_to_previous_unread"), QLatin1String("arrow-left"), tr("&Previous Unread Message"), QLatin1String("P"));
    shortcutHandler->defineAction(QLatin1String("action_mark_as_deleted"), QLatin1String("list-remove"), tr("Mark as &Deleted"), QKeySequence(Qt::Key_Delete).toString());
    shortcutHandler->defineAction(QLatin1String("action_save_message_as"), QLatin1String("document-save"), tr("&Save Message..."));
    shortcutHandler->defineAction(QLatin1String("action_view_message_source"), QString(), tr("View Message &Source..."));
    shortcutHandler->defineAction(QLatin1String("action_view_message_headers"), QString(), tr("View Message &Headers..."), tr("Ctrl+U"));
    shortcutHandler->defineAction(QLatin1String("action_reply_private"), QLatin1String("mail-reply-sender"), tr("&Private Reply"), tr("Ctrl+Shift+A"));
    shortcutHandler->defineAction(QLatin1String("action_reply_all_but_me"), QLatin1String("mail-reply-all"), tr("Reply to All &but Me"), tr("Ctrl+Shift+R"));
    shortcutHandler->defineAction(QLatin1String("action_reply_all"), QLatin1String("mail-reply-all"), tr("Reply to &All"), tr("Ctrl+Alt+Shift+R"));
    shortcutHandler->defineAction(QLatin1String("action_reply_list"), QLatin1String("mail-reply-list"), tr("Reply to &Mailing List"), tr("Ctrl+L"));
    shortcutHandler->defineAction(QLatin1String("action_reply_guess"), QString(), tr("Reply by &Guess"), tr("Ctrl+R"));
    shortcutHandler->defineAction(QLatin1String("action_contact_editor"), QLatin1String("contact-unknown"), tr("Address Book..."));
}

void MainWindow::createActions()
{
    // The shortcuts are a little bit complicated, unfortunately. This is what the other applications use by default:
    //
    // Thunderbird:
    // private: Ctrl+R
    // all: Ctrl+Shift+R
    // list: Ctrl+Shift+L
    // forward: Ctrl+L
    // (no shortcuts for type of forwarding)
    // bounce: ctrl+B
    // new message: Ctrl+N
    //
    // KMail:
    // "reply": R
    // private: Shift+A
    // all: A
    // list: L
    // forward as attachment: F
    // forward inline: Shift+F
    // bounce: E
    // new: Ctrl+N

    m_mainToolbar = addToolBar(tr("Navigation"));
    m_mainToolbar->setObjectName(QLatin1String("mainToolbar"));

    reloadMboxList = new QAction(style()->standardIcon(QStyle::SP_ArrowRight), tr("&Update List of Child Mailboxes"), this);
    connect(reloadMboxList, SIGNAL(triggered()), this, SLOT(slotReloadMboxList()));

    resyncMbox = new QAction(loadIcon(QLatin1String("view-refresh")), tr("Check for &New Messages"), this);
    connect(resyncMbox, SIGNAL(triggered()), this, SLOT(slotResyncMbox()));

    reloadAllMailboxes = new QAction(tr("&Reload Everything"), this);
    // connect later

    exitAction = ShortcutHandler::instance()->createAction(QLatin1String("action_application_exit"), qApp, SLOT(quit()), this);
    exitAction->setStatusTip(tr("Exit the application"));

    QActionGroup *netPolicyGroup = new QActionGroup(this);
    netPolicyGroup->setExclusive(true);
    netOffline = new QAction(loadIcon(QLatin1String("network-offline")), tr("&Offline"), netPolicyGroup);
    netOffline->setCheckable(true);
    // connect later
    netExpensive = new QAction(loadIcon(QLatin1String("network-expensive")), tr("&Expensive Connection"), netPolicyGroup);
    netExpensive->setCheckable(true);
    // connect later
    netOnline = new QAction(loadIcon(QLatin1String("network-online")), tr("&Free Access"), netPolicyGroup);
    netOnline->setCheckable(true);
    // connect later

    //: a debugging tool showing the full contents of the whole IMAP server; all folders, messages and their parts
    showFullView = new QAction(loadIcon(QLatin1String("edit-find-mail")), tr("Show Full &Tree Window"), this);
    showFullView->setCheckable(true);
    connect(showFullView, SIGNAL(triggered(bool)), allDock, SLOT(setVisible(bool)));
    connect(allDock, SIGNAL(visibilityChanged(bool)), showFullView, SLOT(setChecked(bool)));

    //: list of active "tasks", entities which are performing certain action like downloading a message or syncing a mailbox
    showTaskView = new QAction(tr("Show ImapTask t&ree"), this);
    showTaskView->setCheckable(true);
    connect(showTaskView, SIGNAL(triggered(bool)), taskDock, SLOT(setVisible(bool)));
    connect(taskDock, SIGNAL(visibilityChanged(bool)), showTaskView, SLOT(setChecked(bool)));

    showImapLogger = new QAction(tr("Show IMAP protocol &log"), this);
    showImapLogger->setCheckable(true);
    connect(showImapLogger, SIGNAL(toggled(bool)), imapLoggerDock, SLOT(setVisible(bool)));
    connect(imapLoggerDock, SIGNAL(visibilityChanged(bool)), showImapLogger, SLOT(setChecked(bool)));

    //: file to save the debug log into
    logPersistent = new QAction(tr("Log &into %1").arg(Imap::Mailbox::persistentLogFileName()), this);
    logPersistent->setCheckable(true);
    connect(logPersistent, SIGNAL(triggered(bool)), imapLogger, SLOT(slotSetPersistentLogging(bool)));

    showImapCapabilities = new QAction(tr("IMAP Server In&formation..."), this);
    connect(showImapCapabilities, SIGNAL(triggered()), this, SLOT(slotShowImapInfo()));

    showMenuBar = ShortcutHandler::instance()->createAction(QLatin1String("action_show_menubar"), this);
    showMenuBar->setCheckable(true);
    showMenuBar->setChecked(true);
    connect(showMenuBar, SIGNAL(triggered(bool)), menuBar(), SLOT(setVisible(bool)));
    connect(showMenuBar, SIGNAL(triggered(bool)), menuShow, SLOT(setHidden(bool)));
    connect(showMenuBar, SIGNAL(triggered(bool)), m_delayedStateSaving, SLOT(start()));
    connect(menuShow, SIGNAL(clicked()), showMenuBar , SLOT(trigger()));

    showToolBar = new QAction(tr("Show &Toolbar"), this);
    showToolBar->setCheckable(true);
    showToolBar->setChecked(true);
    connect(showToolBar, SIGNAL(triggered(bool)), m_mainToolbar, SLOT(setVisible(bool)));
    connect(m_mainToolbar, SIGNAL(visibilityChanged(bool)), showToolBar, SLOT(setChecked(bool)));
    connect(m_mainToolbar, SIGNAL(visibilityChanged(bool)), m_delayedStateSaving, SLOT(start()));

    configSettings = new QAction(loadIcon(QLatin1String("configure")),  tr("&Settings..."), this);
    connect(configSettings, SIGNAL(triggered()), this, SLOT(slotShowSettings()));

    QAction *triggerSearch = new QAction(this);
    addAction(triggerSearch);
    triggerSearch->setShortcut(QKeySequence(QLatin1String(":, =")));
    connect(triggerSearch, SIGNAL(triggered()), msgListWidget, SLOT(focusRawSearch()));

    triggerSearch = new QAction(this);
    addAction(triggerSearch);
    triggerSearch->setShortcut(QKeySequence(QLatin1String("/")));
    connect(triggerSearch, SIGNAL(triggered()), msgListWidget, SLOT(focusSearch()));

    m_oneAtTimeGoBack = new QAction(loadIcon(QLatin1String("go-previous")), tr("Navigate Back"), this);
    m_oneAtTimeGoBack->setShortcut(QKeySequence::Back);
    m_oneAtTimeGoBack->setEnabled(false);

    composeMail = ShortcutHandler::instance()->createAction("action_compose_mail", this, SLOT(slotComposeMail()), this);
    m_editDraft = ShortcutHandler::instance()->createAction("action_compose_draft", this, SLOT(slotEditDraft()), this);

    expunge = ShortcutHandler::instance()->createAction(QLatin1String("action_expunge"), this, SLOT(slotExpunge()), this);

    markAsRead = ShortcutHandler::instance()->createAction(QLatin1String("action_mark_as_read"), this);
    markAsRead->setCheckable(true);
    msgListWidget->tree->addAction(markAsRead);
    connect(markAsRead, SIGNAL(triggered(bool)), this, SLOT(handleMarkAsRead(bool)));

    m_nextMessage = ShortcutHandler::instance()->createAction(QLatin1String("action_go_to_next_unread"), this, SLOT(slotNextUnread()), this);
    msgListWidget->tree->addAction(m_nextMessage);
    m_messageWidget->messageView->addAction(m_nextMessage);

    m_previousMessage = ShortcutHandler::instance()->createAction(QLatin1String("action_go_to_previous_unread"), this, SLOT(slotPreviousUnread()), this);
    msgListWidget->tree->addAction(m_previousMessage);
    m_messageWidget->messageView->addAction(m_previousMessage);

    markAsDeleted = ShortcutHandler::instance()->createAction(QLatin1String("action_mark_as_deleted"), this);
    markAsDeleted->setCheckable(true);
    msgListWidget->tree->addAction(markAsDeleted);
    connect(markAsDeleted, SIGNAL(triggered(bool)), this, SLOT(handleMarkAsDeleted(bool)));

    saveWholeMessage = ShortcutHandler::instance()->createAction(QLatin1String("action_save_message_as"), this, SLOT(slotSaveCurrentMessageBody()), this);
    msgListWidget->tree->addAction(saveWholeMessage);

    viewMsgSource = ShortcutHandler::instance()->createAction(QLatin1String("action_view_message_source"), this, SLOT(slotViewMsgSource()), this);
    msgListWidget->tree->addAction(viewMsgSource);

    viewMsgHeaders = ShortcutHandler::instance()->createAction(QLatin1String("action_view_message_headers"), this, SLOT(slotViewMsgHeaders()), this);
    msgListWidget->tree->addAction(viewMsgHeaders);

    //: "mailbox" as a "folder of messages", not as a "mail account"
    createChildMailbox = new QAction(tr("Create &Child Mailbox..."), this);
    connect(createChildMailbox, SIGNAL(triggered()), this, SLOT(slotCreateMailboxBelowCurrent()));

    //: "mailbox" as a "folder of messages", not as a "mail account"
    createTopMailbox = new QAction(tr("Create &New Mailbox..."), this);
    connect(createTopMailbox, SIGNAL(triggered()), this, SLOT(slotCreateTopMailbox()));

    m_actionMarkMailboxAsRead = new QAction(tr("&Mark Mailbox as Read"), this);
    connect(m_actionMarkMailboxAsRead, SIGNAL(triggered()), this, SLOT(slotMarkCurrentMailboxRead()));

    //: "mailbox" as a "folder of messages", not as a "mail account"
    deleteCurrentMailbox = new QAction(tr("&Remove Mailbox"), this);
    connect(deleteCurrentMailbox, SIGNAL(triggered()), this, SLOT(slotDeleteCurrentMailbox()));

#ifdef XTUPLE_CONNECT
    xtIncludeMailboxInSync = new QAction(tr("&Synchronize with xTuple"), this);
    xtIncludeMailboxInSync->setCheckable(true);
    connect(xtIncludeMailboxInSync, SIGNAL(triggered()), this, SLOT(slotXtSyncCurrentMailbox()));
#endif

    m_replyPrivate = ShortcutHandler::instance()->createAction(QLatin1String("action_reply_private"), this, SLOT(slotReplyTo()), this);
    m_replyPrivate->setEnabled(false);

    m_replyAllButMe = ShortcutHandler::instance()->createAction(QLatin1String("action_reply_all_but_me"), this, SLOT(slotReplyAllButMe()), this);
    m_replyAllButMe->setEnabled(false);

    m_replyAll = ShortcutHandler::instance()->createAction(QLatin1String("action_reply_all"), this, SLOT(slotReplyAll()), this);
    m_replyAll->setEnabled(false);

    m_replyList = ShortcutHandler::instance()->createAction(QLatin1String("action_reply_list"), this, SLOT(slotReplyList()), this);
    m_replyList->setEnabled(false);

    m_replyGuess = ShortcutHandler::instance()->createAction(QLatin1String("action_reply_guess"), this, SLOT(slotReplyGuess()), this);
    m_replyGuess->setEnabled(true);

    actionThreadMsgList = new QAction(Gui::loadIcon(QLatin1String("mail-view-threaded")), tr("Show Messages in &Threads"), this);
    actionThreadMsgList->setCheckable(true);
    // This action is enabled/disabled by model's capabilities
    actionThreadMsgList->setEnabled(false);
    if (m_settings->value(Common::SettingsNames::guiMsgListShowThreading).toBool()) {
        actionThreadMsgList->setChecked(true);
        // The actual threading will be performed only when model updates its capabilities
    }
    connect(actionThreadMsgList, SIGNAL(triggered(bool)), this, SLOT(slotThreadMsgList()));

    QActionGroup *sortOrderGroup = new QActionGroup(this);
    m_actionSortAscending = new QAction(tr("&Ascending"), sortOrderGroup);
    m_actionSortAscending->setCheckable(true);
    m_actionSortAscending->setChecked(true);
    m_actionSortDescending = new QAction(tr("&Descending"), sortOrderGroup);
    m_actionSortDescending->setCheckable(true);
    connect(sortOrderGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotSortingPreferenceChanged()));

    QActionGroup *sortColumnGroup = new QActionGroup(this);
    m_actionSortNone = new QAction(tr("&No sorting"), sortColumnGroup);
    m_actionSortNone->setCheckable(true);
    m_actionSortThreading = new QAction(tr("Sorted by &Threading"), sortColumnGroup);
    m_actionSortThreading->setCheckable(true);
    m_actionSortByArrival = new QAction(tr("A&rrival"), sortColumnGroup);
    m_actionSortByArrival->setCheckable(true);
    m_actionSortByCc = new QAction(tr("&Cc (Carbon Copy)"), sortColumnGroup);
    m_actionSortByCc->setCheckable(true);
    m_actionSortByDate = new QAction(tr("Date from &Message Headers"), sortColumnGroup);
    m_actionSortByDate->setCheckable(true);
    m_actionSortByFrom = new QAction(tr("&From Address"), sortColumnGroup);
    m_actionSortByFrom->setCheckable(true);
    m_actionSortBySize = new QAction(tr("&Size"), sortColumnGroup);
    m_actionSortBySize->setCheckable(true);
    m_actionSortBySubject = new QAction(tr("S&ubject"), sortColumnGroup);
    m_actionSortBySubject->setCheckable(true);
    m_actionSortByTo = new QAction(tr("T&o Address"), sortColumnGroup);
    m_actionSortByTo->setCheckable(true);
    connect(sortColumnGroup, SIGNAL(triggered(QAction*)), this, SLOT(slotSortingPreferenceChanged()));
    slotSortingConfirmed(-1, Qt::AscendingOrder);

    actionHideRead = new QAction(tr("&Hide Read Messages"), this);
    actionHideRead->setCheckable(true);
    if (m_settings->value(Common::SettingsNames::guiMsgListHideRead).toBool()) {
        actionHideRead->setChecked(true);
        prettyMsgListModel->setHideRead(true);
    }
    connect(actionHideRead, SIGNAL(triggered(bool)), this, SLOT(slotHideRead()));

    QActionGroup *layoutGroup = new QActionGroup(this);
    m_actionLayoutCompact = new QAction(tr("&Compact"), layoutGroup);
    m_actionLayoutCompact->setCheckable(true);
    m_actionLayoutCompact->setChecked(true);
    connect(m_actionLayoutCompact, SIGNAL(triggered()), this, SLOT(slotLayoutCompact()));
    m_actionLayoutWide = new QAction(tr("&Wide"), layoutGroup);
    m_actionLayoutWide->setCheckable(true);
    connect(m_actionLayoutWide, SIGNAL(triggered()), this, SLOT(slotLayoutWide()));
    m_actionLayoutOneAtTime = new QAction(tr("&One At Time"), layoutGroup);
    m_actionLayoutOneAtTime->setCheckable(true);
    connect(m_actionLayoutOneAtTime, SIGNAL(triggered()), this, SLOT(slotLayoutOneAtTime()));


    m_actionShowOnlySubscribed = new QAction(tr("Show Only S&ubscribed Folders"), this);
    m_actionShowOnlySubscribed->setCheckable(true);
    m_actionShowOnlySubscribed->setEnabled(false);
    connect(m_actionShowOnlySubscribed, SIGNAL(toggled(bool)), this, SLOT(slotShowOnlySubscribed()));
    m_actionSubscribeMailbox = new QAction(tr("Su&bscribed"), this);
    m_actionSubscribeMailbox->setCheckable(true);
    m_actionSubscribeMailbox->setEnabled(false);
    connect(m_actionSubscribeMailbox, SIGNAL(triggered()), this, SLOT(slotSubscribeCurrentMailbox()));

    aboutTrojita = new QAction(trUtf8("&About Trojitá..."), this);
    connect(aboutTrojita, SIGNAL(triggered()), this, SLOT(slotShowAboutTrojita()));

    donateToTrojita = new QAction(tr("&Donate to the project"), this);
    connect(donateToTrojita, SIGNAL(triggered()), this, SLOT(slotDonateToTrojita()));

    connectModelActions();

    m_composeMenu = new QMenu(tr("Compose Mail"), this);
    m_composeMenu->addAction(composeMail);
    m_composeMenu->addAction(m_editDraft);
    m_composeButton = new QToolButton(this);
    m_composeButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_composeButton->setMenu(m_composeMenu);
    m_composeButton->setDefaultAction(composeMail);

    m_replyButton = new QToolButton(this);
    m_replyButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_replyMenu = new QMenu(m_replyButton);
    m_replyMenu->addAction(m_replyPrivate);
    m_replyMenu->addAction(m_replyAllButMe);
    m_replyMenu->addAction(m_replyAll);
    m_replyMenu->addAction(m_replyList);
    m_replyButton->setMenu(m_replyMenu);
    m_replyButton->setDefaultAction(m_replyPrivate);

    m_mainToolbar->addWidget(m_composeButton);
    m_mainToolbar->addWidget(m_replyButton);
    m_mainToolbar->addAction(expunge);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(markAsRead);
    m_mainToolbar->addAction(markAsDeleted);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(showMenuBar);
    m_mainToolbar->addAction(configSettings);

    updateMessageFlags();
}

void MainWindow::connectModelActions()
{
    connect(reloadAllMailboxes, SIGNAL(triggered()), model, SLOT(reloadMailboxList()));
    connect(netOffline, SIGNAL(triggered()), m_networkWatcher, SLOT(setNetworkOffline()));
    connect(netExpensive, SIGNAL(triggered()), m_networkWatcher, SLOT(setNetworkExpensive()));
    connect(netOnline, SIGNAL(triggered()), m_networkWatcher, SLOT(setNetworkOnline()));
}

void MainWindow::createMenus()
{
#define ADD_ACTION(MENU, ACTION) \
    MENU->addAction(ACTION); \
    addAction(ACTION);

    QMenu *imapMenu = menuBar()->addMenu(tr("&IMAP"));
    imapMenu->addMenu(m_composeMenu);
    QAction *actionContactEditor = ShortcutHandler::instance()->createAction(QLatin1String("action_contact_editor"), this, SLOT(invokeContactEditor()), this);
    ADD_ACTION(imapMenu, actionContactEditor);
    ADD_ACTION(imapMenu, m_replyGuess);
    ADD_ACTION(imapMenu, m_replyPrivate);
    ADD_ACTION(imapMenu, m_replyAll);
    ADD_ACTION(imapMenu, m_replyAllButMe);
    ADD_ACTION(imapMenu, m_replyList);
    ADD_ACTION(imapMenu, expunge);
    imapMenu->addSeparator()->setText(tr("Network Access"));
    QMenu *netPolicyMenu = imapMenu->addMenu(tr("&Network Access"));
    ADD_ACTION(netPolicyMenu, netOffline);
    ADD_ACTION(netPolicyMenu, netExpensive);
    ADD_ACTION(netPolicyMenu, netOnline);
    QMenu *debugMenu = imapMenu->addMenu(tr("&Debugging"));
    ADD_ACTION(debugMenu, showFullView);
    ADD_ACTION(debugMenu, showTaskView);
    ADD_ACTION(debugMenu, showImapLogger);
    ADD_ACTION(debugMenu, logPersistent);
    ADD_ACTION(debugMenu, showImapCapabilities);
    imapMenu->addSeparator();
    ADD_ACTION(imapMenu, configSettings);
    ADD_ACTION(imapMenu, ShortcutHandler::instance()->shortcutConfigAction());
    imapMenu->addSeparator();
    ADD_ACTION(imapMenu, exitAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    ADD_ACTION(viewMenu, showMenuBar);
    ADD_ACTION(viewMenu, showToolBar);
    QMenu *layoutMenu = viewMenu->addMenu(tr("&Layout"));
    ADD_ACTION(layoutMenu, m_actionLayoutCompact);
    ADD_ACTION(layoutMenu, m_actionLayoutWide);
    ADD_ACTION(layoutMenu, m_actionLayoutOneAtTime);
    viewMenu->addSeparator();
    ADD_ACTION(viewMenu, m_previousMessage);
    ADD_ACTION(viewMenu, m_nextMessage);
    viewMenu->addSeparator();
    QMenu *sortMenu = viewMenu->addMenu(tr("S&orting"));
    ADD_ACTION(sortMenu, m_actionSortNone);
    ADD_ACTION(sortMenu, m_actionSortThreading);
    ADD_ACTION(sortMenu, m_actionSortByArrival);
    ADD_ACTION(sortMenu, m_actionSortByCc);
    ADD_ACTION(sortMenu, m_actionSortByDate);
    ADD_ACTION(sortMenu, m_actionSortByFrom);
    ADD_ACTION(sortMenu, m_actionSortBySize);
    ADD_ACTION(sortMenu, m_actionSortBySubject);
    ADD_ACTION(sortMenu, m_actionSortByTo);
    sortMenu->addSeparator();
    ADD_ACTION(sortMenu, m_actionSortAscending);
    ADD_ACTION(sortMenu, m_actionSortDescending);

    ADD_ACTION(viewMenu, actionThreadMsgList);
    ADD_ACTION(viewMenu, actionHideRead);
    ADD_ACTION(viewMenu, m_actionShowOnlySubscribed);

    QMenu *mailboxMenu = menuBar()->addMenu(tr("&Mailbox"));
    ADD_ACTION(mailboxMenu, resyncMbox);
    mailboxMenu->addSeparator();
    ADD_ACTION(mailboxMenu, reloadAllMailboxes);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    ADD_ACTION(helpMenu, donateToTrojita);
    helpMenu->addSeparator();
    ADD_ACTION(helpMenu, aboutTrojita);

    networkIndicator->setMenu(netPolicyMenu);
    networkIndicator->setDefaultAction(netOnline);

#undef ADD_ACTION
}

void MainWindow::createWidgets()
{
    // The state of the GUI is only saved after a certain time has passed. This is just an optimization to make sure
    // we do not hit the disk continually when e.g. resizing some random widget.
    m_delayedStateSaving = new QTimer(this);
    m_delayedStateSaving->setInterval(1000);
    m_delayedStateSaving->setSingleShot(true);
    connect(m_delayedStateSaving, SIGNAL(timeout()), this, SLOT(saveSizesAndState()));

    mboxTree = new MailBoxTreeView();
    connect(mboxTree, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(showContextMenuMboxTree(const QPoint &)));

    msgListWidget = new MessageListWidget();
    msgListWidget->tree->setContextMenuPolicy(Qt::CustomContextMenu);
    msgListWidget->tree->setAlternatingRowColors(true);
    msgListWidget->setRawSearchEnabled(m_settings->value(Common::SettingsNames::guiAllowRawSearch).toBool());
    connect (msgListWidget, SIGNAL(rawSearchSettingChanged(bool)), SLOT(saveRawStateSetting(bool)));

    connect(msgListWidget->tree, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(showContextMenuMsgListTree(const QPoint &)));
    connect(msgListWidget->tree, SIGNAL(activated(const QModelIndex &)), this, SLOT(msgListClicked(const QModelIndex &)));
    connect(msgListWidget->tree, SIGNAL(clicked(const QModelIndex &)), this, SLOT(msgListClicked(const QModelIndex &)));
    connect(msgListWidget->tree, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(msgListDoubleClicked(const QModelIndex &)));
    connect(msgListWidget, SIGNAL(requestingSearch(QStringList)), this, SLOT(slotSearchRequested(QStringList)));
    connect(msgListWidget->tree->header(), SIGNAL(sectionMoved(int,int,int)), m_delayedStateSaving, SLOT(start()));
    connect(msgListWidget->tree->header(), SIGNAL(sectionResized(int,int,int)), m_delayedStateSaving, SLOT(start()));

    m_messageWidget = new CompleteMessageWidget(this, m_settings);
    connect(m_messageWidget->messageView, SIGNAL(messageChanged()), this, SLOT(scrollMessageUp()));
    connect(m_messageWidget->messageView, SIGNAL(messageChanged()), this, SLOT(slotUpdateMessageActions()));
    connect(m_messageWidget->messageView, SIGNAL(linkHovered(QString)), this, SLOT(slotShowLinkTarget(QString)));
    connect(m_messageWidget->messageView, SIGNAL(addressDetailsRequested(QString,QStringList&)),
            this, SLOT(fillMatchingAbookEntries(QString,QStringList&)));
    connect(m_messageWidget->messageView, SIGNAL(transferError(QString)), this, SLOT(slotDownloadTransferError(QString)));
    if (m_settings->value(Common::SettingsNames::appLoadHomepage, QVariant(true)).toBool() &&
        !m_settings->value(Common::SettingsNames::imapStartOffline).toBool()) {
        m_messageWidget->messageView->setHomepageUrl(QUrl(QString::fromUtf8("http://welcome.trojita.flaska.net/%1").arg(Common::Application::version)));
    }

    allDock = new QDockWidget("Everything", this);
    allDock->setObjectName(QLatin1String("allDock"));
    allTree = new QTreeView(allDock);
    allDock->hide();
    allTree->setUniformRowHeights(true);
    allTree->setHeaderHidden(true);
    allDock->setWidget(allTree);
    addDockWidget(Qt::LeftDockWidgetArea, allDock);
    taskDock = new QDockWidget("IMAP Tasks", this);
    taskDock->setObjectName("taskDock");
    taskTree = new QTreeView(taskDock);
    taskDock->hide();
    taskTree->setHeaderHidden(true);
    taskDock->setWidget(taskTree);
    addDockWidget(Qt::LeftDockWidgetArea, taskDock);

    imapLoggerDock = new QDockWidget(tr("IMAP Protocol"), this);
    imapLoggerDock->setObjectName(QLatin1String("imapLoggerDock"));
    imapLogger = new ProtocolLoggerWidget(imapLoggerDock);
    imapLoggerDock->hide();
    imapLoggerDock->setWidget(imapLogger);
    addDockWidget(Qt::BottomDockWidgetArea, imapLoggerDock);

    busyParsersIndicator = new TaskProgressIndicator(this);
    statusBar()->addPermanentWidget(busyParsersIndicator);
    busyParsersIndicator->hide();

    menuShow = new QToolButton(this);
    menuShow->setText(tr("Show Menu Bar"));
    statusBar()->addPermanentWidget(menuShow);
    menuShow->hide();

    networkIndicator = new QToolButton(this);
    networkIndicator->setPopupMode(QToolButton::InstantPopup);
    statusBar()->addPermanentWidget(networkIndicator);
}

void MainWindow::setupModels()
{
    Imap::Mailbox::SocketFactoryPtr factory;
    Imap::Mailbox::TaskFactoryPtr taskFactory(new Imap::Mailbox::TaskFactory());

    using Common::SettingsNames;
    if (m_settings->value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodTCP) {
        factory.reset(new Streams::TlsAbleSocketFactory(
                          m_settings->value(SettingsNames::imapHostKey).toString(),
                          m_settings->value(SettingsNames::imapPortKey, QString::number(Common::PORT_IMAP)).toUInt()));
        factory->setStartTlsRequired(m_settings->value(SettingsNames::imapStartTlsKey, true).toBool());
    } else if (m_settings->value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodSSL) {
        factory.reset(new Streams::SslSocketFactory(
                          m_settings->value(SettingsNames::imapHostKey).toString(),
                          m_settings->value(SettingsNames::imapPortKey, QString::number(Common::PORT_IMAPS)).toUInt()));
    } else if (m_settings->value(SettingsNames::imapMethodKey).toString() == SettingsNames::methodProcess) {
        QStringList args = m_settings->value(SettingsNames::imapProcessKey).toString().split(QLatin1Char(' '));
        if (args.isEmpty()) {
            // it's going to fail anyway
            args << QLatin1String("");
        }
        QString appName = args.takeFirst();
        factory.reset(new Streams::ProcessSocketFactory(appName, args));
    } else {
        factory.reset(new Streams::FakeSocketFactory(Imap::CONN_STATE_LOGOUT));
    }

    QString cacheDir = Common::writablePath(Common::LOCATION_CACHE);
    Imap::Mailbox::AbstractCache *cache = 0;

    bool shouldUsePersistentCache = m_settings->value(SettingsNames::cacheOfflineKey).toString() != SettingsNames::cacheOfflineNone;
    if (shouldUsePersistentCache) {
        if (! QDir().mkpath(cacheDir)) {
            QMessageBox::critical(this, tr("Cache Error"), tr("Failed to create directory %1").arg(cacheDir));
            shouldUsePersistentCache = false;
        }
        QFile::Permissions expectedPerms = QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner;
        if (QFileInfo(cacheDir).permissions() != expectedPerms) {
            if (!QFile::setPermissions(cacheDir, expectedPerms)) {
#ifndef Q_OS_WIN32
                QMessageBox::critical(this, tr("Cache Error"), tr("Failed to set safe permissions on cache directory %1").arg(cacheDir));
                shouldUsePersistentCache = false;
#endif
            }
        }
    }

    //setProperty( "trojita-sqlcache-commit-period", QVariant(5000) );
    //setProperty( "trojita-sqlcache-commit-delay", QVariant(1000) );

    if (! shouldUsePersistentCache) {
        cache = new Imap::Mailbox::MemoryCache(this);
    } else {
        cache = new Imap::Mailbox::CombinedCache(this, QLatin1String("trojita-imap-cache"), cacheDir);
        connect(cache, SIGNAL(error(QString)), this, SLOT(cacheError(QString)));
        if (! static_cast<Imap::Mailbox::CombinedCache *>(cache)->open()) {
            // Error message was already shown by the cacheError() slot
            cache->deleteLater();
            cache = new Imap::Mailbox::MemoryCache(this);
        } else {
            if (m_settings->value(SettingsNames::cacheOfflineKey).toString() == SettingsNames::cacheOfflineAll) {
                cache->setRenewalThreshold(0);
            } else {
                bool ok;
                int num = m_settings->value(SettingsNames::cacheOfflineNumberDaysKey, 30).toInt(&ok);
                if (!ok)
                    num = 30;
                cache->setRenewalThreshold(num);
            }
        }
    }
    model = new Imap::Mailbox::Model(this, cache, std::move(factory), std::move(taskFactory));
    m_networkWatcher = new Imap::Mailbox::NetworkWatcher(this, model);
    QMetaObject::invokeMethod(m_networkWatcher,
                              m_settings->value(SettingsNames::imapStartOffline).toBool() ?
                                  "setNetworkOffline" : "setNetworkOnline",
                              Qt::QueuedConnection);
    model->setObjectName(QLatin1String("model"));
    model->setCapabilitiesBlacklist(m_settings->value(SettingsNames::imapBlacklistedCapabilities).toStringList());
    if (m_settings->value(SettingsNames::imapEnableId, true).toBool()) {
        model->setProperty("trojita-imap-enable-id", true);
    }
    mboxModel = new Imap::Mailbox::MailboxModel(this, model);
    mboxModel->setObjectName(QLatin1String("mboxModel"));
    prettyMboxModel = new Imap::Mailbox::PrettyMailboxModel(this, mboxModel);
    prettyMboxModel->setObjectName(QLatin1String("prettyMboxModel"));
    msgListModel = new Imap::Mailbox::MsgListModel(this, model);
    msgListModel->setObjectName(QLatin1String("msgListModel"));
    threadingMsgListModel = new Imap::Mailbox::ThreadingMsgListModel(this);
    threadingMsgListModel->setObjectName(QLatin1String("threadingMsgListModel"));
    threadingMsgListModel->setSourceModel(msgListModel);
    connect(threadingMsgListModel, SIGNAL(sortingFailed()), msgListWidget, SLOT(slotSortingFailed()));
    prettyMsgListModel = new Imap::Mailbox::PrettyMsgListModel(this);
    prettyMsgListModel->setSourceModel(threadingMsgListModel);
    prettyMsgListModel->setObjectName(QLatin1String("prettyMsgListModel"));

    connect(mboxTree, SIGNAL(clicked(const QModelIndex &)), msgListModel, SLOT(setMailbox(const QModelIndex &)));
    connect(mboxTree, SIGNAL(activated(const QModelIndex &)), msgListModel, SLOT(setMailbox(const QModelIndex &)));
    connect(msgListModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateMessageFlags()));
    connect(msgListModel, SIGNAL(messagesAvailable()), msgListWidget->tree, SLOT(scrollToBottom()));
    connect(msgListModel, SIGNAL(rowsInserted(QModelIndex,int,int)), msgListWidget, SLOT(slotAutoEnableDisableSearch()));
    connect(msgListModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), msgListWidget, SLOT(slotAutoEnableDisableSearch()));
    connect(msgListModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(updateMessageFlags()));
    connect(msgListModel, SIGNAL(layoutChanged()), msgListWidget, SLOT(slotAutoEnableDisableSearch()));
    connect(msgListModel, SIGNAL(layoutChanged()), this, SLOT(updateMessageFlags()));
    connect(msgListModel, SIGNAL(modelReset()), msgListWidget, SLOT(slotAutoEnableDisableSearch()));
    connect(msgListModel, SIGNAL(modelReset()), this, SLOT(updateMessageFlags()));
    connect(msgListModel, SIGNAL(mailboxChanged(QModelIndex)), this, SLOT(slotMailboxChanged(QModelIndex)));

    connect(model, SIGNAL(alertReceived(const QString &)), this, SLOT(alertReceived(const QString &)));
    connect(model, SIGNAL(connectionError(const QString &)), this, SLOT(connectionError(const QString &)));
    connect(model, SIGNAL(authRequested()), this, SLOT(authenticationRequested()), Qt::QueuedConnection);
    connect(model, SIGNAL(authAttemptFailed(QString)), this, SLOT(authenticationFailed(QString)));
    connect(model, SIGNAL(needsSslDecision(QList<QSslCertificate>,QList<QSslError>)),
            this, SLOT(sslErrors(QList<QSslCertificate>,QList<QSslError>)), Qt::QueuedConnection);
    connect(model, SIGNAL(requireStartTlsInFuture()), this, SLOT(requireStartTlsInFuture()));

    connect(model, SIGNAL(networkPolicyOffline()), this, SLOT(networkPolicyOffline()));
    connect(model, SIGNAL(networkPolicyExpensive()), this, SLOT(networkPolicyExpensive()));
    connect(model, SIGNAL(networkPolicyOnline()), this, SLOT(networkPolicyOnline()));

    connect(model, SIGNAL(connectionStateChanged(QObject *,Imap::ConnectionState)),
            this, SLOT(showConnectionStatus(QObject *,Imap::ConnectionState)));

    connect(model, SIGNAL(mailboxDeletionFailed(QString,QString)), this, SLOT(slotMailboxDeleteFailed(QString,QString)));
    connect(model, SIGNAL(mailboxCreationFailed(QString,QString)), this, SLOT(slotMailboxCreateFailed(QString,QString)));

    connect(model, SIGNAL(logged(uint,Common::LogMessage)), imapLogger, SLOT(slotImapLogged(uint,Common::LogMessage)));

    connect(model, SIGNAL(mailboxFirstUnseenMessage(QModelIndex,QModelIndex)), this, SLOT(slotScrollToUnseenMessage(QModelIndex,QModelIndex)));

    connect(model, SIGNAL(capabilitiesUpdated(QStringList)), this, SLOT(slotCapabilitiesUpdated(QStringList)));

    connect(msgListModel, SIGNAL(modelReset()), this, SLOT(slotUpdateWindowTitle()));
    connect(model, SIGNAL(messageCountPossiblyChanged(QModelIndex)), this, SLOT(slotUpdateWindowTitle()));

    connect(prettyMsgListModel, SIGNAL(sortingPreferenceChanged(int,Qt::SortOrder)), this, SLOT(slotSortingConfirmed(int,Qt::SortOrder)));

    //Imap::Mailbox::ModelWatcher* w = new Imap::Mailbox::ModelWatcher( this );
    //w->setModel( model );

    //ModelTest* tester = new ModelTest( prettyMboxModel, this ); // when testing, test just one model at time

    mboxTree->setModel(prettyMboxModel);
    msgListWidget->tree->setModel(prettyMsgListModel);

    allTree->setModel(model);
    taskTree->setModel(model->taskModel());
    connect(model->taskModel(), SIGNAL(layoutChanged()), taskTree, SLOT(expandAll()));
    connect(model->taskModel(), SIGNAL(modelReset()), taskTree, SLOT(expandAll()));
    connect(model->taskModel(), SIGNAL(rowsInserted(QModelIndex,int,int)), taskTree, SLOT(expandAll()));
    connect(model->taskModel(), SIGNAL(rowsRemoved(QModelIndex,int,int)), taskTree, SLOT(expandAll()));
    connect(model->taskModel(), SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)), taskTree, SLOT(expandAll()));

    busyParsersIndicator->setImapModel(model);
}

void MainWindow::createSysTray()
{
    if (m_trayIcon)
        return;

    qApp->setQuitOnLastWindowClosed(false);

    m_trayIcon = new QSystemTrayIcon(this);
    handleTrayIconChange();

    QAction* quitAction = new QAction(tr("&Quit"), m_trayIcon);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

    QMenu *trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(quitAction);
    m_trayIcon->setContextMenu(trayIconMenu);

    // QMenu cannot be a child of QSystemTrayIcon, and we don't want the QMenu in MainWindow scope.
    connect(m_trayIcon, SIGNAL(destroyed()), trayIconMenu, SLOT(deleteLater()));

    connect(m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(slotIconActivated(QSystemTrayIcon::ActivationReason)));
    connect(model, SIGNAL(messageCountPossiblyChanged(QModelIndex)), this, SLOT(handleTrayIconChange()));
    m_trayIcon->setVisible(true);
    m_trayIcon->show();
}

void MainWindow::removeSysTray()
{
    delete m_trayIcon;
    m_trayIcon = 0;

    qApp->setQuitOnLastWindowClosed(true);
}

void MainWindow::slotToggleSysTray()
{
    bool showSystray = m_settings->value(Common::SettingsNames::guiShowSystray, QVariant(true)).toBool();
    if (showSystray && !m_trayIcon) {
        createSysTray();
    } else if (!showSystray && m_trayIcon) {
        removeSysTray();
    }
}

void MainWindow::handleTrayIconChange()
{
    QModelIndex mailbox = model->index(1, 0, QModelIndex());

    if (mailbox.isValid()) {
        Q_ASSERT(mailbox.data(Imap::Mailbox::RoleMailboxName).toString() == QLatin1String("INBOX"));
        QPixmap pixmap = QPixmap(QLatin1String(":/icons/trojita.png"));
        if (mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt() > 0) {
            QPainter painter(&pixmap);
            QFont f;
            f.setPixelSize(pixmap.height() * 0.59 );
            f.setWeight(QFont::Bold);

            QString text = mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toString();
            QFontMetrics fm(f);
            if (mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toUInt() > 666) {
                // You just have too many messages.
                text = QString::fromUtf8("🐮");
                fm = QFontMetrics(f);
            } else if (fm.width(text) > pixmap.width()) {
                f.setPixelSize(f.pixelSize() * pixmap.width() / fm.width(text));
                fm = QFontMetrics(f);
            }
            painter.setFont(f);

            QRect boundingRect = fm.tightBoundingRect(text);
            boundingRect.setWidth(boundingRect.width() + 2);
            boundingRect.setHeight(boundingRect.height() + 2);
            boundingRect.moveCenter(QPoint(pixmap.width() / 2, pixmap.height() / 2));
            boundingRect = boundingRect.intersected(pixmap.rect());
            painter.setBrush(Qt::white);
            painter.setPen(Qt::white);
            painter.setOpacity(0.7);
            painter.drawRoundedRect(boundingRect, 2.0, 2.0);

            painter.setOpacity(1.0);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(Qt::darkBlue);
            painter.drawText(boundingRect, Qt::AlignCenter, text);
            m_trayIcon->setToolTip(trUtf8("Trojitá - %n unread message(s)", 0, mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt()));
            m_trayIcon->setIcon(QIcon(pixmap));
            return;
        }
    }
    m_trayIcon->setToolTip(trUtf8("Trojitá"));
    m_trayIcon->setIcon(QIcon(QLatin1String(":/icons/trojita.png")));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        Util::askForSomethingUnlessTold(trUtf8("Trojitá"),
                                        tr("The application will continue in systray. This can be disabled within the settings."),
                                        Common::SettingsNames::guiOnSystrayClose, QMessageBox::Ok, this, m_settings);
        hide();
        event->ignore();
    }
}

bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
    if (msgListWidget && msgListWidget->tree && o == msgListWidget->tree->header()->viewport()) {
        // installed if sorting is not really possible.
        QWidget *header = static_cast<QWidget*>(o);
        QMouseEvent *mouse = static_cast<QMouseEvent*>(e);
        if (e->type() == QEvent::MouseButtonPress) {
            if (mouse->button() == Qt::LeftButton && header->cursor().shape() == Qt::ArrowCursor) {
                m_headerDragStart = mouse->pos();
            }
            return false;
        }
        if (e->type() == QEvent::MouseButtonRelease) {
            if (mouse->button() == Qt::LeftButton && header->cursor().shape() == Qt::ArrowCursor &&
               (m_headerDragStart - mouse->pos()).manhattanLength() < QApplication::startDragDistance()) {
                    m_actionSortDescending->toggle();
                    Qt::SortOrder order = m_actionSortDescending->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder;
                    msgListWidget->tree->header()->setSortIndicator(-1, order);
                    return true; // prevent regular click
            }
        }
    }
    return false;
}

void MainWindow::slotIconActivated(const QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        setVisible(!isVisible());
        if (isVisible()) {
            activateWindow();
            raise();
        }
    }
 }

void MainWindow::msgListClicked(const QModelIndex &index)
{
    Q_ASSERT(index.isValid());

    if (qApp->keyboardModifiers() & Qt::ShiftModifier || qApp->keyboardModifiers() & Qt::ControlModifier)
        return;

    if (! index.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;

    // Be sure to update the toolbar/actions with the state of the current message
    updateMessageFlags();

    // Because it's quite possible that we have switched into another mailbox, make sure that we're in the "current" one so that
    // user will be notified about new arrivals, etc.
    QModelIndex translated = Imap::deproxifiedIndex(index);
    model->switchToMailbox(translated.parent().parent());

    if (index.column() == Imap::Mailbox::MsgListModel::SEEN) {
        if (!translated.data(Imap::Mailbox::RoleIsFetched).toBool())
            return;
        Imap::Mailbox::FlagsOperation flagOp = translated.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() ?
                                               Imap::Mailbox::FLAG_REMOVE : Imap::Mailbox::FLAG_ADD;
        model->markMessagesRead(QModelIndexList() << translated, flagOp);

        if (translated == m_messageWidget->messageView->currentMessage()) {
            m_messageWidget->messageView->stopAutoMarkAsRead();
        }
    } else {
        if (m_messageWidget->isVisible() && !m_messageWidget->size().isEmpty()) {
            // isVisible() won't work, the splitter manipulates width, not the visibility state
            m_messageWidget->messageView->setMessage(index);
        }
        msgListWidget->tree->setCurrentIndex(index);
    }
}

void MainWindow::msgListDoubleClicked(const QModelIndex &index)
{
    Q_ASSERT(index.isValid());

    if (! index.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;

    CompleteMessageWidget *widget = new CompleteMessageWidget(0, m_settings);
    connect(widget->messageView, SIGNAL(addressDetailsRequested(QString,QStringList&)),
            this, SLOT(fillMatchingAbookEntries(QString,QStringList&)));
    widget->messageView->setMessage(index);
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->setWindowTitle(index.data(Imap::Mailbox::RoleMessageSubject).toString());
    widget->setAttribute(Qt::WA_DeleteOnClose);
    widget->resize(800, 600);
    widget->show();
}

void MainWindow::showContextMenuMboxTree(const QPoint &position)
{
    QList<QAction *> actionList;
    if (mboxTree->indexAt(position).isValid()) {
        actionList.append(createChildMailbox);
        actionList.append(deleteCurrentMailbox);
        actionList.append(m_actionMarkMailboxAsRead);
        actionList.append(resyncMbox);
        actionList.append(reloadMboxList);

        actionList.append(m_actionSubscribeMailbox);
        m_actionSubscribeMailbox->setChecked(mboxTree->indexAt(position).data(Imap::Mailbox::RoleMailboxIsSubscribed).toBool());

#ifdef XTUPLE_CONNECT
        actionList.append(xtIncludeMailboxInSync);
        xtIncludeMailboxInSync->setChecked(
            m_settings->value(Common::SettingsNames::xtSyncMailboxList).toStringList().contains(
                mboxTree->indexAt(position).data(Imap::Mailbox::RoleMailboxName).toString()));
#endif
    } else {
        actionList.append(createTopMailbox);
    }
    actionList.append(reloadAllMailboxes);
    actionList.append(m_actionShowOnlySubscribed);
    QMenu::exec(actionList, mboxTree->mapToGlobal(position));
}

void MainWindow::showContextMenuMsgListTree(const QPoint &position)
{
    QList<QAction *> actionList;
    QModelIndex index = msgListWidget->tree->indexAt(position);
    if (index.isValid()) {
        updateMessageFlags(index);
        actionList.append(markAsRead);
        actionList.append(markAsDeleted);
        actionList.append(m_actionMarkMailboxAsRead);
        actionList.append(saveWholeMessage);
        actionList.append(viewMsgSource);
        actionList.append(viewMsgHeaders);
    }
    if (! actionList.isEmpty())
        QMenu::exec(actionList, msgListWidget->tree->mapToGlobal(position));
}

/** @short Ask for an updated list of mailboxes situated below the selected one

*/
void MainWindow::slotReloadMboxList()
{
    Q_FOREACH(const QModelIndex &item, mboxTree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        Imap::Mailbox::TreeItemMailbox *mbox = dynamic_cast<Imap::Mailbox::TreeItemMailbox *>(
                Imap::Mailbox::Model::realTreeItem(item)
                                               );
        Q_ASSERT(mbox);
        mbox->rescanForChildMailboxes(model);
    }
}

/** @short Request a check for new messages in selected mailbox */
void MainWindow::slotResyncMbox()
{
    if (! model->isNetworkAvailable())
        return;

    Q_FOREACH(const QModelIndex &item, mboxTree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        model->resyncMailbox(item);
    }
}

void MainWindow::alertReceived(const QString &message)
{
    //: "ALERT" is a special warning which we're required to show to the user
    QMessageBox::warning(this, tr("IMAP Alert"), message);
}

void MainWindow::connectionError(const QString &message)
{
    if (m_settings->contains(Common::SettingsNames::imapMethodKey)) {
        QMessageBox::critical(this, tr("Connection Error"), message);
        // Show the IMAP logger -- maybe some user will take that as a hint that they shall include it in the bug report.
        // </joke>
        showImapLogger->setChecked(true);
    } else {
        // hack: this slot is called even on the first run with no configuration
        // We shouldn't have to worry about that, since the dialog is already scheduled for calling
        // -> do nothing
    }
}

void MainWindow::cacheError(const QString &message)
{
    QMessageBox::critical(this, tr("IMAP Cache Error"),
                          tr("The caching subsystem managing a cache of the data already "
                             "downloaded from the IMAP server is having troubles. "
                             "All caching will be disabled.\n\n%1").arg(message));
    if (model)
        model->setCache(new Imap::Mailbox::MemoryCache(model));
}

void MainWindow::networkPolicyOffline()
{
    netOffline->setChecked(true);
    netExpensive->setChecked(false);
    netOnline->setChecked(false);
    updateActionsOnlineOffline(false);
    networkIndicator->setDefaultAction(netOffline);
    statusBar()->showMessage(tr("Offline"), 0);
}

void MainWindow::networkPolicyExpensive()
{
    netOffline->setChecked(false);
    netExpensive->setChecked(true);
    netOnline->setChecked(false);
    updateActionsOnlineOffline(true);
    networkIndicator->setDefaultAction(netExpensive);
}

void MainWindow::networkPolicyOnline()
{
    netOffline->setChecked(false);
    netExpensive->setChecked(false);
    netOnline->setChecked(true);
    updateActionsOnlineOffline(true);
    networkIndicator->setDefaultAction(netOnline);
}

void MainWindow::slotShowSettings()
{
    SettingsDialog *dialog = new SettingsDialog(this, m_senderIdentities, m_settings);
    if (dialog->exec() == QDialog::Accepted) {
        // FIXME: wipe cache in case we're moving between servers
        nukeModels();
        setupModels();
        connectModelActions();
        // The systray is still connected to the old model -- got to make sure it's getting updated
        removeSysTray();
        slotToggleSysTray();
    }
    QString method = m_settings->value(Common::SettingsNames::imapMethodKey).toString();
    if (method != Common::SettingsNames::methodTCP && method != Common::SettingsNames::methodSSL &&
            method != Common::SettingsNames::methodProcess ) {
        QMessageBox::critical(this, tr("No Configuration"),
                              trUtf8("No IMAP account is configured. Trojitá cannot do much without one."));
    }
    applySizesAndState();
}

void MainWindow::authenticationRequested()
{
    QString user = m_settings->value(Common::SettingsNames::imapUserKey).toString();
    QString pass = m_settings->value(Common::SettingsNames::imapPassKey).toString();
    if (m_ignoreStoredPassword || pass.isEmpty()) {
        bool ok;
        pass = PasswordDialog::getPassword(this, tr("Authentication Required"),
                                           tr("<p>Please provide IMAP password for user <b>%1</b> on <b>%2</b>:</p>").arg(
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                                               Qt::escape(user),
                                               Qt::escape(m_settings->value(Common::SettingsNames::imapHostKey).toString())
#else
                                               user.toHtmlEscaped(),
                                               m_settings->value(Common::SettingsNames::imapHostKey).toString().toHtmlEscaped()
#endif
                                               ),
                                           QString(), &ok);
        if (ok) {
            model->setImapUser(user);
            model->setImapPassword(pass);
        } else {
            model->unsetImapPassword();
        }
    } else {
        model->setImapUser(user);
        model->setImapPassword(pass);
    }
}

void MainWindow::authenticationFailed(const QString &message)
{
    m_ignoreStoredPassword = true;
    QMessageBox::warning(this, tr("Login Failed"), message);
}

void MainWindow::sslErrors(const QList<QSslCertificate> &certificateChain, const QList<QSslError> &errors)
{
    QByteArray lastKnownPubKey = m_settings->value(Common::SettingsNames::imapSslPemPubKey).toByteArray();
    if (!certificateChain.isEmpty() && !lastKnownPubKey.isEmpty() && lastKnownPubKey == certificateChain[0].publicKey().toPem()) {
        // This certificate chain contains the same public keys as the last time; we should accept that
        model->setSslPolicy(certificateChain, errors, true);
        return;
    }

    QString message;
    QString title;
    Imap::Mailbox::CertificateUtils::IconType icon;
    Imap::Mailbox::CertificateUtils::formatSslState(certificateChain, lastKnownPubKey, errors, &title, &message, &icon);

    if (QMessageBox(static_cast<QMessageBox::Icon>(icon), title, message, QMessageBox::Yes | QMessageBox::No, this).exec() == QMessageBox::Yes) {
        if (!certificateChain.isEmpty()) {
            QByteArray buf;
            Q_FOREACH(const QSslCertificate &cert, certificateChain) {
                buf.append(cert.toPem());
            }
            m_settings->setValue(Common::SettingsNames::imapSslPemPubKey, certificateChain[0].publicKey().toPem());
#ifdef XTUPLE_CONNECT
            QSettings xtSettings(QSettings::UserScope, QString::fromAscii("xTuple.com"), QString::fromAscii("xTuple"));
            xtSettings.setValue(Common::SettingsNames::imapSslPemPubKey, certificateChain[0].publicKey().toPem());
#endif
        }
        model->setSslPolicy(certificateChain, errors, true);
    } else {
        model->setSslPolicy(certificateChain, errors, false);
    }
}

void MainWindow::requireStartTlsInFuture()
{
    m_settings->setValue(Common::SettingsNames::imapStartTlsKey, true);
}

void MainWindow::nukeModels()
{
    m_networkWatcher->setNetworkOffline();
    m_messageWidget->messageView->setEmpty();
    mboxTree->setModel(0);
    msgListWidget->tree->setModel(0);
    allTree->setModel(0);
    taskTree->setModel(0);
    delete prettyMsgListModel;
    prettyMsgListModel = 0;
    delete threadingMsgListModel;
    threadingMsgListModel = 0;
    delete msgListModel;
    msgListModel = 0;
    delete mboxModel;
    mboxModel = 0;
    delete prettyMboxModel;
    prettyMboxModel = 0;
    delete model;
    model = 0;
}

void MainWindow::recoverDrafts()
{
    QDir draftPath(Common::writablePath(Common::LOCATION_CACHE) + QLatin1String("Drafts/"));
    QStringList drafts(draftPath.entryList(QStringList() << QLatin1String("*.draft")));
    Q_FOREACH(const QString &draft, drafts) {
        ComposeWidget *cw = invokeComposeDialog();
        if (cw)
            cw->loadDraft(draftPath.filePath(draft));
    }
}

void MainWindow::slotComposeMail()
{
    invokeComposeDialog();
}

void MainWindow::slotEditDraft()
{
    QString path(Common::writablePath(Common::LOCATION_DATA) + tr("Drafts"));
    QDir().mkpath(path);
    path = QFileDialog::getOpenFileName(this, tr("Edit draft"), path, tr("Drafts") + QLatin1String(" (*.draft)"));
    if (!path.isEmpty()) {
        ComposeWidget *cw = invokeComposeDialog();
        if (cw)
            cw->loadDraft(path);
    }
}

void MainWindow::handleMarkAsRead(bool value)
{
    QModelIndexList translatedIndexes;
    Q_FOREACH(const QModelIndex &item, msgListWidget->tree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        if (!item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;
        translatedIndexes << Imap::deproxifiedIndex(item);
    }
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsRead: no valid messages";
    } else {
        if (value)
            model->markMessagesRead(translatedIndexes, Imap::Mailbox::FLAG_ADD);
        else
            model->markMessagesRead(translatedIndexes, Imap::Mailbox::FLAG_REMOVE);
        if (translatedIndexes.contains(m_messageWidget->messageView->currentMessage())) {
            m_messageWidget->messageView->stopAutoMarkAsRead();
        }
    }
}

void MainWindow::slotNextUnread()
{
    QModelIndex current = msgListWidget->tree->currentIndex();

    bool wrapped = false;
    while (current.isValid()) {
        if (!current.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() && msgListWidget->tree->currentIndex() != current) {
            m_messageWidget->messageView->setMessage(current);
            msgListWidget->tree->setCurrentIndex(current);
            return;
        }

        QModelIndex child = current.child(0, 0);
        if (child.isValid()) {
            current = child;
            continue;
        }

        QModelIndex sibling = current.sibling(current.row() + 1, 0);
        if (sibling.isValid()) {
            current = sibling;
            continue;
        }

        while (current.isValid() && msgListWidget->tree->model()->rowCount(current.parent()) - 1 == current.row()) {
            current = current.parent();
        }
        current = current.sibling(current.row() + 1, 0);

        if (!current.isValid() && !wrapped) {
            wrapped = true;
            current = msgListWidget->tree->model()->index(0, 0);
        }
    }
}

void MainWindow::slotPreviousUnread()
{
    QModelIndex current = msgListWidget->tree->currentIndex();

    bool wrapped = false;
    while (current.isValid()) {
        if (!current.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() && msgListWidget->tree->currentIndex() != current) {
            m_messageWidget->messageView->setMessage(current);
            msgListWidget->tree->setCurrentIndex(current);
            return;
        }

        QModelIndex candidate = current.sibling(current.row() - 1, 0);
        while (candidate.isValid() && current.model()->hasChildren(candidate)) {
            candidate = candidate.child(current.model()->rowCount(candidate) - 1, 0);
            Q_ASSERT(candidate.isValid());
        }

        if (candidate.isValid()) {
            current = candidate;
        } else {
            current = current.parent();
        }
        if (!current.isValid() && !wrapped) {
            wrapped = true;
            while (msgListWidget->tree->model()->hasChildren(current)) {
                current = msgListWidget->tree->model()->index(msgListWidget->tree->model()->rowCount(current) - 1, 0, current);
            }
        }
    }
}

void MainWindow::handleMarkAsDeleted(bool value)
{
    QModelIndexList translatedIndexes;
    Q_FOREACH(const QModelIndex &item, msgListWidget->tree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        if (!item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;
        translatedIndexes << Imap::deproxifiedIndex(item);
    }
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsDeleted: no valid messages";
    } else {
        if (value)
            model->markMessagesDeleted(translatedIndexes, Imap::Mailbox::FLAG_ADD);
        else
            model->markMessagesDeleted(translatedIndexes, Imap::Mailbox::FLAG_REMOVE);
    }
}

void MainWindow::slotExpunge()
{
    model->expungeMailbox(msgListModel->currentMailbox());
}

void MainWindow::slotMarkCurrentMailboxRead()
{
    model->markMailboxAsRead(mboxTree->currentIndex());
}

void MainWindow::slotCreateMailboxBelowCurrent()
{
    createMailboxBelow(mboxTree->currentIndex());
}

void MainWindow::slotCreateTopMailbox()
{
    createMailboxBelow(QModelIndex());
}

void MainWindow::createMailboxBelow(const QModelIndex &index)
{
    Imap::Mailbox::TreeItemMailbox *mboxPtr = index.isValid() ?
            dynamic_cast<Imap::Mailbox::TreeItemMailbox *>(
                Imap::Mailbox::Model::realTreeItem(index)) :
            0;

    Ui::CreateMailboxDialog ui;
    QDialog *dialog = new QDialog(this);
    ui.setupUi(dialog);

    dialog->setWindowTitle(mboxPtr ?
                           tr("Create a Subfolder of %1").arg(mboxPtr->mailbox()) :
                           tr("Create a Top-level Mailbox"));

    if (dialog->exec() == QDialog::Accepted) {
        QStringList parts;
        if (mboxPtr)
            parts << mboxPtr->mailbox();
        parts << ui.mailboxName->text();
        if (ui.otherMailboxes->isChecked())
            parts << QString();
        QString targetName = parts.join(mboxPtr ? mboxPtr->separator() : QString());   // FIXME: top-level separator
        model->createMailbox(targetName);
    }
}

void MainWindow::slotDeleteCurrentMailbox()
{
    if (! mboxTree->currentIndex().isValid())
        return;

    QModelIndex mailbox = Imap::deproxifiedIndex(mboxTree->currentIndex());
    Q_ASSERT(mailbox.isValid());
    QString name = mailbox.data(Imap::Mailbox::RoleMailboxName).toString();

    if (QMessageBox::question(this, tr("Delete Mailbox"),
                              tr("Are you sure to delete mailbox %1?").arg(name),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        model->deleteMailbox(name);
    }
}

void MainWindow::updateMessageFlags()
{
    updateMessageFlags(msgListWidget->tree->currentIndex());
}

void MainWindow::updateMessageFlags(const QModelIndex &index)
{
    bool okToModify = model->isNetworkAvailable() && index.isValid() && index.data(Imap::Mailbox::RoleMessageUid).toUInt() > 0;
    markAsRead->setEnabled(okToModify);
    markAsDeleted->setEnabled(okToModify);
    markAsRead->setChecked(okToModify && index.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool());
    markAsDeleted->setChecked(okToModify && index.data(Imap::Mailbox::RoleMessageIsMarkedDeleted).toBool());
}

void MainWindow::updateActionsOnlineOffline(bool online)
{
    reloadMboxList->setEnabled(online);
    resyncMbox->setEnabled(online);
    expunge->setEnabled(online);
    createChildMailbox->setEnabled(online);
    createTopMailbox->setEnabled(online);
    deleteCurrentMailbox->setEnabled(online);
    m_actionMarkMailboxAsRead->setEnabled(online);
    markAsDeleted->setEnabled(online);
    markAsRead->setEnabled(online);
    showImapCapabilities->setEnabled(online);
    if (!online) {
        m_replyGuess->setEnabled(false);
        m_replyPrivate->setEnabled(false);
        m_replyAll->setEnabled(false);
        m_replyAllButMe->setEnabled(false);
        m_replyList->setEnabled(false);
    }
}

void MainWindow::slotUpdateMessageActions()
{
    Composer::RecipientList dummy;
    m_replyPrivate->setEnabled(Composer::Util::replyRecipientList(Composer::REPLY_PRIVATE, senderIdentitiesModel(),
                                                                  m_messageWidget->messageView->currentMessage(), dummy));
    m_replyAllButMe->setEnabled(Composer::Util::replyRecipientList(Composer::REPLY_ALL_BUT_ME, senderIdentitiesModel(),
                                                                   m_messageWidget->messageView->currentMessage(), dummy));
    m_replyAll->setEnabled(Composer::Util::replyRecipientList(Composer::REPLY_ALL, senderIdentitiesModel(),
                                                              m_messageWidget->messageView->currentMessage(), dummy));
    m_replyList->setEnabled(Composer::Util::replyRecipientList(Composer::REPLY_LIST, senderIdentitiesModel(),
                                                               m_messageWidget->messageView->currentMessage(), dummy));
    m_replyGuess->setEnabled(m_replyPrivate->isEnabled() || m_replyAllButMe->isEnabled()
                             || m_replyAll->isEnabled() || m_replyList->isEnabled());

    // Check the default reply mode
    // I suspect this is not going to work for everybody. Suggestions welcome...
    if (m_replyList->isEnabled()) {
        m_replyButton->setDefaultAction(m_replyList);
    } else if (m_replyAllButMe->isEnabled()) {
        m_replyButton->setDefaultAction(m_replyAllButMe);
    } else {
        m_replyButton->setDefaultAction(m_replyPrivate);
    }
}

void MainWindow::scrollMessageUp()
{
    m_messageWidget->area->ensureVisible(0, 0, 0, 0);
}

void MainWindow::slotReplyTo()
{
    m_messageWidget->messageView->reply(this, Composer::REPLY_PRIVATE);
}

void MainWindow::slotReplyAll()
{
    m_messageWidget->messageView->reply(this, Composer::REPLY_ALL);
}

void MainWindow::slotReplyAllButMe()
{
    m_messageWidget->messageView->reply(this, Composer::REPLY_ALL_BUT_ME);
}

void MainWindow::slotReplyList()
{
    m_messageWidget->messageView->reply(this, Composer::REPLY_LIST);
}

void MainWindow::slotReplyGuess()
{
    if (m_replyButton->defaultAction() == m_replyAllButMe) {
        slotReplyAllButMe();
    } else if (m_replyButton->defaultAction() == m_replyAll) {
        slotReplyAll();
    } else if (m_replyButton->defaultAction() == m_replyList) {
        slotReplyList();
    } else {
        slotReplyTo();
    }
}

void MainWindow::slotComposeMailUrl(const QUrl &url)
{
    Imap::Message::MailAddress addr;
    if (!Imap::Message::MailAddress::fromUrl(addr, url, QLatin1String("mailto")))
        return;
    RecipientsType recipients;
    recipients << qMakePair<Composer::RecipientKind,QString>(Composer::ADDRESS_TO, addr.asPrettyString());
    invokeComposeDialog(QString(), QString(), recipients);
}

void MainWindow::slotManageContact(const QUrl &url)
{
    Imap::Message::MailAddress addr;
    if (!Imap::Message::MailAddress::fromUrl(addr, url, QLatin1String("x-trojita-manage-contact")))
        return;

    invokeContactEditor();
    m_contactsWidget->manageContact(addr.mailbox + QLatin1Char('@') + addr.host, addr.name);
}

void MainWindow::invokeContactEditor()
{
    if (m_contactsWidget)
        return;

    m_contactsWidget = new BE::Contacts(dynamic_cast<AbookAddressbook*>(m_addressBook));
    m_contactsWidget->setAttribute(Qt::WA_DeleteOnClose, true);
    m_contactsWidget->show();
}

/** @short Invoke the message composer, optionally with some data pre-filed form arguments

The user is always able to tweak these values as she sees fit.

The semantics of the @arg inReplyTo and @arg references are the same as described for the Composer::MessageComposer,
i.e. the data are not supposed to contain the angle bracket.  If the @arg replyingToMessage is present, it will be used
as an index to a message which will get marked as replied to.  This is needed because IMAP doesn't really support site-wide
search by a Message-Id (and it cannot possibly support it in general, either), and because Trojita's lazy loading and lack
of cross-session persistent indexes means that "mark as replied" and "extract message-id from" are effectively two separate
operations.
*/
ComposeWidget *MainWindow::invokeComposeDialog(const QString &subject, const QString &body,
                                               const RecipientsType &recipients, const QList<QByteArray> &inReplyTo,
                                               const QList<QByteArray> &references, const QModelIndex &replyingToMessage)
{
    using namespace Common;
    QString method = m_settings->value(SettingsNames::msaMethodKey).toString();
    MSA::MSAFactory *msaFactory = 0;
    if (method == SettingsNames::methodSMTP || method == SettingsNames::methodSSMTP) {
        msaFactory = new MSA::SMTPFactory(m_settings->value(SettingsNames::smtpHostKey).toString(),
                                          m_settings->value(SettingsNames::smtpPortKey).toInt(),
                                          (method == SettingsNames::methodSSMTP),
                                          (method == SettingsNames::methodSMTP)
                                          && m_settings->value(SettingsNames::smtpStartTlsKey).toBool(),
                                          m_settings->value(SettingsNames::smtpAuthKey).toBool(),
                                          m_settings->value(SettingsNames::smtpUserKey).toString(),
                                          m_settings->value(SettingsNames::smtpPassKey).toString());
    } else if (method == SettingsNames::methodSENDMAIL) {
        QStringList args = m_settings->value(SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd).toString().split(QLatin1Char(' '));
        if (args.isEmpty()) {
            QMessageBox::critical(this, tr("Error"), tr("Please configure the SMTP or sendmail settings in application settings."));
            return 0;
        }
        QString appName = args.takeFirst();
        msaFactory = new MSA::SendmailFactory(appName, args);
    } else if (method == SettingsNames::methodImapSendmail) {
        if (!imapModel()->capabilities().contains(QLatin1String(""))) {
            QMessageBox::critical(this, tr("Error"), tr("The IMAP server does not support mail submission. Please reconfigure the application."));
            return 0;
        }
        // no particular preparation needed here
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Please configure e-mail delivery method in application settings."));
        return 0;
    }

    ComposeWidget *w = new ComposeWidget(this, m_settings, msaFactory);

    // Trim the References header as per RFC 5537
    QList<QByteArray> trimmedReferences = references;
    int referencesSize = QByteArray("References: ").size();
    const int lineOverhead = 3; // one for the " " prefix, two for the \r\n suffix
    Q_FOREACH(const QByteArray &item, references)
        referencesSize += item.size() + lineOverhead;
    // The magic numbers are from RFC 5537
    while (referencesSize >= 998 && trimmedReferences.size() > 3) {
        referencesSize -= trimmedReferences.takeAt(1).size() + lineOverhead;
    }

    w->setData(recipients, subject, body, inReplyTo, trimmedReferences, replyingToMessage);
    Util::centerWidgetOnScreen(w);
    w->show();
    return w;
}

void MainWindow::slotMailboxDeleteFailed(const QString &mailbox, const QString &msg)
{
    QMessageBox::warning(this, tr("Can't delete mailbox"),
                         tr("Deleting mailbox \"%1\" failed with the following message:\n%2").arg(mailbox, msg));
}

void MainWindow::slotMailboxCreateFailed(const QString &mailbox, const QString &msg)
{
    QMessageBox::warning(this, tr("Can't create mailbox"),
                         tr("Creating mailbox \"%1\" failed with the following message:\n%2").arg(mailbox, msg));
}

void MainWindow::slotMailboxChanged(const QModelIndex &mailbox)
{
    using namespace Imap::Mailbox;
    QString mailboxName = mailbox.data(RoleMailboxName).toString();
    bool isSentMailbox = mailbox.isValid() && !mailboxName.isEmpty() &&
            m_settings->value(Common::SettingsNames::composerSaveToImapKey).toBool() &&
            mailboxName == m_settings->value(Common::SettingsNames::composerImapSentKey).toString();
    QTreeView *tree = msgListWidget->tree;

    // Automatically trigger visibility of the TO and FROM columns
    if (isSentMailbox) {
        if (tree->isColumnHidden(MsgListModel::TO) && !tree->isColumnHidden(MsgListModel::FROM)) {
            tree->hideColumn(MsgListModel::FROM);
            tree->showColumn(MsgListModel::TO);
        }
    } else {
        if (tree->isColumnHidden(MsgListModel::FROM) && !tree->isColumnHidden(MsgListModel::TO)) {
            tree->hideColumn(MsgListModel::TO);
            tree->showColumn(MsgListModel::FROM);
        }
    }

    updateMessageFlags();
}

void MainWindow::showConnectionStatus(QObject *parser, Imap::ConnectionState state)
{
    Q_UNUSED(parser);
    using namespace Imap;
    QString message = connectionStateToString(state);
    enum { DURATION = 10000 };
    bool transient = false;

    switch (state) {
    case CONN_STATE_AUTHENTICATED:
    case CONN_STATE_SELECTED:
        transient = true;
        break;
    default:
        // only the stuff above is transient
        break;
    }
    statusBar()->showMessage(message, transient ? DURATION : 0);
}

void MainWindow::slotShowLinkTarget(const QString &link)
{
    if (link.isEmpty()) {
        statusBar()->clearMessage();
    } else {
        //: target of a hyperlink from the currently visible e-mail that the mouse is pointing to
        statusBar()->showMessage(tr("Link target: %1").arg(link));
    }
}

void MainWindow::fillMatchingAbookEntries(const QString &mail, QStringList &displayNames)
{
    displayNames = addressBook()->prettyNamesForAddress(mail);
}

void MainWindow::slotShowAboutTrojita()
{
    Ui::AboutDialog ui;
    QDialog *widget = new QDialog(this);
    widget->setAttribute(Qt::WA_DeleteOnClose);
    ui.setupUi(widget);
    ui.versionLabel->setText(Common::Application::version);
    QStringList copyright;
    {
        // Find the names of the authors and remove date codes from there
        QFile license(QLatin1String(":/LICENSE"));
        license.open(QFile::ReadOnly);
        const QString prefix(QLatin1String("Copyright (C) "));
        Q_FOREACH(const QString &line, QString::fromUtf8(license.readAll()).split(QLatin1Char('\n'))) {
            if (line.startsWith(prefix)) {
                const int pos = prefix.size();
                copyright << QChar(0xa9 /* COPYRIGHT SIGN */) + QLatin1Char(' ') +
                             line.mid(pos).replace(QRegExp(QLatin1String("(\\d) - (\\d)")),
                                                   QLatin1String("\\1") + QChar(0x2014 /* EM DASH */) + QLatin1String("\\2"));
            }
        }
    }
    ui.credits->setTextFormat(Qt::PlainText);
    ui.credits->setText(copyright.join(QLatin1String("\n")));
    widget->show();
}

void MainWindow::slotDonateToTrojita()
{
    QDesktopServices::openUrl(QString(QLatin1String("http://sourceforge.net/donate/index.php?group_id=339456")));
}

void MainWindow::slotSaveCurrentMessageBody()
{
    Q_FOREACH(const QModelIndex &item, msgListWidget->tree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        if (! item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;

        QModelIndex messageIndex = Imap::deproxifiedIndex(item);

        Imap::Network::MsgPartNetAccessManager *netAccess = new Imap::Network::MsgPartNetAccessManager(this);
        netAccess->setModelMessage(messageIndex);
        Imap::Network::FileDownloadManager *fileDownloadManager =
            new Imap::Network::FileDownloadManager(this, netAccess, messageIndex);
        connect(fileDownloadManager, SIGNAL(succeeded()), fileDownloadManager, SLOT(deleteLater()));
        connect(fileDownloadManager, SIGNAL(transferError(QString)), fileDownloadManager, SLOT(deleteLater()));
        connect(fileDownloadManager, SIGNAL(fileNameRequested(QString *)),
                this, SLOT(slotDownloadMessageFileNameRequested(QString *)));
        connect(fileDownloadManager, SIGNAL(transferError(QString)),
                this, SLOT(slotDownloadTransferError(QString)));
        connect(fileDownloadManager, SIGNAL(destroyed()), netAccess, SLOT(deleteLater()));
        fileDownloadManager->downloadMessage();
    }
}

void MainWindow::slotDownloadTransferError(const QString &errorString)
{
    QMessageBox::critical(this, tr("Can't save into file"),
                          tr("Unable to save into file. Error:\n%1").arg(errorString));
}

void MainWindow::slotDownloadMessageFileNameRequested(QString *fileName)
{
    *fileName = QFileDialog::getSaveFileName(this, tr("Save Message"), *fileName, QString(), 0,
                                             QFileDialog::HideNameFilterDetails);
}

void MainWindow::slotViewMsgSource()
{
    Q_FOREACH(const QModelIndex &item, msgListWidget->tree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        if (! item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;
        QModelIndex messageIndex = Imap::deproxifiedIndex(item);

        MessageSourceWidget *sourceWidget = new MessageSourceWidget(0, messageIndex);
        sourceWidget->setAttribute(Qt::WA_DeleteOnClose);
        QAction *close = new QAction(loadIcon(QLatin1String("window-close")), tr("Close"), sourceWidget);
        sourceWidget->addAction(close);
        close->setShortcut(tr("Ctrl+W"));
        connect(close, SIGNAL(triggered()), sourceWidget, SLOT(close()));
        sourceWidget->setWindowTitle(tr("Message source of UID %1 in %2").arg(
                                    QString::number(messageIndex.data(Imap::Mailbox::RoleMessageUid).toUInt()),
                                    messageIndex.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString()
                                    ));
        sourceWidget->show();
    }
}

void MainWindow::slotViewMsgHeaders()
{
    Q_FOREACH(const QModelIndex &item, msgListWidget->tree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        if (! item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;
        QModelIndex messageIndex = Imap::deproxifiedIndex(item);

        Imap::Network::MsgPartNetAccessManager *netAccess = new Imap::Network::MsgPartNetAccessManager(this);
        netAccess->setModelMessage(messageIndex);

        SimplePartWidget *headers = new SimplePartWidget(0, netAccess,
                                        messageIndex.model()->index(0, Imap::Mailbox::TreeItem::OFFSET_HEADER, messageIndex),
                                                         0);
        headers->setAttribute(Qt::WA_DeleteOnClose);
        connect(headers, SIGNAL(destroyed()), netAccess, SLOT(deleteLater()));
        QAction *close = new QAction(loadIcon(QLatin1String("window-close")), tr("Close"), headers);
        headers->addAction(close);
        close->setShortcut(tr("Ctrl+W"));
        connect(close, SIGNAL(triggered()), headers, SLOT(close()));
        headers->setWindowTitle(tr("Message headers of UID %1 in %2").arg(
                                    QString::number(messageIndex.data(Imap::Mailbox::RoleMessageUid).toUInt()),
                                    messageIndex.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString()
                                    ));
        headers->setWindowIcon(loadIcon(QLatin1String("text-x-hex")));
        headers->show();
    }
}

#ifdef XTUPLE_CONNECT
void MainWindow::slotXtSyncCurrentMailbox()
{
    QModelIndex index = mboxTree->currentIndex();
    if (! index.isValid())
        return;

    QString mailbox = index.data(Imap::Mailbox::RoleMailboxName).toString();
    QSettings s;
    QStringList mailboxes = s.value(Common::SettingsNames::xtSyncMailboxList).toStringList();
    if (xtIncludeMailboxInSync->isChecked()) {
        if (! mailboxes.contains(mailbox)) {
            mailboxes.append(mailbox);
        }
    } else {
        mailboxes.removeAll(mailbox);
    }
    s.setValue(Common::SettingsNames::xtSyncMailboxList, mailboxes);
    QSettings(QSettings::UserScope, QString::fromAscii("xTuple.com"), QString::fromAscii("xTuple")).setValue(Common::SettingsNames::xtSyncMailboxList, mailboxes);
    prettyMboxModel->xtConnectStatusChanged(index);
}
#endif

void MainWindow::slotSubscribeCurrentMailbox()
{
    QModelIndex index = mboxTree->currentIndex();
    if (! index.isValid())
        return;

    QString mailbox = index.data(Imap::Mailbox::RoleMailboxName).toString();
    if (m_actionSubscribeMailbox->isChecked()) {
        model->subscribeMailbox(mailbox);
    } else {
        model->unsubscribeMailbox(mailbox);
    }
}

void MainWindow::slotShowOnlySubscribed()
{
    if (m_actionShowOnlySubscribed->isEnabled()) {
        m_settings->setValue(Common::SettingsNames::guiMailboxListShowOnlySubscribed, m_actionShowOnlySubscribed->isChecked());
        prettyMboxModel->setShowOnlySubscribed(m_actionShowOnlySubscribed->isChecked());
    }
}

void MainWindow::slotScrollToUnseenMessage(const QModelIndex &mailbox, const QModelIndex &message)
{
    // Now this is much, much more reliable than messing around with finding out an "interesting message"...
    Q_UNUSED(mailbox);
    Q_UNUSED(message);
    if (!m_actionSortNone->isChecked() && !m_actionSortThreading->isChecked()) {
        // we're using some funky sorting, better don't scroll anywhere
    }
    if (m_actionSortDescending->isChecked()) {
        msgListWidget->tree->scrollToTop();
    } else {
        msgListWidget->tree->scrollToBottom();
    }
}

void MainWindow::slotThreadMsgList()
{
    // We want to save user's preferences and not override them with "threading disabled" when the server
    // doesn't report them, like in initial greetings. That's why we have to check for isEnabled() here.
    const bool useThreading = actionThreadMsgList->isChecked();

    // Switching between threaded/unthreaded view shall reset the sorting criteria. The goal is to make
    // sorting rather seldomly used as people shall instead use proper threading.
    if (useThreading) {
        m_actionSortThreading->setEnabled(true);
        if (!m_actionSortThreading->isChecked())
            m_actionSortThreading->trigger();
        m_actionSortNone->setEnabled(false);
    } else {
        m_actionSortNone->setEnabled(true);
        if (!m_actionSortNone->isChecked())
            m_actionSortNone->trigger();
        m_actionSortThreading->setEnabled(false);
    }

    QPersistentModelIndex currentItem = msgListWidget->tree->currentIndex();

    if (useThreading && actionThreadMsgList->isEnabled()) {
        msgListWidget->tree->setRootIsDecorated(true);
        threadingMsgListModel->setUserWantsThreading(true);
    } else {
        msgListWidget->tree->setRootIsDecorated(false);
        threadingMsgListModel->setUserWantsThreading(false);
    }
    m_settings->setValue(Common::SettingsNames::guiMsgListShowThreading, QVariant(useThreading));

    if (currentItem.isValid()) {
        msgListWidget->tree->scrollTo(currentItem);
    } else {
        // If we cannot determine the current item, at least scroll to a predictable place. Without this, the view
        // would jump to "weird" places, probably due to some heuristics about trying to show "roughly the same"
        // objects as what was visible before the reshuffling.
        msgListWidget->tree->scrollToBottom();
    }
}

void MainWindow::slotSortingPreferenceChanged()
{
    Qt::SortOrder order = m_actionSortDescending->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder;

    using namespace Imap::Mailbox;

    int column = -1;
    if (m_actionSortByArrival->isChecked()) {
        column = MsgListModel::RECEIVED_DATE;
    } else if (m_actionSortByCc->isChecked()) {
        column = MsgListModel::CC;
    } else if (m_actionSortByDate->isChecked()) {
        column = MsgListModel::DATE;
    } else if (m_actionSortByFrom->isChecked()) {
        column = MsgListModel::FROM;
    } else if (m_actionSortBySize->isChecked()) {
        column = MsgListModel::SIZE;
    } else if (m_actionSortBySubject->isChecked()) {
        column = MsgListModel::SUBJECT;
    } else if (m_actionSortByTo->isChecked()) {
        column = MsgListModel::TO;
    } else {
        column = -1;
    }

    msgListWidget->tree->header()->setSortIndicator(column, order);
}

void MainWindow::slotSortingConfirmed(int column, Qt::SortOrder order)
{
    // don't do anything during initialization
    if (!m_actionSortNone)
        return;

    using namespace Imap::Mailbox;
    QAction *action;

    switch (column) {
    case MsgListModel::SEEN:
    case MsgListModel::COLUMN_COUNT:
    case MsgListModel::BCC:
    case -1:
        if (actionThreadMsgList->isChecked())
            action = m_actionSortThreading;
        else
            action = m_actionSortNone;
        break;
    case MsgListModel::SUBJECT:
        action = m_actionSortBySubject;
        break;
    case MsgListModel::FROM:
        action = m_actionSortByFrom;
        break;
    case MsgListModel::TO:
        action = m_actionSortByTo;
        break;
    case MsgListModel::CC:
        action = m_actionSortByCc;
        break;
    case MsgListModel::DATE:
        action = m_actionSortByDate;
        break;
    case MsgListModel::RECEIVED_DATE:
        action = m_actionSortByArrival;
        break;
    case MsgListModel::SIZE:
        action = m_actionSortBySize;
        break;
    default:
        action = m_actionSortNone;
    }

    action->setChecked(true);
    if (order == Qt::DescendingOrder)
        m_actionSortDescending->setChecked(true);
    else
        m_actionSortAscending->setChecked(true);
}

void MainWindow::slotSearchRequested(const QStringList &searchConditions)
{
    if (!searchConditions.isEmpty() && actionThreadMsgList->isChecked()) {
        // right now, searching and threading doesn't play well together at all
        actionThreadMsgList->trigger();
    }
    threadingMsgListModel->setUserSearchingSortingPreference(searchConditions, threadingMsgListModel->currentSortCriterium(),
                                                             threadingMsgListModel->currentSortOrder());
}

void MainWindow::slotHideRead()
{
    const bool hideRead = actionHideRead->isChecked();
    prettyMsgListModel->setHideRead(hideRead);
    m_settings->setValue(Common::SettingsNames::guiMsgListHideRead, QVariant(hideRead));
}

void MainWindow::slotCapabilitiesUpdated(const QStringList &capabilities)
{
    msgListWidget->tree->header()->viewport()->removeEventFilter(this);
    if (capabilities.contains(QLatin1String("SORT"))) {
        m_actionSortByDate->actionGroup()->setEnabled(true);
    } else {
        m_actionSortByDate->actionGroup()->setEnabled(false);
        msgListWidget->tree->header()->viewport()->installEventFilter(this);
    }

    msgListWidget->setFuzzySearchSupported(capabilities.contains(QLatin1String("SEARCH=FUZZY")));

    m_actionShowOnlySubscribed->setEnabled(capabilities.contains(QLatin1String("LIST-EXTENDED")));
    m_actionShowOnlySubscribed->setChecked(m_actionShowOnlySubscribed->isEnabled() &&
                                           m_settings->value(
                                               Common::SettingsNames::guiMailboxListShowOnlySubscribed, false).toBool());
    m_actionSubscribeMailbox->setEnabled(m_actionShowOnlySubscribed->isEnabled());

    const QStringList supportedCapabilities = Imap::Mailbox::ThreadingMsgListModel::supportedCapabilities();
    Q_FOREACH(const QString &capability, capabilities) {
        if (supportedCapabilities.contains(capability)) {
            actionThreadMsgList->setEnabled(true);
            if (actionThreadMsgList->isChecked())
                slotThreadMsgList();
            return;
        }
    }
    actionThreadMsgList->setEnabled(false);
}

void MainWindow::slotShowImapInfo()
{
    QString caps;
    Q_FOREACH(const QString &cap, model->capabilities()) {
        caps += tr("<li>%1</li>\n").arg(cap);
    }

    QString idString;
    if (!model->serverId().isEmpty() && model->capabilities().contains(QLatin1String("ID"))) {
        QMap<QByteArray,QByteArray> serverId = model->serverId();

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#define IMAP_ID_FIELD(Var, Name) bool has_##Var = serverId.contains(Name); \
    QString Var = has_##Var ? QString::fromUtf8(serverId[Name]).toHtmlEscaped() : tr("Unknown");
#else
#define IMAP_ID_FIELD(Var, Name) bool has_##Var = serverId.contains(Name); \
    QString Var = has_##Var ? Qt::escape(QString::fromAscii(serverId[Name])) : tr("Unknown");
#endif
        IMAP_ID_FIELD(serverName, "name");
        IMAP_ID_FIELD(serverVersion, "version");
        IMAP_ID_FIELD(os, "os");
        IMAP_ID_FIELD(osVersion, "os-version");
        IMAP_ID_FIELD(vendor, "vendor");
        IMAP_ID_FIELD(supportUrl, "support-url");
        IMAP_ID_FIELD(address, "address");
        IMAP_ID_FIELD(date, "date");
        IMAP_ID_FIELD(command, "command");
        IMAP_ID_FIELD(arguments, "arguments");
        IMAP_ID_FIELD(environment, "environment");
#undef IMAP_ID_FIELD
        if (has_serverName) {
            idString = tr("<p>");
            if (has_serverVersion)
                idString += tr("Server: %1 %2").arg(serverName, serverVersion);
            else
                idString += tr("Server: %1").arg(serverName);

            if (has_vendor) {
                idString += tr(" (%1)").arg(vendor);
            }
            if (has_os) {
                if (has_osVersion)
                    idString += tr(" on %1 %2").arg(os, osVersion);
                else
                    idString += tr(" on %1").arg(os);
            }
            idString += tr("</p>");
        } else {
            idString = tr("<p>The IMAP server did not return usable information about itself.</p>");
        }
        QString fullId;
        for (QMap<QByteArray,QByteArray>::const_iterator it = serverId.constBegin(); it != serverId.constEnd(); ++it) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
            fullId += tr("<li>%1: %2</li>").arg(QString::fromUtf8(it.key()).toHtmlEscaped(), QString::fromUtf8(it.value()).toHtmlEscaped());
#else
            fullId += tr("<li>%1: %2</li>").arg(Qt::escape(QString::fromAscii(it.key())), Qt::escape(QString::fromAscii(it.value())));
#endif
        }
        idString += tr("<ul>%1</ul>").arg(fullId);
    } else {
        idString = tr("<p>The server has not provided information about its software version.</p>");
    }

    QMessageBox::information(this, tr("IMAP Server Information"),
                             tr("%1"
                                "<p>The following capabilities are currently advertised:</p>\n"
                                "<ul>\n%2</ul>").arg(idString, caps));
}

QSize MainWindow::sizeHint() const
{
    return QSize(1150, 980);
}

void MainWindow::slotUpdateWindowTitle()
{
    QModelIndex mailbox = msgListModel->currentMailbox();
    QString profileName = QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
    if (!profileName.isEmpty())
        profileName = QLatin1String(" [") + profileName + QLatin1Char(']');
    if (mailbox.isValid()) {
        if (mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt()) {
            setWindowTitle(trUtf8("%1 - %2 unread - Trojitá")
                           .arg(mailbox.data(Imap::Mailbox::RoleShortMailboxName).toString(),
                                mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toString()) + profileName);
        } else {
            setWindowTitle(trUtf8("%1 - Trojitá").arg(mailbox.data(Imap::Mailbox::RoleShortMailboxName).toString()) + profileName);
        }
    } else {
        setWindowTitle(trUtf8("Trojitá") + profileName);
    }
}

void MainWindow::slotLayoutCompact()
{
    saveSizesAndState();
    if (!m_mainHSplitter) {
        m_mainHSplitter = new QSplitter();
        connect(m_mainHSplitter, SIGNAL(splitterMoved(int,int)), m_delayedStateSaving, SLOT(start()));
        connect(m_mainHSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(possiblyLoadMessageOnSplittersChanged()));
    }
    if (!m_mainVSplitter) {
        m_mainVSplitter = new QSplitter();
        m_mainVSplitter->setOrientation(Qt::Vertical);
        connect(m_mainVSplitter, SIGNAL(splitterMoved(int,int)), m_delayedStateSaving, SLOT(start()));
        connect(m_mainVSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(possiblyLoadMessageOnSplittersChanged()));
    }

    m_mainVSplitter->addWidget(msgListWidget);
    m_mainVSplitter->addWidget(m_messageWidget);
    m_mainHSplitter->addWidget(mboxTree);
    m_mainHSplitter->addWidget(m_mainVSplitter);

    mboxTree->show();
    msgListWidget->show();
    m_messageWidget->show();
    m_mainVSplitter->show();
    m_mainHSplitter->show();

    // The mboxTree shall not expand...
    m_mainHSplitter->setStretchFactor(0, 0);
    // ...while the msgListTree shall consume all the remaining space
    m_mainHSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_mainHSplitter);
    setMinimumWidth(MINIMUM_WIDTH_NORMAL);

    delete m_mainStack;

    m_layoutMode = LAYOUT_COMPACT;
    m_settings->setValue(Common::SettingsNames::guiMainWindowLayout, Common::SettingsNames::guiMainWindowLayoutCompact);
    applySizesAndState();
}

void MainWindow::slotLayoutWide()
{
    saveSizesAndState();
    if (!m_mainHSplitter) {
        m_mainHSplitter = new QSplitter();
        connect(m_mainHSplitter, SIGNAL(splitterMoved(int,int)), m_delayedStateSaving, SLOT(start()));
        connect(m_mainHSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(possiblyLoadMessageOnSplittersChanged()));
    }

    m_mainHSplitter->addWidget(mboxTree);
    m_mainHSplitter->addWidget(msgListWidget);
    m_mainHSplitter->addWidget(m_messageWidget);
    m_mainHSplitter->setStretchFactor(0, 0);
    m_mainHSplitter->setStretchFactor(1, 1);
    m_mainHSplitter->setStretchFactor(2, 1);

    mboxTree->show();
    msgListWidget->show();
    m_messageWidget->show();
    m_mainHSplitter->show();

    setCentralWidget(m_mainHSplitter);
    setMinimumWidth(MINIMUM_WIDTH_WIDE);

    delete m_mainStack;
    delete m_mainVSplitter;

    m_layoutMode = LAYOUT_WIDE;
    m_settings->setValue(Common::SettingsNames::guiMainWindowLayout, Common::SettingsNames::guiMainWindowLayoutWide);
    applySizesAndState();
}

void MainWindow::slotLayoutOneAtTime()
{
    saveSizesAndState();
    if (m_mainStack)
        return;

    m_mainStack = new OnePanelAtTimeWidget(this, mboxTree, msgListWidget, m_messageWidget, m_mainToolbar, m_oneAtTimeGoBack);
    setCentralWidget(m_mainStack);
    setMinimumWidth(MINIMUM_WIDTH_NORMAL);

    delete m_mainHSplitter;
    delete m_mainVSplitter;

    m_layoutMode = LAYOUT_ONE_AT_TIME;
    m_settings->setValue(Common::SettingsNames::guiMainWindowLayout, Common::SettingsNames::guiMainWindowLayoutOneAtTime);
    applySizesAndState();
}

Imap::Mailbox::Model *MainWindow::imapModel() const
{
    return model;
}

void MainWindow::desktopGeometryChanged()
{
    QRect geometry = qApp->desktop()->availableGeometry(this);
    m_actionLayoutWide->setEnabled(geometry.width() >= MINIMUM_WIDTH_WIDE);
    if (m_layoutMode == LAYOUT_WIDE && !m_actionLayoutWide->isEnabled()) {
        m_actionLayoutCompact->trigger();
    }
    saveSizesAndState();
}

QString MainWindow::settingsKeyForLayout(const LayoutMode layout)
{
    switch (layout) {
    case LAYOUT_COMPACT:
        return Common::SettingsNames::guiSizesInMainWinWhenCompact;
    case LAYOUT_WIDE:
        return Common::SettingsNames::guiSizesInMainWinWhenWide;
    case LAYOUT_ONE_AT_TIME:
        // nothing is saved here
        break;
    }
    return QString();
}

void MainWindow::saveSizesAndState()
{
    if (m_skipSavingOfUI)
        return;

    QRect geometry = qApp->desktop()->availableGeometry(this);
    QString key = settingsKeyForLayout(m_layoutMode);
    if (key.isEmpty())
        return;

    QList<QByteArray> items;
    items << saveGeometry();
    items << saveState();
    items << (m_mainVSplitter ? m_mainVSplitter->saveState() : QByteArray());
    items << (m_mainHSplitter ? m_mainHSplitter->saveState() : QByteArray());
    items << msgListWidget->tree->header()->saveState();
    items << QByteArray::number(msgListWidget->tree->header()->count());
    for (int i = 0; i < msgListWidget->tree->header()->count(); ++i) {
        items << QByteArray::number(msgListWidget->tree->header()->sectionSize(i));
    }
    // a bool cannot be pushed directly onto a QByteArray so we must convert it to a number
    items << QByteArray::number(menuBar()->isVisible());
    QByteArray buf;
    QDataStream stream(&buf, QIODevice::WriteOnly);
    stream << items.size();
    Q_FOREACH(const QByteArray &item, items) {
        stream << item;
    }

    m_settings->setValue(key.arg(QString::number(geometry.width())), buf);
}

void MainWindow::saveRawStateSetting(bool enabled)
{
    m_settings->setValue(Common::SettingsNames::guiAllowRawSearch, enabled);
}

void MainWindow::applySizesAndState()
{
    QRect geometry = qApp->desktop()->availableGeometry(this);
    QString key = settingsKeyForLayout(m_layoutMode);
    if (key.isEmpty())
        return;

    QByteArray buf = m_settings->value(key.arg(QString::number(geometry.width()))).toByteArray();
    if (buf.isEmpty())
        return;

    int size;
    QDataStream stream(&buf, QIODevice::ReadOnly);
    stream >> size;
    QByteArray item;

    // There are slots connected to various events triggered by both restoreGeometry() and restoreState() which would attempt to
    // save our geometries and state, which is what we must avoid while this method is executing.
    bool skipSaving = m_skipSavingOfUI;
    m_skipSavingOfUI = true;

    if (size-- && !stream.atEnd()) {
        stream >> item;

        // https://bugreports.qt-project.org/browse/QTBUG-30636
        if (windowState() & Qt::WindowMaximized) {
            // restoreGeometry(.) restores the wrong size for at least maximized window
            // However, QWidget does also not notice that the configure request for this
            // is ignored by many window managers (because users really don't like when windows
            // drop themselves out of maximization) and has a wrong QWidget::geometry() idea from
            // the wrong assumption the request would have been hononred.
            //  So we just "fix" the internal geometry immediately afterwards to prevent
            // mislayouting
            // There's atm. no flicker due to this (and because Qt compresses events)
            // In case it ever occurs, we can frame this in setUpdatesEnabled(false/true)
            QRect oldGeometry = MainWindow::geometry();
            restoreGeometry(item);
            if (windowState() & Qt::WindowMaximized)
                setGeometry(oldGeometry);
        } else {
            restoreGeometry(item);
            if (windowState() & Qt::WindowMaximized) {
                // ensure to try setting the proper geometry and have the WM constrain us
                setGeometry(QApplication::desktop()->availableGeometry());
            }
        }
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        restoreState(item);
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        if (m_mainVSplitter) {
            m_mainVSplitter->restoreState(item);
        }
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        if (m_mainHSplitter) {
            m_mainHSplitter->restoreState(item);
        }
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        msgListWidget->tree->header()->restoreState(item);
        // got to manually update the state of the actions which control the visibility state
        msgListWidget->tree->updateActionsAfterRestoredState();
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        bool ok;
        int columns = item.toInt(&ok);
        if (ok) {
            msgListWidget->tree->header()->setStretchLastSection(false);
            for (int i = 0; i < columns && size-- && !stream.atEnd(); ++i) {
                stream >> item;
                int sectionSize = item.toInt();
                QHeaderView::ResizeMode resizeMode = msgListWidget->tree->resizeModeForColumn(i);
                if (sectionSize > 0 && resizeMode == QHeaderView::Interactive) {
                    // fun fact: user cannot resize by mouse when size <= 0
                    msgListWidget->tree->setColumnWidth(i, sectionSize);
                } else {
                    msgListWidget->tree->setColumnWidth(i, msgListWidget->tree->sizeHintForColumn(i));
                }
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
                msgListWidget->tree->header()->setSectionResizeMode(i, resizeMode);
#else
                msgListWidget->tree->header()->setResizeMode(i, resizeMode);
#endif
            }
        }
    }

    if (size-- && !stream.atEnd()) {
        stream >> item;
        bool ok;
        bool visibility = item.toInt(&ok);
        if (ok) {
            menuBar()->setVisible(visibility);
            showMenuBar->setChecked(visibility);
            menuShow->setVisible(!visibility);
        }
    }

    m_skipSavingOfUI = skipSaving;
}

void MainWindow::resizeEvent(QResizeEvent *)
{
    m_delayedStateSaving->start();
}

/** @short Make sure that the message gets loaded after the splitters have changed their position */
void MainWindow::possiblyLoadMessageOnSplittersChanged()
{
    if (m_messageWidget->isVisible() && !m_messageWidget->size().isEmpty()) {
        // We do not have to check whether it's a different message; the setMessage() will do this or us
        // and there are multiple proxy models involved anyway
        QModelIndex index = msgListWidget->tree->currentIndex();
        if (index.isValid()) {
            // OTOH, setting an invalid QModelIndex would happily assert-fail
            m_messageWidget->messageView->setMessage(msgListWidget->tree->currentIndex());
        }
    }
}
}
