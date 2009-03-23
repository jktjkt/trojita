#ifndef FORMATTINGREPLY_H
#define FORMATTINGREPLY_H

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

class MsgPartNetworkReply;

class FormattingReply : public QNetworkReply
{
Q_OBJECT
public:
    FormattingReply( QObject* parent, const Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, Imap::Mailbox::TreeItemPart* _part );
    virtual void abort();
    virtual void close();
    virtual qint64 bytesAvailable() const;
protected:
    virtual qint64 readData( char* data, qint64 maxSize );
    /** @short Launch a request for some message part */
    void requestAnotherPart( Imap::Mailbox::TreeItemPart* anotherPart );
protected slots:
    /** @short This slot is invoked whenever any sub-request has finished loading */
    void anotherReplyFinished();
    /** @short All of the raw data for the original message part is available now */
    void mainReplyFinished();
    void handleAlreadyFinished();
    void everythingFinished();
private:
    void anotherReplyFinished( MsgPartNetworkReply* finishedReply );

    const Imap::Mailbox::Model* model;
    Imap::Mailbox::TreeItemMessage* msg;
    Imap::Mailbox::TreeItemPart* part;
    QBuffer buffer;
    QList<MsgPartNetworkReply*> replies;
    QList<bool> pendingBitmap;
    uint pendingCount;
    QList<MsgPartNetworkReply*> reqAlreadyFetched;
};

}
}

#endif // FORMATTINGREPLY_H
