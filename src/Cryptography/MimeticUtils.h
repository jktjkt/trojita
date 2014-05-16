/* Copyright (C) 2014-2015 Stephan Platz <trojita@paalsteek.de>
   Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#ifndef TROJITA_MIMETIC_UTILS_H
#define TROJITA_MIMETIC_UTILS_H

#include <QList>
#include "Cryptography/MessagePart.h"

namespace mimetic {
class MimeEntity;
struct MailboxList;
struct AddressList;
}

namespace Imap {
namespace Message {
class MailAddress;
}
}

namespace Cryptography {

/** @short Conversion from Mimetic's data types to Trojita's data types */
class MimeticUtils {
public:
    static void storeInterestingFields(const mimetic::MimeEntity &me, LocalMessagePart *part);
    static Cryptography::MessagePart::Ptr mimeEntityToPart(const mimetic::MimeEntity &me, MessagePart *parent, const int row);
    static QList<Imap::Message::MailAddress> mailboxListToQList(const mimetic::MailboxList &list);
    static QList<Imap::Message::MailAddress> addressListToQList(const mimetic::AddressList &list);
};
}

#endif
