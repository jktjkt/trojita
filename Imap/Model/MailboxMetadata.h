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

#include <QDebug>
#include <QStringList>

namespace Imap {
namespace Mailbox {

struct MailboxMetadata {
    QString mailbox;
    QString separator;
    QStringList flags;

    MailboxMetadata( const QString& _mailbox, const QString& _separator, const QStringList& _flags ):
        mailbox(_mailbox), separator(_separator), flags(_flags) {}
    MailboxMetadata() {}
};

/** @short Class for keeping track of information from the SELECT command */
class SyncState {
   uint _exists, _recent, _unSeen, _uidNext, _uidValidity;
   QStringList _flags, _permanentFlags;

   bool _hasExists, _hasRecent, _hasUnSeen, _hasUidNext, _hasUidValidity,
        _hasFlags, _hasPermanentFlags;
public:
    SyncState();
    uint exists() const;
    uint recent() const;
    uint unSeen() const;
    uint uidNext() const;
    uint uidValidity() const;
    QStringList flags() const;
    QStringList permanentFlags() const;

    void setExists( const uint exists );
    void setRecent( const uint recent );
    void setUnSeen( const uint unSeen );
    void setUidNext( const uint uidNext );
    void setUidValidity( const uint uidValidity );
    void setFlags( const QStringList& flags );
    void setPermanentFlags( const QStringList& permanentFlags );

    /** @short Return true if all fields required by RFC3501 are filed */
    bool isComplete() const;
    /** @short Return true if all items really required for re-sync are available

      These fields are just UIDNEXT, UIDVALIDITY and EIXSTS. We don't care about
      crap like RECENT.
    */
    bool isUsableForSyncing() const;
};

}
}

QDebug operator<<( QDebug& dbg, const Imap::Mailbox::MailboxMetadata& metadata );
QDebug operator<<( QDebug& dbg, const Imap::Mailbox::SyncState& state );

QDataStream& operator>>( QDataStream& stream, Imap::Mailbox::SyncState& ss );
QDataStream& operator<<( QDataStream& stream, const Imap::Mailbox::SyncState& ss );
QDataStream& operator>>( QDataStream& stream, Imap::Mailbox::MailboxMetadata& mm );
QDataStream& operator<<( QDataStream& stream, const Imap::Mailbox::MailboxMetadata& mm );

#endif
