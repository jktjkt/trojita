#include "PrettyMailboxModel.h"
#include "MailboxModel.h"

namespace Imap {

namespace Mailbox {

PrettyMailboxModel::PrettyMailboxModel( QObject* parent, MailboxModel* mailboxModel ):
        QSortFilterProxyModel( parent )
{
    setDynamicSortFilter( true );
    setSourceModel( mailboxModel );
}

QVariant PrettyMailboxModel::data( const QModelIndex& index, int role ) const
{
    if ( ! index.isValid() )
        return QVariant();

    if ( index.column() != 0 )
        return QVariant();

    if ( index.row() < 0 || index.row() >= rowCount( index.parent() ) )
        return QVariant();

    switch ( role ) {
        case Qt::DisplayRole:
            {
            QModelIndex translated = mapToSource( index );
            QModelIndex unreadIndex = translated.sibling( translated.row(), MailboxModel::UNREAD_MESSAGE_COUNT );
            qlonglong unreadCount = sourceModel()->data( unreadIndex, Qt::DisplayRole ).toLongLong();
            if ( unreadCount )
                return tr("%1 (%2)").arg(
                        QSortFilterProxyModel::data( index, Qt::DisplayRole ).toString() ).arg(
                        unreadCount );
            else
                return QSortFilterProxyModel::data( index, Qt::DisplayRole );
            }
        default:
            return QSortFilterProxyModel::data( index, role );
    }
}

bool PrettyMailboxModel::filterAcceptsColumn( int source_column, const QModelIndex& source_parent ) const
{
    return source_column == 0;
}

}

}

#include "PrettyMailboxModel.moc"
