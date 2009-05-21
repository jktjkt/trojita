#ifndef LOADABLEPARTWIDGET_H
#define LOADABLEPARTWIDGET_H

#include <QStackedWidget>

#include "SimplePartWidget.h"

class QPushButton;

namespace Gui {

class LoadablePartWidget : public QStackedWidget
{
    Q_OBJECT
public:
    LoadablePartWidget( QWidget* parent,
                        Imap::Network::MsgPartNetAccessManager* _manager,
                        Imap::Mailbox::TreeItemPart* _part,
                        QObject* _wheelEventFilter );
private slots:
    void loadClicked();
private:
    Imap::Network::MsgPartNetAccessManager* manager;
    Imap::Mailbox::TreeItemPart* part;
    SimplePartWidget* realPart;
    QObject* wheelEventFilter;
    QPushButton* loadButton;
};

}

#endif // LOADABLEPARTWIDGET_H
