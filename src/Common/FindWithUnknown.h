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

#ifndef TROJITA_FIND_WITH_UNKNOWN_H
#define TROJITA_FIND_WITH_UNKNOWN_H

namespace Common {

/** @short Perform a linear search between the pair of iterators and find a first occurrence of the specified value

The function will completely ignore any items for which isUnknown returns true.

If no item matching the specification is present in the sequence, a place where it can be inserted without violating these constraints is returned.
*/
template <typename RandomAccessIterator, typename T, typename IsUnknown, typename LessThan>
RandomAccessIterator linearLowerBoundWithUnknownElements(RandomAccessIterator begin, RandomAccessIterator end, const T &value, IsUnknown isUnknown, LessThan lessThan)
{
    while (begin < end) {
        if (isUnknown(*begin)) {
            ++begin;
        } else if (lessThan(*begin, value)) {
            ++begin;
        } else {
            return begin;
        }
    }
    return begin;
}

template <typename RandomAccessIterator, typename T, typename IsUnknown, typename LessThan>
RandomAccessIterator lowerBoundWithUnknownElements(RandomAccessIterator begin, RandomAccessIterator end, const T &value, IsUnknown isUnknown, LessThan lessThan)
{
    RandomAccessIterator middle;
    int n = int(end - begin);
    int half;

    while (n > 0) {
        half = n >> 1;
        middle = begin + half;
        if (isUnknown(*middle)) {
            return linearLowerBoundWithUnknownElements(begin, begin + n, value, isUnknown, lessThan);
        }
        if (lessThan(*middle, value)) {
            begin = middle + 1;
            n -= half + 1;
        } else {
            n = half;
        }
    }
    return begin;
}

}

#endif // TROJITA_FIND_WITH_UNKNOWN_H
