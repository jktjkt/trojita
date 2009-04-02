#ifndef MSGPARTNETWORKREPLY_H
#define MSGPARTNETWORKREPLY_H

#include <QBuffer>
#include <QModelIndex>
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
    MsgPartNetworkReply( QObject* parent, Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, Imap::Mailbox::TreeItemPart* _part );
    virtual void abort();
    virtual void close();
    virtual qint64 bytesAvailable() const;
public slots:
    void slotModelDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight );
    void slotMyDataChanged();
protected:
    virtual qint64 readData( char* data, qint64 maxSize );
private:
    Imap::Mailbox::Model* model;
    Imap::Mailbox::TreeItemMessage* msg;
    Imap::Mailbox::TreeItemPart* part;
    QBuffer buffer;
};

}
}

#endif // MSGPARTNETWORKREPLY_H
