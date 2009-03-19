#ifndef MSGPARTNETWORKREPLY_H
#define MSGPARTNETWORKREPLY_H

#include <QNetworkReply>

namespace Imap {
namespace Mailbox {
class Model;
class TreeItemMessage;
class TreeItemPart;
}

namespace Network {

class MsgPartNetworkReply : public QNetworkReply
{
Q_OBJECT
public:
    MsgPartNetworkReply( QObject* parent, const Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, const QString& _part );
    virtual void abort();
protected:
    virtual qint64 readData( char* data, qint64 maxSize );
private:
    const Imap::Mailbox::Model* model;
    Imap::Mailbox::TreeItemMessage* msg;
    Imap::Mailbox::TreeItemPart* part;
};

}
}

#endif // MSGPARTNETWORKREPLY_H
