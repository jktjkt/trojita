#ifndef GUI_PARTWIDGETFACTORY_H
#define GUI_PARTWIDGETFACTORY_H

#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Gui {

class PartWidgetFactory
{
public:
    PartWidgetFactory( Imap::Network::MsgPartNetAccessManager* _manager );
    QWidget* create( Imap::Mailbox::TreeItemPart* part );
private:
    Imap::Network::MsgPartNetAccessManager* manager;
};

}

#endif // GUI_PARTWIDGETFACTORY_H
