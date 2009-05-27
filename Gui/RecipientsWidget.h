#ifndef RECIPIENTSWIDGET_H
#define RECIPIENTSWIDGET_H

#include <QStyledItemDelegate>
#include <QTableWidget>

namespace Gui {

class RecipientsWidget : public QTableWidget
{
    Q_OBJECT
public:
    RecipientsWidget( QWidget* parent );
    RecipientsWidget( QWidget* parent, const QList<QPair<QString, QString> >& recipients );
    QList<QPair<QString, QString> > recipients() const;
    virtual QSize sizeHint() const;
private:
    void addRecipient( const int position, const QPair<QString, QString>& recipient );
private slots:
    void handleItemChanged( QTableWidgetItem* item );
private:
    void commonInit();
};

class AddressTypeDelegate: public QStyledItemDelegate {
    Q_OBJECT
public:
    AddressTypeDelegate( QObject* parent );
    QWidget* createEditor( QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index ) const;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
    void setEditorData(QWidget* editor, const QModelIndex& index) const;
private slots:
    void emitCommitData();
};

}

#endif // RECIPIENTSWIDGET_H
