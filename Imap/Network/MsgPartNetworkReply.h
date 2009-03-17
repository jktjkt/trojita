#ifndef MSGPARTNETWORKREPLY_H
#define MSGPARTNETWORKREPLY_H

#include <QNetworkReply>

namespace Imap {
namespace Network {

class MsgPartNetworkReply : public QNetworkReply
{
Q_OBJECT
public:
    MsgPartNetworkReply( QObject* parent );
};

}
}

#endif // MSGPARTNETWORKREPLY_H
