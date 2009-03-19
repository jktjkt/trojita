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
    qDebug() << this << "hey, our data has changed!";
    emit readyRead();
}

void MsgPartNetworkReply::abort()
{
    // can't really do anything
}

qint64 MsgPartNetworkReply::readData( char* data, qint64 maxSize )
{
    // FIXME
    return -1;
}

}
}
#include "MsgPartNetworkReply.moc"
