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
#include "MailboxMetadata.h"

namespace Imap
{
namespace Mailbox
{


SyncState::SyncState():
    m_exists(0), m_recent(0), m_unSeenCount(0), m_unSeenOffset(0), m_uidNext(0), m_uidValidity(0), m_highestModSeq(0),
    m_hasExists(false), m_hasRecent(false), m_hasUnSeenCount(false), m_hasUnSeenOffset(false),
    m_hasUidNext(false), m_hasUidValidity(false), m_hasHighestModSeq(false), m_hasFlags(false),
    m_hasPermanentFlags(false)
{
}

bool SyncState::isUsableForNumbers() const
{
    return m_hasExists && m_hasRecent && m_hasUnSeenCount;
}

bool SyncState::isUsableForSyncing() const
{
    return m_hasExists && m_hasUidNext && m_hasUidValidity;
}

bool SyncState::isUsableForCondstore() const
{
    return m_hasHighestModSeq && highestModSeq() > 0 && isUsableForSyncing();
}

uint SyncState::exists() const
{
    return m_exists;
}

void SyncState::setExists(const uint exists)
{
    m_exists = exists;
    m_hasExists = true;
}

QStringList SyncState::flags() const
{
    return m_flags;
}

void SyncState::setFlags(const QStringList &flags)
{
    m_flags = flags;
    m_hasFlags = true;
}

QStringList SyncState::permanentFlags() const
{
    return m_permanentFlags;
}

void SyncState::setPermanentFlags(const QStringList &permanentFlags)
{
    m_permanentFlags = permanentFlags;
    m_hasPermanentFlags = true;
}

uint SyncState::recent() const
{
    return m_recent;
}

void SyncState::setRecent(const uint recent)
{
    m_recent = recent;
    m_hasRecent = true;
}

uint SyncState::uidNext() const
{
    return m_uidNext;
}

void SyncState::setUidNext(const uint uidNext)
{
    m_uidNext = uidNext;
    m_hasUidNext = true;
}

uint SyncState::uidValidity() const
{
    return m_uidValidity;
}

void SyncState::setUidValidity(const uint uidValidity)
{
    m_uidValidity = uidValidity;
    m_hasUidValidity = true;
}

uint SyncState::unSeenCount() const
{
    return m_unSeenCount;
}

void SyncState::setUnSeenCount(const uint unSeen)
{
    m_unSeenCount = unSeen;
    m_hasUnSeenCount = true;
}

uint SyncState::unSeenOffset() const
{
    return m_unSeenOffset;
}

void SyncState::setUnSeenOffset(const uint unSeen)
{
    m_unSeenOffset = unSeen;
    m_hasUnSeenOffset = true;
}

quint64 SyncState::highestModSeq() const
{
    return m_highestModSeq;
}

void SyncState::setHighestModSeq(const quint64 highestModSeq)
{
    m_highestModSeq = highestModSeq;
    m_hasHighestModSeq = true;
}

bool SyncState::completelyEqualTo(const SyncState &other) const
{
    return m_exists == other.m_exists && m_recent == other.m_recent && m_unSeenCount == other.m_unSeenCount &&
            m_unSeenOffset == other.m_unSeenOffset && m_uidNext == other.m_uidNext && m_uidValidity == other.m_uidValidity &&
            m_highestModSeq == other.m_highestModSeq && m_flags == other.m_flags && m_permanentFlags == other.m_permanentFlags &&
            m_hasExists == other.m_hasExists && m_hasRecent == other.m_hasRecent && m_hasUnSeenCount == other.m_hasUnSeenCount &&
            m_hasUnSeenOffset == other.m_hasUnSeenOffset && m_hasUidNext == other.m_hasUidNext &&
            m_hasUidValidity == other.m_hasUidValidity && m_hasHighestModSeq == other.m_hasHighestModSeq &&
            m_hasFlags == other.m_hasFlags && m_hasPermanentFlags == other.m_hasPermanentFlags;
}

QDebug operator<<(QDebug dbg, const Imap::Mailbox::SyncState &state)
{
    dbg << "UIDVALIDITY";
    if (state.m_hasUidValidity)
        dbg << state.uidValidity();
    else
        dbg << "n/a";
    dbg << "UIDNEXT";
    if (state.m_hasUidNext)
        dbg << state.uidNext();
    else
        dbg << "n/a";
    dbg << "EXISTS";
    if (state.m_hasExists)
        dbg << state.exists();
    else
        dbg << "n/a";
    dbg << "HIGHESTMODSEQ";
    if (state.m_hasHighestModSeq)
        dbg << state.highestModSeq();
    else
        dbg << "n/a";
    dbg << "UNSEEN-count";
    if (state.m_hasUnSeenCount)
        dbg << state.unSeenCount();
    else
        dbg << "n/a";
    dbg << "UNSEEN-offset";
    if (state.m_hasUnSeenOffset)
        dbg << state.unSeenOffset();
    else
        dbg << "n/a";
    dbg << "RECENT";
    if (state.m_hasRecent)
        dbg << state.recent();
    else
        dbg << "n/a";
    dbg << "PERMANENTFLAGS";
    if (state.m_hasPermanentFlags)
        dbg << state.permanentFlags();
    else
        dbg << "n/a";
    return dbg;
}

}
}


QDebug operator<<(QDebug dbg, const Imap::Mailbox::MailboxMetadata &metadata)
{
    return dbg << metadata.mailbox << metadata.separator << metadata.flags;
}

QDataStream &operator>>(QDataStream &stream, Imap::Mailbox::SyncState &ss)
{
    uint i;
    quint64 i64;
    QStringList list;
    stream >> i; ss.setExists(i);
    stream >> list; ss.setFlags(list);
    stream >> list; ss.setPermanentFlags(list);
    stream >> i; ss.setRecent(i);
    stream >> i; ss.setUidNext(i);
    stream >> i; ss.setUidValidity(i);
    stream >> i64; ss.setHighestModSeq(i64);
    stream >> i; ss.setUnSeenCount(i);
    stream >> i; ss.setUnSeenOffset(i);
    return stream;
}

QDataStream &operator<<(QDataStream &stream, const Imap::Mailbox::SyncState &ss)
{
    return stream << ss.exists() << ss.flags() << ss.permanentFlags() <<
           ss.recent() << ss.uidNext() << ss.uidValidity() << ss.highestModSeq() << ss.unSeenCount() << ss.unSeenOffset();
}

QDataStream &operator>>(QDataStream &stream, Imap::Mailbox::MailboxMetadata &mm)
{
    return stream >> mm.flags >> mm.mailbox >> mm.separator;
}

QDataStream &operator<<(QDataStream &stream, const Imap::Mailbox::MailboxMetadata &mm)
{
    return stream << mm.flags << mm.mailbox << mm.separator;
}

