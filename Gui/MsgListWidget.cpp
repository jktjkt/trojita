#include "MsgListWidget.h"

#include <QFontMetrics>
#include "Imap/Model/MsgListModel.h"

namespace Gui {

MsgListWidget::MsgListWidget( QWidget* parent ): QTreeView(parent)
{
}

int MsgListWidget::sizeHintForColumn( int column ) const
{
    switch ( column ) {
        case Imap::Mailbox::MsgListModel::SUBJECT:
            return 300;
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

}

#include "MsgListWidget.moc"
