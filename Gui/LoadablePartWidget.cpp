#include "LoadablePartWidget.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Utils.h"

#include <QPushButton>

namespace Gui {

LoadablePartWidget::LoadablePartWidget( QWidget* parent,
                         Imap::Network::MsgPartNetAccessManager* _manager,
                         Imap::Mailbox::TreeItemPart* _part,
                         QObject* _wheelEventFilter ):
QStackedWidget(parent), manager(_manager), part(_part), wheelEventFilter(_wheelEventFilter)
{
    loadButton = new QPushButton( tr("Load %1 (%2)").arg(
            part->mimeType(), Imap::Mailbox::PrettySize::prettySize( part->octets() ) ), this );
    connect( loadButton, SIGNAL(clicked()), this, SLOT(loadClicked()) );
    addWidget( loadButton );
}

void LoadablePartWidget::loadClicked()
{
    realPart = new SimplePartWidget( this, manager, part );
    realPart->installEventFilter( wheelEventFilter );
    addWidget( realPart );
    setCurrentIndex( 1 );
}

}

#include "LoadablePartWidget.moc"
