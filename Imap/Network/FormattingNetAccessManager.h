#ifndef FORMATTINGNETACCESSMANAGER_H
#define FORMATTINGNETACCESSMANAGER_H

#include <QNetworkAccessManager>

namespace Imap {

namespace Mailbox {
class Model;
class TreeItemMessage;
}

namespace Network {

class MsgPartNetAccessManager;

class FormattingNetAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    FormattingNetAccessManager( QObject* parent=0 );
    void setModelMessage( Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _message );
protected:
    virtual QNetworkReply* createRequest( Operation op,
        const QNetworkRequest& req, QIODevice* outgoingData=0 );
private:
    Imap::Mailbox::TreeItemMessage* message;
    MsgPartNetAccessManager* partManager;
};

}
}
#endif // FORMATTINGNETACCESSMANAGER_H
