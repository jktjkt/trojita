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

#ifndef IMAP_PARSER_SEQUENCE_H
#define IMAP_PARSER_SEQUENCE_H

#include <QList>
#include <QString>

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Class specifying a set of messagess to access

  Although named a sequence, there's no reason for a sequence to contain
  only consecutive ranges of numbers. For example, a set of
  { 1, 2, 3, 10, 15, 16, 17 } is perfectly valid sequence.
*/
class Sequence
{
    uint lo, hi;
    QList<uint> list;
    enum { DISTINCT, RANGE, UNLIMITED } kind;
public:
    /** @short Construct an invalid sequence */
    Sequence(): kind(DISTINCT) {}

    /** @short Construct a sequence holding only one number

      Such a sequence can be subsequently expanded by using its add() method.
      There's no way to turn it into an unlimited sequence, though -- use
      the startingAt() for creating sequences that grow to the "infinite".
    */
    explicit Sequence(const uint num);

    /** @short Construct a sequence holding a set of numbers between upper and lower bound

      This sequence can't be expanded ever after. Calling add() on it will
      assert().
    */
    Sequence(const uint lo, const uint hi): lo(lo), hi(hi), kind(RANGE) {}

    /** @short Create an "unlimited" sequence

      That's a sequence that starts at the specified offset and grow to the
      current maximal boundary. There's no way to add a distinct item to
      this set; doing so via the add() method will assert */
    static Sequence startingAt(const uint lo);

    /** @short Add another number to the sequence

      Note that you can only add numbers to a sequence created by the
      Sequence( const uint num ) constructor. Attempting to do so on other
      kinds of sequences will assert().
    */
    Sequence &add(const uint num);

    /** @short Converts sequence to a textual representation suitable for sending over the wire */
    QByteArray toByteArray() const;

    /** @short Converts sequence to a list of UIDs */
    QList<uint> toList() const;

    /** @short Create a sequence from a list of numbers */
    static Sequence fromList(QList<uint> numbers);

    /** @short Return true if the sequence contains at least some items */
    bool isValid() const;

};

bool operator==(const Sequence &a, const Sequence &b);

}
#endif /* IMAP_PARSER_SEQUENCE_H */
