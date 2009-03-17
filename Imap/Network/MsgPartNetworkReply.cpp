#include "MsgPartNetworkReply.h"

namespace Imap {
namespace Network {

MsgPartNetworkReply::MsgPartNetworkReply( QObject* parent ):
    QNetworkReply(parent)
{
}

}
}
#include "MsgPartNetworkReply.moc"
