/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include "MsgListView.h"

#include <QAction>
#include <QFontMetrics>
#include <QHeaderView>
#include <QSignalMapper>
#include "Imap/Model/MsgListModel.h"

namespace Gui {

MsgListView::MsgListView(QWidget* parent): QTreeView(parent)
{
    connect(header(), SIGNAL(geometriesChanged()), this, SLOT(slotFixSize()));
    connect(this, SIGNAL(expanded(QModelIndex)), this, SLOT(slotExpandWholeSubtree(QModelIndex)));
    connect(header(), SIGNAL(sectionCountChanged(int,int)), this, SLOT(slotSectionCountChanged()));
    header()->setContextMenuPolicy(Qt::ActionsContextMenu);
    headerFieldsMapper = new QSignalMapper(this);
    connect(headerFieldsMapper, SIGNAL(mapped(int)), this, SLOT(slotHeaderSectionVisibilityToggled(int)));
}

int MsgListView::sizeHintForColumn( int column ) const
{
    switch ( column ) {
        case Imap::Mailbox::MsgListModel::SUBJECT:
            return 200;
        case Imap::Mailbox::MsgListModel::SEEN:
            return 16;
        case Imap::Mailbox::MsgListModel::FROM:
        case Imap::Mailbox::MsgListModel::TO:
        case Imap::Mailbox::MsgListModel::CC:
        case Imap::Mailbox::MsgListModel::BCC:
            return fontMetrics().size( Qt::TextSingleLine, QLatin1String("Blesmrt Trojita Foo Bar") ).width();
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

void MsgListView::slotFixSize()
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

void MsgListView::slotExpandWholeSubtree(const QModelIndex &rootIndex)
{
    if ( rootIndex.parent().isValid() )
        return;

    QVector<QModelIndex> queue(1, rootIndex);
    for ( int i = 0; i < queue.size(); ++i ) {
        const QModelIndex &currentIndex = queue[i];
        // Append all children to the queue...
        for ( int j = 0; j < currentIndex.model()->rowCount(currentIndex); ++j )
            queue.append(currentIndex.child(j, 0));
        // ...and expand the current index
        expand(currentIndex);
    }
}

void MsgListView::slotSectionCountChanged()
{
    Q_ASSERT(header());
    // At first, remove all actions
    QList<QAction*> actions = header()->actions();
    Q_FOREACH(QAction *action, actions) {
        header()->removeAction(action);
        headerFieldsMapper->removeMappings(action);
        action->deleteLater();
    }
    actions.clear();
    // Now add them again
    for ( int i = 0; i < header()->count(); ++i ) {
        QString message = header()->model() ? header()->model()->headerData(i, Qt::Horizontal).toString() : QString::number(i);
        QAction *action = new QAction( message, this );
        action->setCheckable(true);
        action->setChecked(true);
        connect(action, SIGNAL(toggled(bool)), headerFieldsMapper, SLOT(map()));
        headerFieldsMapper->setMapping(action, i);
        header()->addAction(action);

        // Next, add some special handling of certain columns
        switch (i) {
        case Imap::Mailbox::MsgListModel::SEEN:
            // This column doesn't have a textual description
            action->setText(tr("Seen status"));
            break;
        case Imap::Mailbox::MsgListModel::TO:
        case Imap::Mailbox::MsgListModel::CC:
        case Imap::Mailbox::MsgListModel::BCC:
            // And these should be hidden by default
            action->toggle();
            break;
        default:
            break;
        }
    }
}

void MsgListView::slotHeaderSectionVisibilityToggled(int section)
{
    QList<QAction*> actions = header()->actions();
    if ( section >= actions.size() || section < 0 )
        return;
    bool hide = ! actions[section]->isChecked();

    if ( hide && header()->hiddenSectionCount() == header()->count() - 1 ) {
        // This would hide the very last section, which would hide the whole header view
        actions[section]->setChecked(true);
    } else {
        header()->setSectionHidden(section, hide);
    }
}

}


