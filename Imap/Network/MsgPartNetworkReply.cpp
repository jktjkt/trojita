#include <QDebug>
#include <QStringList>
#include <QTimer>

#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Imap {

namespace Network {

MsgPartNetworkReply::MsgPartNetworkReply( QObject* parent,
        Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg,
        Imap::Mailbox::TreeItemPart* _part ):
    QNetworkReply(parent), model(_model), msg(_msg), part(_part)
{
    setOpenMode( QIODevice::ReadOnly | QIODevice::Unbuffered );
    Q_ASSERT( model );
    Q_ASSERT( msg );
    Q_ASSERT( part );

    connect( _model, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
             this, SLOT( slotModelDataChanged( const QModelIndex&, const QModelIndex& ) ) );

    // We have to ask for contents before we check whether it's already fetched
    part->fetch( model );
    if ( part->fetched() ) {
        QTimer::singleShot( 0, this, SLOT( slotMyDataChanged() ) );
    }

    buffer.setBuffer( part->dataPtr() );
    buffer.open( QIODevice::ReadOnly );
}

void MsgPartNetworkReply::slotModelDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    // FIXME: use bottomRight as well!
    if ( topLeft.model() != model ) {
        return;
    }
    Imap::Mailbox::TreeItemPart* receivedPart = dynamic_cast<Imap::Mailbox::TreeItemPart*> (
            Imap::Mailbox::Model::realTreeItem( topLeft ) );
    if ( receivedPart == part ) {
        slotMyDataChanged();
    }
}

void MsgPartNetworkReply::slotMyDataChanged()
{
    if ( part->mimeType().startsWith( QLatin1String( "text/" ) ) ) {
        setHeader( QNetworkRequest::ContentTypeHeader,
                   part->charset().isEmpty() ?
                    part->mimeType() :
                    QString::fromAscii("%1; charset=%2").arg( part->mimeType(), part->charset() ) );
    } else {
        setHeader( QNetworkRequest::ContentTypeHeader, part->mimeType() );
    }
    emit readyRead();
    emit finished();
}

void MsgPartNetworkReply::abort()
{
    close();
}

void MsgPartNetworkReply::close()
{
    buffer.close();
}

qint64 MsgPartNetworkReply::bytesAvailable() const
{
    return buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
}

qint64 MsgPartNetworkReply::readData( char* data, qint64 maxSize )
{
    return buffer.read( data, maxSize );
}

}
}
#include "MsgPartNetworkReply.moc"
