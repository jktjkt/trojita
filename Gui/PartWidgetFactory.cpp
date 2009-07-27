#include "PartWidgetFactory.h"
#include "AttachmentView.h"
#include "LoadablePartWidget.h"
#include "SimplePartWidget.h"
#include "Rfc822HeaderView.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
    return create( part, 0 );
}

QWidget* PartWidgetFactory::create( Imap::Mailbox::TreeItemPart* part, int recursionDepth )
{
    using namespace Imap::Mailbox;
    Q_ASSERT( part );

    if ( recursionDepth > 1000 ) {
        return new QLabel( tr("This message contains too deep nesting of MIME message parts.\n"
                              "To prevent stack exhaustion and your head from exploding, only\n"
                              "the top-most thousand items or so are shown."), 0 );
    }

    if ( part->mimeType().startsWith( QLatin1String("multipart/") ) ) {
        // it's a compound part
        if ( part->mimeType() == QLatin1String("multipart/alternative") ) {
            QTabWidget* top = new QTabWidget();
            for ( uint i = 0; i < part->childrenCount( manager->model ); ++i ) {
                TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                        part->child( i, manager->model ) );
                Q_ASSERT( anotherPart );
                QWidget* item = create( anotherPart, recursionDepth + 1 );
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
            layout->addWidget( create( anotherPart, recursionDepth + 1 ) );
            return top;
        } else {
            // multipart/mixed or anything else, as mandated by RFC 2046, Section 5.1.3
            QGroupBox* top = new QGroupBox( tr("Multipart Message"), 0 );
            QVBoxLayout* layout = new QVBoxLayout( top );
            for ( uint i = 0; i < part->childrenCount( manager->model ); ++i ) {
                TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                        part->child( i, manager->model ) );
                Q_ASSERT( anotherPart );
                QWidget* res = create( anotherPart, recursionDepth + 1 );
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
            QWidget* res = create( anotherPart, recursionDepth + 1 );
            layout->addWidget( res );
        }
        return top;
    } else {
        bool showInline = false;
        QStringList allowedMimeTypes;
        allowedMimeTypes << "text/html" << "text/plain" << "image/jpeg" <<
                "image/jpg" << "image/png" << "image/gif";
        // The problem is that some nasty MUAs (hint hint Thunderbird) would
        // happily attach a .tar.gz and call it "inline"
        if ( part->bodyDisposition().toLower() == "attachment" ) {
            // show as attachment
        } else if ( allowedMimeTypes.contains( part->mimeType() ) ) {
            showInline = true;
        }
        if ( showInline ) {
            part->fetchFromCache( manager->model );
            bool showDirectly = true;
            if ( ! part->fetched() )
                showDirectly = manager->model->isNetworkOnline() || part->octets() <= ExpensiveFetchThreshold;

            QWidget* widget = 0;
            if ( showDirectly )
                widget = new SimplePartWidget( 0, manager, part );
            else if ( manager->model->isNetworkAvailable() )
                widget = new LoadablePartWidget( 0, manager, part, wheelEventFilter );
            else
                widget = new QLabel( tr("Offline"), 0 );
            widget->installEventFilter( wheelEventFilter );
            return widget;
        } else {
            return new AttachmentView( 0, manager, part );
        }
    }
    QLabel* lbl = new QLabel( part->mimeType(), 0 );
    lbl->setMinimumSize( 800, 600 );
    return lbl;
}

}
