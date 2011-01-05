/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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

#include <QAuthenticator>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>

#include "Window.h"
#include "ComposeWidget.h"
#include "ProtocolLoggerWidget.h"
#include "MessageView.h"
#include "MsgListView.h"
#include "SettingsDialog.h"
#include "Common/SettingsNames.h"
#include "SimplePartWidget.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/ModelWatcher.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/CombinedCache.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/PrettyMailboxModel.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "Imap/Network/FileDownloadManager.h"
#include "Streams/SocketFactory.h"

#include "ui_CreateMailboxDialog.h"

#include "Imap/Model/ModelTest/modeltest.h"

/** @short All user-facing widgets and related classes */
namespace Gui {

MainWindow::MainWindow(): QMainWindow(), model(0)
{
    setWindowTitle( trUtf8("Trojitá") );
    createWidgets();

    QSettings s;
    if ( ! s.contains( Common::SettingsNames::imapMethodKey ) ) {
        QTimer::singleShot( 0, this, SLOT(slotShowSettings()) );
    }

    setupModels();
    createActions();
    createMenus();

    // Please note that Qt 4.6.1 really requires passing the method signature this way, *not* using the SLOT() macro
    QDesktopServices::setUrlHandler( QLatin1String("mailto"), this, "slotComposeMailUrl" );
}

void MainWindow::createActions()
{
    reloadMboxList = new QAction( style()->standardIcon(QStyle::SP_ArrowRight), tr("Update List of Child Mailboxes"), this );
    connect( reloadMboxList, SIGNAL( triggered() ), this, SLOT( slotReloadMboxList() ) );

    resyncMbox = new QAction( QIcon::fromTheme( QLatin1String("view-refresh") ), tr("Check for New Messages"), this );
    connect( resyncMbox, SIGNAL(triggered()), this, SLOT(slotResyncMbox()) );

    reloadAllMailboxes = new QAction( tr("Reload Everything"), this );
    // connect later

    exitAction = new QAction( QIcon::fromTheme( QLatin1String("application-exit") ), tr("E&xit"), this );
    exitAction->setShortcut( tr("Ctrl+Q") );
    exitAction->setStatusTip( tr("Exit the application") );
    connect( exitAction, SIGNAL( triggered() ), this, SLOT( close() ) );

    QActionGroup* netPolicyGroup = new QActionGroup( this );
    netPolicyGroup->setExclusive( true );
    netOffline = new QAction( QIcon( QLatin1String(":/icons/network-offline") ), tr("Offline"), netPolicyGroup );
    netOffline->setCheckable( true );
    // connect later
    netExpensive = new QAction( QIcon( QLatin1String(":/icons/network-expensive") ), tr("Expensive Connection"), netPolicyGroup );
    netExpensive->setCheckable( true );
    // connect later
    netOnline = new QAction( QIcon( QLatin1String(":/icons/network-online") ), tr("Free Access"), netPolicyGroup );
    netOnline->setCheckable( true );
    // connect later

    showFullView = new QAction( QIcon::fromTheme( QLatin1String("edit-find-mail") ), tr("Show Full Tree Window"), this );
    showFullView->setCheckable( true );
    connect( showFullView, SIGNAL( triggered(bool) ), allDock, SLOT(setVisible(bool)) );
    connect( allDock, SIGNAL( visibilityChanged(bool) ), showFullView, SLOT( setChecked(bool) ) );

    showImapLogger = new QAction( tr("Show IMAP protocol log"), this );
    showImapLogger->setCheckable( true );
    connect( showImapLogger, SIGNAL(triggered(bool)), imapLoggerDock, SLOT(setVisible(bool)) );
    connect( imapLoggerDock, SIGNAL(visibilityChanged(bool)), showImapLogger, SLOT(setChecked(bool)) );

    showMenuBar = new QAction( QIcon::fromTheme( QLatin1String("view-list-text") ),  tr("Show Main Menu Bar"), this );
    showMenuBar->setCheckable( true );
    showMenuBar->setChecked( true );
    showMenuBar->setShortcut( tr("Ctrl+M") );
    addAction( showMenuBar ); // otherwise it won't work with hidden menu bar
    connect( showMenuBar, SIGNAL( triggered(bool) ), menuBar(), SLOT(setVisible(bool)) );

    configSettings = new QAction( QIcon::fromTheme( QLatin1String("configure") ),  tr("Settings..."), this );
    connect( configSettings, SIGNAL( triggered() ), this, SLOT( slotShowSettings() ) );

    composeMail = new QAction( QIcon::fromTheme( QLatin1String("document-edit") ),  tr("Compose Mail..."), this );
    connect( composeMail, SIGNAL(triggered()), this, SLOT(slotComposeMail()) );

    expunge = new QAction( QIcon::fromTheme( QLatin1String("trash-empty") ),  tr("Expunge Mailbox"), this );
    connect( expunge, SIGNAL(triggered()), this, SLOT(slotExpunge()) );

    markAsRead = new QAction( QIcon::fromTheme( QLatin1String("mail-mark-read") ),  tr("Mark as Read"), this );
    markAsRead->setCheckable( true );
    markAsRead->setShortcut( Qt::Key_M );
    msgListTree->addAction( markAsRead );
    connect( markAsRead, SIGNAL(triggered(bool)), this, SLOT(handleMarkAsRead(bool)) );

    markAsDeleted = new QAction( QIcon::fromTheme( QLatin1String("list-remove") ),  tr("Mark as Deleted"), this );
    markAsDeleted->setCheckable( true );
    markAsDeleted->setShortcut( Qt::Key_Delete );
    msgListTree->addAction( markAsDeleted );
    connect( markAsDeleted, SIGNAL(triggered(bool)), this, SLOT(handleMarkAsDeleted(bool)) );

    saveWholeMessage = new QAction( QIcon::fromTheme( QLatin1String("file-save") ), tr("Save Message..."), this );
    msgListTree->addAction( saveWholeMessage );
    connect( saveWholeMessage, SIGNAL(triggered()), this, SLOT(slotSaveCurrentMessageBody()) );

    viewMsgHeaders = new QAction( tr("View Message Headers..."), this );
    msgListTree->addAction( viewMsgHeaders );
    connect( viewMsgHeaders, SIGNAL(triggered()), this, SLOT(slotViewMsgHeaders()) );

    createChildMailbox = new QAction( tr("Create Child Mailbox..."), this );
    connect( createChildMailbox, SIGNAL(triggered()), this, SLOT(slotCreateMailboxBelowCurrent()) );

    createTopMailbox = new QAction( tr("Create New Mailbox..."), this );
    connect( createTopMailbox, SIGNAL(triggered()), this, SLOT(slotCreateTopMailbox()) );

    deleteCurrentMailbox = new QAction( tr("Delete Mailbox"), this );
    connect( deleteCurrentMailbox, SIGNAL(triggered()), this, SLOT(slotDeleteCurrentMailbox()) );

#ifdef XTUPLE_CONNECT
    xtIncludeMailboxInSync = new QAction( tr("Synchronize with xTuple"), this );
    xtIncludeMailboxInSync->setCheckable( true );
    connect( xtIncludeMailboxInSync, SIGNAL(triggered()), this, SLOT(slotXtSyncCurrentMailbox()) );
#endif

    releaseMessageData = new QAction( tr("Release memory for this message"), this );
    connect( releaseMessageData, SIGNAL(triggered()), this, SLOT(slotReleaseSelectedMessage()) );

    replyTo = new QAction( tr("Reply..."), this );
    replyTo->setShortcut( tr("Ctrl+R") );
    connect( replyTo, SIGNAL(triggered()), this, SLOT(slotReplyTo()) );

    replyAll = new QAction( tr("Reply All..."), this );
    replyAll->setShortcut( tr("Ctrl+Shift+R") );
    connect( replyAll, SIGNAL(triggered()), this, SLOT(slotReplyAll()) );

    aboutTrojita = new QAction( trUtf8("About Trojitá..."), this );
    connect( aboutTrojita, SIGNAL(triggered()), this, SLOT(slotShowAboutTrojita()) );

    donateToTrojita = new QAction( tr("Donate to the project"), this );
    connect( donateToTrojita, SIGNAL(triggered()), this, SLOT(slotDonateToTrojita()) );

    connectModelActions();

    QToolBar *toolBar = addToolBar(tr("Navigation"));
    toolBar->addAction(composeMail);
    toolBar->addAction(replyTo);
    toolBar->addAction(replyAll);
    toolBar->addAction(expunge);
    toolBar->addSeparator();
    toolBar->addAction(markAsRead);
    toolBar->addAction(markAsDeleted);
    toolBar->addSeparator();
    toolBar->addAction(showMenuBar);
    toolBar->addAction(configSettings);
}

void MainWindow::connectModelActions()
{
    connect( reloadAllMailboxes, SIGNAL( triggered() ), model, SLOT( reloadMailboxList() ) );
    connect( netOffline, SIGNAL( triggered() ), model, SLOT( setNetworkOffline() ) );
    connect( netExpensive, SIGNAL( triggered() ), model, SLOT( setNetworkExpensive() ) );
    connect( netOnline, SIGNAL( triggered() ), model, SLOT( setNetworkOnline() ) );
}

void MainWindow::createMenus()
{
    QMenu* imapMenu = menuBar()->addMenu( tr("IMAP") );
    imapMenu->addAction( composeMail );
    imapMenu->addAction( replyTo );
    imapMenu->addAction( replyAll );
    imapMenu->addAction( expunge );
    imapMenu->addSeparator()->setText( tr("Network Access") );
    QMenu* netPolicyMenu = imapMenu->addMenu( tr("Network Access"));
    netPolicyMenu->addAction( netOffline );
    netPolicyMenu->addAction( netExpensive );
    netPolicyMenu->addAction( netOnline );
    imapMenu->addSeparator();
    imapMenu->addAction( showFullView );
    imapMenu->addAction( showImapLogger );
    imapMenu->addSeparator();
    imapMenu->addAction( configSettings );
    imapMenu->addAction( showMenuBar );
    imapMenu->addSeparator();
    imapMenu->addAction( exitAction );

    QMenu* mailboxMenu = menuBar()->addMenu( tr("Mailbox") );
    mailboxMenu->addAction( resyncMbox );
    mailboxMenu->addSeparator();
    mailboxMenu->addAction( reloadAllMailboxes );

    QMenu* helpMenu = menuBar()->addMenu( tr("Help") );
    helpMenu->addAction( donateToTrojita );
    helpMenu->addSeparator();
    helpMenu->addAction( aboutTrojita );

    networkIndicator->setMenu( netPolicyMenu );
    networkIndicator->setDefaultAction( netOnline );
}

void MainWindow::createWidgets()
{
    mboxTree = new QTreeView();
    mboxTree->setUniformRowHeights( true );
    mboxTree->setContextMenuPolicy(Qt::CustomContextMenu);
    mboxTree->setSelectionMode( QAbstractItemView::SingleSelection );
    mboxTree->setAllColumnsShowFocus( true );
    mboxTree->setAcceptDrops( true );
    mboxTree->setDropIndicatorShown( true );
    mboxTree->setHeaderHidden( true );
    connect( mboxTree, SIGNAL( customContextMenuRequested( const QPoint & ) ),
            this, SLOT( showContextMenuMboxTree( const QPoint& ) ) );

    msgListTree = new MsgListView();
    msgListTree->setUniformRowHeights( true );
    msgListTree->setContextMenuPolicy(Qt::CustomContextMenu);
    msgListTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
    msgListTree->setAllColumnsShowFocus( true );
    msgListTree->setAlternatingRowColors( true );
    msgListTree->setDragEnabled( true );

    connect( msgListTree, SIGNAL( customContextMenuRequested( const QPoint & ) ),
            this, SLOT( showContextMenuMsgListTree( const QPoint& ) ) );

    msgView = new MessageView();
    area = new QScrollArea();
    area->setWidget( msgView );
    area->setWidgetResizable( true );
    connect( msgView, SIGNAL(messageChanged()), this, SLOT(scrollMessageUp()) );

    QSplitter* hSplitter = new QSplitter();
    QSplitter* vSplitter = new QSplitter();
    vSplitter->setOrientation( Qt::Vertical );
    vSplitter->addWidget( msgListTree );
    vSplitter->addWidget( area );
    hSplitter->addWidget( mboxTree );
    hSplitter->addWidget( vSplitter );
    setCentralWidget( hSplitter );

    allDock = new QDockWidget( "Everything", this );
    allTree = new QTreeView( allDock );
    allDock->hide();
    allTree->setUniformRowHeights( true );
    allTree->setHeaderHidden( true );
    allDock->setWidget( allTree );
    addDockWidget( Qt::LeftDockWidgetArea, allDock );

    imapLoggerDock = new QDockWidget( tr("IMAP Protocol"), this );
    imapLogger = new ProtocolLoggerWidget( imapLoggerDock );
    imapLoggerDock->hide();
    imapLoggerDock->setWidget( imapLogger );
    addDockWidget( Qt::BottomDockWidgetArea, imapLoggerDock );

    busyParsersIndicator = new QProgressBar( this );
    statusBar()->addPermanentWidget( busyParsersIndicator );
    busyParsersIndicator->setMinimum(0);
    busyParsersIndicator->setMaximum(0);
    busyParsersIndicator->hide();

    networkIndicator = new QToolButton( this );
    networkIndicator->setPopupMode( QToolButton::InstantPopup );
    statusBar()->addPermanentWidget( networkIndicator );
}

void MainWindow::setupModels()
{
    Imap::Mailbox::SocketFactoryPtr factory;
    Imap::Mailbox::TaskFactoryPtr taskFactory( new Imap::Mailbox::TaskFactory() );
    QSettings s;

    using Common::SettingsNames;
    if ( s.value( SettingsNames::imapMethodKey ).toString() == SettingsNames::methodTCP ) {
        factory.reset( new Imap::Mailbox::TlsAbleSocketFactory(
                s.value( SettingsNames::imapHostKey ).toString(),
                s.value( SettingsNames::imapPortKey ).toUInt() ) );
        factory->setStartTlsRequired( s.value( SettingsNames::imapStartTlsKey, true ).toBool() );
    } else if ( s.value( SettingsNames::imapMethodKey ).toString() == SettingsNames::methodSSL ) {
        factory.reset( new Imap::Mailbox::SslSocketFactory(
                s.value( SettingsNames::imapHostKey ).toString(),
                s.value( SettingsNames::imapPortKey ).toUInt() ) );
    } else {
        QStringList args = s.value( SettingsNames::imapProcessKey ).toString().split( QLatin1Char(' ') );
        Q_ASSERT( ! args.isEmpty() ); // FIXME
        QString appName = args.takeFirst();
        factory.reset( new Imap::Mailbox::ProcessSocketFactory( appName, args ) );
    }

    QString cacheDir = QDesktopServices::storageLocation( QDesktopServices::CacheLocation );
    if ( cacheDir.isEmpty() )
        cacheDir = QDir::homePath() + QLatin1String("/.") + QCoreApplication::applicationName();
    Imap::Mailbox::AbstractCache* cache = 0;

    bool shouldUsePersistentCache = s.value( SettingsNames::cacheMetadataKey).toString() == SettingsNames::cacheMetadataPersistent;

    if ( shouldUsePersistentCache ) {
        if ( ! QDir().mkpath( cacheDir ) ) {
            QMessageBox::critical( this, tr("Cache Error"), tr("Failed to create directory %1").arg( cacheDir ) );
            shouldUsePersistentCache = false;
        }
    }

    //setProperty( "trojita-sqlcache-commit-period", QVariant(5000) );
    //setProperty( "trojita-sqlcache-commit-delay", QVariant(1000) );

    if ( ! shouldUsePersistentCache ) {
        cache = new Imap::Mailbox::MemoryCache( this, QString() );
    } else {
        cache = new Imap::Mailbox::CombinedCache( this, QLatin1String("trojita-imap-cache"), cacheDir );
        connect( cache, SIGNAL(error(QString)), this, SLOT(cacheError(QString)) );
        if ( ! static_cast<Imap::Mailbox::CombinedCache*>( cache )->open() ) {
            // Error message was already shown by the cacheError() slot
            cache->deleteLater();
            cache = new Imap::Mailbox::MemoryCache( this, QString() );
        }
    }
    model = new Imap::Mailbox::Model( this, cache, factory, taskFactory, s.value( SettingsNames::imapStartOffline ).toBool() );
    model->setObjectName( QLatin1String("model") );
    mboxModel = new Imap::Mailbox::MailboxModel( this, model );
    mboxModel->setObjectName( QLatin1String("mboxModel") );
    prettyMboxModel = new Imap::Mailbox::PrettyMailboxModel( this, mboxModel );
    prettyMboxModel->setObjectName( QLatin1String("prettyMboxModel") );
    msgListModel = new Imap::Mailbox::MsgListModel( this, model );
    msgListModel->setObjectName( QLatin1String("msgListModel") );
    threadingMsgListModel = new Imap::Mailbox::ThreadingMsgListModel( this );
    threadingMsgListModel->setSourceModel( msgListModel );;
    threadingMsgListModel->setObjectName( QLatin1String("threadingMsgListModel") );

    connect( mboxTree, SIGNAL( clicked(const QModelIndex&) ), msgListModel, SLOT( setMailbox(const QModelIndex&) ) );
    connect( mboxTree, SIGNAL( activated(const QModelIndex&) ), msgListModel, SLOT( setMailbox(const QModelIndex&) ) );
    connect( msgListModel, SIGNAL( mailboxChanged() ), this, SLOT( slotResizeMsgListColumns() ) );
    connect( msgListModel, SIGNAL( dataChanged(QModelIndex,QModelIndex) ), this, SLOT( updateMessageFlags() ) );
    connect( msgListModel, SIGNAL(messagesAvailable()), msgListTree, SLOT(scrollToBottom()) );

    connect( msgListTree, SIGNAL( activated(const QModelIndex&) ), this, SLOT( msgListActivated(const QModelIndex&) ) );
    connect( msgListTree, SIGNAL( clicked(const QModelIndex&) ), this, SLOT( msgListClicked(const QModelIndex&) ) );
    connect( msgListTree, SIGNAL( doubleClicked( const QModelIndex& ) ), this, SLOT( msgListDoubleClicked( const QModelIndex& ) ) );
    connect( msgListModel, SIGNAL( modelAboutToBeReset() ), msgView, SLOT( setEmpty() ) );
    connect( msgListModel, SIGNAL( messageRemoved( void* ) ), msgView, SLOT( handleMessageRemoved( void* ) ) );

    connect( model, SIGNAL( alertReceived( const QString& ) ), this, SLOT( alertReceived( const QString& ) ) );
    connect( model, SIGNAL( connectionError( const QString& ) ), this, SLOT( connectionError( const QString& ) ) );
    connect( model, SIGNAL( authRequested( QAuthenticator* ) ), this, SLOT( authenticationRequested( QAuthenticator* ) ) );

    connect( model, SIGNAL( networkPolicyOffline() ), this, SLOT( networkPolicyOffline() ) );
    connect( model, SIGNAL( networkPolicyExpensive() ), this, SLOT( networkPolicyExpensive() ) );
    connect( model, SIGNAL( networkPolicyOnline() ), this, SLOT( networkPolicyOnline() ) );

    connect( model, SIGNAL(connectionStateChanged(QObject*,Imap::ConnectionState)),
             this, SLOT(showConnectionStatus(QObject*,Imap::ConnectionState)) );

    connect( model, SIGNAL(activityHappening(bool)), this, SLOT(updateBusyParsers(bool)) );

    connect( model, SIGNAL(logParserLineReceived(uint,QByteArray)), imapLogger, SLOT(parserLineReceived(uint,QByteArray)) );
    connect( model, SIGNAL(logParserLineSent(uint,QByteArray)), imapLogger, SLOT(parserLineSent(uint,QByteArray)) );
    connect( model, SIGNAL(logParserFatalError(uint,QString,QString,QByteArray,int)),
             imapLogger, SLOT(parserFatalError(uint,QString,QString,QByteArray,int)) );

    connect( model, SIGNAL(mailboxDeletionFailed(QString,QString)), this, SLOT(slotMailboxDeleteFailed(QString,QString)) );
    connect( model, SIGNAL(mailboxCreationFailed(QString,QString)), this, SLOT(slotMailboxCreateFailed(QString,QString)) );

    connect( model, SIGNAL(mailboxFirstUnseenMessage(QModelIndex,QModelIndex)), this, SLOT(slotScrollToUnseenMessage(QModelIndex,QModelIndex)) );

    //Imap::Mailbox::ModelWatcher* w = new Imap::Mailbox::ModelWatcher( this );
    //w->setModel( model );

    //ModelTest* tester = new ModelTest( prettyMboxModel, this ); // when testing, test just one model at time

    mboxTree->setModel( prettyMboxModel );
    msgListTree->setModel( threadingMsgListModel ); // FIXME: fix the asserts at various places of this file...
    connect( msgListTree->selectionModel(), SIGNAL( selectionChanged( const QItemSelection&, const QItemSelection& ) ),
             this, SLOT( msgListSelectionChanged( const QItemSelection&, const QItemSelection& ) ) );

    allTree->setModel( model );
}

void MainWindow::msgListSelectionChanged( const QItemSelection& selected, const QItemSelection& deselected )
{
    Q_UNUSED( deselected );
    if ( selected.indexes().isEmpty() )
        return;

    QModelIndex index = selected.indexes().front();
    if ( index.data( Imap::Mailbox::RoleMessageUid ).isValid() ) {
        updateMessageFlags( index );
        msgView->setMessage( index );
    }
}

void MainWindow::msgListActivated( const QModelIndex& index )
{
    Q_ASSERT( index.isValid() );

    if ( ! index.data( Imap::Mailbox::RoleMessageUid ).isValid() )
        return;

    if ( index.column() != Imap::Mailbox::MsgListModel::SEEN ) {
        msgView->setMessage( index );
        msgListTree->setCurrentIndex( index );
    }
}

void MainWindow::msgListClicked( const QModelIndex& index )
{
    Q_ASSERT( index.isValid() );

    if ( ! index.data( Imap::Mailbox::RoleMessageUid ).isValid() )
        return;

    if ( index.column() == Imap::Mailbox::MsgListModel::SEEN ) {
        Imap::Mailbox::TreeItemMessage* message = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(
                Imap::Mailbox::Model::realTreeItem( index )
                );
        Q_ASSERT( message );
        if ( ! message->fetched() )
            return;
        model->markMessageRead( message, ! message->isMarkedAsRead() );
    }
}

void MainWindow::msgListDoubleClicked( const QModelIndex& index )
{
    Q_ASSERT( index.isValid() );

    if ( ! index.data( Imap::Mailbox::RoleMessageUid ).isValid() )
        return;

    MessageView* newView = new MessageView( 0 );
    QModelIndex realIndex;
    const Imap::Mailbox::Model *realModel;
    Imap::Mailbox::TreeItemMessage* message = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(
            Imap::Mailbox::Model::realTreeItem( index, &realModel, &realIndex ) );
    Q_ASSERT( message );
    Q_ASSERT( realModel == model );
    newView->setMessage( index );

    // Now make sure we check all possible paths that could possibly lead to problems when a message gets deleted
    // big fat FIXME: This is too simplified, albeit safe. What we really want here, however,
    // is to have a way in which the *real* Model signals a deletion of a particular message.
    // That way we can safely ignore signals coming from msgListModel which might be result of,
    // say, switching the view to another mailbox...
    connect( msgListModel, SIGNAL( modelAboutToBeReset() ), newView, SLOT( setEmpty() ) );
    connect( msgListModel, SIGNAL( messageRemoved( void* ) ), newView, SLOT( handleMessageRemoved( void* ) ) );
    connect( msgListModel, SIGNAL( destroyed() ), newView, SLOT( setEmpty() ) );

    QScrollArea* widget = new QScrollArea();
    widget->setWidget( newView );
    widget->setWidgetResizable( true );
    widget->setWindowTitle( message->envelope( model ).subject );
    widget->setAttribute( Qt::WA_DeleteOnClose );
    widget->show();
}

void MainWindow::slotResizeMsgListColumns()
{
    for ( int i = 0; i < msgListTree->header()->count(); ++i )
        msgListTree->resizeColumnToContents( i );
}

void MainWindow::showContextMenuMboxTree( const QPoint& position )
{
    QList<QAction*> actionList;
    if ( mboxTree->indexAt( position ).isValid() ) {
        actionList.append( createChildMailbox );
        actionList.append( deleteCurrentMailbox );
        actionList.append( resyncMbox );
        actionList.append( reloadMboxList );

#ifdef XTUPLE_CONNECT
        actionList.append( xtIncludeMailboxInSync );
        xtIncludeMailboxInSync->setChecked(
                QSettings().value( Common::SettingsNames::xtSyncMailboxList ).toStringList().contains(
                        mboxTree->indexAt( position ).data( Imap::Mailbox::RoleMailboxName ).toString() ) );
#endif
    } else {
        actionList.append( createTopMailbox );
    }
    actionList.append( reloadAllMailboxes );
    QMenu::exec( actionList, mboxTree->mapToGlobal( position ) );
}

void MainWindow::showContextMenuMsgListTree( const QPoint& position )
{
    QList<QAction*> actionList;
    QModelIndex index = msgListTree->indexAt( position );
    if ( index.isValid() ) {
        updateMessageFlags( index );
        actionList.append( markAsRead );
        actionList.append( markAsDeleted );
        actionList.append( saveWholeMessage );
        actionList.append( viewMsgHeaders );
        actionList.append( releaseMessageData );
    }
    if ( ! actionList.isEmpty() )
        QMenu::exec( actionList, msgListTree->mapToGlobal( position ) );
}

/** @short Ask for an updated list of mailboxes situated below the selected one

*/
void MainWindow::slotReloadMboxList()
{
    QModelIndexList indices = mboxTree->selectionModel()->selectedIndexes();
    for ( QModelIndexList::const_iterator it = indices.begin(); it != indices.end(); ++it ) {
        Q_ASSERT( it->isValid() );
        if ( it->column() != 0 )
            continue;
        Imap::Mailbox::TreeItemMailbox* mbox = dynamic_cast<Imap::Mailbox::TreeItemMailbox*>(
                Imap::Mailbox::Model::realTreeItem( *it )
                );
        Q_ASSERT( mbox );
        mbox->rescanForChildMailboxes( model );
    }
}

/** @short Request a check for new messages in selected mailbox */
void MainWindow::slotResyncMbox()
{
    if ( ! model->isNetworkAvailable() )
        return;

    QModelIndexList indices = mboxTree->selectionModel()->selectedIndexes();
    for ( QModelIndexList::const_iterator it = indices.begin(); it != indices.end(); ++it ) {
        Q_ASSERT( it->isValid() );
        if ( it->column() != 0 )
            continue;
        Imap::Mailbox::TreeItemMailbox* mbox = dynamic_cast<Imap::Mailbox::TreeItemMailbox*>(
                    Imap::Mailbox::Model::realTreeItem( *it )
                );
        Q_ASSERT( mbox );
        model->resyncMailbox( *it );
    }
}

void MainWindow::alertReceived( const QString& message )
{
    QMessageBox::warning( this, tr("IMAP Alert"), message );
}

void MainWindow::connectionError( const QString& message )
{
    if ( QSettings().contains( Common::SettingsNames::imapMethodKey ) ) {
        QMessageBox::critical( this, tr("Connection Error"), message );
    } else {
        // hack: this slot is called even on the first run with no configuration
        // We shouldn't have to worry about that, since the dialog is already scheduled for calling
        // -> do nothing
    }
    netOffline->trigger();
}

void MainWindow::cacheError( const QString& message )
{
    QMessageBox::critical( this, tr("IMAP Cache Error"),
                           tr("The caching subsystem managing a cache of the data already "
                              "downloaded from the IMAP server is having troubles. "
                              "All caching will be disabled.\n\n%1").arg( message ) );
    if ( model )
        model->setCache( new Imap::Mailbox::MemoryCache( model, QString() ) );
}

void MainWindow::networkPolicyOffline()
{
    netOffline->setChecked( true );
    netExpensive->setChecked( false );
    netOnline->setChecked( false );
    updateActionsOnlineOffline( false );
    networkIndicator->setDefaultAction( netOffline );
    statusBar()->showMessage( tr("Offline"), 0 );
}

void MainWindow::networkPolicyExpensive()
{
    netOffline->setChecked( false );
    netExpensive->setChecked( true );
    netOnline->setChecked( false );
    updateActionsOnlineOffline( true );
    networkIndicator->setDefaultAction( netExpensive );
}

void MainWindow::networkPolicyOnline()
{
    netOffline->setChecked( false );
    netExpensive->setChecked( false );
    netOnline->setChecked( true );
    updateActionsOnlineOffline( true );
    networkIndicator->setDefaultAction( netOnline );
}

void MainWindow::slotShowSettings()
{
    SettingsDialog* dialog = new SettingsDialog();
    if ( dialog->exec() == QDialog::Accepted ) {
        // FIXME: wipe cache in case we're moving between servers
        nukeModels();
        setupModels();
        connectModelActions();
    }
}

void MainWindow::authenticationRequested( QAuthenticator* auth )
{
    QSettings s;
    QString user = s.value( Common::SettingsNames::imapUserKey ).toString();
    QString pass = s.value( Common::SettingsNames::imapPassKey ).toString();
    if ( pass.isEmpty() ) {
        bool ok;
        pass = QInputDialog::getText( this, tr("Password"),
                                      tr("Please provide password for %1").arg( user ),
                                      QLineEdit::Password, QString::null, &ok );
        if ( ok ) {
            auth->setUser( user );
            auth->setPassword( pass );
        }
    } else {
        auth->setUser( user );
        auth->setPassword( pass );
    }
}

void MainWindow::nukeModels()
{
    msgView->setEmpty();
    mboxTree->setModel( 0 );
    msgListTree->setModel( 0 );
    allTree->setModel( 0 );
    threadingMsgListModel->deleteLater();
    threadingMsgListModel = 0;
    msgListModel->deleteLater();
    msgListModel = 0;
    mboxModel->deleteLater();
    mboxModel = 0;
    prettyMboxModel->deleteLater();
    prettyMboxModel = 0;
    model->deleteLater();
    model = 0;
}

void MainWindow::slotComposeMail()
{
    invokeComposeDialog();
}

void MainWindow::handleMarkAsRead( bool value )
{
    QModelIndexList indices = msgListTree->selectionModel()->selectedIndexes();
    for ( QModelIndexList::const_iterator it = indices.begin(); it != indices.end(); ++it ) {
        Q_ASSERT( it->isValid() );
        if ( it->column() != 0 )
            continue;
        if ( ! it->data( Imap::Mailbox::RoleMessageUid ).isValid() )
            continue;
        Imap::Mailbox::TreeItemMessage* message = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(
                Imap::Mailbox::Model::realTreeItem( *it )
                );
        Q_ASSERT( message );
        model->markMessageRead( message, value );
    }
}

void MainWindow::handleMarkAsDeleted( bool value )
{
    QModelIndexList indices = msgListTree->selectionModel()->selectedIndexes();
    for ( QModelIndexList::const_iterator it = indices.begin(); it != indices.end(); ++it ) {
        Q_ASSERT( it->isValid() );
        if ( it->column() != 0 )
            continue;
        if ( ! it->data( Imap::Mailbox::RoleMessageUid ).isValid() )
            continue;
        Imap::Mailbox::TreeItemMessage* message = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(
                Imap::Mailbox::Model::realTreeItem( *it )
                );
        Q_ASSERT( message );
        model->markMessageDeleted( message, value );
    }
}

void MainWindow::slotExpunge()
{
    model->expungeMailbox( msgListModel->currentMailbox() );
}

void MainWindow::slotCreateMailboxBelowCurrent()
{
    createMailboxBelow( mboxTree->currentIndex() );
}

void MainWindow::slotCreateTopMailbox()
{
    createMailboxBelow( QModelIndex() );
}

void MainWindow::createMailboxBelow( const QModelIndex& index )
{
    Imap::Mailbox::TreeItemMailbox* mboxPtr = index.isValid() ?
        dynamic_cast<Imap::Mailbox::TreeItemMailbox*>(
                Imap::Mailbox::Model::realTreeItem( index ) ) :
        0;

    Ui::CreateMailboxDialog ui;
    QDialog* dialog = new QDialog( this );
    ui.setupUi( dialog );

    dialog->setWindowTitle( mboxPtr ?
        tr("Create a Subfolder of %1").arg( mboxPtr->mailbox() ) :
        tr("Create a Top-level Mailbox") );

    if ( dialog->exec() == QDialog::Accepted ) {
        QStringList parts;
        if ( mboxPtr )
            parts << mboxPtr->mailbox();
        parts << ui.mailboxName->text();
        if ( ui.otherMailboxes->isChecked() )
            parts << QString();
        QString targetName = parts.join( mboxPtr ? mboxPtr->separator() : QString() ); // FIXME: top-level separator
        model->createMailbox( targetName );
    }
}

void MainWindow::slotDeleteCurrentMailbox()
{
    if ( ! mboxTree->currentIndex().isValid() )
        return;

    Imap::Mailbox::TreeItemMailbox* mailbox = dynamic_cast<Imap::Mailbox::TreeItemMailbox*>(
        Imap::Mailbox::Model::realTreeItem( mboxTree->currentIndex() ) );
    Q_ASSERT( mailbox );

    if ( QMessageBox::question( this, tr("Delete Mailbox"),
                                tr("Are you sure to delete mailbox %1?").arg( mailbox->mailbox() ),
                                QMessageBox::Yes | QMessageBox::No ) == QMessageBox::Yes ) {
        model->deleteMailbox( mailbox->mailbox() );
    }
}

void MainWindow::updateMessageFlags()
{
    updateMessageFlags( msgListTree->currentIndex() );
}

void MainWindow::updateMessageFlags(const QModelIndex &index)
{
    markAsRead->setChecked( index.data( Imap::Mailbox::RoleMessageIsMarkedRead ).toBool() );
    markAsDeleted->setChecked( index.data( Imap::Mailbox::RoleMessageIsMarkedDeleted ).toBool() );
}

void MainWindow::updateActionsOnlineOffline( bool online )
{
    reloadMboxList->setEnabled( online );
    resyncMbox->setEnabled( online );
    expunge->setEnabled( online );
    createChildMailbox->setEnabled( online );
    createTopMailbox->setEnabled( online );
    deleteCurrentMailbox->setEnabled( online );
    markAsDeleted->setEnabled( online );
    markAsRead->setEnabled( online );
}

void MainWindow::scrollMessageUp()
{
    area->ensureVisible( 0, 0, 0, 0 );
}

void MainWindow::slotReplyTo()
{
    msgView->reply( this, MessageView::REPLY_SENDER_ONLY );
}

void MainWindow::slotReplyAll()
{
    msgView->reply( this, MessageView::REPLY_ALL );
}

void MainWindow::slotComposeMailUrl( const QUrl &url )
{
    Q_ASSERT( url.scheme().toLower() == QLatin1String("mailto") );

    // FIXME
}

void MainWindow::invokeComposeDialog( const QString& subject, const QString& body,
                                      const QList<QPair<QString,QString> >& recipients )
{
    QSettings s;
    ComposeWidget* w = new ComposeWidget( this );
    w->setData( QString::fromAscii("%1 <%2>").arg(
            s.value( Common::SettingsNames::realNameKey ).toString(),
            s.value( Common::SettingsNames::addressKey ).toString() ),
        recipients, subject, body );
    w->setAttribute( Qt::WA_DeleteOnClose, true );
    w->show();
}

void MainWindow::slotMailboxDeleteFailed( const QString& mailbox, const QString& msg )
{
    QMessageBox::warning( this, tr("Can't delete mailbox"),
                          tr("Deleting mailbox \"%1\" failed with the following message:\n%2").arg( mailbox, msg ) );
}

void MainWindow::slotMailboxCreateFailed( const QString& mailbox, const QString& msg )
{
    QMessageBox::warning( this, tr("Can't create mailbox"),
                          tr("Creating mailbox \"%1\" failed with the following message:\n%2").arg( mailbox, msg ) );
}

void MainWindow::showConnectionStatus( QObject* parser, Imap::ConnectionState state )
{
    Q_UNUSED( parser );
    using namespace Imap;
    QString message = connectionStateToString( state );
    enum { DURATION = 10000 };
    bool transient = false;

    switch ( state ) {
    case CONN_STATE_ESTABLISHED:
    case CONN_STATE_LOGIN_FAILED:
    case CONN_STATE_AUTHENTICATED:
    case CONN_STATE_SELECTED:
        transient = true;
        break;
    default:
        // only the stuff above is transient
        break;
    }
    statusBar()->showMessage( message, transient ? DURATION : 0 );
}

void MainWindow::updateBusyParsers(bool busy)
{
    static bool wasBusy = false;
    busyParsersIndicator->setVisible(busy);
    if ( ! wasBusy && busy ) {
        wasBusy = busy;
        QApplication::setOverrideCursor( Qt::WaitCursor );
    } else if ( wasBusy && ! busy ) {
        wasBusy = busy;
        QApplication::restoreOverrideCursor();
    }
}

void MainWindow::slotShowAboutTrojita()
{
    QMessageBox::about( this, trUtf8("About Trojitá"),
                        trUtf8("<p>This is <b>Trojitá</b>, a Qt IMAP e-mail client</p>"
                               "<p>Copyright &copy; 2007-2010 Jan Kundrát &lt;jkt@flaska.net&gt;</p>"
                               "<p>More information at http://trojita.flaska.net/</p>"
                               "<p>You are using version %1.</p>").arg(
                                       QApplication::applicationVersion() ) );
}

void MainWindow::slotDonateToTrojita()
{
    QDesktopServices::openUrl( QString::fromAscii("http://sourceforge.net/donate/index.php?group_id=339456") );
}

void MainWindow::slotSaveCurrentMessageBody()
{
    QModelIndexList indices = msgListTree->selectionModel()->selectedIndexes();
    for ( QModelIndexList::const_iterator it = indices.begin(); it != indices.end(); ++it ) {
        Q_ASSERT( it->isValid() );
        if ( it->column() != 0 )
            continue;
        if ( ! it->data( Imap::Mailbox::RoleMessageUid ).isValid() )
            continue;
        Imap::Mailbox::TreeItemMessage* message = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(
                Imap::Mailbox::Model::realTreeItem( *it )
                );
        Q_ASSERT( message );

        Imap::Network::MsgPartNetAccessManager* netAccess = new Imap::Network::MsgPartNetAccessManager( this );
        netAccess->setModelMessage( model, message );
        Imap::Network::FileDownloadManager* fileDownloadManager =
                new Imap::Network::FileDownloadManager( this, netAccess,
                    dynamic_cast<Imap::Mailbox::TreeItemPart*>(
                            message->specialColumnPtr( 0, Imap::Mailbox::TreeItem::OFFSET_HEADER ) ) );
        // FIXME: change from "header" into "whole message"
        connect( fileDownloadManager, SIGNAL(succeeded()), fileDownloadManager, SLOT(deleteLater()) );
        connect( fileDownloadManager, SIGNAL(transferError(QString)), fileDownloadManager, SLOT(deleteLater()) );
        connect( fileDownloadManager, SIGNAL(fileNameRequested(QString*)),
                 this, SLOT(slotDownloadMessageFileNameRequested(QString*)) );
        connect( fileDownloadManager, SIGNAL(transferError(QString)),
                 this, SLOT(slotDownloadMessageTransferError(QString)) );
        connect( fileDownloadManager, SIGNAL(destroyed()), netAccess, SLOT(deleteLater()) );
        fileDownloadManager->slotDownloadNow();
    }
}

void MainWindow::slotDownloadMessageTransferError( const QString& errorString )
{
    QMessageBox::critical( this, tr("Can't save message"),
                           tr("Unable to save the attachment. Error:\n%1").arg( errorString ) );
}

void MainWindow::slotDownloadMessageFileNameRequested( QString* fileName )
{
    *fileName = QFileDialog::getSaveFileName( this, tr("Save Message"),
                                  *fileName, QString(),
                                  0, QFileDialog::HideNameFilterDetails
                                  );
}

void MainWindow::slotViewMsgHeaders()
{
    QModelIndexList indices = msgListTree->selectionModel()->selectedIndexes();
    for ( QModelIndexList::const_iterator it = indices.begin(); it != indices.end(); ++it ) {
        Q_ASSERT( it->isValid() );
        if ( it->column() != 0 )
            continue;
        if ( ! it->data( Imap::Mailbox::RoleMessageUid ).isValid() )
            continue;
        Imap::Mailbox::TreeItemMessage* message = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(
                Imap::Mailbox::Model::realTreeItem( *it )
                );
        Q_ASSERT( message );

        Imap::Network::MsgPartNetAccessManager* netAccess = new Imap::Network::MsgPartNetAccessManager( this );
        netAccess->setModelMessage( model, message );

        QScrollArea* area = new QScrollArea();
        area->setWidgetResizable(true);
        SimplePartWidget* headers = new SimplePartWidget( 0, netAccess,
            dynamic_cast<Imap::Mailbox::TreeItemPart*>(
                    message->specialColumnPtr( 0, Imap::Mailbox::TreeItem::OFFSET_HEADER ) ) );
        area->setWidget( headers );
        area->setAttribute( Qt::WA_DeleteOnClose );
        connect( area, SIGNAL(destroyed()), headers, SLOT(deleteLater()) );
        connect( area, SIGNAL(destroyed()), netAccess, SLOT(deleteLater()) );
        area->show();
        // FIXME: add an event filter for scrolling...
    }
}

#ifdef XTUPLE_CONNECT
void MainWindow::slotXtSyncCurrentMailbox()
{
    QModelIndex index = mboxTree->currentIndex();
    if ( ! index.isValid() )
        return;

    QString mailbox = index.data( Imap::Mailbox::RoleMailboxName ).toString();
    QSettings s;
    QStringList mailboxes = s.value( Common::SettingsNames::xtSyncMailboxList ).toStringList();
    if ( xtIncludeMailboxInSync->isChecked() ) {
        if ( ! mailboxes.contains( mailbox ) ) {
            mailboxes.append( mailbox );
        }
    } else {
        mailboxes.removeAll( mailbox );
    }
    s.setValue( Common::SettingsNames::xtSyncMailboxList, mailboxes );
}
#endif

void MainWindow::slotScrollToUnseenMessage( const QModelIndex &mailbox, const QModelIndex &message )
{
    // FIXME: unit tests
    Q_ASSERT(msgListModel);
    Q_ASSERT(msgListTree);
    if ( model->realTreeItem( mailbox ) != msgListModel->currentMailbox() )
        return;

    // So, we have an index from Model, and have to map it through unspecified number of proxies here...
    QList<QAbstractProxyModel*> stack;
    QAbstractItemModel *tempModel = msgListModel;
    while ( QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>( tempModel ) ) {
        stack.insert( 0, proxy );
        tempModel = proxy->sourceModel();
    }
    QModelIndex targetIndex = message;
    Q_FOREACH( QAbstractProxyModel *proxy, stack ) {
        targetIndex = proxy->mapFromSource( targetIndex );
    }

    // ...now, we can scroll, using the translated index
    msgListTree->scrollTo( targetIndex );
}

void MainWindow::slotReleaseSelectedMessage()
{
    QModelIndex index = msgListTree->currentIndex();
    if ( ! index.isValid() )
        return;
    if ( ! index.data( Imap::Mailbox::RoleMessageUid ).isValid() )
        return;

    model->releaseMessageData( index );
}

}


