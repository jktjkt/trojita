#ifndef MSGLISTVIEW_H
#define MSGLISTVIEW_H

#include <QTreeView>

namespace Gui {


/** @short A slightly tweaked QTreeView optimized for showing a list of messages in one mailbox */
class MsgListView : public QTreeView
{
    Q_OBJECT
public:
    MsgListView( QWidget* parent=0 );
protected:
    virtual int sizeHintForColumn( int column ) const;
private slots:
    void slotFixSize();
};

}

#endif // MSGLISTVIEW_H
