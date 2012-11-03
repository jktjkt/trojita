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
    struct LightMessageDataBundle {
        Imap::Message::Envelope envelope;
        QByteArray serializedBodyStructure;
        QDateTime internalDate;
        uint size;
        QList<QByteArray> hdrReferences;
    };

    MemoryCache(QObject *parent, const QString &fileName);
    ~MemoryCache();

    virtual QList<MailboxMetadata> childMailboxes(const QString &mailbox) const;
    virtual bool childMailboxesFresh(const QString &mailbox) const;
    virtual void setChildMailboxes(const QString &mailbox, const QList<MailboxMetadata> &data);
    virtual void forgetChildMailboxes(const QString &mailbox);

    virtual SyncState mailboxSyncState(const QString &mailbox) const;
    virtual void setMailboxSyncState(const QString &mailbox, const SyncState &state);

    virtual void setUidMapping(const QString &mailbox, const QList<uint> &mapping);
    virtual void clearUidMapping(const QString &mailbox);
    virtual QList<uint> uidMapping(const QString &mailbox) const;

    virtual void clearAllMessages(const QString &mailbox);
    virtual void clearMessage(const QString mailbox, uint uid);

    virtual MessageDataBundle messageMetadata(const QString &mailbox, uint uid) const;
    virtual void setMessageMetadata(const QString &mailbox, uint uid, const MessageDataBundle &metadata);

    virtual QStringList msgFlags(const QString &mailbox, uint uid) const;
    virtual void setMsgFlags(const QString &mailbox, uint uid, const QStringList &newFlags);

    virtual QByteArray messagePart(const QString &mailbox, uint uid, const QString &partId) const;
    virtual void setMsgPart(const QString &mailbox, uint uid, const QString &partId, const QByteArray &data);

    virtual QVector<Imap::Responses::ThreadingNode> messageThreading(const QString &mailbox);
    virtual void setMessageThreading(const QString &mailbox, const QVector<Imap::Responses::ThreadingNode> &threading);

private:
    bool loadData();
    bool saveData() const;

    QMap<QString, QList<MailboxMetadata> > mailboxes;
    QMap<QString, SyncState> syncState;
    QMap<QString, QList<uint> > seqToUid;
    QMap<QString, QMap<uint,QStringList> > flags;
    QMap<QString, QMap<uint, LightMessageDataBundle> > msgMetadata;
    QMap<QString, QMap<uint, QMap<QString, QByteArray> > > parts;
    QMap<QString, QVector<Imap::Responses::ThreadingNode> > threads;


    QString fileName;
};

}

}

QDataStream &operator>>(QDataStream &stream, Imap::Mailbox::MemoryCache::LightMessageDataBundle &x);
QDataStream &operator<<(QDataStream &stream, const Imap::Mailbox::MemoryCache::LightMessageDataBundle &x);

#endif /* IMAP_MODEL_MEMORYCACHE_H */
