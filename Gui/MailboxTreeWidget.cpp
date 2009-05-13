#include "MailboxTreeWidget.h"

#include <QFontMetrics>
#include <QHeaderView>
#include "Imap/Model/MailboxModel.h"

namespace Gui {

MailboxTreeWidget::MailboxTreeWidget( QWidget* parent ): QTreeView(parent)
{
    connect( header(), SIGNAL(geometriesChanged()), this, SLOT(slotFixSize()) );
}

int MailboxTreeWidget::sizeHintForColumn( int column ) const
{
    switch ( column ) {
        case Imap::Mailbox::MailboxModel::NAME:
            return 150;
        case Imap::Mailbox::MailboxModel::TOTAL_MESSAGE_COUNT:
        case Imap::Mailbox::MailboxModel::UNREAD_MESSAGE_COUNT:
            return fontMetrics().size( Qt::TextSingleLine, QLatin1String("88888") ).width();
        default:
            return QTreeView::sizeHintForColumn( column );
    }
}

void MailboxTreeWidget::slotFixSize()
{
    if ( header()->visualIndex( Imap::Mailbox::MailboxModel::TOTAL_MESSAGE_COUNT ) == -1 ) {
        // calling setResizeMode() would assert()
        qDebug() << "Can't fix the header size of the icon, sorry";
        return;
    }
    header()->setStretchLastSection( false );
    header()->setResizeMode( Imap::Mailbox::MailboxModel::NAME, QHeaderView::Stretch );
    header()->setResizeMode( Imap::Mailbox::MailboxModel::TOTAL_MESSAGE_COUNT, QHeaderView::Fixed );
    header()->setResizeMode( Imap::Mailbox::MailboxModel::UNREAD_MESSAGE_COUNT, QHeaderView::Fixed );
}

}

#include "MailboxTreeWidget.moc"
