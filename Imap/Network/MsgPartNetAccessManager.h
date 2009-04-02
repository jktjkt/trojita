#ifndef MSGPARTNETACCESSMANAGER_H
#define MSGPARTNETACCESSMANAGER_H

#include <QNetworkAccessManager>

namespace Imap {

namespace Mailbox {
class Model;
class TreeItemMessage;
class TreeItemPart;
}

namespace Network {

class FormattingNetAccessManager;

class MsgPartNetAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    MsgPartNetAccessManager( QObject* parent=0 );
    void setModelMessage( Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _message );
    Imap::Mailbox::TreeItemPart* pathToPart( const QString& path );
protected:
    virtual QNetworkReply* createRequest( Operation op,
        const QNetworkRequest& req, QIODevice* outgoingData=0 );
private:
    friend class FormattingNetAccessManager;
    Imap::Mailbox::Model* model;
    Imap::Mailbox::TreeItemMessage* message;
};

}
}
#endif // MSGPARTNETACCESSMANAGER_H
