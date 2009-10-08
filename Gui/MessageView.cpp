#include <QKeyEvent>
#include <QLabel>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QtWebKit/QWebHistory>
#include <QDebug>

#include "MessageView.h"
#include "EmbeddedWebView.h"
#include "PartWidgetFactory.h"

#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Gui {

MessageView::MessageView( QWidget* parent ): QWidget(parent), message(0), model(0)
{
    netAccess = new Imap::Network::MsgPartNetAccessManager( this );
    emptyView = new EmbeddedWebView( this, netAccess );
    factory = new PartWidgetFactory( netAccess, this );

    layout = new QVBoxLayout( this );
    viewer = emptyView;
    header = new QLabel( this );
    header->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse );
    layout->addWidget( header );
    layout->addWidget( viewer );
    layout->setContentsMargins( 0, 0, 0, 0 );

    markAsReadTimer = new QTimer( this );
    markAsReadTimer->setSingleShot( true );
    connect( markAsReadTimer, SIGNAL(timeout()), this, SLOT(markAsRead()) );

    header->setIndent( 5 );
    header->setWordWrap( true );
}

void MessageView::handleMessageRemoved( void* msg )
{
    if ( msg == message )
        setEmpty();
}

void MessageView::setEmpty()
{
    markAsReadTimer->stop();
    header->setText( QString() );
    message = 0;
    if ( viewer != emptyView ) {
        layout->removeWidget( viewer );
        viewer->deleteLater();
        viewer = emptyView;
        viewer->show();
        layout->addWidget( viewer );
        emit messageChanged();
    }
}

void MessageView::setMessage( const QModelIndex& index )
{
    // first, let's get a real model
    Imap::Mailbox::Model* modelCandidate = dynamic_cast<Imap::Mailbox::Model*>( const_cast<QAbstractItemModel*>( index.model() ) );
    const Imap::Mailbox::MsgListModel* msgListModel = dynamic_cast<const Imap::Mailbox::MsgListModel*>( index.model() );
    Imap::Mailbox::TreeItem* item = Imap::Mailbox::Model::realTreeItem( index );

    if ( modelCandidate ) {
        model = modelCandidate;
    } else {
        Q_ASSERT( msgListModel );
        model = dynamic_cast<Imap::Mailbox::Model*>( msgListModel->sourceModel() );
    }

    // now let's find a real message root
    Imap::Mailbox::TreeItemMessage* messageCandidate = dynamic_cast<Imap::Mailbox::TreeItemMessage*>( item );
    Q_ASSERT( model );
    Q_ASSERT( messageCandidate );

    if ( ! messageCandidate->fetched() ) {
        qDebug() << "Attempted to load a message that hasn't been synced yet";
        setEmpty();
        return;
    }

    Imap::Mailbox::TreeItemPart* part = dynamic_cast<Imap::Mailbox::TreeItemPart*>( messageCandidate->child( 0, model ) );
    Q_ASSERT( part );

    if ( message != messageCandidate ) {
        emptyView->hide();
        layout->removeWidget( viewer );
        if ( viewer != emptyView ) {
            viewer->setParent( 0 );
            viewer->deleteLater();
        }
        message = messageCandidate;
        netAccess->setModelMessage( model, message );
        viewer = factory->create( part );
        viewer->setParent( this );
        layout->addWidget( viewer );
        viewer->show();
        header->setText( headerText() );
        emit messageChanged();

        // We want to propagate the QWheelEvent to upper layers
        viewer->installEventFilter( this );
    }

    if ( model->isNetworkAvailable() )
        markAsReadTimer->start( 2000 ); // FIXME: make this configurable
}

void MessageView::markAsRead()
{
    if ( ! message || ! model->isNetworkAvailable() )
        return;
    model->markMessageRead( message, true );
}

bool MessageView::eventFilter( QObject* object, QEvent* event )
{
    if ( event->type() == QEvent::Wheel ) {
        MessageView::event( event );
        return true;
    } else if ( event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease ) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>( event );
        switch ( keyEvent->key() ) {
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
            case Qt::Key_Home:
            case Qt::Key_End:
                MessageView::event( event );
                return true;
            default:
                return QObject::eventFilter( object, event );
        }
    } else {
        return QObject::eventFilter( object, event );
    }
}

QString MessageView::headerText()
{
    if ( ! message )
        return QString();

    QString res;
    if ( ! message->envelope( model ).from.isEmpty() )
        res += tr("<b>From:</b>&nbsp;%1<br/>").arg( Qt::escape(
                Imap::Message::MailAddress::prettyList( message->envelope( model ).from, false ) ) );
    if ( ! message->envelope( model ).to.isEmpty() )
        res += tr("<b>To:</b>&nbsp;%1<br/>").arg( Qt::escape(
                Imap::Message::MailAddress::prettyList( message->envelope( model ).to, false ) ) );
    if ( ! message->envelope( model ).cc.isEmpty() )
        res += tr("<b>Cc:</b>&nbsp;%1<br/>").arg( Qt::escape(
                Imap::Message::MailAddress::prettyList( message->envelope( model ).cc, false ) ) );
    if ( ! message->envelope( model ).bcc.isEmpty() )
        res += tr("<b>Bcc:</b>&nbsp;%1<br/>").arg( Qt::escape(
                Imap::Message::MailAddress::prettyList( message->envelope( model ).bcc, false ) ) );
    res += tr("<b>Subject:</b>&nbsp;%1").arg( Qt::escape( message->envelope( model ).subject ) );
    if ( message->envelope( model ).date.isValid() )
        res += tr("<br/><b>Date:</b>&nbsp;%1").arg(
                message->envelope( model ).date.toString( Qt::SystemLocaleLongDate ) );
    return res;
}

}

#include "MessageView.moc"
