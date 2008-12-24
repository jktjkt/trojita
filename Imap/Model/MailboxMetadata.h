/* Copyright (C) 2007 - 2008 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef IMAP_MODEL_MAILBOXMETADATA_H
#define IMAP_MODEL_MAILBOXMETADATA_H

#include <QStringList>

namespace Imap {
namespace Mailbox {

struct MailboxMetadata {
    QString mailbox;
    QString separator;
    QStringList flags;

    MailboxMetadata( const QString& _mailbox, const QString& _separator, const QStringList& _flags ):
        mailbox(_mailbox), separator(_separator), flags(_flags) {};
    MailboxMetadata() {};
};

}
}

#endif
