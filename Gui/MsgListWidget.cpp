#include "MsgListWidget.h"

#include <QFontMetrics>
#include <QHeaderView>
#include "Imap/Model/MsgListModel.h"

namespace Gui {

MsgListWidget::MsgListWidget( QWidget* parent ): QTreeView(parent)
{
    connect( header(), SIGNAL(geometriesChanged()), this, SLOT(slotFixSize()) );
}

int MsgListWidget::sizeHintForColumn( int column ) const
{
    switch ( column ) {
        case Imap::Mailbox::MsgListModel::SUBJECT:
            return 200;
        case Imap::Mailbox::MsgListModel::SEEN:
            return 16;
        case Imap::Mailbox::MsgListModel::FROM:
        case Imap::Mailbox::MsgListModel::TO:
            return fontMetrics().size( Qt::TextSingleLine, QLatin1String("Blesmrt Trojita") ).width();
        case Imap::Mailbox::MsgListModel::DATE:
            return fontMetrics().size( Qt::TextSingleLine,
                                       QVariant( QDateTime::currentDateTime() ).toString()
                                       // because that's what the model's doing
                                       ).width();
        case Imap::Mailbox::MsgListModel::SIZE:
            return fontMetrics().size( Qt::TextSingleLine, QLatin1String("888.1 kB") ).width();
        default:
            return QTreeView::sizeHintForColumn( column );
    }
}

void MsgListWidget::slotFixSize()
{
    if ( header()->visualIndex( Imap::Mailbox::MsgListModel::SEEN ) == -1 ) {
        // calling setResizeMode() would assert()
        qDebug() << "Can't fix the header size of the icon, sorry";
        return;
    }
    header()->setStretchLastSection( false );
    header()->setResizeMode( Imap::Mailbox::MsgListModel::SUBJECT, QHeaderView::Stretch );
    header()->setResizeMode( Imap::Mailbox::MsgListModel::SEEN, QHeaderView::Fixed );
}

}

#include "MsgListWidget.moc"
