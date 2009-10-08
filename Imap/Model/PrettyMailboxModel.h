#ifndef PRETTYMAILBOXMODEL_H
#define PRETTYMAILBOXMODEL_H

#include <QSortFilterProxyModel>
#include "Imap/Model/MailboxModel.h"

namespace Imap {

namespace Mailbox {

/** @short A pretty proxy model which increases sexiness of the MailboxModel */
class PrettyMailboxModel: public QSortFilterProxyModel
{
    Q_OBJECT
public:
    PrettyMailboxModel( QObject* parent, MailboxModel* mailboxModel );
    virtual QVariant data( const QModelIndex& index, int role ) const;
    virtual bool filterAcceptsColumn( int source_column, const QModelIndex& source_parent ) const;
};

}

}

#endif // PRETTYMAILBOXMODEL_H
