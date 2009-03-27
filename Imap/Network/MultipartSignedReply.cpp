#include "MultipartSignedReply.h"

#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"

#include <QDebug>

namespace Imap {
namespace Network {

MultipartSignedReply::MultipartSignedReply( QObject* parent,
    const Imap::Mailbox::Model* _model, Imap::Mailbox::TreeItemMessage* _msg,
    Imap::Mailbox::TreeItemPart* _part ):
FormattingReply( parent, _model, _msg, _part)
{
}

void MultipartSignedReply::mainReplyFinished()
{
    if ( part->childrenCount( model ) != 2 ) {
        setData( QLatin1String("text/plain"), "Malformed multipart/signed reply" );
        FormattingReply::everythingFinished();
    } else {
        requestAnotherPart( static_cast<Imap::Mailbox::TreeItemPart*>( part->child( 0, model ) ) );
        requestAnotherPart( static_cast<Imap::Mailbox::TreeItemPart*>( part->child( 1, model ) ) );
    }
}

void MultipartSignedReply::everythingFinished()
{
    // The two sub-requests are finished by now (this is a hard guarantee; if
    // the message was mallformed, this function won't be called
    // FIXME: check the signature later :)
    setData( replies[1]->header( QNetworkRequest::ContentTypeHeader ).toString(), replies[1]->readAll() );
    FormattingReply::everythingFinished();
}

}
}

#include "MultipartSignedReply.moc"
