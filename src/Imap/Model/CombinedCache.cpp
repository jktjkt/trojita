/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#include "CombinedCache.h"
#include "DiskPartCache.h"
#include "SQLCache.h"

namespace Imap
{
namespace Mailbox
{

CombinedCache::CombinedCache(QObject *parent, const QString &name, const QString &cacheDir):
    AbstractCache(parent), name(name), cacheDir(cacheDir)
{
    sqlCache = new SQLCache(this);
    connect(sqlCache, SIGNAL(error(QString)), this, SIGNAL(error(QString)));
    diskPartCache = new DiskPartCache(this, cacheDir);
    connect(diskPartCache, SIGNAL(error(QString)), this, SIGNAL(error(QString)));
}

CombinedCache::~CombinedCache()
{
}

bool CombinedCache::open()
{
    return sqlCache->open(name, cacheDir + QLatin1String("/imap.cache.sqlite"));
}

QList<MailboxMetadata> CombinedCache::childMailboxes(const QString &mailbox) const
{
    return sqlCache->childMailboxes(mailbox);
}

bool CombinedCache::childMailboxesFresh(const QString &mailbox) const
{
    return sqlCache->childMailboxesFresh(mailbox);
}

void CombinedCache::setChildMailboxes(const QString &mailbox, const QList<MailboxMetadata> &data)
{
    sqlCache->setChildMailboxes(mailbox, data);
}

void CombinedCache::forgetChildMailboxes(const QString &mailbox)
{
    sqlCache->forgetChildMailboxes(mailbox);
}

SyncState CombinedCache::mailboxSyncState(const QString &mailbox) const
{
    return sqlCache->mailboxSyncState(mailbox);
}

void CombinedCache::setMailboxSyncState(const QString &mailbox, const SyncState &state)
{
    sqlCache->setMailboxSyncState(mailbox, state);
}

QList<uint> CombinedCache::uidMapping(const QString &mailbox) const
{
    return sqlCache->uidMapping(mailbox);
}

void CombinedCache::setUidMapping(const QString &mailbox, const QList<uint> &seqToUid)
{
    sqlCache->setUidMapping(mailbox, seqToUid);
}

void CombinedCache::clearUidMapping(const QString &mailbox)
{
    sqlCache->clearUidMapping(mailbox);
}

void CombinedCache::clearAllMessages(const QString &mailbox)
{
    sqlCache->clearAllMessages(mailbox);
    diskPartCache->clearAllMessages(mailbox);
}

void CombinedCache::clearMessage(const QString mailbox, uint uid)
{
    sqlCache->clearMessage(mailbox, uid);
    diskPartCache->clearMessage(mailbox, uid);
}

QStringList CombinedCache::msgFlags(const QString &mailbox, uint uid) const
{
    return sqlCache->msgFlags(mailbox, uid);
}

void CombinedCache::setMsgFlags(const QString &mailbox, uint uid, const QStringList &flags)
{
    sqlCache->setMsgFlags(mailbox, uid, flags);
}

AbstractCache::MessageDataBundle CombinedCache::messageMetadata(const QString &mailbox, uint uid) const
{
    return sqlCache->messageMetadata(mailbox, uid);
}

void CombinedCache::setMessageMetadata(const QString &mailbox, uint uid, const MessageDataBundle &metadata)
{
    sqlCache->setMessageMetadata(mailbox, uid, metadata);
}

QByteArray CombinedCache::messagePart(const QString &mailbox, uint uid, const QString &partId) const
{
    QByteArray res = sqlCache->messagePart(mailbox, uid, partId);
    if (res.isEmpty()) {
        res = diskPartCache->messagePart(mailbox, uid, partId);
    }
    return res;
}

void CombinedCache::setMsgPart(const QString &mailbox, uint uid, const QString &partId, const QByteArray &data)
{
    if (data.size() < 1000) {
        sqlCache->setMsgPart(mailbox, uid, partId, data);
    } else {
        diskPartCache->setMsgPart(mailbox, uid, partId, data);
    }
}

QVector<Imap::Responses::ThreadingNode> CombinedCache::messageThreading(const QString &mailbox)
{
    return sqlCache->messageThreading(mailbox);
}

void CombinedCache::setMessageThreading(const QString &mailbox, const QVector<Imap::Responses::ThreadingNode> &threading)
{
    sqlCache->setMessageThreading(mailbox, threading);
}

}
}
