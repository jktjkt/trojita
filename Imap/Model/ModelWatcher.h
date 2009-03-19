#ifndef MODELWATCHER_H
#define MODELWATCHER_H

#include <QModelIndex>
#include <QAbstractItemModel>

namespace Imap {

namespace Mailbox {

class ModelWatcher : public QObject
{
Q_OBJECT
public:
    ModelWatcher( QObject* parent=0 ): QObject(parent) {};
    void setModel( QAbstractItemModel* model );

private slots:
    void columnsAboutToBeInserted( const QModelIndex&, int, int );
    void columnsAboutToBeRemoved( const QModelIndex&, int, int );
    void columnsInserted( const QModelIndex&, int, int );
    void columnsRemoved( const QModelIndex&, int, int );
    void dataChanged( const QModelIndex&, const QModelIndex& );
    void headerDataChanged( Qt::Orientation, int, int );
    void layoutAboutToBeChanged();
    void layoutChanged();
    void modelAboutToBeReset();
    void modelReset();
    void rowsAboutToBeInserted( const QModelIndex&, int, int );
    void rowsAboutToBeRemoved( const QModelIndex&, int, int );
    void rowsInserted( const QModelIndex&, int, int );
    void rowsRemoved( const QModelIndex&, int, int );

private:
    QAbstractItemModel* _model;
};

}

}

#endif // MODELWATCHER_H
