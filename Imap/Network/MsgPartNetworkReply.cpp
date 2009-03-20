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
        const QString& _part ):
    QNetworkReply(parent), model(_model), msg(_msg), part(0)
{
    setOpenMode( QIODevice::ReadOnly | QIODevice::Unbuffered );
    Q_ASSERT( model );
    Q_ASSERT( msg );

    QStringList items = _part.split( '/', QString::SkipEmptyParts );
    Imap::Mailbox::TreeItem* target = msg;
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

    if ( ok ) {
        Q_ASSERT( part );

        connect( _model, SIGNAL( dataChanged( const QModelIndex&, const QModelIndex& ) ),
                 this, SLOT( slotModelDataChanged( const QModelIndex&, const QModelIndex& ) ) );

        if ( part->fetched() ) {
            QTimer::singleShot( 0, this, SLOT( slotMyDataChanged() ) );
        }

        buffer.setBuffer( part->dataPtr() );
        buffer.open( QIODevice::ReadOnly );
        //open( QIODevice::ReadOnly | QIODevice::Unbuffered );
        qDebug() << "iodevice opened";
        part->fetch( model );
    } else {
        qDebug() << "not ok";
    }
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
    setHeader( QNetworkRequest::ContentTypeHeader, QVariant( "text/plain" ) );
    qDebug() << this << "hey, our data has changed!" << part->partId();
    emit downloadProgress( 0, buffer.bytesAvailable() );
    emit downloadProgress( buffer.bytesAvailable(), buffer.bytesAvailable() );
    emit readyRead();
    emit finished();
}

void MsgPartNetworkReply::abort()
{
    // can't really do anything
    qDebug() << Q_FUNC_INFO;
}

void MsgPartNetworkReply::close()
{
    qDebug() << Q_FUNC_INFO;
    buffer.close();
}

qint64 MsgPartNetworkReply::bytesAvailable() const
{
    qint64 res = buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
    qDebug() << Q_FUNC_INFO << res << buffer.bytesAvailable() << QNetworkReply::bytesAvailable();
    return res;
}

qint64 MsgPartNetworkReply::readData( char* data, qint64 maxSize )
{
    qDebug() << Q_FUNC_INFO << maxSize << buffer.bytesAvailable();
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
