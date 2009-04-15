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

#include <QDockWidget>
#include <QHeaderView>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QTreeView>

#include "Window.h"
#include "MessageView.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/ModelWatcher.h"
#include "Imap/Model/MsgListModel.h"
#include "Streams/SocketFactory.h"

#include "Imap/Model/ModelTest/modeltest.h"

namespace Gui {

MainWindow::MainWindow(): QMainWindow()
{
    createWidgets();
    setupModels();
    createActions();
    createMenus();
}

void MainWindow::createActions()
{
    reloadMboxList = new QAction( tr("Rescan Child Mailboxes"), this );
    connect( reloadMboxList, SIGNAL( triggered() ), this, SLOT( slotReloadMboxList() ) );

    reloadAllMailboxes = new QAction( tr("Reload All Mailboxes"), this );
    connect( reloadAllMailboxes, SIGNAL( triggered() ), model, SLOT( reloadMailboxList() ) );

    exitAction = new QAction( tr("E&xit"), this );
    exitAction->setShortcut( tr("Ctrl+Q") );
    exitAction->setStatusTip( tr("Exit the application") );
    connect( exitAction, SIGNAL( triggered() ), this, SLOT( close() ) );

    QActionGroup* netPolicyGroup = new QActionGroup( this );
    netPolicyGroup->setExclusive( true );
    netOffline = new QAction( tr("Offline"), netPolicyGroup );
    netOffline->setCheckable( true );
    connect( netOffline, SIGNAL( triggered() ), model, SLOT( setNetworkOffline() ) );
    netExpensive = new QAction( tr("Expensive Connection"), netPolicyGroup );
    netExpensive->setCheckable( true );
    connect( netExpensive, SIGNAL( triggered() ), model, SLOT( setNetworkExpensive() ) );
    netOnline = new QAction( tr("Free Access"), netPolicyGroup );
    netOnline->setCheckable( true );
    connect( netOnline, SIGNAL( triggered() ), model, SLOT( setNetworkOnline() ) );

    showFullView = new QAction( tr("Show Full Tree Window"), this );
    showFullView->setCheckable( true );
    connect( showFullView, SIGNAL( triggered(bool) ), this, SLOT( fullViewToggled(bool) ) );
    connect( allDock, SIGNAL( visibilityChanged(bool) ), showFullView, SLOT( setChecked(bool) ) );
}

void MainWindow::createMenus()
{
    QMenu* imapMenu = menuBar()->addMenu( tr("IMAP") );
    QMenu* newMenu = imapMenu->addMenu( tr("New") );
    imapMenu->addSeparator()->setText( tr("Network Access") );
    imapMenu->addAction( netOffline );
    imapMenu->addAction( netExpensive );
    imapMenu->addAction( netOnline );
    imapMenu->addSeparator();
    imapMenu->addAction( showFullView );
    imapMenu->addSeparator();
    imapMenu->addAction( exitAction );

    QMenu* mailboxMenu = menuBar()->addMenu( tr("Mailbox") );
    mailboxMenu->addAction( reloadAllMailboxes );
}

void MainWindow::createWidgets()
{
    mboxTree = new QTreeView();
    mboxTree->setUniformRowHeights( true );
    mboxTree->setContextMenuPolicy(Qt::CustomContextMenu);
    mboxTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
    mboxTree->setAllColumnsShowFocus( true );
    connect( mboxTree, SIGNAL( customContextMenuRequested( const QPoint & ) ),
            this, SLOT( showContextMenuMboxTree( const QPoint& ) ) );

    msgListTree = new QTreeView();
    msgListTree->setUniformRowHeights( true );
    msgListTree->setSelectionMode( QAbstractItemView::ExtendedSelection );
    msgListTree->setAllColumnsShowFocus( true );

    msgView = new MessageView();

    QSplitter* hSplitter = new QSplitter();
    QSplitter* vSplitter = new QSplitter();
    vSplitter->setOrientation( Qt::Vertical );
    vSplitter->addWidget( msgListTree );
    vSplitter->addWidget( msgView );
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
    Imap::Mailbox::SocketFactoryPtr factory(
            new Imap::Mailbox::UnixProcessSocketFactory( "/usr/sbin/dovecot",
                QStringList() << "--exec-mail" << "imap" )
            /*new Imap::Mailbox::ProcessSocketFactory( "ssh",
                QStringList() << "sosna.fzu.cz" << "/usr/sbin/imapd" )*/
            );

    cache.reset( new Imap::Mailbox::NoCache() );
    model = new Imap::Mailbox::Model( this, cache, authenticator, factory );
    model->setObjectName( QLatin1String("model") );
    mboxModel = new Imap::Mailbox::MailboxModel( this, model );
    mboxModel->setObjectName( QLatin1String("mboxModel") );
    msgListModel = new Imap::Mailbox::MsgListModel( this, model );
    msgListModel->setObjectName( QLatin1String("msgListModel") );

    QObject::connect( mboxTree, SIGNAL( clicked(const QModelIndex&) ), msgListModel, SLOT( setMailbox(const QModelIndex&) ) );
    QObject::connect( mboxTree, SIGNAL( activated(const QModelIndex&) ), msgListModel, SLOT( setMailbox(const QModelIndex&) ) );
    QObject::connect( msgListModel, SIGNAL( mailboxChanged() ), this, SLOT( slotResizeMsgListColumns() ) );

    QObject::connect( msgListTree, SIGNAL( clicked(const QModelIndex&) ), msgView, SLOT( setMessage(const QModelIndex&) ) );
    QObject::connect( msgListTree, SIGNAL( activated(const QModelIndex&) ), msgView, SLOT( setMessage(const QModelIndex&) ) );

    connect( model, SIGNAL( alertReceived( const QString& ) ), this, SLOT( alertReceived( const QString& ) ) );

    connect( model, SIGNAL( networkPolicyOffline() ), this, SLOT( networkPolicyOffline() ) );
    connect( model, SIGNAL( networkPolicyExpensive() ), this, SLOT( networkPolicyExpensive() ) );
    connect( model, SIGNAL( networkPolicyOnline() ), this, SLOT( networkPolicyOnline() ) );

    //Imap::Mailbox::ModelWatcher* w = new Imap::Mailbox::ModelWatcher( this );
    //w->setModel( model );

    //ModelTest* tester = new ModelTest( mboxModel, this ); // when testing, test just one model at time

    mboxTree->setModel( mboxModel );
    msgListTree->setModel( msgListModel );
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
        actionList.append( reloadMboxList );
    }
    actionList.append( reloadAllMailboxes );
    QMenu::exec( actionList, mboxTree->mapToGlobal( position ) );
}

void MainWindow::slotReloadMboxList()
{
    QModelIndexList indices = mboxTree->selectionModel()->selectedIndexes();
    for ( QModelIndexList::const_iterator it = indices.begin(); it != indices.end(); ++it ) {
        Q_ASSERT( it->isValid() );
        Q_ASSERT( it->model() == mboxModel );
        if ( it->column() != 0 )
            continue;
        Imap::Mailbox::TreeItemMailbox* mbox = dynamic_cast<Imap::Mailbox::TreeItemMailbox*>(
                static_cast<Imap::Mailbox::TreeItem*>(
                    mboxModel->mapToSource( *it ).internalPointer()
                    )
                );
        Q_ASSERT( mbox );
        mbox->rescanForChildMailboxes( model );
    }
}

void MainWindow::alertReceived( const QString& message )
{
    QMessageBox::warning( this, tr("IMAP Alert"), message );
}

void MainWindow::networkPolicyOffline()
{
    netOffline->setChecked( true );
    netExpensive->setChecked( false );
    netOnline->setChecked( false );
}

void MainWindow::networkPolicyExpensive()
{
    netOffline->setChecked( false );
    netExpensive->setChecked( true );
    netOnline->setChecked( false );
}

void MainWindow::networkPolicyOnline()
{
    netOffline->setChecked( false );
    netExpensive->setChecked( false );
    netOnline->setChecked( true );
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

}

#include "Window.moc"
