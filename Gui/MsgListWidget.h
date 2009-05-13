#ifndef MSGLISTWIDGET_H
#define MSGLISTWIDGET_H

#include <QTreeView>

namespace Gui {

class MsgListWidget : public QTreeView
{
    Q_OBJECT
public:
    MsgListWidget( QWidget* parent=0 );
protected:
    virtual int sizeHintForColumn( int column ) const;
};

}

#endif // MSGLISTWIDGET_H
