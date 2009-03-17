#include "MsgPartNetworkReply.h"

namespace Imap {
namespace Network {

MsgPartNetworkReply::MsgPartNetworkReply( QObject* parent,
        const Imap::Mailbox::Model* _model,
        const Imap::Mailbox::TreeItemMessage* _msg ):
    QNetworkReply(parent), model(_model), msg(_msg)
{
}

void MsgPartNetworkReply::abort()
{
    // can't really do anything
}

qint64 MsgPartNetworkReply::readData( char* data, qint64 maxSize )
{
    // FIXME
    return -1;
}

}
}
#include "MsgPartNetworkReply.moc"
