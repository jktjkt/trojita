#include <QEvent>
#include <QLabel>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QtWebKit/QWebFrame>
#include <QtWebKit/QWebHistory>
#include <QtWebKit/QWebView>
#include <QDebug>

#include "MessageView.h"

#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Network/FormattingNetAccessManager.h"

namespace Gui {

MessageView::MessageView( QWidget* parent ): QWidget(parent), message(0), model(0)
{
    layout = new QVBoxLayout( this );
    webView = new QWebView( this );
    header = new QLabel( this );
    layout->addWidget( header );
    layout->addWidget( webView );
    layout->setContentsMargins( 0, 0, 0, 0 );

    QWebSettings* s = webView->settings();
    s->setAttribute( QWebSettings::JavascriptEnabled, false );
    s->setAttribute( QWebSettings::JavaEnabled, false );
    s->setAttribute( QWebSettings::PluginsEnabled, false );
    s->setAttribute( QWebSettings::PrivateBrowsingEnabled, true );
    s->setAttribute( QWebSettings::JavaEnabled, false );
    s->setAttribute( QWebSettings::OfflineStorageDatabaseEnabled, false );
    s->setAttribute( QWebSettings::OfflineWebApplicationCacheEnabled, false );
    s->setAttribute( QWebSettings::LocalStorageDatabaseEnabled, false );

    netAccess = new Imap::Network::FormattingNetAccessManager( webView );
    webView->page()->setNetworkAccessManager( netAccess );

    markAsReadTimer = new QTimer( this );
    markAsReadTimer->setSingleShot( true );
    connect( markAsReadTimer, SIGNAL(timeout()), this, SLOT(markAsRead()) );

    // Scrolling is implemented on upper layers
    webView->page()->mainFrame()->setScrollBarPolicy( Qt::Horizontal, Qt::ScrollBarAlwaysOff );
    webView->page()->mainFrame()->setScrollBarPolicy( Qt::Vertical, Qt::ScrollBarAlwaysOff );
    connect( webView, SIGNAL(loadFinished(bool)), this, SLOT(pageLoadFinished()) );

    // We want to propagate the QWheelEvent to upper layers
    webView->installEventFilter( this );

    header->setIndent( 5 );
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
    webView->setUrl( QUrl("about:blank") );
    webView->page()->history()->clear();
    webView->setMinimumSize( 1, 1 );
    message = 0;
}

void MessageView::setMessage( const QModelIndex& index )
{
    // first, let's get a real model
    Imap::Mailbox::Model* modelCandidate = dynamic_cast<Imap::Mailbox::Model*>( const_cast<QAbstractItemModel*>( index.model() ) );
    const Imap::Mailbox::MsgListModel* msgListModel = dynamic_cast<const Imap::Mailbox::MsgListModel*>( index.model() );
    Imap::Mailbox::TreeItem* item = static_cast<Imap::Mailbox::TreeItem*>( index.internalPointer() );

    if ( modelCandidate ) {
        model = modelCandidate;
    } else {
        Q_ASSERT( msgListModel );
        model = dynamic_cast<Imap::Mailbox::Model*>( msgListModel->sourceModel() );
    }

    // now let's find a real message root
    Imap::Mailbox::TreeItemMessage* messageCandidate = dynamic_cast<Imap::Mailbox::TreeItemMessage*>( item );
    if ( ! messageCandidate ) {
        const Imap::Mailbox::TreeItemPart* part = dynamic_cast<const Imap::Mailbox::TreeItemPart*>( item );
        if ( part ) {
            messageCandidate = part->message();
            Q_ASSERT( messageCandidate );
        } else {
            // it's something completely different -> let's ignore altogether, perhaps user clicked on a model  or something
            qDebug() << Q_FUNC_INFO << "Can't find out, sorry";
            return;
        }
    }

    Q_ASSERT( model );
    Q_ASSERT( messageCandidate );

    if ( message != messageCandidate ) {
        // So that we don't needlessly re-initialize stuff
        message = messageCandidate;
        webView->setMinimumSize( 1, 1 );
        netAccess->setModelMessage( model, message );
        webView->setUrl( QUrl( QString("trojita-imap://msg/0") ) );
        // There is never more than one top-level child item, so we can safely use /0 as the path
        webView->page()->history()->clear();
        header->setText( headerText() );
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

void MessageView::pageLoadFinished()
{
    webView->setMinimumSize( webView->page()->mainFrame()->contentsSize() );
}

bool MessageView::eventFilter( QObject* object, QEvent* event )
{
    if ( event->type() == QEvent::Wheel ) {
        MessageView::event( event );
        return true;
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
