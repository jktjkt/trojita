#include "SimplePartWidget.h"
#include "Imap/Model/MailboxTree.h"

namespace Gui {

SimplePartWidget::SimplePartWidget( QWidget* parent,
                                    Imap::Network::MsgPartNetAccessManager* manager,
                                    Imap::Mailbox::TreeItemPart* part ):
        EmbeddedWebView( parent, manager )
{
    QUrl url;
    url.setScheme( QLatin1String("trojita-imap") );
    url.setHost( QLatin1String("msg") );
    url.setPath( part->pathToPart() );
    load( url );
}

}

#include "SimplePartWidget.moc"
