#include <QNetworkRequest>
#include <QStringList>
#include <QDebug>

#include "MsgPartNetAccessManager.h"
#include "ForbiddenReply.h"
#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"

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
    Imap::Mailbox::TreeItemPart* part = pathToPart( req.url().path() );
    if ( req.url().scheme() == QLatin1String( "trojita-imap" ) &&
         req.url().host() == QLatin1String("msg") && part ) {
        return new Imap::Network::MsgPartNetworkReply( this, model, message,
                                                       part );
    } else {
        qDebug() << "Forbidden per policy:" << req.url();
        return new Imap::Network::ForbiddenReply( this );
    }
}

Imap::Mailbox::TreeItemPart* MsgPartNetAccessManager::pathToPart( const QString& path )
{
    Imap::Mailbox::TreeItemPart* part = 0;
    QStringList items = path.split( '/', QString::SkipEmptyParts );
    Imap::Mailbox::TreeItem* target = message;
    bool ok = ! items.isEmpty(); // if it's empty, it's a bogous URL

    for( QStringList::const_iterator it = items.begin(); it != items.end(); ++it ) {
        uint offset = it->toUInt( &ok );
        if ( !ok )
            break;
        if ( offset >= target->childrenCount( model ) ) {
            ok = false;
            break;
        }
        target = target->child( offset, model );
    }
    part = dynamic_cast<Imap::Mailbox::TreeItemPart*>( target );
    if ( ok )
        Q_ASSERT( part );
    return part;
}

}
}

#include "MsgPartNetAccessManager.moc"
