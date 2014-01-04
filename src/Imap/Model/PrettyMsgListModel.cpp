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
#include "PrettyMsgListModel.h"
#include <QFont>
#include "Gui/IconLoader.h"
#include "ItemRoles.h"
#include "MsgListModel.h"
#include "ThreadingMsgListModel.h"
#include "Utils.h"


namespace Imap
{

namespace Mailbox
{

PrettyMsgListModel::PrettyMsgListModel(QObject *parent): QSortFilterProxyModel(parent), m_hideRead(false)
{
    setDynamicSortFilter(true);
}

QVariant PrettyMsgListModel::data(const QModelIndex &index, int role) const
{
    if (! index.isValid() || index.model() != this)
        return QVariant();

    if (index.column() < 0 || index.column() >= columnCount(index.parent()))
        return QVariant();

    if (index.row() < 0 || index.row() >= rowCount(index.parent()))
        return QVariant();

    QModelIndex translated = mapToSource(index);

    switch (role) {

    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        switch (index.column()) {
        case MsgListModel::TO:
        case MsgListModel::FROM:
        case MsgListModel::CC:
        case MsgListModel::BCC:
        {
            int backendRole = 0;
            switch (index.column()) {
            case MsgListModel::FROM:
                backendRole = RoleMessageFrom;
                break;
            case MsgListModel::TO:
                backendRole = RoleMessageTo;
                break;
            case MsgListModel::CC:
                backendRole = RoleMessageCc;
                break;
            case MsgListModel::BCC:
                backendRole = RoleMessageBcc;
                break;
            }
            QVariantList items = translated.data(backendRole).toList();
            return Imap::Message::MailAddress::prettyList(items, role == Qt::DisplayRole ?
                    Imap::Message::MailAddress::FORMAT_JUST_NAME :
                    Imap::Message::MailAddress::FORMAT_READABLE);
        }
        case MsgListModel::DATE:
        case MsgListModel::RECEIVED_DATE:
        {
            QDateTime res = translated.data(RoleMessageDate).toDateTime();
            if (role == Qt::ToolTipRole) {
                // tooltips shall always show the full and complete data
                return res.toLocalTime().toString(Qt::DefaultLocaleLongDate);
            }
            return prettyFormatDate(res.toLocalTime());
        }
        case MsgListModel::SIZE:
        {
            QVariant size = translated.data(RoleMessageSize);
            if (!size.isValid()) {
                return QVariant();
            }
            return PrettySize::prettySize(size.toUInt());
        }
        case MsgListModel::SUBJECT:
        {
            if (!translated.data(RoleIsFetched).toBool())
                return tr("Loading...");
            QString subject = translated.data(RoleMessageSubject).toString();
            return subject.isEmpty() ? tr("(no subject)") : subject;
        }
        }
        break;


    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case MsgListModel::SIZE:
            return Qt::AlignRight;
        default:
            return QVariant();
        }

    case Qt::DecorationRole:
        // We will need the data, but asking for Flags or IsMarkedXYZ doesn't cause a fetch
        translated.data(RoleMessageSubject);

        switch (index.column()) {
        case MsgListModel::SUBJECT:
        {
            if (! translated.data(RoleIsFetched).toBool())
                return QVariant();

            bool isForwarded = translated.data(RoleMessageIsMarkedForwarded).toBool();
            bool isReplied = translated.data(RoleMessageIsMarkedReplied).toBool();

            if (translated.data(RoleMessageIsMarkedDeleted).toBool())
                return Gui::loadIcon(QLatin1String("mail-deleted"));
            else if (isForwarded && isReplied)
                return Gui::loadIcon(QLatin1String("mail-replied-forw"));
            else if (isReplied)
                return Gui::loadIcon(QLatin1String("mail-replied"));
            else if (isForwarded)
                return Gui::loadIcon(QLatin1String("mail-forwarded"));
            else if (translated.data(RoleMessageIsMarkedRecent).toBool())
                return Gui::loadIcon(QLatin1String("mail-recent"));
            else
                return QIcon(QLatin1String(":/icons/transparent.png"));
        }
        case MsgListModel::SEEN:
            if (! translated.data(RoleIsFetched).toBool())
                return QVariant();
            if (! translated.data(RoleMessageIsMarkedRead).toBool())
                return QIcon(QLatin1String(":/icons/mail-unread.png"));
            else
                return QIcon(QLatin1String(":/icons/mail-read.png"));
        default:
            return QVariant();
        }

    case Qt::FontRole: {
        // We will need the data, but asking for Flags or IsMarkedXYZ doesn't cause a fetch
        translated.data(RoleMessageSubject);

        // These items should definitely *not* be rendered in bold
        if (!translated.data(RoleIsFetched).toBool())
            return QVariant();

        QFont font;
        if (translated.data(RoleMessageIsMarkedDeleted).toBool())
            font.setStrikeOut(true);

        if (! translated.data(RoleMessageIsMarkedRead).toBool()) {
            // If any message is marked as unread, show it in bold and be done with it
            font.setBold(true);
        } else if (translated.model()->hasChildren(translated.sibling(translated.row(), 0)) &&
                   translated.data(RoleThreadRootWithUnreadMessages).toBool()) {
            // If this node is not marked as read, is a root of some thread and that thread
            // contains unread messages, display the thread's root underlined
            font.setUnderline(true);
        }

        if (index.column() == MsgListModel::SUBJECT && translated.data(RoleMessageSubject).toString().isEmpty()) {
            font.setItalic(true);
        }

        return font;
    }
    }

    return QSortFilterProxyModel::data(index, role);
}

/** @short Format a QDateTime for compact display in one column of the view */
QString PrettyMsgListModel::prettyFormatDate(const QDateTime &dateTime) const
{
    // The time is not always synced properly, so better accept even slightly too new messages as "from today"
    QDateTime now = QDateTime::currentDateTime().addSecs(15*60);
    if (dateTime >= now) {
        // Messages from future shall always be shown using full format to prevent nasty surprises.
        return dateTime.toString(Qt::DefaultLocaleShortDate);
    } else if (dateTime.date() == now.date() || dateTime > now.addSecs(-6 * 3600)) {
        // It's a "today's message", i.e. something which is either literally from today or at least something not older than
        // six hours (an arbitraty magic number).
        // Originally, the cut-off time interval was set to 24 hours, but it led to weird things in the GUI like showing mails
        // from yesterday's 18:33 just as "18:33" even though the local time was "18:20" already. In a perfect world, we would
        // also periodically emit dataChanged() in order to force a wrap once the view has been open for too long, but that will
        // have to wait a bit.
        // The time is displayed without seconds to conserve space as well.
        return dateTime.time().toString(tr("hh:mm"));
    } else if (dateTime > now.addDays(-7)) {
        // Messages from the last seven days can be formatted just with the weekday name
        return dateTime.toString(tr("ddd hh:mm"));
    } else if (dateTime > now.addYears(-1)) {
        // Messages newer than one year don't have to show year
        return dateTime.toString(tr("d MMM hh:mm"));
    } else {
        // Old messagees shall have a full date
        return dateTime.toString(Qt::DefaultLocaleShortDate);
    }
}

void PrettyMsgListModel::setHideRead(bool value)
{
    m_hideRead = value;
    invalidateFilter();
}

bool PrettyMsgListModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!m_hideRead)
        return true;

    QModelIndex source_index = sourceModel()->index(source_row, 0, source_parent);

    for (QModelIndex test = source_index; test.isValid(); test = test.parent())
        if (test.data(RoleThreadRootWithUnreadMessages).toBool() || test.data(RoleMessageWasUnread).toBool())
            return true;

    return false;
}


void PrettyMsgListModel::sort(int column, Qt::SortOrder order)
{
    ThreadingMsgListModel *threadingModel = qobject_cast<ThreadingMsgListModel*>(sourceModel());
    Q_ASSERT(threadingModel);

    ThreadingMsgListModel::SortCriterium criterium = ThreadingMsgListModel::SORT_NONE;
    switch (column) {
    case MsgListModel::SEEN:
    case MsgListModel::COLUMN_COUNT:
    case MsgListModel::BCC:
    case -1:
        criterium = ThreadingMsgListModel::SORT_NONE;
        break;
    case MsgListModel::SUBJECT:
        criterium = ThreadingMsgListModel::SORT_SUBJECT;
        break;
    case MsgListModel::FROM:
        criterium = ThreadingMsgListModel::SORT_FROM;
        break;
    case MsgListModel::TO:
        criterium = ThreadingMsgListModel::SORT_TO;
        break;
    case MsgListModel::CC:
        criterium = ThreadingMsgListModel::SORT_CC;
        break;
    case MsgListModel::DATE:
        criterium = ThreadingMsgListModel::SORT_DATE;
        break;
    case MsgListModel::RECEIVED_DATE:
        criterium = ThreadingMsgListModel::SORT_ARRIVAL;
        break;
    case MsgListModel::SIZE:
        criterium = ThreadingMsgListModel::SORT_SIZE;
        break;
    }

    bool willSort = threadingModel->setUserSearchingSortingPreference(threadingModel->currentSearchCondition(), criterium, order);

    // Now let the view know about whether we accept such a sorting criteria.
    // This is needed because the QHeaderView doesn't offer a way to say "hey, you cannot sort in columns XYZ, only on ABC".
    if (criterium != ThreadingMsgListModel::SORT_NONE && willSort)
        emit sortingPreferenceChanged(column, order);
    else
        emit sortingPreferenceChanged(-1, order);
}

}

}
