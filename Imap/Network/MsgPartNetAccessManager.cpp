#include <QNetworkRequest>
#include <QDebug>

#include "MsgPartNetAccessManager.h"
#include "ForbiddenReply.h"
#include "MsgPartNetworkReply.h"

namespace Imap {
namespace Network {

MsgPartNetAccessManager::MsgPartNetAccessManager( QObject* parent ):
    QNetworkAccessManager( parent )
{
}

void MsgPartNetAccessManager::setModelMessage( const Imap::Mailbox::Model* _model,
    Imap::Mailbox::TreeItemMessage* _message )
{
    model = _model;
    message = _message;
}

QNetworkReply* MsgPartNetAccessManager::createRequest( Operation op,
    const QNetworkRequest& req, QIODevice* outgoingData )
{
    if ( req.url().scheme() == QLatin1String( "trojita-imap" ) &&
         req.url().host() == QLatin1String("msg") ) {
        return new Imap::Network::MsgPartNetworkReply( this, model, message,
                                                       req.url().path() );
    } else {
        qDebug() << "Forbidden per policy:" << req.url();
        return new Imap::Network::ForbiddenReply( this );
    }
}

}
}

#include "MsgPartNetAccessManager.moc"
