#ifndef MAILBOXTREEVIEW_H
#define MAILBOXTREEVIEW_H

#include <QTreeView>

namespace Gui{

/** @short subclassed to ask user for desired action upon dropEvent
*/
class MailBoxTreeView : public QTreeView
{
public:
    MailBoxTreeView();
protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dragMoveEvent(QDragMoveEvent *event);
    void dropEvent(QDropEvent *event);
private:
    bool dropMightBeAccepted;
};
}

#endif // MAILBOXTREEVIEW_H
