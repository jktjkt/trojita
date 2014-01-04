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

inline bool operator==(const MailboxMetadata &a, const MailboxMetadata &b)
{
    return a.mailbox == b.mailbox && a.separator == b.separator && a.flags == b.flags;
}

inline bool operator!=(const MailboxMetadata &a, const MailboxMetadata &b)
{
    return ! (a == b);
}

/** @short Class for keeping track of information from the SELECT command */
class SyncState
{
    uint m_exists, m_recent, m_unSeenCount, m_unSeenOffset, m_uidNext, m_uidValidity;
    quint64 m_highestModSeq;
    QStringList m_flags, m_permanentFlags;

    bool m_hasExists, m_hasRecent, m_hasUnSeenCount, m_hasUnSeenOffset, m_hasUidNext, m_hasUidValidity,
         m_hasHighestModSeq, m_hasFlags, m_hasPermanentFlags;

    friend QDebug operator<<(QDebug dbg, const Imap::Mailbox::SyncState &state);
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

    bool isUsableForCondstore() const;

    /** @short Compare all members (including the hidden ones) with other */
    bool completelyEqualTo(const SyncState &other) const;
};

QDebug operator<<(QDebug dbg, const Imap::Mailbox::SyncState &state);

}
}

QDebug operator<<(QDebug dbg, const Imap::Mailbox::MailboxMetadata &metadata);

QDataStream &operator>>(QDataStream &stream, Imap::Mailbox::SyncState &ss);
QDataStream &operator<<(QDataStream &stream, const Imap::Mailbox::SyncState &ss);
QDataStream &operator>>(QDataStream &stream, Imap::Mailbox::MailboxMetadata &mm);
QDataStream &operator<<(QDataStream &stream, const Imap::Mailbox::MailboxMetadata &mm);

#endif
