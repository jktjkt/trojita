#include "PartWidget.h"

#include <QLabel>
#include <QVBoxLayout>

#include "PartWidgetFactory.h"
#include "Rfc822HeaderView.h"
#include "Imap/Model/MailboxTree.h"

namespace Gui {

MultipartAlternativeWidget::MultipartAlternativeWidget(QWidget *parent,
    PartWidgetFactory *factory, Imap::Mailbox::TreeItemPart *part,
    const int recursionDepth ):
        QTabWidget( parent )
{
    for ( uint i = 0; i < part->childrenCount( factory->model() ); ++i ) {
        using namespace Imap::Mailbox;
        TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                part->child( i, factory->model() ) );
        Q_ASSERT( anotherPart );
        QWidget* item = factory->create( anotherPart, recursionDepth + 1 );
        addTab( item, anotherPart->mimeType() );
    }
    setCurrentIndex( part->childrenCount( factory->model() ) - 1 );
}

QString MultipartAlternativeWidget::quoteMe() const
{
    return QString();
}

MultipartSignedWidget::MultipartSignedWidget(QWidget *parent,
    PartWidgetFactory *factory, Imap::Mailbox::TreeItemPart *part,
    const int recursionDepth ):
        QGroupBox( tr("Signed Message"), parent )
{
    using namespace Imap::Mailbox;
    QVBoxLayout* layout = new QVBoxLayout( this );
    uint childrenCount = part->childrenCount( factory->model() );
    if ( childrenCount != 2 ) {
        QLabel* lbl = new QLabel( tr("Mallformed multipart/signed message"), this );
        layout->addWidget( lbl );
        return;
    }
    TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
            part->child( 0, factory->model() ) );
    layout->addWidget( factory->create( anotherPart, recursionDepth + 1 ) );
}

QString MultipartSignedWidget::quoteMe() const
{
    return QString();
}

GenericMultipartWidget::GenericMultipartWidget(QWidget *parent,
    PartWidgetFactory *factory, Imap::Mailbox::TreeItemPart *part,
    int recursionDepth):
        QGroupBox( tr("Multipart Message"), parent )
{
    // multipart/mixed or anything else, as mandated by RFC 2046, Section 5.1.3
    QVBoxLayout* layout = new QVBoxLayout( this );
    for ( uint i = 0; i < part->childrenCount( factory->model() ); ++i ) {
        using namespace Imap::Mailbox;
        TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                part->child( i, factory->model() ) );
        Q_ASSERT( anotherPart );
        QWidget* res = factory->create( anotherPart, recursionDepth + 1 );
        layout->addWidget( res );
    }
}

QString GenericMultipartWidget::quoteMe() const
{
    return QString();
}

Message822Widget::Message822Widget(QWidget *parent,
    PartWidgetFactory *factory, Imap::Mailbox::TreeItemPart *part,
    int recursionDepth):
        QGroupBox( tr("Message"), parent )
{
    QVBoxLayout* layout = new QVBoxLayout( this );
    QLabel* header = new Rfc822HeaderView( 0, factory->model(), part );
    layout->addWidget( header );
    for ( uint i = 0; i < part->childrenCount( factory->model() ); ++i ) {
        using namespace Imap::Mailbox;
        TreeItemPart* anotherPart = dynamic_cast<TreeItemPart*>(
                part->child( i, factory->model() ) );
        Q_ASSERT( anotherPart );
        QWidget* res = factory->create( anotherPart, recursionDepth + 1 );
        layout->addWidget( res );
    }
}

QString Message822Widget::quoteMe() const
{
    return QString();
}

}

#include "PartWidget.moc"
