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

#ifndef IMAP_MODEL_MEMORYCACHE_H
#define IMAP_MODEL_MEMORYCACHE_H

#include "Cache.h"
#include <QMap>

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

/** @short A cache implementation that uses in-memory cache

    It also has an optional feature to dump the data to a local file and read
    it back in. Is isn't suitable for real production use, but it's a good start.
 */
class MemoryCache : public AbstractCache
{
    Q_OBJECT
public:
    explicit MemoryCache(QObject *parent);

    virtual QList<MailboxMetadata> childMailboxes(const QString &mailbox) const;
    virtual bool childMailboxesFresh(const QString &mailbox) const;
    virtual void setChildMailboxes(const QString &mailbox, const QList<MailboxMetadata> &data);

    virtual SyncState mailboxSyncState(const QString &mailbox) const;
    virtual void setMailboxSyncState(const QString &mailbox, const SyncState &state);

    virtual void setUidMapping(const QString &mailbox, const QList<uint> &mapping);
    virtual void clearUidMapping(const QString &mailbox);
    virtual QList<uint> uidMapping(const QString &mailbox) const;

    virtual void clearAllMessages(const QString &mailbox);
    virtual void clearMessage(const QString mailbox, const uint uid);

    virtual MessageDataBundle messageMetadata(const QString &mailbox, const uint uid) const;
    virtual void setMessageMetadata(const QString &mailbox, const uint uid, const MessageDataBundle &metadata);

    virtual QStringList msgFlags(const QString &mailbox, const uint uid) const;
    virtual void setMsgFlags(const QString &mailbox, const uint uid, const QStringList &newFlags);

    virtual QByteArray messagePart(const QString &mailbox, const uint uid, const QString &partId) const;
    virtual void setMsgPart(const QString &mailbox, const uint uid, const QString &partId, const QByteArray &data);
    virtual void forgetMessagePart(const QString &mailbox, const uint uid, const QString &partId);

    virtual QVector<Imap::Responses::ThreadingNode> messageThreading(const QString &mailbox);
    virtual void setMessageThreading(const QString &mailbox, const QVector<Imap::Responses::ThreadingNode> &threading);

    virtual void setRenewalThreshold(const int days);

private:
    QMap<QString, QList<MailboxMetadata> > mailboxes;
    QMap<QString, SyncState> syncState;
    QMap<QString, QList<uint> > seqToUid;
    QMap<QString, QMap<uint,QStringList> > flags;
    QMap<QString, QMap<uint, MessageDataBundle> > msgMetadata;
    QMap<QString, QMap<uint, QMap<QString, QByteArray> > > parts;
    QMap<QString, QVector<Imap::Responses::ThreadingNode> > threads;
};

}

}

#endif /* IMAP_MODEL_MEMORYCACHE_H */
