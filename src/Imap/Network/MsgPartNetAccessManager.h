#ifndef MSGPARTNETACCESSMANAGER_H
#define MSGPARTNETACCESSMANAGER_H

#include <QNetworkAccessManager>

class QUrl;

namespace Gui {
class PartWidgetFactory;
}

namespace Imap {

namespace Mailbox {
class Model;
class TreeItem;
class TreeItemMessage;
class TreeItemPart;
}

namespace Network {

class MsgPartNetAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    MsgPartNetAccessManager( QObject* parent=0 );
    void setModelMessage( Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _message );
    Imap::Mailbox::TreeItemPart* pathToPart( const QString& path );
    Imap::Mailbox::TreeItemPart* cidToPart( const QByteArray& cid, Imap::Mailbox::TreeItem* root );
protected:
    virtual QNetworkReply* createRequest( Operation op,
        const QNetworkRequest& req, QIODevice* outgoingData=0 );
signals:
    void requestingExternal( const QUrl& url );
public slots:
    void setExternalsEnabled( bool enabled );
private:
    friend class Gui::PartWidgetFactory;
    Imap::Mailbox::Model* model;
    Imap::Mailbox::TreeItemMessage* message;

    bool _externalsEnabled;

    MsgPartNetAccessManager(const MsgPartNetAccessManager&); // don't implement
    MsgPartNetAccessManager& operator=(const MsgPartNetAccessManager&); // don't implement
};

}
}
#endif // MSGPARTNETACCESSMANAGER_H
