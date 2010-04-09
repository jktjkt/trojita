#include "PartWidgetFactory.h"
#include "AttachmentView.h"
#include "LoadablePartWidget.h"
#include "PartWidget.h"
#include "SimplePartWidget.h"
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
            return new MultipartAlternativeWidget( 0, this, part, recursionDepth );
        } else if ( part->mimeType() == QLatin1String("multipart/signed") ) {
            return new MultipartSignedWidget( 0, this, part, recursionDepth );
        } else {
            return new GenericMultipartWidget( 0, this, part, recursionDepth );
        }
    } else if ( part->mimeType() == QLatin1String("message/rfc822") ) {
        return new Message822Widget( 0, this, part, recursionDepth );
    } else {
        QStringList allowedMimeTypes;
        allowedMimeTypes << "text/html" << "text/plain" << "image/jpeg" <<
                "image/jpg" << "image/png" << "image/gif";
        // The problem is that some nasty MUAs (hint hint Thunderbird) would
        // happily attach a .tar.gz and call it "inline"
        bool showInline = part->bodyDisposition().toLower() != "attachment" &&
                          allowedMimeTypes.contains( part->mimeType() );

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
    return lbl;
}

Imap::Mailbox::Model* PartWidgetFactory::model() const
{
    return manager->model;
}

}
