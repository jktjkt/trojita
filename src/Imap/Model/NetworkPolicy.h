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

#ifndef TROJITA_IMAP_NETWORKPOLICY_H
#define TROJITA_IMAP_NETWORKPOLICY_H

namespace Imap {
namespace Mailbox {

/** @short Policy for accessing network */
enum NetworkPolicy {
    /**< @short No access to the network at all

    All network activity is suspended. If an action requires network access,
    it will either fail or be queued for later. */
    NETWORK_OFFLINE,

    /** @short Connections are possible, but expensive

    Information that is cached is preferred, as long as it is usable.
    Trojita will never miss a mail in this mode, but for example it won't
    check for new mailboxes. */
    NETWORK_EXPENSIVE,

    /** @short Connections have zero cost

    Normal mode of operation. All network activity is assumed to have zero
    cost and Trojita is free to ask network as often as possible. It will
    still use local cache when it makes sense, though. */
    NETWORK_ONLINE,
};

}
}

#endif
