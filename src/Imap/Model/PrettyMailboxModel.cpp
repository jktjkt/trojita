/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "PrettyMailboxModel.h"
#include "MailboxModel.h"
#include "ItemRoles.h"

#ifdef XTUPLE_CONNECT
# include "Common/SettingsNames.h"
# include <QSettings>
#endif

#include <QFont>
#include "Gui/IconLoader.h"

namespace Imap
{

namespace Mailbox
{

PrettyMailboxModel::PrettyMailboxModel(QObject *parent, MailboxModel *mailboxModel):
    QSortFilterProxyModel(parent), m_showOnlySubscribed(false)
{
    setDynamicSortFilter(true);
    setSourceModel(mailboxModel);
}

QVariant PrettyMailboxModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.column() != 0)
        return QVariant();

    if (index.row() < 0 || index.row() >= rowCount(index.parent()) || index.model() != this)
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
    {
        QModelIndex translated = mapToSource(index);
        qlonglong unreadCount = translated.data(RoleUnreadMessageCount).toLongLong();
        qlonglong recentCount = translated.data(RoleRecentMessageCount).toLongLong();
        // The "problem" is that the \Recent and (lack of) \Seen flags are orthogonal states.
        //
        // There is no rule saying that a recent message is still unseen (trivial example: an already-read message being copied),
        // or that an unread message is recent.
        //
        // That's not really so much of a problem after the mailbox has been synced because we have all flags for each and every
        // message, and can therefore easily compute the difference. The problem, however, is that there is no such feature
        // in the SELECT command (and before you ask, SEARCH requires a selected mailbox, and "selected" is very close to
        // "synchronized" in Trojita). Trojita tries to satisfy all requests for "message numbers" at once, via the STATUS command
        // for unselected mailboxes, or through the already-known and cached information in the message flags for the recently
        // synchronized ones. In theory, it is possible to implement a workaround involving EXAMINE and (E)SEARCH, but even that
        // has its drawbacks (like the necessary serialization of requests and the requirement for a ton of new code).
        //
        // So in short, this is why there's no "(1 + 5)" result with six unread an one recent message.
        //
        // We also deliberately put an emphasis on the "unread count", even to an extent where there's no special information
        // for mailboxes with some recent, but no unread messages.
        if (recentCount && unreadCount) {
            return tr("%1 (%2/%3)")
                   .arg(QSortFilterProxyModel::data(index, RoleShortMailboxName).toString(),
                        QString::number(recentCount), QString::number(unreadCount));
        } else if (unreadCount) {
            return tr("%1 (%2)")
                   .arg(QSortFilterProxyModel::data(index, RoleShortMailboxName).toString(),
                        QString::number(unreadCount));
        } else {
            return QSortFilterProxyModel::data(index, RoleShortMailboxName);
        }
    }
    case Qt::FontRole:
    {
        QModelIndex translated = mapToSource(index);
        if (translated.data(RoleMailboxNumbersFetched).toBool() &&
            translated.data(RoleUnreadMessageCount).toULongLong() > 0) {
            QFont font;
            font.setBold(true);
            return font;
        } else {
            return QVariant();
        }
    }
    case Qt::DecorationRole:
    {
        QModelIndex translated = mapToSource(index);
        if (translated.data(RoleMailboxItemsAreLoading).toBool())
            return Gui::loadIcon(QLatin1String("folder-grey"));
#ifdef XTUPLE_CONNECT
        else if (QSettings().value(Common::SettingsNames::xtSyncMailboxList).toStringList().contains(
                     translated.data(RoleMailboxName).toString()))
            return Gui::loadIcon(QLatin1String("folder-xt-sync.png"));
#endif
        else if (translated.data(RoleMailboxIsINBOX).toBool())
            return Gui::loadIcon(QLatin1String("mail-folder-inbox"));
        else if (translated.data(RoleRecentMessageCount).toInt() > 0)
            return Gui::loadIcon(QLatin1String("folder-bookmark"));
        else if (translated.data(RoleMailboxIsSelectable).toBool())
            return Gui::loadIcon(QLatin1String("folder"));
        else
            return Gui::loadIcon(QLatin1String("folder-open"));
    }
    case Qt::ToolTipRole:
    {
        QModelIndex translated = mapToSource(index);
        return tr("<p>%1</p>\n<p>%2 messages<br/>%3 unread<br/>%4 recent</p>")
               .arg(translated.data(RoleShortMailboxName).toString(), translated.data(RoleTotalMessageCount).toString(),
                    translated.data(RoleUnreadMessageCount).toString(), translated.data(RoleRecentMessageCount).toString());
    }
    default:
        return QSortFilterProxyModel::data(index, role);
    }
}

bool PrettyMailboxModel::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent);
    return source_column == 0;
}

bool PrettyMailboxModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!m_showOnlySubscribed)
        return true;

    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    Q_ASSERT(index.isValid());

    // FIXME: walk the tree resursively from here instead of just checking for children
    return index.data(RoleMailboxIsSubscribed).toBool() || sourceModel()->hasChildren(index);
}

bool PrettyMailboxModel::hasChildren(const QModelIndex &parent) const
{
    return dynamic_cast<const MailboxModel *>(sourceModel())->hasChildren(mapToSource(parent));
}

#ifdef XTUPLE_CONNECT
void PrettyMailboxModel::xtConnectStatusChanged(QModelIndex index)
{
    Q_ASSERT(index.model() == this);
    emit dataChanged(index, index);
}
#endif

void PrettyMailboxModel::setShowOnlySubscribed(bool filterUnsubscribed)
{
    m_showOnlySubscribed = filterUnsubscribed;
    invalidateFilter();
}

}

}
