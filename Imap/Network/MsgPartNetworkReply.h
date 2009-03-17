#ifndef MSGPARTNETWORKREPLY_H
#define MSGPARTNETWORKREPLY_H

#include <QNetworkReply>

namespace Imap {
namespace Mailbox {
class Model;
class TreeItemMessage;
}

namespace Network {

class MsgPartNetworkReply : public QNetworkReply
{
Q_OBJECT
public:
    MsgPartNetworkReply( QObject* parent, const Imap::Mailbox::Model* _model,
        const Imap::Mailbox::TreeItemMessage* _msg );
    virtual void abort();
protected:
    virtual qint64 readData( char* data, qint64 maxSize );
private:
    const Imap::Mailbox::Model* model;
    const Imap::Mailbox::TreeItemMessage* msg;
};

}
}

#endif // MSGPARTNETWORKREPLY_H
