#include "PartWidgetFactory.h"
#include "SimplePartWidget.h"
#include "Imap/Model/MailboxTree.h"

#include <QLabel>

namespace Gui {

PartWidgetFactory::PartWidgetFactory( Imap::Network::MsgPartNetAccessManager* _manager ):
        manager(_manager)
{
}

QWidget* PartWidgetFactory::create( Imap::Mailbox::TreeItemPart* part )
{
    Q_ASSERT( part );
    if ( part->mimeType().startsWith( QLatin1String("multipart/") ) ) {
        // it's a compound part
    } else if ( part->mimeType() == QLatin1String("message/rfc822") ) {
        // dtto
    } else {
        // can't do much besides forwarding
        SimplePartWidget* widget = new SimplePartWidget( 0, manager, part );
        return widget;
    }
    QLabel* lbl = new QLabel( part->mimeType(), 0 );
    lbl->setMinimumSize( 800, 600 );
    return lbl;
}

}
