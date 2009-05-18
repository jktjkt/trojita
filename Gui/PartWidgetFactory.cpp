#include "PartWidgetFactory.h"
#include "SimplePartWidget.h"
#include "Imap/Model/MailboxTree.h"

#include <QLabel>
#include <QTabWidget>

namespace Gui {

PartWidgetFactory::PartWidgetFactory( Imap::Network::MsgPartNetAccessManager* _manager ):
        manager(_manager)
{
}

QWidget* PartWidgetFactory::create( Imap::Mailbox::TreeItemPart* part )
{
    using namespace Imap::Mailbox;
    Q_ASSERT( part );
    if ( part->mimeType().startsWith( QLatin1String("multipart/") ) ) {
        // it's a compound part
        if ( part->mimeType() == QLatin1String("multipart/alternative") ) {
            QTabWidget* top = new QTabWidget();
            for ( uint i = 0; i < part->childrenCount( manager->model ); ++i ) {
                TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                        part->child( i, manager->model ) );
                Q_ASSERT( anotherPart );
                QWidget* item = create( anotherPart );
                top->addTab( item, anotherPart->mimeType() );
            }
            top->setCurrentIndex( part->childrenCount( manager->model ) - 1 );
            return top;
        }
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
