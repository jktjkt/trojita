#include "MultipartAlternativeReply.h"

#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"

#include <QDebug>

namespace Imap {
namespace Network {

MultipartAlternativeReply::MultipartAlternativeReply( QObject* parent,
    Imap::Mailbox::Model* _model, Imap::Mailbox::TreeItemMessage* _msg,
    Imap::Mailbox::TreeItemPart* _part ):
FormattingReply( parent, _model, _msg, _part)
{
    Q_ASSERT( msg->childrenCount( model ) );

    QStringList allowed;
    allowed << QLatin1String("text/plain") << QLatin1String("text/html");
    Imap::Mailbox::TreeItemPart* target = 0;
    for ( int i = msg->childrenCount( model ) - 1; i >=0; --i ) {
        Imap::Mailbox::TreeItemPart* anotherPart =
                dynamic_cast<Imap::Mailbox::TreeItemPart*>( part->child( i, model ) );
        Q_ASSERT( anotherPart );
        if ( allowed.contains( anotherPart->mimeType() ) ) {
            target = anotherPart;
            break;
        }
    }
    if ( ! target )
        target = static_cast<Imap::Mailbox::TreeItemPart*>( part->child( 0, model ) );
    requestAnotherPart( target );
}

void MultipartAlternativeReply::mainReplyFinished()
{
    // ignore
}

void MultipartAlternativeReply::everythingFinished()
{
    setData( replies[1]->header( QNetworkRequest::ContentTypeHeader ).toString(), replies[1]->readAll() );
    FormattingReply::everythingFinished();
}

}
}

#include "MultipartAlternativeReply.moc"
