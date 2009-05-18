#include "MultipartRelatedReply.h"

#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"

#include <QDebug>

namespace Imap {
namespace Network {

MultipartRelatedReply::MultipartRelatedReply( QObject* parent,
    Imap::Mailbox::Model* _model, Imap::Mailbox::TreeItemMessage* _msg,
    Imap::Mailbox::TreeItemPart* _part ):
FormattingReply( parent, _model, _msg, _part)
{
    Q_ASSERT( msg->childrenCount( model ) );

}

void MultipartRelatedReply::mainReplyFinished()
{
    // ignore
}

void MultipartRelatedReply::everythingFinished()
{
    setData( replies[1]->header( QNetworkRequest::ContentTypeHeader ).toString(), replies[1]->readAll() );
    FormattingReply::everythingFinished();
}

}
}

#include "MultipartRelatedReply.moc"
