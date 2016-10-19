/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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


#ifndef TROJITA_UIUTILS_QAIM_DFS_H
#define TROJITA_UIUTILS_QAIM_DFS_H

#include <functional>
#include <QModelIndex>

namespace UiUtils {

class QaimDfsIterator
{
public:
    explicit QaimDfsIterator(const QModelIndex &index);
    explicit QaimDfsIterator(const QModelIndex &index, const QAbstractItemModel *model);
    QaimDfsIterator();
    QaimDfsIterator(const QaimDfsIterator &) = default;
    QaimDfsIterator & operator++();
    QaimDfsIterator & operator--();
    const QModelIndex &operator*() const;
    const QModelIndex *operator->() const;
    bool operator==(const QaimDfsIterator &other);
    bool operator!=(const QaimDfsIterator &other);
private:
    QModelIndex m_current;
    const QAbstractItemModel *m_model;
};

void gotoNext(const QAbstractItemModel *model, const QModelIndex &currentIndex,
              std::function<bool(const QModelIndex &)> matcher,
              std::function<void(const QModelIndex &)> onSuccess,
              std::function<void()> onFailure);
void gotoPrevious(const QAbstractItemModel *model, const QModelIndex &currentIndex,
                  std::function<bool(const QModelIndex &)> matcher,
                  std::function<void(const QModelIndex &)> onSuccess,
                  std::function<void()> onFailure);

}

namespace std {
template<> struct iterator_traits<UiUtils::QaimDfsIterator>
{
    using difference_type = int;
    using value_type = QModelIndex;
    using pointer = const QModelIndex *;
    using reference = const QModelIndex &;
    using iterator_category = std::input_iterator_tag;
    // yes, really -- because we're a stashing iterator and our operator* doesn't point to a true reference
};
}

Q_DECLARE_METATYPE(UiUtils::QaimDfsIterator);


#endif
