#include <QDockWidget>
#include <QTreeView>

#include "Window.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxModel.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Streams/SocketFactory.h"


namespace Gui {

MainWindow::MainWindow(): QMainWindow()
{
    createDockWindows();
    setupModels();
}

void MainWindow::createDockWindows()
{
    QDockWidget* dock = new QDockWidget( "Mailbox Folders", this );
    mboxTree = new QTreeView( dock );
    mboxTree->setUniformRowHeights( true );
    mboxTree->setHeaderHidden( true );
    dock->setWidget( mboxTree );
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    dock = new QDockWidget( "Messages", this );
    msgListTree = new QTreeView( dock );
    msgListTree->setUniformRowHeights( true );
    msgListTree->setHeaderHidden( true );
    dock->setWidget( msgListTree );
    addDockWidget(Qt::RightDockWidgetArea, dock);

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
    mboxModel = new Imap::Mailbox::MailboxModel( this, model );
    msgListModel = new Imap::Mailbox::MsgListModel( this, model );

    QObject::connect( mboxTree, SIGNAL( clicked(const QModelIndex&) ), msgListModel, SLOT( setMailbox(const QModelIndex&) ) );

    mboxTree->setModel( mboxModel );
    msgListTree->setModel( msgListModel );
}

}

#include "Window.moc"
