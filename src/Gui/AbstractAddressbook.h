/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>

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

#ifndef ABSTRACT_ADDRESSBOOK
#define ABSTRACT_ADDRESSBOOK

#include <QStringList>

namespace Gui
{

/** @short An abstract adressbook interface */
class AbstractAddressbook {
public:
    virtual ~AbstractAddressbook() {}
    /** @short Returns a list of matching contacts in the form "[Name <]addy@mail.com[>]"
     *
     * - Matches are case INsensitive
     * - The match aligns to either Name or addy@mail.com, ie. "ja" or "jkt" match
     *   "Jan Kundrát <jkt@gentoo.org>" but "gentoo" does not
     * - Strings in the ignore list are NOT included in the return, despite they may match.
     * - The return can be empty.
     *
     * - @p max defines the demanded maximum reply length
     * It is intended to improve performance in the implementations
     * If you reimplement this, please notice that the return can be longer, but the client
     * will not make use of that. Negative values mean "uncapped"
    */
    // A maximum value is sufficient since typing "a" and getting a list with a hundred entries which
    // one will then navigate for the address unlikely a real use case - so there's no point in presenting
    // more than <place random number here> entries (or look them up, or transmit them)
    virtual QStringList complete(const QString &string, const QStringList &ignore, int max = -1) const = 0;

    /** @short Return a list of display names matching the given e-mail address */
    virtual QStringList prettyNamesForAddress(const QString &mail) const = 0;
};

}

#endif // ABSTRACT_ADDRESSBOOK
