#include "DeletionWatcher.h"
#include <QDebug>

DeletionWatcher::DeletionWatcher(QObject *parent, QObject *what) :
    QObject(parent)
{
    connect(what, SIGNAL(destroyed()), this, SLOT(handleDeleted()));
}

void DeletionWatcher::handleDeleted()
{
    qDebug() << "DELETING" << sender() << sender()->objectName();
    deleteLater();
}
