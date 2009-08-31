#ifndef RECIPIENTSWIDGET_H
#define RECIPIENTSWIDGET_H

#include <QStyledItemDelegate>
#include <QTableWidget>

namespace Gui {

/** @short Grid-like widget for input of mail recipients

    Design of this widget is inspired by the addresses input widget from Mozilla
    Thunderbird.  It provides an area for specifying a list of recipients and for
    each of them, a kind of delivery (To, Cc and Bcc).
*/
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
