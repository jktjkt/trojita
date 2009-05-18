#ifndef GUI_PARTWIDGETFACTORY_H
#define GUI_PARTWIDGETFACTORY_H

#include "Imap/Network/MsgPartNetAccessManager.h"

#include <QCoreApplication>

namespace Gui {

class PartWidgetFactory
{
    Q_DECLARE_TR_FUNCTIONS(PartWidgetFactory)
public:
    PartWidgetFactory( Imap::Network::MsgPartNetAccessManager* _manager, QObject* _wheelEventFilter );
    QWidget* create( Imap::Mailbox::TreeItemPart* part );
private:
    Imap::Network::MsgPartNetAccessManager* manager;
    QObject* wheelEventFilter;
};

}

#endif // GUI_PARTWIDGETFACTORY_H
