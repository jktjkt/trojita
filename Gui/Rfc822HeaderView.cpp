#include "Rfc822HeaderView.h"

#include <QModelIndex>

#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Gui {

Rfc822HeaderView::Rfc822HeaderView( QWidget* parent,
                                    Imap::Mailbox::Model* _model,
                                    Imap::Mailbox::TreeItemPart* _part):
    QLabel(parent), model(_model), part(_part)
{
    part->fetch( model );
    if ( part->fetched() ) {
        setCorrectText();
    } else {
        setText( tr("Loading...") );
        connect( model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)) );
    }
}

void Rfc822HeaderView::handleDataChanged( const QModelIndex& topLeft, const QModelIndex& bottomRight )
{
    Q_ASSERT( topLeft.model() == model );
    Imap::Mailbox::TreeItemPart* source = dynamic_cast<Imap::Mailbox::TreeItemPart*>(
            static_cast<Imap::Mailbox::TreeItem*>( topLeft.internalPointer() ) );
    if ( source == part )
        setCorrectText();
}

void Rfc822HeaderView::setCorrectText()
{
    setText( *part->dataPtr() );
}

}

#include "Rfc822HeaderView.moc"
