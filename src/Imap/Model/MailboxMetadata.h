/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef IMAP_MODEL_MAILBOXMETADATA_H
#define IMAP_MODEL_MAILBOXMETADATA_H

#include <QDebug>
#include <QStringList>

namespace Imap
{
namespace Mailbox
{

struct MailboxMetadata {
    QString mailbox;
    QString separator;
    QStringList flags;

    MailboxMetadata(const QString &mailbox, const QString &separator, const QStringList &flags):
        mailbox(mailbox), separator(separator), flags(flags) {}
    MailboxMetadata() {}
};

/** @short Class for keeping track of information from the SELECT command */
class SyncState
{
    uint m_exists, m_recent, m_unSeenCount, m_unSeenOffset, m_uidNext, m_uidValidity;
    quint64 m_highestModSeq;
    QStringList m_flags, m_permanentFlags;

    bool m_hasExists, m_hasRecent, m_hasUnSeenCount, m_hasUnSeenOffset, m_hasUidNext, m_hasUidValidity,
         m_hasHighestModSeq, m_hasFlags, m_hasPermanentFlags;

    friend QDebug operator<<(QDebug &dbg, const Imap::Mailbox::SyncState &state);
public:
    SyncState();
    uint exists() const;
    uint recent() const;
    uint unSeenCount() const;
    uint unSeenOffset() const;
    uint uidNext() const;
    uint uidValidity() const;
    quint64 highestModSeq() const;
    QStringList flags() const;
    QStringList permanentFlags() const;

    void setExists(const uint exists);
    void setRecent(const uint recent);
    void setUnSeenCount(const uint unSeenCount);
    void setUnSeenOffset(const uint unSeenOffset);
    void setUidNext(const uint uidNext);
    void setUidValidity(const uint uidValidity);
    void setHighestModSeq(const quint64 highestModSeq);
    void setFlags(const QStringList &flags);
    void setPermanentFlags(const QStringList &permanentFlags);

    /** @short Return true if the record contains all items needed to display message numbers

      Ie. check for EXISTS, RECENT and UNSEEN.
    */
    bool isUsableForNumbers() const;
    /** @short Return true if all items really required for re-sync are available

      These fields are just UIDNEXT, UIDVALIDITY and EXISTS. We don't care about
      crap like RECENT.
    */
    bool isUsableForSyncing() const;

    /** @short Compare all members (including the hidden ones) with other */
    bool completelyEqualTo(const SyncState &other) const;
};

QDebug operator<<(QDebug &dbg, const Imap::Mailbox::SyncState &state);

}
}

QDebug operator<<(QDebug &dbg, const Imap::Mailbox::MailboxMetadata &metadata);

QDataStream &operator>>(QDataStream &stream, Imap::Mailbox::SyncState &ss);
QDataStream &operator<<(QDataStream &stream, const Imap::Mailbox::SyncState &ss);
QDataStream &operator>>(QDataStream &stream, Imap::Mailbox::MailboxMetadata &mm);
QDataStream &operator<<(QDataStream &stream, const Imap::Mailbox::MailboxMetadata &mm);

#endif
