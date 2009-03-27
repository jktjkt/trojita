#include <QDebug>
#include <QStringList>
#include <QTimer>

#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Imap {

namespace Network {

MsgPartNetworkReply::MsgPartNetworkReply( QObject* parent,
        const Imap::Mailbox::Model* _model,
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

    if ( part->fetched() ) {
        QTimer::singleShot( 0, this, SLOT( slotMyDataChanged() ) );
    }

    buffer.setBuffer( part->dataPtr() );
    buffer.open( QIODevice::ReadOnly );
    part->fetch( model );
}

void MsgPartNetworkReply::slotModelDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    // FIXME: use bottomRight as well!
    if ( topLeft.model() != model ) {
        return;
    }
    Imap::Mailbox::TreeItemPart* receivedPart = dynamic_cast<Imap::Mailbox::TreeItemPart*> (
            static_cast<Imap::Mailbox::TreeItem*>( topLeft.internalPointer() ) );
    if ( receivedPart == part ) {
        slotMyDataChanged();
    }
}

void MsgPartNetworkReply::slotMyDataChanged()
{
    // FIXME :)
    setHeader( QNetworkRequest::ContentTypeHeader, part->mimeType() );
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
    qint64 res = buffer.read( data, maxSize );
    if ( res <= 0 )
        emit finished();
    else
        emit readyRead();
    return res;
}

}
}
#include "MsgPartNetworkReply.moc"
