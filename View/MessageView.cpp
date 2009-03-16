#include <QVBoxLayout>
#include <QtWebKit/QWebView>
#include <QDebug>

#include "MessageView.h"

#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/MailboxTree.h"

namespace Gui {

MessageView::MessageView( QWidget* parent ): QWidget(parent), message(0), model(0)
{
    layout = new QVBoxLayout( this );
    webView = new QWebView( this );
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
}

void MessageView::setMessage( const QModelIndex& index )
{
    // first, let's get a real model
    const Imap::Mailbox::Model* modelCandidate = dynamic_cast<const Imap::Mailbox::Model*>( index.model() );
    const Imap::Mailbox::MsgListModel* msgListModel = dynamic_cast<const Imap::Mailbox::MsgListModel*>( index.model() );
    const Imap::Mailbox::TreeItem* item = static_cast<Imap::Mailbox::TreeItem*>( index.internalPointer() );

    if ( modelCandidate ) {
        model = modelCandidate;
    } else {
        Q_ASSERT( msgListModel );
        model = dynamic_cast<const Imap::Mailbox::Model*>( msgListModel->sourceModel() );
    }

    // now let's find a real message root
    const Imap::Mailbox::TreeItemMessage* messageCandidate = dynamic_cast<const Imap::Mailbox::TreeItemMessage*>( item );
    if ( messageCandidate ) {
        message = messageCandidate;
    } else {
        const Imap::Mailbox::TreeItemPart* part = dynamic_cast<const Imap::Mailbox::TreeItemPart*>( item );
        if ( part ) {
            messageCandidate = part->message();
            Q_ASSERT( messageCandidate );
            message = messageCandidate;
        } else {
            // it's something completely different -> let's ignore altogether, perhaps user clicked on a model  or something
            qDebug() << Q_FUNC_INFO << "Can't find out, sorry";
            return;
        }
    }

    Q_ASSERT( model );
    Q_ASSERT( message );

    webView->setUrl( QUrl( QString("trojita-imap://current-msg") ) );
}

}

#include "MessageView.moc"
