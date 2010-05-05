#ifndef IMAP_NET_FORBIDDENREPLY_H
#define IMAP_NET_FORBIDDENREPLY_H

#include <QNetworkReply>

namespace Imap {
namespace Network {

class ForbiddenReply : public QNetworkReply
{
Q_OBJECT
public:
    ForbiddenReply( QObject* parent );
protected:
    virtual qint64 readData( char* data, qint64 maxSize )
    {
        Q_UNUSED( data); Q_UNUSED( maxSize);
        return -1;
    };
    virtual void abort() {};
private slots:
    void slotFinish();
};

}
}

#endif // IMAP_NET_FORBIDDENREPLY_H
