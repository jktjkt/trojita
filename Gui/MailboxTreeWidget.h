#ifndef MAILBOXLISTWIDGET_H
#define MAILBOXLISTWIDGET_H

#include <QTreeView>

namespace Gui {

class MailboxTreeWidget : public QTreeView
{
    Q_OBJECT
public:
    MailboxTreeWidget( QWidget* parent=0 );
protected:
    virtual int sizeHintForColumn( int column ) const;
private slots:
    void slotFixSize();
};

}

#endif // MAILBOXLISTWIDGET_H
