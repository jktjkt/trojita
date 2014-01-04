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

#include "MemoryCache.h"
#include <QDebug>
#include <QFile>

//#define CACHE_DEBUG

namespace Imap
{
namespace Mailbox
{

MemoryCache::MemoryCache(QObject *parent): AbstractCache(parent)
{
}

QList<MailboxMetadata> MemoryCache::childMailboxes(const QString &mailbox) const
{
    return mailboxes[ mailbox ];
}

bool MemoryCache::childMailboxesFresh(const QString &mailbox) const
{
    return mailboxes.contains(mailbox);
}

void MemoryCache::setChildMailboxes(const QString &mailbox, const QList<MailboxMetadata> &data)
{
#ifdef CACHE_DEBUG
    qDebug() << "setting child mailboxes for" << mailbox << "to" << data;
#endif
    mailboxes[mailbox] = data;
}

SyncState MemoryCache::mailboxSyncState(const QString &mailbox) const
{
    return syncState[mailbox];
}

void MemoryCache::setMailboxSyncState(const QString &mailbox, const SyncState &state)
{
#ifdef CACHE_DEBUG
    qDebug() << "setting mailbox sync state of" << mailbox << "to" << state;
#endif
    syncState[mailbox] = state;
}

void MemoryCache::setUidMapping(const QString &mailbox, const QList<uint> &mapping)
{
#ifdef CACHE_DEBUG
    qDebug() << "saving UID mapping for" << mailbox << "to" << mapping;
#endif
    seqToUid[mailbox] = mapping;
}

void MemoryCache::clearUidMapping(const QString &mailbox)
{
#ifdef CACHE_DEBUG
    qDebug() << "clearing UID mapping for" << mailbox;
#endif
    seqToUid.remove(mailbox);
}

void MemoryCache::clearAllMessages(const QString &mailbox)
{
#ifdef CACHE_DEBUG
    qDebug() << "pruging all info for mailbox" << mailbox;
#endif
    flags.remove(mailbox);
    msgMetadata.remove(mailbox);
    parts.remove(mailbox);
    threads.remove(mailbox);
}

void MemoryCache::clearMessage(const QString mailbox, const uint uid)
{
#ifdef CACHE_DEBUG
    qDebug() << "pruging all info for message" << mailbox << uid;
#endif
    if (flags.contains(mailbox))
        flags[mailbox].remove(uid);
    if (msgMetadata.contains(mailbox))
        msgMetadata[mailbox].remove(uid);
    if (parts.contains(mailbox))
        parts[mailbox].remove(uid);
}

void MemoryCache::setMsgPart(const QString &mailbox, const uint uid, const QString &partId, const QByteArray &data)
{
#ifdef CACHE_DEBUG
    qDebug() << "set message part" << mailbox << uid << partId << data.size();
#endif
    parts[mailbox][uid][partId] = data;
}

void MemoryCache::forgetMessagePart(const QString &mailbox, const uint uid, const QString &partId)
{
#ifdef CACHE_DEBUG
    qDebug() << "forget message part" << mailbox << uid << partId;
#endif
    parts[mailbox][uid].remove(partId);

}

void MemoryCache::setMsgFlags(const QString &mailbox, uint uid, const QStringList &newFlags)
{
#ifdef CACHE_DEBUG
    qDebug() << "set FLAGS for" << mailbox << uid << newFlags;
#endif
    flags[mailbox][uid] = newFlags;
}

QStringList MemoryCache::msgFlags(const QString &mailbox, const uint uid) const
{
    return flags[mailbox][uid];
}

QList<uint> MemoryCache::uidMapping(const QString &mailbox) const
{
    return seqToUid[ mailbox ];
}

void MemoryCache::setMessageMetadata(const QString &mailbox, const uint uid, const MessageDataBundle &metadata)
{
    msgMetadata[mailbox][uid] = metadata;
}

MemoryCache::MessageDataBundle MemoryCache::messageMetadata(const QString &mailbox, const uint uid) const
{
    const QMap<uint, MessageDataBundle> &firstLevel = msgMetadata[ mailbox ];
    QMap<uint, MessageDataBundle>::const_iterator it = firstLevel.find(uid);
    if (it == firstLevel.end()) {
        return MessageDataBundle();
    }
    return *it;
}

QByteArray MemoryCache::messagePart(const QString &mailbox, const uint uid, const QString &partId) const
{
    if (! parts.contains(mailbox))
        return QByteArray();
    const QMap<uint, QMap<QString, QByteArray> > &mailboxParts = parts[ mailbox ];
    if (! mailboxParts.contains(uid))
        return QByteArray();
    const QMap<QString, QByteArray> &messageParts = mailboxParts[ uid ];
    if (! messageParts.contains(partId))
        return QByteArray();
    return messageParts[ partId ];
}

QVector<Imap::Responses::ThreadingNode> MemoryCache::messageThreading(const QString &mailbox)
{
    return threads[mailbox];
}

void MemoryCache::setMessageThreading(const QString &mailbox, const QVector<Imap::Responses::ThreadingNode> &threading)
{
    threads[mailbox] = threading;
}

void MemoryCache::setRenewalThreshold(const int days)
{
    Q_UNUSED(days);
}

}
}
