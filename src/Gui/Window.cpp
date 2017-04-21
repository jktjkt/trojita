/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2013 - 2015 Pali Rohár <pali.rohar@gmail.com>

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
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QScrollBar>
#include <QSplitter>
#include <QSslError>
#include <QSslKey>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextDocument>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QUrl>
#include <QWheelEvent>

#include "configure.cmake.h"
#include "Common/Application.h"
#include "Common/Paths.h"
#include "Common/PortNumbers.h"
#include "Common/SettingsNames.h"
#include "Composer/Mailto.h"
#include "Composer/SenderIdentitiesModel.h"
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
#  ifdef TROJITA_HAVE_GPGMEPP
#    include "Cryptography/GpgMe++.h"
#  endif
#endif
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/ModelWatcher.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/NetworkWatcher.h"
#include "Imap/Model/PrettyMailboxModel.h"
#include "Imap/Model/PrettyMsgListModel.h"
#include "Imap/Model/SpecialFlagNames.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Model/FavoriteTagsModel.h"
#include "Imap/Model/Utils.h"
#include "Imap/Tasks/ImapTask.h"
#include "Imap/Network/FileDownloadManager.h"
#include "MSA/ImapSubmit.h"
#include "MSA/Sendmail.h"
#include "MSA/SMTP.h"
#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PasswordPlugin.h"
#include "Plugins/PluginManager.h"
#include "CompleteMessageWidget.h"
#include "ComposeWidget.h"
#include "MailBoxTreeView.h"
#include "MessageListWidget.h"
#include "MessageView.h"
#include "MessageSourceWidget.h"
#include "Gui/MessageHeadersWidget.h"
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
#include "UiUtils/IconLoader.h"
#include "UiUtils/QaimDfsIterator.h"

/** @short All user-facing widgets and related classes */
namespace Gui
{

    static const char * const netErrorUnseen = "net_error_unseen";

MainWindow::MainWindow(QSettings *settings): QMainWindow(), m_imapAccess(0), m_mainHSplitter(0), m_mainVSplitter(0),
    m_mainStack(0), m_layoutMode(LAYOUT_COMPACT), m_skipSavingOfUI(true), m_delayedStateSaving(0), m_actionSortNone(0),
    m_ignoreStoredPassword(false), m_settings(settings), m_pluginManager(0), m_networkErrorMessageBox(0), m_trayIcon(0)
{
    setAttribute(Qt::WA_AlwaysShowToolTips);
    // m_pluginManager must be created before calling createWidgets
    m_pluginManager = new Plugins::PluginManager(this, m_settings,
                                                 Common::SettingsNames::addressbookPlugin, Common::SettingsNames::passwordPlugin);
    connect(m_pluginManager, &Plugins::PluginManager::pluginsChanged, this, &MainWindow::slotPluginsChanged);
    connect(m_pluginManager, &Plugins::PluginManager::pluginError, this, [this](const QString &errorMessage) {
        Gui::Util::messageBoxWarning(this, tr("Plugin Error"),
                                     //: The %1 placeholder is a full error message as provided by Qt, ready for human consumption.
                                     trUtf8("A plugin failed to load, therefore some functionality might be lost. "
                                            "You might want to update your system or report a bug to your vendor."
                                            "\n\n%1").arg(errorMessage));
    });
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
    Plugins::PluginManager::MimePartReplacers replacers;
#ifdef TROJITA_HAVE_GPGMEPP
    replacers.emplace_back(std::make_shared<Cryptography::GpgMeReplacer>());
#endif
    m_pluginManager->setMimePartReplacers(replacers);
#endif

    // ImapAccess contains a wrapper for retrieving passwords through some plugin.
    // That PasswordWatcher is used by the SettingsDialog's widgets *and* by this class,
    // which means that ImapAccess has to be constructed before we go and open the settings dialog.

    // FIXME: use another account-id at some point in future
    //        we are now using the profile to avoid overwriting passwords of
    //        other profiles in secure storage
    QString profileName = QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
    m_imapAccess = new Imap::ImapAccess(this, m_settings, m_pluginManager, profileName);
    connect(m_imapAccess, &Imap::ImapAccess::cacheError, this, &MainWindow::cacheError);
    connect(m_imapAccess, &Imap::ImapAccess::checkSslPolicy, this, &MainWindow::checkSslPolicy, Qt::QueuedConnection);

    ShortcutHandler *shortcutHandler = new ShortcutHandler(this);
    shortcutHandler->setSettingsObject(m_settings);
    defineActions();
    shortcutHandler->readSettings(); // must happen after defineActions()

    // must be created before calling createWidgets
    m_favoriteTags = new Imap::Mailbox::FavoriteTagsModel(this);
    m_favoriteTags->loadFromSettings(*m_settings);

    createWidgets();

    Imap::migrateSettings(m_settings);

    m_senderIdentities = new Composer::SenderIdentitiesModel(this);
    m_senderIdentities->loadFromSettings(*m_settings);

    if (! m_settings->contains(Common::SettingsNames::imapMethodKey)) {
        QTimer::singleShot(0, this, SLOT(slotShowSettings()));
    }


    setupModels();
    createActions();
    createMenus();
    slotToggleSysTray();
    slotPluginsChanged();

    slotFavoriteTagsChanged();
    connect(m_favoriteTags, &QAbstractItemModel::modelReset, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::layoutChanged, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsMoved, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsInserted, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::rowsRemoved, this, &MainWindow::slotFavoriteTagsChanged);
    connect(m_favoriteTags, &QAbstractItemModel::dataChanged, this, &MainWindow::slotFavoriteTagsChanged);

    // Please note that Qt 4.6.1 really requires passing the method signature this way, *not* using the SLOT() macro
    QDesktopServices::setUrlHandler(QStringLiteral("mailto"), this, "slotComposeMailUrl");
    QDesktopServices::setUrlHandler(QStringLiteral("x-trojita-manage-contact"), this, "slotManageContact");

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

    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [&](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive && m_networkErrorMessageBox && m_networkErrorMessageBox->property(netErrorUnseen).toBool()) {
                    m_networkErrorMessageBox->setProperty(netErrorUnseen, false);
                    m_networkErrorMessageBox->show();
                }
            });

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
    connect(delayedResize, &QTimer::timeout, this, &MainWindow::desktopGeometryChanged);
    connect(qApp->desktop(), &QDesktopWidget::workAreaResized, delayedResize, static_cast<void (QTimer::*)()>(&QTimer::start));
    m_skipSavingOfUI = false;
}

void MainWindow::defineActions()
{
    ShortcutHandler *shortcutHandler = ShortcutHandler::instance();
    shortcutHandler->defineAction(QStringLiteral("action_application_exit"), QStringLiteral("application-exit"), tr("E&xit"), QKeySequence::Quit);
    shortcutHandler->defineAction(QStringLiteral("action_compose_mail"), QStringLiteral("document-edit"), tr("&New Message..."), QKeySequence::New);
    shortcutHandler->defineAction(QStringLiteral("action_compose_draft"), QStringLiteral("document-open-recent"), tr("&Edit Draft..."));
    shortcutHandler->defineAction(QStringLiteral("action_show_menubar"), QStringLiteral("view-list-text"), tr("Show Main Menu &Bar"), tr("Ctrl+M"));
    shortcutHandler->defineAction(QStringLiteral("action_expunge"), QStringLiteral("trash-empty"), tr("Exp&unge"), tr("Ctrl+E"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_read"), QStringLiteral("mail-mark-read"), tr("Mark as &Read"), QStringLiteral("M"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_next_unread"), QStringLiteral("arrow-right"), tr("&Next Unread Message"), QStringLiteral("N"));
    shortcutHandler->defineAction(QStringLiteral("action_go_to_previous_unread"), QStringLiteral("arrow-left"), tr("&Previous Unread Message"), QStringLiteral("P"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_deleted"), QStringLiteral("list-remove"), tr("Mark as &Deleted"), QKeySequence(Qt::Key_Delete).toString());
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_flagged"), QStringLiteral("mail-flagged"), tr("Mark as &Flagged"), QStringLiteral("S"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_junk"), QStringLiteral("mail-mark-junk"), tr("Mark as &Junk"), QStringLiteral("J"));
    shortcutHandler->defineAction(QStringLiteral("action_mark_as_notjunk"), QStringLiteral("mail-mark-notjunk"), tr("Mark as Not &junk"), QStringLiteral("Shift+J"));
    shortcutHandler->defineAction(QStringLiteral("action_save_message_as"), QStringLiteral("document-save"), tr("&Save Message..."));
    shortcutHandler->defineAction(QStringLiteral("action_view_message_source"), QString(), tr("View Message &Source..."));
    shortcutHandler->defineAction(QStringLiteral("action_view_message_headers"), QString(), tr("View Message &Headers..."), tr("Ctrl+U"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_private"), QStringLiteral("mail-reply-sender"), tr("&Private Reply"), tr("Ctrl+Shift+A"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_all_but_me"), QStringLiteral("mail-reply-all"), tr("Reply to All &but Me"), tr("Ctrl+Shift+R"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_all"), QStringLiteral("mail-reply-all"), tr("Reply to &All"), tr("Ctrl+Alt+Shift+R"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_list"), QStringLiteral("mail-reply-list"), tr("Reply to &Mailing List"), tr("Ctrl+L"));
    shortcutHandler->defineAction(QStringLiteral("action_reply_guess"), QString(), tr("Reply by &Guess"), tr("Ctrl+R"));
    shortcutHandler->defineAction(QStringLiteral("action_forward_attachment"), QStringLiteral("mail-forward"), tr("&Forward"), tr("Ctrl+Shift+F"));
    shortcutHandler->defineAction(QStringLiteral("action_bounce"), QStringLiteral("mail-bounce"), tr("Edit as New E-Mail Message..."));
    shortcutHandler->defineAction(QStringLiteral("action_archive"), QStringLiteral("mail-move-to-archive"), tr("&Archive"), QStringLiteral("A"));
    shortcutHandler->defineAction(QStringLiteral("action_contact_editor"), QStringLiteral("contact-unknown"), tr("Address Book..."));
    shortcutHandler->defineAction(QStringLiteral("action_network_offline"), QStringLiteral("network-disconnect"), tr("&Offline"));
    shortcutHandler->defineAction(QStringLiteral("action_network_expensive"), QStringLiteral("network-wireless"), tr("&Expensive Connection"));
    shortcutHandler->defineAction(QStringLiteral("action_network_online"), QStringLiteral("network-connect"), tr("&Free Access"));
    shortcutHandler->defineAction(QStringLiteral("action_messagewindow_close"), QStringLiteral("window-close"), tr("Close Standalone Message Window"));
    shortcutHandler->defineAction(QStringLiteral("action_open_messagewindow"), QString(), tr("Open message in New Window..."), QStringLiteral("Ctrl+Return"));
    shortcutHandler->defineAction(QStringLiteral("action_oneattime_go_back"), QStringLiteral("go-previous"), tr("Navigate Back"), QKeySequence(QKeySequence::Back).toString());
    shortcutHandler->defineAction(QStringLiteral("action_zoom_in"), QStringLiteral("zoom-in"), tr("Zoom In"), QKeySequence::ZoomIn);
    shortcutHandler->defineAction(QStringLiteral("action_zoom_out"), QStringLiteral("zoom-out"), tr("Zoom Out"), QKeySequence::ZoomOut);
    shortcutHandler->defineAction(QStringLiteral("action_zoom_original"), QStringLiteral("zoom-original"), tr("Original Size"));
    shortcutHandler->defineAction(QStringLiteral("action_focus_mailbox_tree"), QString(), tr("Move Focus to Mailbox List"));
    shortcutHandler->defineAction(QStringLiteral("action_focus_msg_list"), QString(), tr("Move Focus to Message List"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_1"), QStringLiteral("mail-tag-1"), tr("Tag with &1st tag"), QStringLiteral("1"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_2"), QStringLiteral("mail-tag-2"), tr("Tag with &2nd tag"), QStringLiteral("2"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_3"), QStringLiteral("mail-tag-3"), tr("Tag with &3rd tag"), QStringLiteral("3"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_4"), QStringLiteral("mail-tag-4"), tr("Tag with &4th tag"), QStringLiteral("4"));
    shortcutHandler->defineAction(QStringLiteral("action_tag_5"), QStringLiteral("mail-tag-5"), tr("Tag with &5th tag"), QStringLiteral("5"));
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

    m_actionContactEditor = ShortcutHandler::instance()->createAction(QStringLiteral("action_contact_editor"), this, SLOT(invokeContactEditor()), this);

    m_mainToolbar = addToolBar(tr("Navigation"));
    m_mainToolbar->setObjectName(QStringLiteral("mainToolbar"));

    reloadMboxList = new QAction(style()->standardIcon(QStyle::SP_ArrowRight), tr("&Update List of Child Mailboxes"), this);
    connect(reloadMboxList, &QAction::triggered, this, &MainWindow::slotReloadMboxList);

    resyncMbox = new QAction(UiUtils::loadIcon(QStringLiteral("view-refresh")), tr("Check for &New Messages"), this);
    connect(resyncMbox, &QAction::triggered, this, &MainWindow::slotResyncMbox);

    reloadAllMailboxes = new QAction(tr("&Reload Everything"), this);
    // connect later

    exitAction = ShortcutHandler::instance()->createAction(QStringLiteral("action_application_exit"), qApp, SLOT(quit()), this);
    exitAction->setStatusTip(tr("Exit the application"));

    netOffline = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_offline"));
    netOffline->setCheckable(true);
    // connect later
    netExpensive = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_expensive"));
    netExpensive->setCheckable(true);
    // connect later
    netOnline = ShortcutHandler::instance()->createAction(QStringLiteral("action_network_online"));
    netOnline->setCheckable(true);
    // connect later

    QActionGroup *netPolicyGroup = new QActionGroup(this);
    netPolicyGroup->setExclusive(true);
    netPolicyGroup->addAction(netOffline);
    netPolicyGroup->addAction(netExpensive);
    netPolicyGroup->addAction(netOnline);

    //: a debugging tool showing the full contents of the whole IMAP server; all folders, messages and their parts
    showFullView = new QAction(UiUtils::loadIcon(QStringLiteral("edit-find-mail")), tr("Show Full &Tree Window"), this);
    showFullView->setCheckable(true);
    connect(showFullView, &QAction::triggered, allDock, &QWidget::setVisible);
    connect(allDock, &QDockWidget::visibilityChanged, showFullView, &QAction::setChecked);

    //: list of active "tasks", entities which are performing certain action like downloading a message or syncing a mailbox
    showTaskView = new QAction(tr("Show ImapTask t&ree"), this);
    showTaskView->setCheckable(true);
    connect(showTaskView, &QAction::triggered, taskDock, &QWidget::setVisible);
    connect(taskDock, &QDockWidget::visibilityChanged, showTaskView, &QAction::setChecked);

    //: a debugging tool showing the mime tree of the current message
    showMimeView = new QAction(tr("Show &MIME tree"), this);
    showMimeView->setCheckable(true);
    connect(showMimeView, &QAction::triggered, mailMimeDock, &QWidget::setVisible);
    connect(mailMimeDock, &QDockWidget::visibilityChanged, showMimeView, &QAction::setChecked);

    showImapLogger = new QAction(tr("Show IMAP protocol &log"), this);
    showImapLogger->setCheckable(true);
    connect(showImapLogger, &QAction::toggled, imapLoggerDock, &QWidget::setVisible);
    connect(imapLoggerDock, &QDockWidget::visibilityChanged, showImapLogger, &QAction::setChecked);

    //: file to save the debug log into
    logPersistent = new QAction(tr("Log &into %1").arg(Imap::Mailbox::persistentLogFileName()), this);
    logPersistent->setCheckable(true);
    connect(logPersistent, &QAction::triggered, imapLogger, &ProtocolLoggerWidget::slotSetPersistentLogging);
    connect(imapLogger, &ProtocolLoggerWidget::persistentLoggingChanged, logPersistent, &QAction::setChecked);

    showImapCapabilities = new QAction(tr("IMAP Server In&formation..."), this);
    connect(showImapCapabilities, &QAction::triggered, this, &MainWindow::slotShowImapInfo);

    showMenuBar = ShortcutHandler::instance()->createAction(QStringLiteral("action_show_menubar"), this);
    showMenuBar->setCheckable(true);
    showMenuBar->setChecked(true);
    connect(showMenuBar, &QAction::triggered, menuBar(), &QMenuBar::setVisible);
    connect(showMenuBar, &QAction::triggered, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));

    showToolBar = new QAction(tr("Show &Toolbar"), this);
    showToolBar->setCheckable(true);
    connect(showToolBar, &QAction::triggered, m_mainToolbar, &QWidget::setVisible);
    connect(m_mainToolbar, &QToolBar::visibilityChanged, showToolBar, &QAction::setChecked);
    connect(m_mainToolbar, &QToolBar::visibilityChanged, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));

    configSettings = new QAction(UiUtils::loadIcon(QStringLiteral("configure")),  tr("&Settings..."), this);
    connect(configSettings, &QAction::triggered, this, &MainWindow::slotShowSettings);

    QAction *triggerSearch = new QAction(this);
    addAction(triggerSearch);
    triggerSearch->setShortcut(QKeySequence(QStringLiteral(":, =")));
    connect(triggerSearch, &QAction::triggered, msgListWidget, &MessageListWidget::focusRawSearch);

    triggerSearch = new QAction(this);
    addAction(triggerSearch);
    triggerSearch->setShortcut(QKeySequence(QStringLiteral("/")));
    connect(triggerSearch, &QAction::triggered, msgListWidget, &MessageListWidget::focusSearch);

    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_focus_mailbox_tree"), mboxTree,
            SLOT(setFocus()), this));
    addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_focus_msg_list"), msgListWidget->tree,
            SLOT(setFocus()), this));

    m_oneAtTimeGoBack = ShortcutHandler::instance()->createAction(QStringLiteral("action_oneattime_go_back"), this);
    m_oneAtTimeGoBack->setEnabled(false);

    composeMail = ShortcutHandler::instance()->createAction(QStringLiteral("action_compose_mail"), this, SLOT(slotComposeMail()), this);
    m_editDraft = ShortcutHandler::instance()->createAction(QStringLiteral("action_compose_draft"), this, SLOT(slotEditDraft()), this);

    expunge = ShortcutHandler::instance()->createAction(QStringLiteral("action_expunge"), this, SLOT(slotExpunge()), this);

    m_forwardAsAttachment = ShortcutHandler::instance()->createAction(QStringLiteral("action_forward_attachment"), this, SLOT(slotForwardAsAttachment()), this);
    m_bounce = ShortcutHandler::instance()->createAction(QStringLiteral("action_bounce"), this, SLOT(slotBounce()), this);
    markAsRead = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_read"), this);
    markAsRead->setCheckable(true);
    msgListWidget->tree->addAction(markAsRead);
    connect(markAsRead, &QAction::triggered, this, &MainWindow::handleMarkAsRead);

    m_nextMessage = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_next_unread"), this, SLOT(slotNextUnread()), this);
    msgListWidget->tree->addAction(m_nextMessage);
    m_messageWidget->messageView->addAction(m_nextMessage);

    m_previousMessage = ShortcutHandler::instance()->createAction(QStringLiteral("action_go_to_previous_unread"), this, SLOT(slotPreviousUnread()), this);
    msgListWidget->tree->addAction(m_previousMessage);
    m_messageWidget->messageView->addAction(m_previousMessage);

    markAsDeleted = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_deleted"), this);
    markAsDeleted->setCheckable(true);
    msgListWidget->tree->addAction(markAsDeleted);
    connect(markAsDeleted, &QAction::triggered, this, &MainWindow::handleMarkAsDeleted);

    markAsFlagged = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_flagged"), this);
    markAsFlagged->setCheckable(true);
    connect(markAsFlagged, &QAction::triggered, this, &MainWindow::handleMarkAsFlagged);

    markAsJunk = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_junk"), this);
    markAsJunk->setCheckable(true);
    connect(markAsJunk, &QAction::triggered, this, &MainWindow::handleMarkAsJunk);

    markAsNotJunk = ShortcutHandler::instance()->createAction(QStringLiteral("action_mark_as_notjunk"), this);
    markAsNotJunk->setCheckable(true);
    connect(markAsNotJunk, &QAction::triggered, this, &MainWindow::handleMarkAsNotJunk);

    saveWholeMessage = ShortcutHandler::instance()->createAction(QStringLiteral("action_save_message_as"), this, SLOT(slotSaveCurrentMessageBody()), this);
    msgListWidget->tree->addAction(saveWholeMessage);

    viewMsgSource = ShortcutHandler::instance()->createAction(QStringLiteral("action_view_message_source"), this, SLOT(slotViewMsgSource()), this);
    msgListWidget->tree->addAction(viewMsgSource);

    viewMsgHeaders = ShortcutHandler::instance()->createAction(QStringLiteral("action_view_message_headers"), this, SLOT(slotViewMsgHeaders()), this);
    msgListWidget->tree->addAction(viewMsgHeaders);

    msgListWidget->tree->addAction(ShortcutHandler::instance()->createAction(QStringLiteral("action_open_messagewindow"), this,
            SLOT(openCompleteMessageWidget()), this));

    moveToArchive = ShortcutHandler::instance()->createAction(QStringLiteral("action_archive"), this);
    connect(moveToArchive, &QAction::triggered, this, &MainWindow::handleMoveToArchive);

    auto addTagAction = [=](int row) {
        QAction *tag = ShortcutHandler::instance()->createAction(QStringLiteral("action_tag_").append(QString::number(row)), this);
        tag->setCheckable(true);
        msgListWidget->tree->addAction(tag);
        connect(tag, &QAction::triggered, this, [=](const bool checked) {
            handleTag(checked, row - 1);
        });
        return tag;
    };
    tag1 = addTagAction(1);
    tag2 = addTagAction(2);
    tag3 = addTagAction(3);
    tag4 = addTagAction(4);
    tag5 = addTagAction(5);

    //: "mailbox" as a "folder of messages", not as a "mail account"
    createChildMailbox = new QAction(tr("Create &Child Mailbox..."), this);
    connect(createChildMailbox, &QAction::triggered, this, &MainWindow::slotCreateMailboxBelowCurrent);

    //: "mailbox" as a "folder of messages", not as a "mail account"
    createTopMailbox = new QAction(tr("Create &New Mailbox..."), this);
    connect(createTopMailbox, &QAction::triggered, this, &MainWindow::slotCreateTopMailbox);

    m_actionMarkMailboxAsRead = new QAction(tr("&Mark Mailbox as Read"), this);
    connect(m_actionMarkMailboxAsRead, &QAction::triggered, this, &MainWindow::slotMarkCurrentMailboxRead);

    //: "mailbox" as a "folder of messages", not as a "mail account"
    deleteCurrentMailbox = new QAction(tr("&Remove Mailbox"), this);
    connect(deleteCurrentMailbox, &QAction::triggered, this, &MainWindow::slotDeleteCurrentMailbox);

#ifdef XTUPLE_CONNECT
    xtIncludeMailboxInSync = new QAction(tr("&Synchronize with xTuple"), this);
    xtIncludeMailboxInSync->setCheckable(true);
    connect(xtIncludeMailboxInSync, SIGNAL(triggered()), this, SLOT(slotXtSyncCurrentMailbox()));
#endif

    m_replyPrivate = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_private"), this, SLOT(slotReplyTo()), this);
    m_replyPrivate->setEnabled(false);

    m_replyAllButMe = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_all_but_me"), this, SLOT(slotReplyAllButMe()), this);
    m_replyAllButMe->setEnabled(false);

    m_replyAll = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_all"), this, SLOT(slotReplyAll()), this);
    m_replyAll->setEnabled(false);

    m_replyList = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_list"), this, SLOT(slotReplyList()), this);
    m_replyList->setEnabled(false);

    m_replyGuess = ShortcutHandler::instance()->createAction(QStringLiteral("action_reply_guess"), this, SLOT(slotReplyGuess()), this);
    m_replyGuess->setEnabled(true);

    actionThreadMsgList = new QAction(UiUtils::loadIcon(QStringLiteral("format-justify-right")), tr("Show Messages in &Threads"), this);
    actionThreadMsgList->setCheckable(true);
    // This action is enabled/disabled by model's capabilities
    actionThreadMsgList->setEnabled(false);
    if (m_settings->value(Common::SettingsNames::guiMsgListShowThreading).toBool()) {
        actionThreadMsgList->setChecked(true);
        // The actual threading will be performed only when model updates its capabilities
    }
    connect(actionThreadMsgList, &QAction::triggered, this, &MainWindow::slotThreadMsgList);

    QActionGroup *sortOrderGroup = new QActionGroup(this);
    m_actionSortAscending = new QAction(tr("&Ascending"), sortOrderGroup);
    m_actionSortAscending->setCheckable(true);
    m_actionSortAscending->setChecked(true);
    m_actionSortDescending = new QAction(tr("&Descending"), sortOrderGroup);
    m_actionSortDescending->setCheckable(true);
    // QActionGroup has no toggle signal, but connecting descending will implicitly catch the acscending complement ;-)
    connect(m_actionSortDescending, &QAction::toggled, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(m_actionSortDescending, &QAction::toggled, this, &MainWindow::slotScrollToCurrent);
    connect(sortOrderGroup, &QActionGroup::triggered, this, &MainWindow::slotSortingPreferenceChanged);

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
    connect(sortColumnGroup, &QActionGroup::triggered, this, &MainWindow::slotSortingPreferenceChanged);
    slotSortingConfirmed(-1, Qt::AscendingOrder);

    actionHideRead = new QAction(tr("&Hide Read Messages"), this);
    actionHideRead->setCheckable(true);
    if (m_settings->value(Common::SettingsNames::guiMsgListHideRead).toBool()) {
        actionHideRead->setChecked(true);
        prettyMsgListModel->setHideRead(true);
    }
    connect(actionHideRead, &QAction::triggered, this, &MainWindow::slotHideRead);

    QActionGroup *layoutGroup = new QActionGroup(this);
    m_actionLayoutCompact = new QAction(tr("&Compact"), layoutGroup);
    m_actionLayoutCompact->setCheckable(true);
    m_actionLayoutCompact->setChecked(true);
    connect(m_actionLayoutCompact, &QAction::triggered, this, &MainWindow::slotLayoutCompact);
    m_actionLayoutWide = new QAction(tr("&Wide"), layoutGroup);
    m_actionLayoutWide->setCheckable(true);
    connect(m_actionLayoutWide, &QAction::triggered, this, &MainWindow::slotLayoutWide);
    m_actionLayoutOneAtTime = new QAction(tr("&One At Time"), layoutGroup);
    m_actionLayoutOneAtTime->setCheckable(true);
    connect(m_actionLayoutOneAtTime, &QAction::triggered, this, &MainWindow::slotLayoutOneAtTime);


    m_actionShowOnlySubscribed = new QAction(tr("Show Only S&ubscribed Folders"), this);
    m_actionShowOnlySubscribed->setCheckable(true);
    m_actionShowOnlySubscribed->setEnabled(false);
    connect(m_actionShowOnlySubscribed, &QAction::toggled, this, &MainWindow::slotShowOnlySubscribed);
    m_actionSubscribeMailbox = new QAction(tr("Su&bscribed"), this);
    m_actionSubscribeMailbox->setCheckable(true);
    m_actionSubscribeMailbox->setEnabled(false);
    connect(m_actionSubscribeMailbox, &QAction::triggered, this, &MainWindow::slotSubscribeCurrentMailbox);

    aboutTrojita = new QAction(trUtf8("&About Trojitá..."), this);
    connect(aboutTrojita, &QAction::triggered, this, &MainWindow::slotShowAboutTrojita);

    donateToTrojita = new QAction(tr("&Donate to the project"), this);
    connect(donateToTrojita, &QAction::triggered, this, &MainWindow::slotDonateToTrojita);

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
    m_mainToolbar->addAction(m_forwardAsAttachment);
    m_mainToolbar->addAction(expunge);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(markAsRead);
    m_mainToolbar->addAction(markAsDeleted);
    m_mainToolbar->addAction(markAsFlagged);
    m_mainToolbar->addAction(markAsJunk);
    m_mainToolbar->addAction(markAsNotJunk);
    m_mainToolbar->addAction(moveToArchive);

    // Push the status indicators all the way to the other side of the toolbar -- either to the far right, or far bottom.
    QWidget *toolbarSpacer = new QWidget(m_mainToolbar);
    toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mainToolbar->addWidget(toolbarSpacer);

    m_mainToolbar->addSeparator();
    m_mainToolbar->addWidget(busyParsersIndicator);

    networkIndicator = new QToolButton(this);
    // This is deliberate; we want to show this button in the same style as the other ones in the toolbar
    networkIndicator->setPopupMode(QToolButton::MenuButtonPopup);
    m_mainToolbar->addWidget(networkIndicator);

    m_menuFromToolBar = new QToolButton(this);
    m_menuFromToolBar->setIcon(UiUtils::loadIcon(QStringLiteral("menu_new")));
    m_menuFromToolBar->setText(QChar(0x205d)); // Unicode 'TRICOLON'
    m_menuFromToolBar->setPopupMode(QToolButton::MenuButtonPopup);
    connect(m_menuFromToolBar, &QAbstractButton::clicked, m_menuFromToolBar, &QToolButton::showMenu);
    m_mainToolbar->addWidget(m_menuFromToolBar);
    connect(showMenuBar, &QAction::toggled, [this](const bool menuBarVisible) {
        // https://bugreports.qt.io/browse/QTBUG-35768 , we have to work on the QAction, not QToolButton
        m_mainToolbar->actions().last()->setVisible(!menuBarVisible);
    });
    m_mainToolbar->actions().last()->setVisible(false); // initial state to complement the default of the QMenuBar's visibility

    busyParsersIndicator->setFixedSize(m_mainToolbar->iconSize());

    {
        // Custom widgets which are added into a QToolBar are by default aligned to the left, while QActions are justified.
        // That sucks, because some of our widgets use multiple actions with an expanding arrow at right.
        // Make sure everything is aligned to the left, so that the actual buttons are aligned properly and the extra arrows
        // are, well, at right.
        // I have no idea how this works on RTL layouts.
        QLayout *lay = m_mainToolbar->layout();
        for (int i = 0; i < lay->count(); ++i) {
            QLayoutItem *it = lay->itemAt(i);
            if (it->widget() == toolbarSpacer) {
                // Don't align this one, otherwise it won't push stuff when in horizontal direction
                continue;
            }
            if (it->widget() == busyParsersIndicator) {
                // It looks much better when centered
                it->setAlignment(Qt::AlignJustify);
                continue;
            }
            it->setAlignment(Qt::AlignLeft);
        }
    }

    updateMessageFlags();
}

void MainWindow::connectModelActions()
{
    connect(reloadAllMailboxes, &QAction::triggered, imapModel(), &Imap::Mailbox::Model::reloadMailboxList);
    connect(netOffline, &QAction::triggered,
            qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()), &Imap::Mailbox::NetworkWatcher::setNetworkOffline);
    connect(netExpensive, &QAction::triggered,
            qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()), &Imap::Mailbox::NetworkWatcher::setNetworkExpensive);
    connect(netOnline, &QAction::triggered,
            qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()), &Imap::Mailbox::NetworkWatcher::setNetworkOnline);
    netExpensive->setEnabled(imapAccess()->isConfigured());
    netOnline->setEnabled(imapAccess()->isConfigured());
}

void MainWindow::createMenus()
{
#define ADD_ACTION(MENU, ACTION) \
    MENU->addAction(ACTION); \
    addAction(ACTION);

    QMenu *imapMenu = menuBar()->addMenu(tr("&IMAP"));
    imapMenu->addMenu(m_composeMenu);
    ADD_ACTION(imapMenu, m_actionContactEditor);
    ADD_ACTION(imapMenu, m_replyGuess);
    ADD_ACTION(imapMenu, m_replyPrivate);
    ADD_ACTION(imapMenu, m_replyAll);
    ADD_ACTION(imapMenu, m_replyAllButMe);
    ADD_ACTION(imapMenu, m_replyList);
    imapMenu->addSeparator();
    ADD_ACTION(imapMenu, m_forwardAsAttachment);
    ADD_ACTION(imapMenu, m_bounce);
    imapMenu->addSeparator();
    ADD_ACTION(imapMenu, expunge);
    imapMenu->addSeparator()->setText(tr("Network Access"));
    QMenu *netPolicyMenu = imapMenu->addMenu(tr("&Network Access"));
    ADD_ACTION(netPolicyMenu, netOffline);
    ADD_ACTION(netPolicyMenu, netExpensive);
    ADD_ACTION(netPolicyMenu, netOnline);
    QMenu *debugMenu = imapMenu->addMenu(tr("&Debugging"));
    ADD_ACTION(debugMenu, showFullView);
    ADD_ACTION(debugMenu, showTaskView);
    ADD_ACTION(debugMenu, showMimeView);
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

    QMenu *mainMenuBehindToolBar = new QMenu(this);
    m_menuFromToolBar->setMenu(mainMenuBehindToolBar);
    m_menuFromToolBar->menu()->addMenu(imapMenu);
    m_menuFromToolBar->menu()->addMenu(viewMenu);
    m_menuFromToolBar->menu()->addMenu(mailboxMenu);
    m_menuFromToolBar->menu()->addMenu(helpMenu);
    m_menuFromToolBar->menu()->addSeparator();
    m_menuFromToolBar->menu()->addAction(showMenuBar);

    networkIndicator->setMenu(netPolicyMenu);
    m_netToolbarDefaultAction = new QAction(this);
    networkIndicator->setDefaultAction(m_netToolbarDefaultAction);
    connect(m_netToolbarDefaultAction, &QAction::triggered, networkIndicator, &QToolButton::showMenu);
    connect(netOffline, &QAction::toggled, this, &MainWindow::updateNetworkIndication);
    connect(netExpensive, &QAction::toggled, this, &MainWindow::updateNetworkIndication);
    connect(netOnline, &QAction::toggled, this, &MainWindow::updateNetworkIndication);

#undef ADD_ACTION
}

void MainWindow::createWidgets()
{
    // The state of the GUI is only saved after a certain time has passed. This is just an optimization to make sure
    // we do not hit the disk continually when e.g. resizing some random widget.
    m_delayedStateSaving = new QTimer(this);
    m_delayedStateSaving->setInterval(1000);
    m_delayedStateSaving->setSingleShot(true);
    connect(m_delayedStateSaving, &QTimer::timeout, this, &MainWindow::saveSizesAndState);

    mboxTree = new MailBoxTreeView();
    mboxTree->setDesiredExpansion(m_settings->value(Common::SettingsNames::guiExpandedMailboxes).toStringList());
    connect(mboxTree, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenuMboxTree);
    connect(mboxTree, &MailBoxTreeView::mailboxExpansionChanged, this, [this](const QStringList &mailboxNames) {
        m_settings->setValue(Common::SettingsNames::guiExpandedMailboxes, mailboxNames);
    });

    msgListWidget = new MessageListWidget();
    msgListWidget->tree->setContextMenuPolicy(Qt::CustomContextMenu);
    msgListWidget->tree->setAlternatingRowColors(true);
    msgListWidget->setRawSearchEnabled(m_settings->value(Common::SettingsNames::guiAllowRawSearch).toBool());
    connect (msgListWidget, &MessageListWidget::rawSearchSettingChanged, this, &MainWindow::saveRawStateSetting);

    connect(msgListWidget->tree, &QWidget::customContextMenuRequested, this, &MainWindow::showContextMenuMsgListTree);
    connect(msgListWidget->tree, &QAbstractItemView::activated, this, &MainWindow::msgListClicked);
    connect(msgListWidget->tree, &QAbstractItemView::clicked, this, &MainWindow::msgListClicked);
    connect(msgListWidget->tree, &QAbstractItemView::doubleClicked, this, &MainWindow::openCompleteMessageWidget);
    connect(msgListWidget, &MessageListWidget::requestingSearch, this, &MainWindow::slotSearchRequested);
    connect(msgListWidget->tree->header(), &QHeaderView::sectionMoved, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(msgListWidget->tree->header(), &QHeaderView::sectionResized, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));

    msgListWidget->tree->installEventFilter(this);

    m_messageWidget = new CompleteMessageWidget(this, m_settings, m_pluginManager, m_favoriteTags);
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, &MainWindow::scrollMessageUp);
    connect(m_messageWidget->messageView, &MessageView::messageChanged, this, &MainWindow::slotUpdateMessageActions);
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    connect(m_messageWidget->messageView, &MessageView::linkHovered, [](const QString &url) {
        if (url.isEmpty()) {
            QToolTip::hideText();
        } else {
            // indirection due to https://bugs.kde.org/show_bug.cgi?id=363783
            QTimer::singleShot(250, [url]() {
                QToolTip::showText(QCursor::pos(), QObject::tr("Link target: %1").arg(UiUtils::Formatting::htmlEscaped(url)));
            });
        }
    });
#endif
    connect(m_messageWidget->messageView, &MessageView::transferError, this, &MainWindow::slotDownloadTransferError);
    // Do not try to get onto the homepage when we are on EXPENSIVE connection
    if (m_settings->value(Common::SettingsNames::appLoadHomepage, QVariant(true)).toBool() &&
        m_imapAccess->preferredNetworkPolicy() == Imap::Mailbox::NETWORK_ONLINE) {
        m_messageWidget->messageView->setHomepageUrl(QUrl(QStringLiteral("http://welcome.trojita.flaska.net/%1").arg(Common::Application::version)));
    }

    allDock = new QDockWidget(tr("Everything"), this);
    allDock->setObjectName(QStringLiteral("allDock"));
    allTree = new QTreeView(allDock);
    allDock->hide();
    allTree->setUniformRowHeights(true);
    allTree->setHeaderHidden(true);
    allDock->setWidget(allTree);
    addDockWidget(Qt::LeftDockWidgetArea, allDock);
    taskDock = new QDockWidget(tr("IMAP Tasks"), this);
    taskDock->setObjectName(QStringLiteral("taskDock"));
    taskTree = new QTreeView(taskDock);
    taskDock->hide();
    taskTree->setHeaderHidden(true);
    taskDock->setWidget(taskTree);
    addDockWidget(Qt::LeftDockWidgetArea, taskDock);
    mailMimeDock = new QDockWidget(tr("MIME Tree"), this);
    mailMimeDock->setObjectName(QStringLiteral("mailMimeDock"));
    mailMimeTree = new QTreeView(mailMimeDock);
    mailMimeDock->hide();
    mailMimeTree->setUniformRowHeights(true);
    mailMimeTree->setHeaderHidden(true);
    mailMimeDock->setWidget(mailMimeTree);
    addDockWidget(Qt::RightDockWidgetArea, mailMimeDock);
    connect(m_messageWidget->messageView, &MessageView::messageModelChanged, this, &MainWindow::slotMessageModelChanged);

    imapLoggerDock = new QDockWidget(tr("IMAP Protocol"), this);
    imapLoggerDock->setObjectName(QStringLiteral("imapLoggerDock"));
    imapLogger = new ProtocolLoggerWidget(imapLoggerDock);
    imapLoggerDock->hide();
    imapLoggerDock->setWidget(imapLogger);
    addDockWidget(Qt::BottomDockWidgetArea, imapLoggerDock);

    busyParsersIndicator = new TaskProgressIndicator(this);
}

void MainWindow::setupModels()
{
    m_imapAccess->reloadConfiguration();
    m_imapAccess->doConnect();

    m_messageWidget->messageView->setNetworkWatcher(qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()));

    auto realThreadingModel = qobject_cast<Imap::Mailbox::ThreadingMsgListModel*>(m_imapAccess->threadingMsgListModel());
    Q_ASSERT(realThreadingModel);
    auto realMsgListModel = qobject_cast<Imap::Mailbox::MsgListModel*>(m_imapAccess->msgListModel());
    Q_ASSERT(realMsgListModel);

    prettyMboxModel = new Imap::Mailbox::PrettyMailboxModel(this, qobject_cast<QAbstractItemModel *>(m_imapAccess->mailboxModel()));
    prettyMboxModel->setObjectName(QStringLiteral("prettyMboxModel"));
    connect(realThreadingModel, &Imap::Mailbox::ThreadingMsgListModel::sortingFailed,
            msgListWidget, &MessageListWidget::slotSortingFailed);
    prettyMsgListModel = new Imap::Mailbox::PrettyMsgListModel(this, m_favoriteTags);
    prettyMsgListModel->setSourceModel(m_imapAccess->threadingMsgListModel());
    prettyMsgListModel->setObjectName(QStringLiteral("prettyMsgListModel"));

    connect(mboxTree, &MailBoxTreeView::clicked,
            realMsgListModel,
            static_cast<void (Imap::Mailbox::MsgListModel::*)(const QModelIndex &)>(&Imap::Mailbox::MsgListModel::setMailbox));
    connect(mboxTree, &MailBoxTreeView::activated,
            realMsgListModel,
            static_cast<void (Imap::Mailbox::MsgListModel::*)(const QModelIndex &)>(&Imap::Mailbox::MsgListModel::setMailbox));
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::dataChanged, this, &MainWindow::updateMessageFlags);
    connect(qobject_cast<Imap::Mailbox::MsgListModel*>(m_imapAccess->msgListModel()), &Imap::Mailbox::MsgListModel::messagesAvailable,
            this, &MainWindow::slotScrollToUnseenMessage);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsInserted, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsRemoved, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateMessageFlags);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::layoutChanged, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::layoutChanged, this, &MainWindow::updateMessageFlags);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::modelReset, msgListWidget, &MessageListWidget::slotAutoEnableDisableSearch);
    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::modelReset, this, &MainWindow::updateMessageFlags);
    connect(realMsgListModel, &Imap::Mailbox::MsgListModel::mailboxChanged, this, &MainWindow::slotMailboxChanged);

    connect(imapModel(), &Imap::Mailbox::Model::alertReceived, this, &MainWindow::alertReceived);
    connect(imapModel(), &Imap::Mailbox::Model::imapError, this, &MainWindow::imapError);
    connect(imapModel(), &Imap::Mailbox::Model::networkError, this, &MainWindow::networkError);
    connect(imapModel(), &Imap::Mailbox::Model::authRequested, this, &MainWindow::authenticationRequested, Qt::QueuedConnection);
    connect(imapModel(), &Imap::Mailbox::Model::authAttemptFailed, this, [this]() {
        m_ignoreStoredPassword = true;
    });

    connect(imapModel(), &Imap::Mailbox::Model::networkPolicyOffline, this, &MainWindow::networkPolicyOffline);
    connect(imapModel(), &Imap::Mailbox::Model::networkPolicyExpensive, this, &MainWindow::networkPolicyExpensive);
    connect(imapModel(), &Imap::Mailbox::Model::networkPolicyOnline, this, &MainWindow::networkPolicyOnline);
    connect(imapModel(), &Imap::Mailbox::Model::connectionStateChanged, this, [this](uint, const Imap::ConnectionState state) {
        if (state == Imap::CONN_STATE_AUTHENTICATED) {
            m_ignoreStoredPassword = false;
        }
    });
    connect(imapModel(), &Imap::Mailbox::Model::connectionStateChanged, this, &MainWindow::showConnectionStatus);

    connect(imapModel(), &Imap::Mailbox::Model::mailboxDeletionFailed, this, &MainWindow::slotMailboxDeleteFailed);
    connect(imapModel(), &Imap::Mailbox::Model::mailboxCreationFailed, this, &MainWindow::slotMailboxCreateFailed);
    connect(imapModel(), &Imap::Mailbox::Model::mailboxSyncFailed, this, &MainWindow::slotMailboxSyncFailed);

    connect(imapModel(), &Imap::Mailbox::Model::logged, imapLogger, &ProtocolLoggerWidget::slotImapLogged);
    connect(imapModel(), &Imap::Mailbox::Model::connectionStateChanged, imapLogger, &ProtocolLoggerWidget::onConnectionClosed);

    auto nw = qobject_cast<Imap::Mailbox::NetworkWatcher *>(m_imapAccess->networkWatcher());
    Q_ASSERT(nw);
    connect(nw, &Imap::Mailbox::NetworkWatcher::reconnectAttemptScheduled,
            this, [this](const int timeout) {
            showStatusMessage(tr("Attempting to reconnect in %n seconds..", 0, timeout/1000));
            });
    connect(nw, &Imap::Mailbox::NetworkWatcher::resetReconnectState, this, &MainWindow::slotResetReconnectState);

    connect(imapModel(), &Imap::Mailbox::Model::mailboxFirstUnseenMessage, this, &MainWindow::slotScrollToUnseenMessage);

    connect(imapModel(), &Imap::Mailbox::Model::capabilitiesUpdated, this, &MainWindow::slotCapabilitiesUpdated);

    connect(m_imapAccess->msgListModel(), &QAbstractItemModel::modelReset, this, &MainWindow::slotUpdateWindowTitle);
    connect(imapModel(), &Imap::Mailbox::Model::messageCountPossiblyChanged, this, &MainWindow::slotUpdateWindowTitle);

    connect(prettyMsgListModel, &Imap::Mailbox::PrettyMsgListModel::sortingPreferenceChanged, this, &MainWindow::slotSortingConfirmed);

    //Imap::Mailbox::ModelWatcher* w = new Imap::Mailbox::ModelWatcher( this );
    //w->setModel( imapModel() );

    //ModelTest* tester = new ModelTest( prettyMboxModel, this ); // when testing, test just one model at time

    mboxTree->setModel(prettyMboxModel);
    msgListWidget->tree->setModel(prettyMsgListModel);
    connect(msgListWidget->tree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::updateMessageFlags);

    allTree->setModel(imapModel());
    taskTree->setModel(imapModel()->taskModel());
    connect(imapModel()->taskModel(), &QAbstractItemModel::layoutChanged, taskTree, &QTreeView::expandAll);
    connect(imapModel()->taskModel(), &QAbstractItemModel::modelReset, taskTree, &QTreeView::expandAll);
    connect(imapModel()->taskModel(), &QAbstractItemModel::rowsInserted, taskTree, &QTreeView::expandAll);
    connect(imapModel()->taskModel(), &QAbstractItemModel::rowsRemoved, taskTree, &QTreeView::expandAll);
    connect(imapModel()->taskModel(), &QAbstractItemModel::rowsMoved, taskTree, &QTreeView::expandAll);

    busyParsersIndicator->setImapModel(imapModel());

    auto accountIconName = m_settings->value(Common::SettingsNames::imapAccountIcon).toString();
    if (accountIconName.isEmpty()) {
        qApp->setWindowIcon(UiUtils::loadIcon(QStringLiteral("trojita")));
    } else if (accountIconName.contains(QDir::separator())) {
        // Absolute paths are OK for users, but unsupported by our icon loader
        qApp->setWindowIcon(QIcon(accountIconName));
    } else {
        qApp->setWindowIcon(UiUtils::loadIcon(accountIconName));
    }
}

void MainWindow::createSysTray()
{
    if (m_trayIcon)
        return;

    qApp->setQuitOnLastWindowClosed(false);

    m_trayIcon = new QSystemTrayIcon(this);
    handleTrayIconChange();

    QAction* quitAction = new QAction(tr("&Quit"), m_trayIcon);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    QMenu *trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(quitAction);
    m_trayIcon->setContextMenu(trayIconMenu);

    // QMenu cannot be a child of QSystemTrayIcon, and we don't want the QMenu in MainWindow scope.
    connect(m_trayIcon, &QObject::destroyed, trayIconMenu, &QObject::deleteLater);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::slotIconActivated);
    connect(imapModel(), &Imap::Mailbox::Model::messageCountPossiblyChanged, this, &MainWindow::handleTrayIconChange);
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
    if (showSystray && !m_trayIcon && QSystemTrayIcon::isSystemTrayAvailable()) {
        createSysTray();
    } else if (!showSystray && m_trayIcon) {
        removeSysTray();
    }
}

void MainWindow::handleTrayIconChange()
{
    if (!m_trayIcon)
        return;

    const bool isOffline = qobject_cast<Imap::Mailbox::NetworkWatcher *>(m_imapAccess->networkWatcher())->effectiveNetworkPolicy()
            == Imap::Mailbox::NETWORK_OFFLINE;
    auto pixmap = qApp->windowIcon()
                .pixmap(QSize(32, 32), isOffline ? QIcon::Disabled : QIcon::Normal);
    QString tooltip;
    auto profileName = QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
    if (profileName.isEmpty()) {
        tooltip = QStringLiteral("Trojitá");
    } else {
        tooltip = QStringLiteral("Trojitá [%1]").arg(profileName);
    }

    uint unreadCount = 0;
    bool numbersValid = false;

    auto watchingMode = settings()->value(Common::SettingsNames::watchedFoldersKey).toString();
    if (watchingMode == Common::SettingsNames::watchAll || watchingMode == Common::SettingsNames::watchSubscribed) {
        bool subscribedOnly = watchingMode == Common::SettingsNames::watchSubscribed;
        unreadCount = std::accumulate(UiUtils::QaimDfsIterator(m_imapAccess->mailboxModel()->index(0, 0)),
                                      UiUtils::QaimDfsIterator(), 0, [subscribedOnly](const uint acc, const QModelIndex &idx) {

            if (subscribedOnly && !idx.data(Imap::Mailbox::RoleMailboxIsSubscribed).toBool())
                return acc;

            auto x = idx.data(Imap::Mailbox::RoleUnreadMessageCount).toInt();
            if (x > 0) {
                return acc + x;
            } else {
                return acc;
            }
        });
        // only show stuff if there are some mailboxes, and if there are such messages
        numbersValid = m_imapAccess->mailboxModel()->hasChildren() && unreadCount > 0;

    } else {
        // just for the INBOX
        QModelIndex mailbox = imapModel()->index(1, 0, QModelIndex());
        if (mailbox.isValid() && mailbox.data(Imap::Mailbox::RoleMailboxName).toString() == QLatin1String("INBOX")
                && mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt() > 0) {
            unreadCount = mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt();
            numbersValid = true;
        }
    }

    if (numbersValid) {
        QFont f;
        f.setPixelSize(pixmap.height() * 0.59);
        f.setWeight(QFont::Bold);

        QString text = QString::number(unreadCount);
        QFontMetrics fm(f);
        if (unreadCount > 666) {
            // You just have too many messages.
            text = QStringLiteral("🐮");
            fm = QFontMetrics(f);
        } else if (fm.width(text) > pixmap.width()) {
            f.setPixelSize(f.pixelSize() * pixmap.width() / fm.width(text));
            fm = QFontMetrics(f);
        }

        QRect boundingRect = fm.tightBoundingRect(text);
        boundingRect.setWidth(boundingRect.width() + 2);
        boundingRect.setHeight(boundingRect.height() + 2);
        boundingRect.moveCenter(QPoint(pixmap.width() / 2, pixmap.height() / 2));
        boundingRect = boundingRect.intersected(pixmap.rect());

        QPainterPath path;
        path.addText(boundingRect.bottomLeft(), f, text);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QColor(255,255,255, 180));
        painter.setBrush(isOffline ? Qt::red : Qt::black);
        painter.drawPath(path);

        //: This is a tooltip for the tray icon. It will be prefixed by something like "Trojita" or "Trojita [work]"
        tooltip += trUtf8(" - %n unread message(s)", 0, unreadCount);
    } else if (isOffline) {
        //: A tooltip suffix when offline. The prefix is something like "Trojita" or "Trojita [work]"
        tooltip += tr(" - offline");
    }
    m_trayIcon->setToolTip(tooltip);
    m_trayIcon->setIcon(QIcon(pixmap));
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
    if (msgListWidget && o == msgListWidget->tree && m_messageWidget->messageView) {
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
            if (keyEvent->key() == Qt::Key_Space || keyEvent->key() == Qt::Key_Backspace) {
                QCoreApplication::sendEvent(m_messageWidget, keyEvent);
                return true;
            }
            return false;
        }
        return false;
    }
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
        if (isVisible())
            showMainWindow();
    }
}

void MainWindow::showMainWindow()
{
    setVisible(true);
    activateWindow();
    raise();
}

void MainWindow::msgListClicked(const QModelIndex &index)
{
    Q_ASSERT(index.isValid());

    if (qApp->keyboardModifiers() & Qt::ShiftModifier || qApp->keyboardModifiers() & Qt::ControlModifier)
        return;

    if (! index.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;

    // Because it's quite possible that we have switched into another mailbox, make sure that we're in the "current" one so that
    // user will be notified about new arrivals, etc.
    QModelIndex translated = Imap::deproxifiedIndex(index);
    imapModel()->switchToMailbox(translated.parent().parent());

    if (index.column() == Imap::Mailbox::MsgListModel::SEEN) {
        if (!translated.data(Imap::Mailbox::RoleIsFetched).toBool())
            return;
        Imap::Mailbox::FlagsOperation flagOp = translated.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool() ?
                                               Imap::Mailbox::FLAG_REMOVE : Imap::Mailbox::FLAG_ADD;
        imapModel()->markMessagesRead(QModelIndexList() << translated, flagOp);

        if (translated == m_messageWidget->messageView->currentMessage()) {
            m_messageWidget->messageView->stopAutoMarkAsRead();
        }
    } else if (index.column() == Imap::Mailbox::MsgListModel::FLAGGED) {
        if (!translated.data(Imap::Mailbox::RoleIsFetched).toBool())
            return;

        Imap::Mailbox::FlagsOperation flagOp = translated.data(Imap::Mailbox::RoleMessageIsMarkedFlagged).toBool() ?
                                               Imap::Mailbox::FLAG_REMOVE : Imap::Mailbox::FLAG_ADD;
        imapModel()->setMessageFlags(QModelIndexList() << translated, Imap::Mailbox::FlagNames::flagged, flagOp);
    } else {
        if ((m_messageWidget->isVisible() && !m_messageWidget->size().isEmpty()) || m_layoutMode == LAYOUT_ONE_AT_TIME) {
            // isVisible() won't work, the splitter manipulates width, not the visibility state
            m_messageWidget->messageView->setMessage(index);
        }
        msgListWidget->tree->setCurrentIndex(index);
    }
}

void MainWindow::openCompleteMessageWidget()
{
    const QModelIndex index = msgListWidget->tree->currentIndex();

    if (! index.data(Imap::Mailbox::RoleMessageUid).isValid())
        return;

    CompleteMessageWidget *widget = new CompleteMessageWidget(0, m_settings, m_pluginManager, m_favoriteTags);
    widget->messageView->setMessage(index);
    widget->messageView->setNetworkWatcher(qobject_cast<Imap::Mailbox::NetworkWatcher*>(m_imapAccess->networkWatcher()));
    widget->setFocusPolicy(Qt::StrongFocus);
    widget->setWindowTitle(index.data(Imap::Mailbox::RoleMessageSubject).toString());
    widget->setAttribute(Qt::WA_DeleteOnClose);
    QAction *closeAction = ShortcutHandler::instance()->createAction(QStringLiteral("action_messagewindow_close"), widget, SLOT(close()), widget);
    widget->addAction(closeAction);
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
        updateMessageFlagsOf(index);
        actionList.append(markAsRead);
        actionList.append(markAsDeleted);
        actionList.append(markAsFlagged);
        actionList.append(markAsJunk);
        actionList.append(markAsNotJunk);
        actionList.append(moveToArchive);
        actionList.append(m_actionMarkMailboxAsRead);
        actionList.append(saveWholeMessage);
        actionList.append(viewMsgSource);
        actionList.append(viewMsgHeaders);
        auto appendTagIfExists = [this,&actionList](const int row, QAction *tag) {
            if (m_favoriteTags->rowCount() > row - 1)
                actionList.append(tag);
        };
        appendTagIfExists(1, tag1);
        appendTagIfExists(2, tag2);
        appendTagIfExists(3, tag3);
        appendTagIfExists(4, tag4);
        appendTagIfExists(5, tag5);
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
        mbox->rescanForChildMailboxes(imapModel());
    }
}

/** @short Request a check for new messages in selected mailbox */
void MainWindow::slotResyncMbox()
{
    if (! imapModel()->isNetworkAvailable())
        return;

    Q_FOREACH(const QModelIndex &item, mboxTree->selectionModel()->selectedIndexes()) {
        Q_ASSERT(item.isValid());
        if (item.column() != 0)
            continue;
        imapModel()->resyncMailbox(item);
    }
}

void MainWindow::alertReceived(const QString &message)
{
    //: "ALERT" is a special warning which we're required to show to the user
    Gui::Util::messageBoxWarning(this, tr("IMAP Alert"), message);
}

void MainWindow::imapError(const QString &message)
{
    Gui::Util::messageBoxCritical(this, tr("IMAP Protocol Error"), message);
    // Show the IMAP logger -- maybe some user will take that as a hint that they shall include it in the bug report.
    // </joke>
    showImapLogger->setChecked(true);
}

void MainWindow::networkError(const QString &message)
{
    const QString title = tr("Network Error");
    if (!m_networkErrorMessageBox) {
        m_networkErrorMessageBox = new QMessageBox(QMessageBox::Critical, title,
                                                   QString(), QMessageBox::Ok, this);
    }
    // User must be informed about a new (but not recurring) error
    if (message != m_networkErrorMessageBox->text()) {
        m_networkErrorMessageBox->setText(message);
        if (qApp->applicationState() == Qt::ApplicationActive) {
            m_networkErrorMessageBox->setProperty(netErrorUnseen, false);
            m_networkErrorMessageBox->show();
        } else {
            m_networkErrorMessageBox->setProperty(netErrorUnseen, true);
            if (m_trayIcon && m_trayIcon->isVisible())
                m_trayIcon->showMessage(title, message, QSystemTrayIcon::Warning, 3333);
        }
    }
}

void MainWindow::cacheError(const QString &message)
{
    Gui::Util::messageBoxCritical(this, tr("IMAP Cache Error"),
                                  tr("The caching subsystem managing a cache of the data already "
                                     "downloaded from the IMAP server is having troubles. "
                                     "All caching will be disabled.\n\n%1").arg(message));
}

void MainWindow::networkPolicyOffline()
{
    netExpensive->setChecked(false);
    netOnline->setChecked(false);
    netOffline->setChecked(true);
    updateActionsOnlineOffline(false);
    showStatusMessage(tr("Offline"));
    handleTrayIconChange();
}

void MainWindow::networkPolicyExpensive()
{
    netOffline->setChecked(false);
    netOnline->setChecked(false);
    netExpensive->setChecked(true);
    updateActionsOnlineOffline(true);
    handleTrayIconChange();
}

void MainWindow::networkPolicyOnline()
{
    netOffline->setChecked(false);
    netExpensive->setChecked(false);
    netOnline->setChecked(true);
    updateActionsOnlineOffline(true);
    handleTrayIconChange();
}

/** @short Deletes a network error message box instance upon resetting of reconnect state */
void MainWindow::slotResetReconnectState()
{
    if (m_networkErrorMessageBox) {
        delete m_networkErrorMessageBox;
        m_networkErrorMessageBox = 0;
    }
}

void MainWindow::slotShowSettings()
{
    SettingsDialog *dialog = new SettingsDialog(this, m_senderIdentities, m_favoriteTags, m_settings);
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
        Gui::Util::messageBoxCritical(this, tr("No Configuration"),
                                      trUtf8("No IMAP account is configured. Trojitá cannot do much without one."));
    }
    applySizesAndState();
}

void MainWindow::authenticationRequested()
{
    Plugins::PasswordPlugin *password = pluginManager()->password();
    if (password) {
        // FIXME: use another account-id at some point in future
        //        Currently the accountName will be empty unless Trojita has been
        //        called with a profile, and then the profile will be used as the
        //        accountName.
        QString accountName = m_imapAccess->accountName();
        if (accountName.isEmpty())
            accountName = QStringLiteral("account-0");
        Plugins::PasswordJob *job = password->requestPassword(accountName, QStringLiteral("imap"));
        if (job) {
            connect(job, &Plugins::PasswordJob::passwordAvailable, this, [this](const QString &password) {
                authenticationContinue(password);
            });
            connect(job, &Plugins::PasswordJob::error, this, [this](const Plugins::PasswordJob::Error error, const QString &message) {
                if (error == Plugins::PasswordJob::Error::NoSuchPassword) {
                    authenticationContinue(QString());
                } else {
                    authenticationContinue(QString(), tr("Failed to retrieve password from the store: %1").arg(message));
                }
            });
            job->setAutoDelete(true);
            job->start();
            return;
        }
    }

    authenticationContinue(QString());

}

void MainWindow::authenticationContinue(const QString &password, const QString &errorMessage)
{
    const QString &user = m_settings->value(Common::SettingsNames::imapUserKey).toString();
    QString pass = password;
    if (m_ignoreStoredPassword || pass.isEmpty()) {
        auto dialog = PasswordDialog::getPassword(this, tr("Authentication Required"),
                                                  tr("<p>Please provide IMAP password for user <b>%1</b> on <b>%2</b>:</p>").arg(
                                                      user.toHtmlEscaped(),
                                                      m_settings->value(Common::SettingsNames::imapHostKey).toString().toHtmlEscaped()
                                                      ),
                                                  errorMessage + (errorMessage.isEmpty() ? QString() : QStringLiteral("\n\n"))
                                                  + imapModel()->imapAuthError());
        connect(dialog, &PasswordDialog::gotPassword, imapModel(), &Imap::Mailbox::Model::setImapPassword);
        connect(dialog, &PasswordDialog::rejected, imapModel(), &Imap::Mailbox::Model::unsetImapPassword);
    } else {
        imapModel()->setImapPassword(pass);
    }
}

void MainWindow::checkSslPolicy()
{
    m_imapAccess->setSslPolicy(QMessageBox(static_cast<QMessageBox::Icon>(m_imapAccess->sslInfoIcon()),
                                           m_imapAccess->sslInfoTitle(), m_imapAccess->sslInfoMessage(),
                                           QMessageBox::Yes | QMessageBox::No, this).exec() == QMessageBox::Yes);
}

void MainWindow::nukeModels()
{
    m_messageWidget->messageView->setEmpty();
    mboxTree->setModel(0);
    msgListWidget->tree->setModel(0);
    allTree->setModel(0);
    taskTree->setModel(0);
    delete prettyMsgListModel;
    prettyMsgListModel = 0;
    delete prettyMboxModel;
    prettyMboxModel = 0;
}

void MainWindow::recoverDrafts()
{
    QDir draftPath(Common::writablePath(Common::LOCATION_CACHE) + QLatin1String("Drafts/"));
    QStringList drafts(draftPath.entryList(QStringList() << QStringLiteral("*.draft")));
    Q_FOREACH(const QString &draft, drafts) {
        ComposeWidget *w = ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createDraft(this, draftPath.filePath(draft)), this);
        // No need to further try creating widgets for drafts if a nullptr is being returned by ComposeWidget::warnIfMsaNotConfigured
        if (!w)
            break;
    }
}

void MainWindow::slotComposeMail()
{
    ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createBlank(this), this);
}

void MainWindow::slotEditDraft()
{
    QString path(Common::writablePath(Common::LOCATION_DATA) + tr("Drafts"));
    QDir().mkpath(path);
    path = QFileDialog::getOpenFileName(this, tr("Edit draft"), path, tr("Drafts") + QLatin1String(" (*.draft)"));
    if (!path.isNull()) {
        ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createDraft(this, path), this);
    }
}

QModelIndexList MainWindow::translatedSelection() const
{
    QModelIndexList translatedIndexes;
    QModelIndexList selected = msgListWidget->tree->selectionModel()->selectedIndexes();
    const int originalItems = selected.length(); // only check collapsed/expanded status on original selection
    for (int i = 0; i < selected.length(); ++i) {
        const QModelIndex item = selected[i];
        if (item.column() != 0 || !item.data(Imap::Mailbox::RoleMessageUid).isValid())
            continue;
        translatedIndexes << Imap::deproxifiedIndex(item);
        // Now see if this is a collapsed thread and include all the collapsed items as needed
        // Also note that this is recursive - each child found is run through this same item loop for validity/child checks as well
        if (i >= originalItems || !msgListWidget->tree->isExpanded(item)) {
            for (int j = 0; j < item.model()->rowCount(item); ++j) {
                selected << item.child(j, 0); // Make sure this is run through the main loop as well - don't add it directly
            }
        }
    }
    return translatedIndexes;
}

void MainWindow::handleMarkAsRead(bool value)
{
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsRead: no valid messages";
    } else {
        imapModel()->markMessagesRead(translatedIndexes, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
        if (translatedIndexes.contains(m_messageWidget->messageView->currentMessage())) {
            m_messageWidget->messageView->stopAutoMarkAsRead();
        }
    }
}

void MainWindow::slotNextUnread()
{
    QModelIndex current = msgListWidget->tree->currentIndex();

    UiUtils::gotoNext(msgListWidget->tree->model(), current,
    [](const QModelIndex &idx) { return !idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool(); },
    [this](const QModelIndex &idx) {
        Q_ASSERT(!idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool());
        m_messageWidget->messageView->setMessage(idx);
        msgListWidget->tree->setCurrentIndex(idx);
    },
    []() {
        // nothing to do
    });
}

void MainWindow::slotPreviousUnread()
{
    QModelIndex current = msgListWidget->tree->currentIndex();

    UiUtils::gotoPrevious(msgListWidget->tree->model(), current,
    [](const QModelIndex &idx) { return !idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool(); },
    [this](const QModelIndex &idx) {
        Q_ASSERT(!idx.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool());
        m_messageWidget->messageView->setMessage(idx);
        msgListWidget->tree->setCurrentIndex(idx);
    },
    []() {
        // nothing to do
    });
}

void MainWindow::handleTag(const bool checked, const int index)
{
    const QModelIndexList &translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleTag: no valid messages";
    } else {
        const auto &tagName = m_favoriteTags->tagNameByIndex(index);
        if (!tagName.isEmpty())
            imapModel()->setMessageFlags(translatedIndexes, tagName, checked ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
    }
}

void MainWindow::handleMarkAsDeleted(bool value)
{
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsDeleted: no valid messages";
    } else {
        imapModel()->markMessagesDeleted(translatedIndexes, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
    }
}

void MainWindow::handleMarkAsFlagged(const bool value)
{
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsFlagged: no valid messages";
    } else {
        imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::flagged, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
    }
}

void MainWindow::handleMarkAsJunk(const bool value)
{
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsJunk: no valid messages";
    } else {
        if (value) {
            imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::notjunk, Imap::Mailbox::FLAG_REMOVE);
        }
        imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::junk, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
    }
}

void MainWindow::handleMarkAsNotJunk(const bool value)
{
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMarkAsNotJunk: no valid messages";
    } else {
        if (value) {
          imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::junk, Imap::Mailbox::FLAG_REMOVE);
        }
        imapModel()->setMessageFlags(translatedIndexes, Imap::Mailbox::FlagNames::notjunk, value ? Imap::Mailbox::FLAG_ADD : Imap::Mailbox::FLAG_REMOVE);
    }
}

void MainWindow::slotMoveToArchiveFailed(const QString &error)
{
    // XXX disable busy cursor
    QMessageBox::critical(this, tr("Failed to archive"), error);
}

void MainWindow::handleMoveToArchive()
{
    const QModelIndexList translatedIndexes = translatedSelection();
    if (translatedIndexes.isEmpty()) {
        qDebug() << "Model::handleMoveToArchive: no valid messages";
    } else {
        auto archiveFolderName = m_settings->value(Common::SettingsNames::imapArchiveFolderName).toString();
        auto copyMoveMessagesTask = imapModel()->copyMoveMessages(
            archiveFolderName.isEmpty() ? Common::SettingsNames::imapDefaultArchiveFolderName : archiveFolderName,
            translatedIndexes, Imap::Mailbox::CopyMoveOperation::MOVE);
        connect(copyMoveMessagesTask, &Imap::Mailbox::ImapTask::failed, this, &MainWindow::slotMoveToArchiveFailed);
    }
}


void MainWindow::slotExpunge()
{
    imapModel()->expungeMailbox(qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel())->currentMailbox());
}

void MainWindow::slotMarkCurrentMailboxRead()
{
    imapModel()->markMailboxAsRead(mboxTree->currentIndex());
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
        imapModel()->createMailbox(targetName,
                                   ui.subscribe->isChecked() ?
                                       Imap::Mailbox::AutoSubscription::SUBSCRIBE :
                                       Imap::Mailbox::AutoSubscription::NO_EXPLICIT_SUBSCRIPTION
                                       );
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
        imapModel()->deleteMailbox(name);
    }
}

void MainWindow::updateMessageFlags()
{
    updateMessageFlagsOf(QModelIndex());
}

void MainWindow::updateMessageFlagsOf(const QModelIndex &index)
{
    QModelIndexList indexes = index.isValid() ? QModelIndexList() << index : translatedSelection();
    const bool isValid = !indexes.isEmpty() &&
                         // either we operate on the -already valided- selection or the index must be valid
                         (!index.isValid() || index.data(Imap::Mailbox::RoleMessageUid).toUInt() > 0);
    const bool okToModify = imapModel()->isNetworkAvailable() && isValid;

    markAsRead->setEnabled(okToModify);
    markAsDeleted->setEnabled(okToModify);
    markAsFlagged->setEnabled(okToModify);
    markAsJunk->setEnabled(okToModify);
    markAsNotJunk->setEnabled(okToModify);

    // There's no point in moving from Archive to, well, Archive
    auto archiveFolderName = m_settings->value(Common::SettingsNames::imapArchiveFolderName).toString();
    if (archiveFolderName.isEmpty()) {
        archiveFolderName = Common::SettingsNames::imapDefaultArchiveFolderName;
    }
    moveToArchive->setEnabled(okToModify &&
                              std::any_of(indexes.cbegin(), indexes.cend(),
                                          [archiveFolderName](const QModelIndex &i) {
        return i.data(Imap::Mailbox::RoleMailboxName) != archiveFolderName;
    }));

    tag1->setEnabled(okToModify);
    tag2->setEnabled(okToModify);
    tag3->setEnabled(okToModify);
    tag4->setEnabled(okToModify);
    tag5->setEnabled(okToModify);

    bool isRead    = isValid,
         isDeleted = isValid,
         isFlagged = isValid,
         isJunk    = isValid,
         isNotJunk = isValid,
         hasTag1   = isValid,
         hasTag2   = isValid,
         hasTag3   = isValid,
         hasTag4   = isValid,
         hasTag5   = isValid;
    auto updateTag = [=](const QModelIndex &i, bool &hasTag, int index) {
        if (hasTag && !m_favoriteTags->tagNameByIndex(index).isEmpty() &&
                !i.data(Imap::Mailbox::RoleMessageFlags).toStringList().contains(m_favoriteTags->tagNameByIndex(index)))
        {
            hasTag = false;
        }
    };
    Q_FOREACH (const QModelIndex &i, indexes) {
#define UPDATE_STATE(PROP) \
        if (is##PROP && !i.data(Imap::Mailbox::RoleMessageIsMarked##PROP).toBool()) \
            is##PROP = false;
        UPDATE_STATE(Read)
        UPDATE_STATE(Deleted)
        UPDATE_STATE(Flagged)
        UPDATE_STATE(Junk)
        UPDATE_STATE(NotJunk)
#undef UPDATE_STATE
        updateTag(i, hasTag1, 0);
        updateTag(i, hasTag2, 1);
        updateTag(i, hasTag3, 2);
        updateTag(i, hasTag4, 3);
        updateTag(i, hasTag5, 4);
    }
    markAsRead->setChecked(isRead);
    markAsDeleted->setChecked(isDeleted);
    markAsFlagged->setChecked(isFlagged);
    markAsJunk->setChecked(isJunk && !isNotJunk);
    markAsNotJunk->setChecked(isNotJunk && !isJunk);

    tag1->setChecked(hasTag1);
    tag2->setChecked(hasTag2);
    tag3->setChecked(hasTag3);
    tag4->setChecked(hasTag4);
    tag5->setChecked(hasTag5);
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
    updateMessageFlags();
    showImapCapabilities->setEnabled(online);
    if (!online) {
        m_replyGuess->setEnabled(false);
        m_replyPrivate->setEnabled(false);
        m_replyAll->setEnabled(false);
        m_replyAllButMe->setEnabled(false);
        m_replyList->setEnabled(false);
        m_forwardAsAttachment->setEnabled(false);
        m_bounce->setEnabled(false);
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

    m_forwardAsAttachment->setEnabled(m_messageWidget->messageView->currentMessage().isValid());
    m_bounce->setEnabled(m_messageWidget->messageView->currentMessage().isValid());
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

void MainWindow::slotForwardAsAttachment()
{
    m_messageWidget->messageView->forward(this, Composer::ForwardMode::FORWARD_AS_ATTACHMENT);
}

void MainWindow::slotBounce()
{
    QModelIndex index;
    Imap::Mailbox::Model::realTreeItem(msgListWidget->tree->currentIndex(), nullptr, &index);
    if (!index.isValid())
        return;

    auto recipients = QList<QPair<Composer::RecipientKind,QString>>();
    for (const auto &kind: {Imap::Mailbox::RoleMessageTo, Imap::Mailbox::RoleMessageCc, Imap::Mailbox::RoleMessageBcc}) {
        for (const auto &oneAddr : index.data(kind).toList()) {
            Q_ASSERT(oneAddr.type() == QVariant::StringList);
            QStringList item = oneAddr.toStringList();
            Q_ASSERT(item.size() == 4);
            Imap::Message::MailAddress a(item[0], item[1], item[2], item[3]);
            Composer::RecipientKind translatedKind = Composer::RecipientKind::ADDRESS_TO;
            switch (kind) {
            case Imap::Mailbox::RoleMessageTo:
                translatedKind = Composer::RecipientKind::ADDRESS_RESENT_TO;
                break;
            case Imap::Mailbox::RoleMessageCc:
                translatedKind = Composer::RecipientKind::ADDRESS_RESENT_CC;
                break;
            case Imap::Mailbox::RoleMessageBcc:
                translatedKind = Composer::RecipientKind::ADDRESS_RESENT_BCC;
                break;
            default:
                Q_ASSERT(false);
                break;
            }
            recipients.push_back({translatedKind, a.asPrettyString()});
        }
    }

    ComposeWidget::warnIfMsaNotConfigured(
                ComposeWidget::createFromReadOnly(this, index, recipients),
                this);

}

void MainWindow::slotComposeMailUrl(const QUrl &url)
{
    ComposeWidget::warnIfMsaNotConfigured(ComposeWidget::createFromUrl(this, url), this);
}

void MainWindow::slotManageContact(const QUrl &url)
{
    Imap::Message::MailAddress addr;
    if (!Imap::Message::MailAddress::fromUrl(addr, url, QStringLiteral("x-trojita-manage-contact")))
        return;

    Plugins::AddressbookPlugin *addressbook = pluginManager()->addressbook();
    if (!addressbook)
        return;

    addressbook->openContactWindow(addr.mailbox + QLatin1Char('@') + addr.host, addr.name);
}

void MainWindow::invokeContactEditor()
{
    Plugins::AddressbookPlugin *addressbook = pluginManager()->addressbook();
    if (!addressbook)
        return;

    addressbook->openAddressbookWindow();
}

/** @short Create an MSAFactory as per the settings */
MSA::MSAFactory *MainWindow::msaFactory()
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
                                          m_settings->value(SettingsNames::smtpAuthReuseImapCredsKey, false).toBool() ?
                                              m_settings->value(SettingsNames::imapUserKey).toString() :
                                              m_settings->value(SettingsNames::smtpUserKey).toString());
    } else if (method == SettingsNames::methodSENDMAIL) {
        QStringList args = m_settings->value(SettingsNames::sendmailKey, SettingsNames::sendmailDefaultCmd).toString().split(QLatin1Char(' '));
        if (args.isEmpty()) {
            return 0;
        }
        QString appName = args.takeFirst();
        msaFactory = new MSA::SendmailFactory(appName, args);
    } else if (method == SettingsNames::methodImapSendmail) {
        if (!imapModel()->capabilities().contains(QStringLiteral("X-DRAFT-I01-SENDMAIL"))) {
            return 0;
        }
        msaFactory = new MSA::ImapSubmitFactory(qobject_cast<Imap::Mailbox::Model*>(imapAccess()->imapModel()));
    } else {
        return 0;
    }
    return msaFactory;
}

void MainWindow::slotMailboxDeleteFailed(const QString &mailbox, const QString &msg)
{
    Gui::Util::messageBoxWarning(this, tr("Can't delete mailbox"),
                                 tr("Deleting mailbox \"%1\" failed with the following message:\n%2").arg(mailbox, msg));
}

void MainWindow::slotMailboxCreateFailed(const QString &mailbox, const QString &msg)
{
    Gui::Util::messageBoxWarning(this, tr("Can't create mailbox"),
                                 tr("Creating mailbox \"%1\" failed with the following message:\n%2").arg(mailbox, msg));
}

void MainWindow::slotMailboxSyncFailed(const QString &mailbox, const QString &msg)
{
    Gui::Util::messageBoxWarning(this, tr("Can't open mailbox"),
                                 tr("Opening mailbox \"%1\" failed with the following message:\n%2").arg(mailbox, msg));
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
    slotScrollToUnseenMessage();
}

void MainWindow::showConnectionStatus(uint parserId, Imap::ConnectionState state)
{
    Q_UNUSED(parserId);
    static Imap::ConnectionState previousState = Imap::ConnectionState::CONN_STATE_NONE;
    QString message = connectionStateToString(state);

    if (state == Imap::ConnectionState::CONN_STATE_SELECTED && previousState >= Imap::ConnectionState::CONN_STATE_SELECTED) {
        // prevent excessive popups when we "reset the state" to something which is shown quite often
        showStatusMessage(QString());
    } else {
        showStatusMessage(message);
    }
    previousState = state;
}

void MainWindow::slotShowLinkTarget(const QString &link)
{
    if (link.isEmpty()) {
        QToolTip::hideText();
    } else {
        QToolTip::showText(QCursor::pos(), tr("Link target: %1").arg(UiUtils::Formatting::htmlEscaped(link)));
    }
}

void MainWindow::slotShowAboutTrojita()
{
    Ui::AboutDialog ui;
    QDialog *widget = new QDialog(this);
    widget->setAttribute(Qt::WA_DeleteOnClose);
    ui.setupUi(widget);
    ui.versionLabel->setText(Common::Application::version);
    ui.qtVersion->setText(QStringLiteral("<a href=\"about-qt\">Qt " QT_VERSION_STR "</a>"));
    connect(ui.qtVersion, &QLabel::linkActivated, qApp, &QApplication::aboutQt);

    std::vector<std::pair<QString, bool>> features;
    features.emplace_back(tr("Plugins"),
#ifdef WITH_SHARED_PLUGINS
            true
#else
            false
#endif
                       );
    features.emplace_back(tr("Encrypted and signed messages"),
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
            true
#else
            false
#endif
                       );
    features.emplace_back(tr("IMAP compression"),
#ifdef TROJITA_HAVE_ZLIB
            true
#else
            false
#endif
                       );

    QString featuresText = QStringLiteral("<ul>");
    for (const auto x: features) {
        featuresText += x.second ?
                    tr("<li>%1: supported</li>").arg(x.first)
                  : tr("<li>%1: <strong>disabled</strong></li>").arg(x.first);
    }
    featuresText += QStringLiteral("</ul>");
    ui.descriptionLabel->setText(ui.descriptionLabel->text() + featuresText);

    QStringList copyright;
    {
        // Find the names of the authors and remove date codes from there
        QFile license(QStringLiteral(":/LICENSE"));
        license.open(QFile::ReadOnly);
        const QString prefix(QStringLiteral("Copyright (C) "));
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
    ui.credits->setText(copyright.join(QStringLiteral("\n")));
    widget->show();
}

void MainWindow::slotDonateToTrojita()
{
    QDesktopServices::openUrl(QStringLiteral("https://sourceforge.net/p/trojita/donate/"));
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
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::succeeded, fileDownloadManager, &QObject::deleteLater);
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::transferError, fileDownloadManager, &QObject::deleteLater);
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::fileNameRequested,
                this, &MainWindow::slotDownloadMessageFileNameRequested);
        connect(fileDownloadManager, &Imap::Network::FileDownloadManager::transferError,
                this, &MainWindow::slotDownloadTransferError);
        connect(fileDownloadManager, &QObject::destroyed, netAccess, &QObject::deleteLater);
        fileDownloadManager->downloadMessage();
    }
}

void MainWindow::slotDownloadTransferError(const QString &errorString)
{
    Gui::Util::messageBoxCritical(this, tr("Can't save into file"),
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
        auto w = messageSourceWidget(item);
        //: Translators: %1 is the UID of a message (a number) and %2 is the name of a mailbox.
        w->setWindowTitle(tr("Message source of UID %1 in %2").arg(
                              QString::number(item.data(Imap::Mailbox::RoleMessageUid).toUInt()),
                              Imap::deproxifiedIndex(item).parent().parent().data(Imap::Mailbox::RoleMailboxName).toString()
                              ));
        w->show();
    }
}

QWidget *MainWindow::messageSourceWidget(const QModelIndex &message)
{
    QModelIndex messageIndex = Imap::deproxifiedIndex(message);
    MessageSourceWidget *sourceWidget = new MessageSourceWidget(0, messageIndex);
    sourceWidget->setAttribute(Qt::WA_DeleteOnClose);
    QAction *close = new QAction(UiUtils::loadIcon(QStringLiteral("window-close")), tr("Close"), sourceWidget);
    sourceWidget->addAction(close);
    close->setShortcut(tr("Ctrl+W"));
    connect(close, &QAction::triggered, sourceWidget, &QWidget::close);
    return sourceWidget;
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

        auto widget = new MessageHeadersWidget(nullptr, messageIndex);
        widget->setAttribute(Qt::WA_DeleteOnClose);
        QAction *close = new QAction(UiUtils::loadIcon(QStringLiteral("window-close")), tr("Close"), widget);
        widget->addAction(close);
        close->setShortcut(tr("Ctrl+W"));
        connect(close, &QAction::triggered, widget, &QWidget::close);
        widget->show();
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
        imapModel()->subscribeMailbox(mailbox);
    } else {
        imapModel()->unsubscribeMailbox(mailbox);
    }
}

void MainWindow::slotShowOnlySubscribed()
{
    if (m_actionShowOnlySubscribed->isEnabled()) {
        m_settings->setValue(Common::SettingsNames::guiMailboxListShowOnlySubscribed, m_actionShowOnlySubscribed->isChecked());
        prettyMboxModel->setShowOnlySubscribed(m_actionShowOnlySubscribed->isChecked());
    }
}

void MainWindow::slotScrollToUnseenMessage()
{
    // Now this is much, much more reliable than messing around with finding out an "interesting message"...
    if (!m_actionSortNone->isChecked() && !m_actionSortThreading->isChecked()) {
        // we're using some funky sorting, better don't scroll anywhere
    }
    if (m_actionSortDescending->isChecked()) {
        msgListWidget->tree->scrollToTop();
    } else {
        msgListWidget->tree->scrollToBottom();
    }
}

void MainWindow::slotScrollToCurrent()
{
    // TODO: begs for lambda
    if (QScrollBar *vs = msgListWidget->tree->verticalScrollBar()) {
        vs->setValue(vs->maximum() - vs->value()); // implies vs->minimum() == 0
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
        qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel())->setUserWantsThreading(true);
    } else {
        msgListWidget->tree->setRootIsDecorated(false);
        qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel())->setUserWantsThreading(false);
    }
    m_settings->setValue(Common::SettingsNames::guiMsgListShowThreading, QVariant(useThreading));

    if (currentItem.isValid()) {
        msgListWidget->tree->scrollTo(currentItem);
    } else {
        // If we cannot determine the current item, at least scroll to a predictable place. Without this, the view
        // would jump to "weird" places, probably due to some heuristics about trying to show "roughly the same"
        // objects as what was visible before the reshuffling.
        slotScrollToUnseenMessage();
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
    case MsgListModel::FLAGGED:
    case MsgListModel::ATTACHMENT:
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
    Imap::Mailbox::ThreadingMsgListModel * threadingMsgListModel =
            qobject_cast<Imap::Mailbox::ThreadingMsgListModel *>(m_imapAccess->threadingMsgListModel());
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
    if (capabilities.contains(QStringLiteral("SORT"))) {
        m_actionSortByDate->actionGroup()->setEnabled(true);
    } else {
        m_actionSortByDate->actionGroup()->setEnabled(false);
        msgListWidget->tree->header()->viewport()->installEventFilter(this);
    }

    msgListWidget->setFuzzySearchSupported(capabilities.contains(QStringLiteral("SEARCH=FUZZY")));

    m_actionShowOnlySubscribed->setEnabled(capabilities.contains(QStringLiteral("LIST-EXTENDED")));
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
    Q_FOREACH(const QString &cap, imapModel()->capabilities()) {
        caps += tr("<li>%1</li>\n").arg(cap);
    }

    QString idString;
    if (!imapModel()->serverId().isEmpty() && imapModel()->capabilities().contains(QStringLiteral("ID"))) {
        QMap<QByteArray,QByteArray> serverId = imapModel()->serverId();

#define IMAP_ID_FIELD(Var, Name) bool has_##Var = serverId.contains(Name); \
    QString Var = has_##Var ? QString::fromUtf8(serverId[Name]).toHtmlEscaped() : tr("Unknown");
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
                    idString += tr(" on %1 %2", "%1 is the operating system of an IMAP server and %2 is its version.").arg(os, osVersion);
                else
                    idString += tr(" on %1", "%1 is the operationg system of an IMAP server.").arg(os);
            }
            idString += tr("</p>");
        } else {
            idString = tr("<p>The IMAP server did not return usable information about itself.</p>");
        }
        QString fullId;
        for (QMap<QByteArray,QByteArray>::const_iterator it = serverId.constBegin(); it != serverId.constEnd(); ++it) {
            fullId += tr("<li>%1: %2</li>").arg(QString::fromUtf8(it.key()).toHtmlEscaped(), QString::fromUtf8(it.value()).toHtmlEscaped());
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
    QModelIndex mailbox = qobject_cast<Imap::Mailbox::MsgListModel *>(m_imapAccess->msgListModel())->currentMailbox();
    QString profileName = QString::fromUtf8(qgetenv("TROJITA_PROFILE"));
    if (!profileName.isEmpty())
        profileName = QLatin1String(" [") + profileName + QLatin1Char(']');
    if (mailbox.isValid()) {
        if (mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt()) {
            setWindowTitle(trUtf8("%1 - %n unread - Trojitá", 0, mailbox.data(Imap::Mailbox::RoleUnreadMessageCount).toInt())
                           .arg(mailbox.data(Imap::Mailbox::RoleShortMailboxName).toString()) + profileName);
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
        connect(m_mainHSplitter.data(), &QSplitter::splitterMoved, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
        connect(m_mainHSplitter.data(), &QSplitter::splitterMoved, this, &MainWindow::possiblyLoadMessageOnSplittersChanged);
    }
    if (!m_mainVSplitter) {
        m_mainVSplitter = new QSplitter();
        m_mainVSplitter->setOrientation(Qt::Vertical);
        connect(m_mainVSplitter.data(), &QSplitter::splitterMoved, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
        connect(m_mainVSplitter.data(), &QSplitter::splitterMoved, this, &MainWindow::possiblyLoadMessageOnSplittersChanged);
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
    // The CompleteMessageWidget shall not not collapse
    m_mainVSplitter->setCollapsible(m_mainVSplitter->indexOf(m_messageWidget), false);

    setCentralWidget(m_mainHSplitter);

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
        connect(m_mainHSplitter.data(), &QSplitter::splitterMoved, m_delayedStateSaving, static_cast<void (QTimer::*)()>(&QTimer::start));
        connect(m_mainHSplitter.data(), &QSplitter::splitterMoved, this, &MainWindow::possiblyLoadMessageOnSplittersChanged);
    }

    m_mainHSplitter->addWidget(mboxTree);
    m_mainHSplitter->addWidget(msgListWidget);
    m_mainHSplitter->addWidget(m_messageWidget);
    msgListWidget->resize(mboxTree->size());
    m_messageWidget->resize(mboxTree->size());
    m_mainHSplitter->setStretchFactor(0, 0);
    m_mainHSplitter->setStretchFactor(1, 1);
    m_mainHSplitter->setStretchFactor(2, 1);

    m_mainHSplitter->setCollapsible(m_mainHSplitter->indexOf(m_messageWidget), false);

    mboxTree->show();
    msgListWidget->show();
    m_messageWidget->show();
    m_mainHSplitter->show();

    setCentralWidget(m_mainHSplitter);

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

    delete m_mainHSplitter;
    delete m_mainVSplitter;

    m_layoutMode = LAYOUT_ONE_AT_TIME;
    m_settings->setValue(Common::SettingsNames::guiMainWindowLayout, Common::SettingsNames::guiMainWindowLayoutOneAtTime);
    applySizesAndState();
}

Imap::Mailbox::Model *MainWindow::imapModel() const
{
    return qobject_cast<Imap::Mailbox::Model *>(m_imapAccess->imapModel());
}

void MainWindow::desktopGeometryChanged()
{
    m_delayedStateSaving->start();
}

QString MainWindow::settingsKeyForLayout(const LayoutMode layout)
{
    switch (layout) {
    case LAYOUT_COMPACT:
        return Common::SettingsNames::guiSizesInMainWinWhenCompact;
    case LAYOUT_WIDE:
        return Common::SettingsNames::guiSizesInMainWinWhenWide;
    case LAYOUT_ONE_AT_TIME:
        return Common::SettingsNames::guiSizesInaMainWinWhenOneAtATime;
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

    connect(msgListWidget->tree->header(), &QHeaderView::sectionCountChanged, msgListWidget->tree, &MsgListView::slotHandleNewColumns);

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
                msgListWidget->tree->header()->setSectionResizeMode(i, resizeMode);
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

Imap::ImapAccess *MainWindow::imapAccess() const
{
    return m_imapAccess;
}

void MainWindow::enableLoggingToDisk()
{
    imapLogger->slotSetPersistentLogging(true);
}

void MainWindow::slotPluginsChanged()
{
    Plugins::AddressbookPlugin *addressbook = pluginManager()->addressbook();
    if (!addressbook || !(addressbook->features() & Plugins::AddressbookPlugin::FeatureAddressbookWindow))
        m_actionContactEditor->setEnabled(false);
    else
        m_actionContactEditor->setEnabled(true);
}

/** @short Update the default action to make sure that we show a correct status of the network connection */
void MainWindow::updateNetworkIndication()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        if (action->isChecked()) {
            m_netToolbarDefaultAction->setIcon(action->icon());
        }
    }
}

void MainWindow::showStatusMessage(const QString &message)
{
    networkIndicator->setToolTip(message);
    if (isActiveWindow())
        QToolTip::showText(networkIndicator->mapToGlobal(QPoint(0, 0)), message);
}

void MainWindow::slotMessageModelChanged(QAbstractItemModel *model)
{
    mailMimeTree->setModel(model);
}

void MainWindow::slotFavoriteTagsChanged()
{
    for (int i = 1; i <= m_favoriteTags->rowCount(); ++i) {
        QAction *action = ShortcutHandler::instance()->action(QStringLiteral("action_tag_") + QString::number(i));
        if (action)
            action->setText(tr("Tag with \"%1\"").arg(m_favoriteTags->tagNameByIndex(i - 1)));
    }
}

}
