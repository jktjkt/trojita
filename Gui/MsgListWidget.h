#ifndef MSGLISTWIDGET_H
#define MSGLISTWIDGET_H

#include <QTreeView>

namespace Gui {


/** @short A slightly tweaked QTreeView optimized for showing a list of messages in one mailbox */
class MsgListWidget : public QTreeView
{
    Q_OBJECT
public:
    MsgListWidget( QWidget* parent=0 );
protected:
    virtual int sizeHintForColumn( int column ) const;
private slots:
    void slotFixSize();
};

}

#endif // MSGLISTWIDGET_H
