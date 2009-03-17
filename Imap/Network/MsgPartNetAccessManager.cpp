#include <QNetworkRequest>
#include <QDebug>

#include "MsgPartNetAccessManager.h"
#include "Imap/Network/ForbiddenReply.h"

namespace Imap {
namespace Network {

MsgPartNetAccessManager::MsgPartNetAccessManager( QObject* parent ):
    QNetworkAccessManager( parent )
{
}

void MsgPartNetAccessManager::setModelMessage( const Imap::Mailbox::Model* _model,
    const Imap::Mailbox::TreeItemMessage* _message )
{
    model = _model;
    message = _message;
}

QNetworkReply* MsgPartNetAccessManager::createRequest( Operation op,
    const QNetworkRequest& req, QIODevice* outgoingData )
{
    if ( req.url().scheme() == QLatin1String( "trojita-imap" ) ) {
        qDebug() << req.url();
        // FIXME: return IMAP stuff...
        return new Imap::Network::ForbiddenReply( this );
    } else {
        qDebug() << "Forbidden per policy:" << req.url();
        return new Imap::Network::ForbiddenReply( this );
        //return QNetworkAccessManager::createRequest( op, req, outgoingData );
    }
}

}
}

#include "MsgPartNetAccessManager.moc"
