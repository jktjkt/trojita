#ifndef DELETIONWATCHER_H
#define DELETIONWATCHER_H

#include <QObject>

class DeletionWatcher : public QObject
{
Q_OBJECT
public:
    DeletionWatcher(QObject *parent, QObject* what);
public slots:
    void handleDeleted();

};

#endif // DELETIONWATCHER_H
