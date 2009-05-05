#include "RecipientsWidget.h"

#include <QComboBox>
#include <QHeaderView>

namespace Gui {

RecipientsWidget::RecipientsWidget( QWidget* parent ): QTableWidget( parent )
{
    commonInit();
    addRecipient( 0, qMakePair( tr("To"), QString() ) );
}

RecipientsWidget::RecipientsWidget( QWidget* parent, const QList<QPair<QString, QString> >& recipients ):
        QTableWidget( parent )
{
    commonInit();
    if ( recipients.isEmpty() ) {
        addRecipient( 0, qMakePair( tr("To"), QString() ) );
    } else {
        for ( int i = 0; i < recipients.size(); ++i ) {
            addRecipient( i, recipients[i] );
        }
    }
}

void RecipientsWidget::commonInit()
{
    setSelectionMode( NoSelection );
    setColumnCount( 2 );
    horizontalHeader()->hide();
    horizontalHeader()->setStretchLastSection( true );
    verticalHeader()->hide();
    setItemDelegate( new AddressTypeDelegate( this ) );
    connect( this, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(handleItemChanged(QTableWidgetItem*)) );
}

void RecipientsWidget::addRecipient( const int position, const QPair<QString, QString>& recipient )
{
    insertRow( position );
    QTableWidgetItem* item = new QTableWidgetItem( recipient.first );
    setItem( position, 0, item );
    openPersistentEditor( item );
    item = new QTableWidgetItem( recipient.second );
    setItem( position, 1, item );
}

void RecipientsWidget::handleItemChanged( QTableWidgetItem* changedItem )
{
    if ( changedItem->column() != 1 )
        return;

    if ( changedItem->row() == rowCount() - 1 ) {
        if ( ! changedItem->text().isEmpty() ) {
            addRecipient( changedItem->row() + 1, qMakePair( item( changedItem->row(), 0 )->text(), QString() ) );
        }
    } else if ( changedItem->text().isEmpty() && rowCount() != 1 ) {
        removeRow( changedItem->row() );
    }
}

QList<QPair<QString, QString> > RecipientsWidget::recipients() const
{
    QList<QPair<QString, QString> > res;
    for ( int i = 0; i < rowCount(); ++i ) {
        QString kind = QLatin1String("To");
        if ( item( i, 0 )->text() == tr("Cc") )
            kind = QLatin1String("Cc");
        else if ( item( i, 0 )->text() == tr("Bcc") )
            kind = QLatin1String("Bcc");
        res << qMakePair( kind, item( i, 1 )->text() );
    }
    return res;
}


AddressTypeDelegate::AddressTypeDelegate( QObject* parent ): QStyledItemDelegate( parent )
{
}

QWidget* AddressTypeDelegate::createEditor( QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    if ( index.column() == 0 ) {
        QComboBox* combo = new QComboBox( parent );
        combo->addItem( tr("To") );
        combo->addItem( tr("Cc") );
        combo->addItem( tr("Bcc") );
        connect( combo, SIGNAL(activated(int)), this, SLOT(emitCommitData()) );
        return combo;
    } else {
        return QStyledItemDelegate::createEditor( parent, option, index );
    }
}

void AddressTypeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    QComboBox* combo = qobject_cast<QComboBox*>( editor );
    if ( combo )
        model->setData( index, combo->currentText() );
    else
        QStyledItemDelegate::setModelData( editor, model, index );
}

void AddressTypeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    QComboBox* combo = qobject_cast<QComboBox*>( editor );
    if ( combo )
        combo->setCurrentIndex( combo->findText( index.model()->data( index ).toString() ) );
    else
        QStyledItemDelegate::setEditorData( editor, index );
}

void AddressTypeDelegate::emitCommitData()
{
    emit commitData( qobject_cast<QWidget*>( sender() ) );
}

}

#include "RecipientsWidget.moc"
