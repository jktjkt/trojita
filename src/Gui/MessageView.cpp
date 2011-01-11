/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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
#include <QKeyEvent>
#include <QLabel>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtWebKit/QWebHistory>
#include <QDebug>

#include "MessageView.h"
#include "AbstractPartWidget.h"
#include "EmbeddedWebView.h"
#include "ExternalElementsWidget.h"
#include "Window.h"
#include "PartWidgetFactory.h"

#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Gui {

MessageView::MessageView( QWidget* parent ): QWidget(parent), message(0), model(0)
{
    netAccess = new Imap::Network::MsgPartNetAccessManager( this );
    connect(netAccess, SIGNAL(requestingExternal(QUrl)), this, SLOT(externalsRequested(QUrl)));
    emptyView = new EmbeddedWebView( this, netAccess );
    factory = new PartWidgetFactory( netAccess, this );

    layout = new QVBoxLayout( this );
    viewer = emptyView;
    header = new QLabel( this );
    header->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse );
    connect( header, SIGNAL(linkHovered(QString)), this, SLOT(linkInTitleHovered(QString)) );
    externalElements = new ExternalElementsWidget( this );
    externalElements->hide();
    connect(externalElements, SIGNAL(loadingEnabled()), this, SLOT(externalsEnabled()));
    layout->addWidget( externalElements );
    layout->addWidget( header );
    layout->addWidget( viewer );
    layout->setContentsMargins( 0, 0, 0, 0 );

    markAsReadTimer = new QTimer( this );
    markAsReadTimer->setSingleShot( true );
    connect( markAsReadTimer, SIGNAL(timeout()), this, SLOT(markAsRead()) );

    header->setIndent( 5 );
    header->setWordWrap( true );
}

MessageView::~MessageView()
{
    delete factory;
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
    Imap::Mailbox::TreeItem *item;
    const Imap::Mailbox::Model *realModel;
    item = Imap::Mailbox::Model::realTreeItem( index, &realModel, 0 );
    Q_ASSERT(item);
    model = const_cast<Imap::Mailbox::Model*>(realModel);

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
        netAccess->setExternalsEnabled( false );
        externalElements->hide();
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
        res += tr("<b>From:</b>&nbsp;%1<br/>").arg(
                Imap::Message::MailAddress::prettyList( message->envelope( model ).from, Imap::Message::MailAddress::FORMAT_CLICKABLE ) );
    if ( ! message->envelope( model ).to.isEmpty() )
        res += tr("<b>To:</b>&nbsp;%1<br/>").arg(
                Imap::Message::MailAddress::prettyList( message->envelope( model ).to, Imap::Message::MailAddress::FORMAT_CLICKABLE ) );
    if ( ! message->envelope( model ).cc.isEmpty() )
        res += tr("<b>Cc:</b>&nbsp;%1<br/>").arg(
                Imap::Message::MailAddress::prettyList( message->envelope( model ).cc, Imap::Message::MailAddress::FORMAT_CLICKABLE ) );
    if ( ! message->envelope( model ).bcc.isEmpty() )
        res += tr("<b>Bcc:</b>&nbsp;%1<br/>").arg(
                Imap::Message::MailAddress::prettyList( message->envelope( model ).bcc, Imap::Message::MailAddress::FORMAT_CLICKABLE ) );
    res += tr("<b>Subject:</b>&nbsp;%1").arg( Qt::escape( message->envelope( model ).subject ) );
    if ( message->envelope( model ).date.isValid() )
        res += tr("<br/><b>Date:</b>&nbsp;%1").arg(
                message->envelope( model ).date.toString( Qt::SystemLocaleLongDate ) );
    return res;
}

QString MessageView::quoteText() const
{
    const AbstractPartWidget* w = dynamic_cast<const AbstractPartWidget*>( viewer );
    return w ? w->quoteMe() : QString();
}

void MessageView::reply( MainWindow* mainWindow, ReplyMode mode )
{
    if ( ! message )
        return;

    const Imap::Message::Envelope& envelope = message->envelope( model );
    QList<QPair<QString,QString> > recipients;
    for ( QList<Imap::Message::MailAddress>::const_iterator it = envelope.from.begin(); it != envelope.from.end(); ++it ) {
        recipients << qMakePair( tr("To"), QString::fromAscii("%1@%2").arg( it->mailbox, it->host ));
    }
    if ( mode == REPLY_ALL ) {
        for ( QList<Imap::Message::MailAddress>::const_iterator it = envelope.to.begin(); it != envelope.to.end(); ++it ) {
            recipients << qMakePair( tr("Cc"), QString::fromAscii("%1@%2").arg( it->mailbox, it->host ));
        }
        for ( QList<Imap::Message::MailAddress>::const_iterator it = envelope.cc.begin(); it != envelope.cc.end(); ++it ) {
            recipients << qMakePair( tr("Cc"), QString::fromAscii("%1@%2").arg( it->mailbox, it->host ));
        }
    }
    mainWindow->invokeComposeDialog( replySubject( envelope.subject ), quoteText(), recipients );
}

QString MessageView::replySubject( const QString& subject )
{
    if ( ! subject.startsWith(tr("Re:")) )
        return tr("Re: ") + subject;
    else
        return subject;
}

void MessageView::externalsRequested( const QUrl &url )
{
    Q_UNUSED(url);
    externalElements->show();
}

void MessageView::externalsEnabled()
{
    netAccess->setExternalsEnabled( true );
    externalElements->hide();
    AbstractPartWidget* w = dynamic_cast<AbstractPartWidget*>( viewer );
    if ( w )
        w->reloadContents();
}

void MessageView::linkInTitleHovered( const QString &target )
{
    if ( target.isEmpty() ) {
        header->setToolTip( QString() );
        return;
    }

    QUrl url(target);
    QString niceName = url.queryItemValue( QLatin1String("X-Trojita-DisplayName") );
    if ( niceName.isEmpty() )
        header->setToolTip( QString::fromAscii("%1@%2").arg(
                Qt::escape( url.userName() ), Qt::escape( url.host() ) ) );
    else
        header->setToolTip( QString::fromAscii("<p style='white-space:pre'>%1 &lt;%2@%3&gt;</p>").arg(
                Qt::escape( niceName ), Qt::escape( url.userName() ), Qt::escape( url.host() ) ) );
}

}

