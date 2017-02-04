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

#include "Common/StashingReverseIterator.h"
#include "UiUtils/QaimDfsIterator.h"

namespace UiUtils {

QaimDfsIterator::QaimDfsIterator(const QModelIndex &index)
    : m_current(index)
    , m_model(index.model())
{
    Q_ASSERT(m_model);
}

QaimDfsIterator::QaimDfsIterator(const QModelIndex &index, const QAbstractItemModel *model)
    : m_current(index)
    , m_model(model)
{
    Q_ASSERT(m_model);
    Q_ASSERT(!index.isValid() || m_model == index.model());
}

QaimDfsIterator::QaimDfsIterator()
    : m_model(nullptr)
{
}

QaimDfsIterator &QaimDfsIterator::operator++()
{
    bool wentUp = false;
    while (m_current.isValid()) {
        // if there are (unvisited) children, descent into them
        auto firstChild = m_current.child(0, 0);
        if (!wentUp && firstChild.isValid()) {
            m_current = firstChild;
            return *this;
        }

        // if there are siblings, go there; we surely haven't been there yet
        auto nextSibling = m_current.sibling(m_current.row() + 1, 0);
        if (nextSibling.isValid()) {
            m_current = nextSibling;
            return *this;
        }

        // else, check our parent
        m_current = m_current.parent();
        wentUp = true;
    }
    Q_ASSERT(!m_current.isValid());
    return *this;
}

QaimDfsIterator &QaimDfsIterator::operator--()
{
    auto deepestChild = [](const QAbstractItemModel *model, QModelIndex where) -> QModelIndex {
        while (model->hasChildren(where)) {
            where = model->index(model->rowCount(where) - 1, 0, where);
        }
        return where;
    };

    // special case: do not wrap around
    if (!m_model) {
        return *this;
    }

    // special case: if we're at the end, find "the last child"
    if (!m_current.isValid()) {
        m_current = deepestChild(m_model, QModelIndex());
        return *this;
    }

    // check if we can visit our previous sibling
    auto previousSibling = m_current.sibling(m_current.row() - 1, 0);
    if (previousSibling.isValid()) {
        // actually, its children, if there are any
        m_current = deepestChild(previousSibling.model(), previousSibling);
        return *this;
    }

    // OK, we're a first child of something, apparently
    Q_ASSERT(m_current.row() == 0);
    if (!m_current.parent().isValid()) {
        // a special case: prevent circling back to the very end
        m_model = nullptr;
    }
    m_current = m_current.parent();
    return *this;
}

const QModelIndex &QaimDfsIterator::operator*() const
{
    return m_current;
}

const QModelIndex *QaimDfsIterator::operator->() const
{
    return &m_current;
}

bool QaimDfsIterator::operator==(const QaimDfsIterator &other)
{
    if (m_model) {
        return m_current == other.m_current;
    } else {
        return !other.m_model || !other.m_current.isValid();
    }
}

bool QaimDfsIterator::operator!=(const QaimDfsIterator &other)
{
    return m_current != other.m_current;
    // yes, this ignores m_model, which means that an iterator which got decremented too much
    // won't be considered different from one which is at the end
}

/** @short Search between start1 and end1, wrap around to start2 and continue to end2 */
template <typename Iterator>
static void wrappedFind(Iterator start1, Iterator end1, Iterator start2, Iterator end2,
                        std::function<bool(typename std::iterator_traits<Iterator>::reference)> itValidityChecker,
                        std::function<bool(typename std::iterator_traits<Iterator>::reference)> matcher,
                        std::function<void(typename std::iterator_traits<Iterator>::reference)> onSuccess,
                        std::function<void()> onFailure)
{
    auto it = start1;
    auto rejectedResult = end1;
    if (itValidityChecker(*it)) {
        it = std::find_if(it, end1, matcher);
    }

    if (!itValidityChecker(*it)) {
        it = std::find_if(start2, end2, matcher);
        rejectedResult = end2;
    }

    if (itValidityChecker(*it) && it != rejectedResult) {
        onSuccess(*it);
    } else {
        onFailure();
    }
}

void gotoNext(const QAbstractItemModel *model, const QModelIndex &currentIndex,
              std::function<bool(const QModelIndex &)> matcher,
              std::function<void(const QModelIndex &)> onSuccess,
              std::function<void()> onFailure)
{
    auto it = UiUtils::QaimDfsIterator(currentIndex, model);
    ++it;
    // explicitly specify the template to help GCC 4.8.5 realize that our lambda is copmatible with that std::function
    UiUtils::wrappedFind<decltype(it)>(
                it,
                QaimDfsIterator(QModelIndex(), model),
                QaimDfsIterator(model->index(0, 0, QModelIndex())),
                QaimDfsIterator(currentIndex, model),
                [](const QModelIndex &idx) { return idx.isValid(); },
                matcher,
                onSuccess,
                onFailure);
}

void gotoPrevious(const QAbstractItemModel *model, const QModelIndex &currentIndex,
                  std::function<bool(const QModelIndex &)> matcher,
                  std::function<void(const QModelIndex &)> onSuccess,
                  std::function<void()> onFailure)
{
    auto it = Common::make_stashing_reverse_iterator(QaimDfsIterator(currentIndex, model));
    --it;

    UiUtils::wrappedFind<decltype(it)>(
                Common::make_stashing_reverse_iterator(QaimDfsIterator(currentIndex, model)),
                Common::make_stashing_reverse_iterator(QaimDfsIterator(model->index(0, 0, QModelIndex()), model)),
                Common::make_stashing_reverse_iterator(QaimDfsIterator(QModelIndex(), model)),
                it,
                [](const QModelIndex &idx) { return idx.isValid(); },
                matcher,
                onSuccess,
                onFailure);
}

}
