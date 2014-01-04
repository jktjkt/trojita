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

#include "Sequence.h"
#include <QStringList>
#include <QTextStream>

namespace Imap
{

Sequence::Sequence(const uint num): kind(DISTINCT)
{
    list << num;
}

Sequence Sequence::startingAt(const uint lo)
{
    Sequence res(lo);
    res.lo = lo;
    res.kind = UNLIMITED;
    return res;
}

QByteArray Sequence::toByteArray() const
{
    switch (kind) {
    case DISTINCT:
    {
        Q_ASSERT(! list.isEmpty());

        QStringList res;
        int i = 0;
        while (i < list.size()) {
            int old = i;
            while (i < list.size() - 1 &&
                   list[i] == list[ i + 1 ] - 1)
                ++i;
            if (old != i) {
                // we've found a sequence
                res << QString::number(list[old]) + QLatin1Char(':') + QString::number(list[i]);
            } else {
                res << QString::number(list[i]);
            }
            ++i;
        }
        return res.join(QLatin1String(",")).toLocal8Bit();
    }
    case RANGE:
        Q_ASSERT(lo <= hi);
        if (lo == hi)
            return QByteArray::number(lo);
        else
            return QByteArray::number(lo) + ':' + QByteArray::number(hi);
    case UNLIMITED:
        return QByteArray::number(lo) + ":*";
    }
    // fix gcc warning
    Q_ASSERT(false);
    return QByteArray();
}

QList<uint> Sequence::toList() const
{
    switch (kind) {
    case DISTINCT:
        Q_ASSERT(!list.isEmpty());
        return list;
    case RANGE:
        Q_ASSERT(lo <= hi);
        if (lo == hi) {
            return QList<uint>() << lo;
        } else {
            QList<uint> res;
            for (uint i = lo; i < hi; ++i)
                res << i;
            return res;
        }
    case UNLIMITED:
        Q_ASSERT(false);
        return QList<uint>();
    }
    Q_ASSERT(false);
    return QList<uint>();
}

Sequence &Sequence::add(uint num)
{
    Q_ASSERT(kind == DISTINCT);
    QList<uint>::iterator it = qLowerBound(list.begin(), list.end(), num);
    if (it == list.end() || *it != num)
        list.insert(it, num);
    return *this;
}

Sequence Sequence::fromList(QList<uint> numbers)
{
    Q_ASSERT(!numbers.isEmpty());
    qSort(numbers);
    Sequence seq(numbers.first());
    for (int i = 1; i < numbers.size(); ++i) {
        seq.add(numbers[i]);
    }
    return seq;
}

bool Sequence::isValid() const
{
    if (kind == DISTINCT && list.isEmpty())
        return false;
    else
        return true;
}

QTextStream &operator<<(QTextStream &stream, const Sequence &s)
{
    return stream << s.toByteArray();
}

bool operator==(const Sequence &a, const Sequence &b)
{
    // This operator is used only in the test suite, so performance doesn't matter and this was *so* easy to hack together...
    return a.toList() == b.toList();
}

}
