#ifndef IMAP_NET_AUXILIARYREPLY_H
#define IMAP_NET_AUXILIARYREPLY_H

#include <QBuffer>
#include <QNetworkReply>

namespace Imap {
namespace Network {

class AuxiliaryReply : public QNetworkReply
{
Q_OBJECT
public:
    AuxiliaryReply( QObject* parent, const QString& textualMessage );
    virtual void close();
    virtual qint64 bytesAvailable() const;
protected:
    virtual qint64 readData( char* data, qint64 maxSize );
    virtual void abort();
private slots:
    void slotFinish();
private:
    QBuffer buffer;
    QByteArray message;
};

}
}

#endif // IMAP_NET_AUXILIARYREPLY_H
