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
}

void MultipartAlternativeReply::mainReplyFinished()
{
    // FIXME
}

void MultipartAlternativeReply::everythingFinished()
{
    // FIXME
}

}
}

#include "MultipartAlternativeReply.moc"
