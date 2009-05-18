#include "PartWidgetFactory.h"
#include "SimplePartWidget.h"
#include "Rfc822HeaderView.h"
#include "Imap/Model/MailboxTree.h"

#include <QGroupBox>
#include <QLabel>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Gui {

PartWidgetFactory::PartWidgetFactory( Imap::Network::MsgPartNetAccessManager* _manager, QObject* _wheelEventFilter ):
        manager(_manager), wheelEventFilter(_wheelEventFilter)
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
        } else if ( part->mimeType() == QLatin1String("multipart/signed") ) {
            uint childrenCount = part->childrenCount( manager->model );
            if ( childrenCount != 2 ) {
                QLabel* lbl = new QLabel( tr("Mallformed multipart/signed message"), 0 );
                return lbl;
            }
            QGroupBox* top = new QGroupBox( tr("Signed Message"), 0 );
            QVBoxLayout* layout = new QVBoxLayout( top );
            TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                    part->child( 0, manager->model ) );
            layout->addWidget( create( anotherPart ) );
            return top;
        } else {
            // multipart/mixed or anything else, as mandated by RFC 2046, Section 5.1.3
            QGroupBox* top = new QGroupBox( tr("Multipart Message"), 0 );
            QVBoxLayout* layout = new QVBoxLayout( top );
            for ( uint i = 0; i < part->childrenCount( manager->model ); ++i ) {
                TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                        part->child( i, manager->model ) );
                Q_ASSERT( anotherPart );
                QWidget* res = create( anotherPart );
                layout->addWidget( res );
            }
            return top;
        }
    } else if ( part->mimeType() == QLatin1String("message/rfc822") ) {
        QGroupBox* top = new QGroupBox( tr("Message"), 0 );
        QVBoxLayout* layout = new QVBoxLayout( top );
        QLabel* header = new Rfc822HeaderView( 0, manager->model, part );
        layout->addWidget( header );
        for ( uint i = 0; i < part->childrenCount( manager->model ); ++i ) {
            TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                    part->child( i, manager->model ) );
            Q_ASSERT( anotherPart );
            QWidget* res = create( anotherPart );
            layout->addWidget( res );
        }
        return top;
    } else {
        // can't do much besides forwarding
        SimplePartWidget* widget = new SimplePartWidget( 0, manager, part );
        widget->installEventFilter( wheelEventFilter );
        return widget;
    }
    QLabel* lbl = new QLabel( part->mimeType(), 0 );
    lbl->setMinimumSize( 800, 600 );
    return lbl;
}

}
