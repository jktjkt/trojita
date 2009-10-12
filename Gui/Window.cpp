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

#include <QAuthenticator>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSplitter>
#include <QToolBar>

#include "Window.h"
#include "ComposeWidget.h"
#include "MessageView.h"
#include "MsgListWidget.h"
#include "SettingsDialog.h"
#include "SettingsNames.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/ModelWatcher.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/MemoryCache.h"
#include "Imap/Model/PrettyMailboxModel.h"
#include "Streams/SocketFactory.h"

#include "ui_CreateMailboxDialog.h"

#include "Imap/Model/ModelTest/modeltest.h"

#include "iconloader/qticonloader.h"

/** @short All user-facing widgets and related classes */
namespace Gui {

MainWindow::MainWindow(): QMainWindow()
{
    setWindowTitle( trUtf8("Trojitá") );
    createWidgets();
    setupModels();
    createActions();
    createMenus();
}

void MainWindow::createActions()
{
    reloadMboxList = new QAction( style()->standardIcon(QStyle::SP_ArrowRight), tr("Update List of Child Mailboxes"), this );
    connect( reloadMboxList, SIGNAL( triggered() ), this, SLOT( slotReloadMboxList() ) );

    resyncMbox = new QAction( QtIconLoader::icon( QLatin1String("view-refresh") ), tr("Check for New Messages"), this );
    connect( resyncMbox, SIGNAL(triggered()), this, SLOT(slotResyncMbox()) );

    reloadAllMailboxes = new QAction( tr("Reload Everything"), this );
    // connect later

    exitAction = new QAction( QtIconLoader::icon( QLatin1String("application-exit") ), tr("E&xit"), this );
    exitAction->setShortcut( tr("Ctrl+Q") );
    exitAction->setStatusTip( tr("Exit the application") );
    connect( exitAction, SIGNAL( triggered() ), this, SLOT( close() ) );

    QActionGroup* netPolicyGroup = new QActionGroup( this );
    netPolicyGroup->setExclusive( true );
    netOffline = new QAction( tr("Offline"), netPolicyGroup );
    netOffline->setCheckable( true );
    // connect later
    netExpensive = new QAction( tr("Expensive Connection"), netPolicyGroup );
    netExpensive->setCheckable( true );
    // connect later
    netOnline = new QAction( tr("Free Access"), netPolicyGroup );
    netOnline->setCheckable( true );
    // connect later

    showFullView = new QAction( QtIconLoader::icon( QLatin1String("edit-find-mail") ), tr("Show Full Tree Window"), this );
    showFullView->setCheckable( true );
    connect( showFullView, SIGNAL( triggered(bool) ), this, SLOT( fullViewToggled(bool) ) );
    connect( allDock, SIGNAL( visibilityChanged(bool) ), showFullView, SLOT( setChecked(bool) ) );

    showMenuBar = new QAction( QtIconLoader::icon( QLatin1String("view-list-text") ),  tr("Show Main Menu Bar"), this );
    showMenuBar->setCheckable( true );
    showMenuBar->setChecked( true );
    showMenuBar->setShortcut( tr("Ctrl+M") );
    addAction( showMenuBar ); // otherwise it won't work with hidden menu bar
    connect( showMenuBar, SIGNAL( triggered(bool) ), menuBar(), SLOT(setShown(bool)) );

    configSettings = new QAction( QtIconLoader::icon( QLatin1String("configure") ),  tr("Settings..."), this );
    connect( configSettings, SIGNAL( triggered() ), this, SLOT( slotShowSettings() ) );

    composeMail = new QAction( QtIconLoader::icon( QLatin1String("document-edit") ),  tr("Compose Mail..."), this );
    connect( composeMail, SIGNAL(triggered()), this, SLOT(slotComposeMail()) );

    expunge = new QAction( QtIconLoader::icon( QLatin1String("trash-empty") ),  tr("Expunge Mailbox"), this );
    connect( expunge, SIGNAL(triggered()), this, SLOT(slotExpunge()) );

    markAsRead = new QAction( QtIconLoader::icon( QLatin1String("mail-mark-read") ),  tr("Mark as Read"), this );
    markAsRead->setCheckable( true );
    markAsRead->setShortcut( Qt::Key_M );
    msgListTree->addAction( markAsRead );
    connect( markAsRead, SIGNAL(triggered(bool)), this, SLOT(handleMarkAsRead(bool)) );

    markAsDeleted = new QAction( QtIconLoader::icon( QLatin1String("list-remove") ),  tr("Mark as Deleted"), this );
    markAsDeleted->setCheckable( true );
    markAsDeleted->setShortcut( Qt::Key_Delete );
    msgListTree->addAction( markAsDeleted );
    connect( markAsDeleted, SIGNAL(triggered(bool)), this, SLOT(handleMarkAsDeleted(bool)) );

    createChildMailbox = new QAction( tr("Create Child Mailbox..."), this );
    connect( createChildMailbox, SIGNAL(triggered()), this, SLOT(slotCreateMailboxBelowCurrent()) );

    createTopMailbox = new QAction( tr("Create New Mailbox..."), this );
    connect( createTopMailbox, SIGNAL(triggered()), this, SLOT(slotCreateTopMailbox()) );

    deleteCurrentMailbox = new QAction( tr("Delete Mailbox"), this );
    connect( deleteCurrentMailbox, SIGNAL(triggered()), this, SLOT(slotDeleteCurrentMailbox()) );

    connectModelActions();

    QToolBar *toolBar = addToolBar(tr("Navigation"));
    toolBar->addAction(composeMail);
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
    imapMenu->addAction( expunge );
    imapMenu->addSeparator()->setText( tr("Network Access") );
    QMenu* netPolicyMenu = imapMenu->addMenu( tr("Network Access"));
    netPolicyMenu->addAction( netOffline );
    netPolicyMenu->addAction( netExpensive );
    netPolicyMenu->addAction( netOnline );
    imapMenu->addSeparator();
    imapMenu->addAction( showFullView );
    imapMenu->addSeparator();
    imapMenu->addAction( configSettings );
    imapMenu->addAction( showMenuBar );
    imapMenu->addSeparator();
    imapMenu->addAction( exitAction );

    QMenu* mailboxMenu = menuBar()->addMenu( tr("Mailbox") );
    mailboxMenu->addAction( resyncMbox );
    mailboxMenu->addSeparator();
    mailboxMenu->addAction( reloadAllMailboxes );
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
    connect( mboxTree, SIGNAL( customContextMenuRequested( const QPoint & ) ),
            this, SLOT( showContextMenuMboxTree( const QPoint& ) ) );

    msgListTree = new MsgListWidget();
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
    area->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
    area->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
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
}

void MainWindow::setupModels()
{
    Imap::Mailbox::SocketFactoryPtr factory;
    QSettings s;

    if ( ! s.contains( SettingsNames::imapMethodKey ) ) {
        QTimer::singleShot( 0, this, SLOT(slotShowSettings()) );
    }

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
    if ( ! QDir().mkpath( cacheDir ) ) {
        QMessageBox::critical( this, tr("Cache Error"), tr("Failed to create directory %1").arg( cacheDir ) );
        cacheDir = QString();
    } else {
        cacheDir += QLatin1String("/imap.cache");
    }
    cache = Imap::Mailbox::CachePtr( new Imap::Mailbox::MemoryCache( cacheDir ) );
    model = new Imap::Mailbox::Model( this, cache, factory, s.value( SettingsNames::imapStartOffline ).toBool() );
    model->setObjectName( QLatin1String("model") );
    mboxModel = new Imap::Mailbox::MailboxModel( this, model );
    mboxModel->setObjectName( QLatin1String("mboxModel") );
    prettyMboxModel = new Imap::Mailbox::PrettyMailboxModel( this, mboxModel );
    prettyMboxModel->setObjectName( QLatin1String("prettyMboxModel") );
    msgListModel = new Imap::Mailbox::MsgListModel( this, model );
    msgListModel->setObjectName( QLatin1String("msgListModel") );

    connect( mboxTree, SIGNAL( clicked(const QModelIndex&) ), msgListModel, SLOT( setMailbox(const QModelIndex&) ) );
    connect( mboxTree, SIGNAL( activated(const QModelIndex&) ), msgListModel, SLOT( setMailbox(const QModelIndex&) ) );
    connect( mboxTree, SIGNAL( clicked(const QModelIndex&) ), model, SLOT( switchToMailbox( const QModelIndex& ) ) );
    connect( mboxTree, SIGNAL( activated(const QModelIndex&) ), model, SLOT( switchToMailbox( const QModelIndex& ) ) );
    connect( msgListModel, SIGNAL( mailboxChanged() ), this, SLOT( slotResizeMsgListColumns() ) );
    connect( msgListModel, SIGNAL( dataChanged(QModelIndex,QModelIndex) ), this, SLOT( updateMessageFlags() ) );

    connect( msgListTree, SIGNAL( activated(const QModelIndex&) ), this, SLOT( msgListClicked(const QModelIndex&) ) );
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

    //Imap::Mailbox::ModelWatcher* w = new Imap::Mailbox::ModelWatcher( this );
    //w->setModel( model );

    //ModelTest* tester = new ModelTest( prettyMboxModel, this ); // when testing, test just one model at time

    mboxTree->setModel( prettyMboxModel );
    msgListTree->setModel( msgListModel );
    connect( msgListTree->selectionModel(), SIGNAL( selectionChanged( const QItemSelection&, const QItemSelection& ) ),
             this, SLOT( msgListSelectionChanged( const QItemSelection&, const QItemSelection& ) ) );
}

void MainWindow::msgListSelectionChanged( const QItemSelection& selected, const QItemSelection& deselected )
{
    Q_UNUSED( deselected );
    if ( selected.indexes().isEmpty() )
        return;

    QModelIndex index = selected.indexes().front();
    markAsRead->setChecked( msgListModel->data( index, Imap::Mailbox::MsgListModel::RoleIsMarkedAsRead ).toBool() );
    markAsDeleted->setChecked( msgListModel->data( index, Imap::Mailbox::MsgListModel::RoleIsMarkedAsDeleted ).toBool() );
    msgView->setMessage( index );
}

void MainWindow::msgListClicked( const QModelIndex& index )
{
    Q_ASSERT( index.isValid() );
    Q_ASSERT( index.model() == msgListModel );

    if ( index.column() == Imap::Mailbox::MsgListModel::SEEN ) {
        Imap::Mailbox::TreeItemMessage* message = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(
                Imap::Mailbox::Model::realTreeItem( index )
                );
        Q_ASSERT( message );
        if ( ! message->fetched() )
            return;
        model->markMessageRead( message, ! message->isMarkedAsRead() );
    } else {
        msgView->setMessage( index );
        msgListTree->setCurrentIndex( index );
    }
}

void MainWindow::msgListDoubleClicked( const QModelIndex& index )
{
    Q_ASSERT( index.isValid() );
    Q_ASSERT( index.model() == msgListModel );

    MessageView* newView = new MessageView( 0 );
    QModelIndex realIndex = msgListModel->mapToSource( index );
    Imap::Mailbox::TreeItemMessage* message = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(
            Imap::Mailbox::Model::realTreeItem( index ) );
    Q_ASSERT( message );
    Q_ASSERT( realIndex.model() == model );
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
    widget->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
    widget->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
    widget->setWindowTitle( message->envelope( 0 ).subject ); // FIXME: nullptr!
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
        markAsRead->setChecked( msgListModel->data( index, Imap::Mailbox::MsgListModel::RoleIsMarkedAsRead ).toBool() );
        actionList.append( markAsRead );
        markAsDeleted->setChecked( msgListModel->data( index, Imap::Mailbox::MsgListModel::RoleIsMarkedAsDeleted ).toBool() );
        actionList.append( markAsDeleted );
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
        model->resyncMailbox( mbox );
    }
}

void MainWindow::alertReceived( const QString& message )
{
    QMessageBox::warning( this, tr("IMAP Alert"), message );
}

void MainWindow::connectionError( const QString& message )
{
    // hack: this slot is called even on the first run with no configuration
    if ( QSettings().contains( SettingsNames::imapMethodKey ) ) {
        QMessageBox::critical( this, tr("Connection Error"), message );
    } else {
        slotShowSettings();
    }
    netOffline->trigger();
}

void MainWindow::networkPolicyOffline()
{
    netOffline->setChecked( true );
    netExpensive->setChecked( false );
    netOnline->setChecked( false );
    updateActionsOnlineOffline( false );
}

void MainWindow::networkPolicyExpensive()
{
    netOffline->setChecked( false );
    netExpensive->setChecked( true );
    netOnline->setChecked( false );
    updateActionsOnlineOffline( true );
}

void MainWindow::networkPolicyOnline()
{
    netOffline->setChecked( false );
    netExpensive->setChecked( false );
    netOnline->setChecked( true );
    updateActionsOnlineOffline( true );
}

void MainWindow::fullViewToggled( bool showIt )
{
    if ( showIt ) {
        allDock->show();
        allTree->setModel( model );
        addDockWidget(Qt::LeftDockWidgetArea, allDock);
    } else {
        removeDockWidget( allDock );
    }
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
    QString user = s.value( SettingsNames::imapUserKey ).toString();
    QString pass = s.value( SettingsNames::imapPassKey ).toString();
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
    msgListModel->deleteLater();
    msgListModel = 0;
    mboxModel->deleteLater();
    mboxModel = 0;
    prettyMboxModel->deleteLater();
    prettyMboxModel = 0;
    model->deleteLater();
    model = 0;
    cache.clear();
}

void MainWindow::slotComposeMail()
{
    QSettings s;
    ComposeWidget* w = new ComposeWidget( this );
    w->setData( QString::fromAscii("%1 <%2>").arg(
                s.value( SettingsNames::realNameKey ).toString(),
                s.value( SettingsNames::addressKey ).toString() ),
        QList<QPair<QString,QString> >(),
        QString(), QString() );
    w->setAttribute( Qt::WA_DeleteOnClose, true );
    w->show();
}

void MainWindow::handleMarkAsRead( bool value )
{
    QModelIndexList indices = msgListTree->selectionModel()->selectedIndexes();
    for ( QModelIndexList::const_iterator it = indices.begin(); it != indices.end(); ++it ) {
        Q_ASSERT( it->isValid() );
        Q_ASSERT( it->model() == msgListModel );
        if ( it->column() != 0 )
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
        Q_ASSERT( it->model() == msgListModel );
        if ( it->column() != 0 )
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
    markAsRead->setChecked( msgListModel->data( msgListTree->currentIndex(), Imap::Mailbox::MsgListModel::RoleIsMarkedAsRead ).toBool() );
    markAsDeleted->setChecked( msgListModel->data( msgListTree->currentIndex(), Imap::Mailbox::MsgListModel::RoleIsMarkedAsDeleted ).toBool() );
}

void MainWindow::updateActionsOnlineOffline( bool online )
{
    reloadMboxList->setEnabled( online );
    reloadAllMailboxes->setEnabled( online );
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

}

#include "Window.moc"
