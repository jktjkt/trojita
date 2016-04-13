/* Copyright (C) 2014 - 2015 Stephan Platz <trojita@paalsteek.de>
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

#ifndef CRYPTOGRAPHY_MESSAGEPART_H
#define CRYPTOGRAPHY_MESSAGEPART_H

#include <memory>
#include <QModelIndex>
#include <QUrl>
#include <map>

//#define MIME_TREE_DEBUG

#ifdef MIME_TREE_DEBUG
class QDebug;
#endif

namespace Imap {
namespace Message {
class Envelope;
}
}

namespace Cryptography {
class MessageModel;

class MessagePart
{
protected:
    /** @short Availability of an item */
    enum class FetchingState {
        NONE, /**< @short No attempt to decrypt/load an item has been made yet */
        UNAVAILABLE, /**< @short Item could not be decrypted/loaded */
        LOADING, /**< @short Decryption/Loading of an item is already scheduled */
        DONE /**< @short Item is available right now */
    };

public:
    using Ptr = std::unique_ptr<MessagePart>;

    MessagePart(MessagePart *parent, const int row);
    virtual ~MessagePart();

    virtual void fetchChildren(MessageModel *model) = 0;

    MessagePart* parent() const;
    int row() const;
    MessagePart *child(MessageModel *model, int row, int column) const;
    virtual int rowCount(MessageModel *model) const;
    virtual int columnCount(MessageModel *model) const;

    virtual QVariant data(int role) const = 0;

    void setSpecialParts(Ptr headerPart, Ptr textPart, Ptr mimePart, Ptr rawPart);

protected:
#ifdef MIME_TREE_DEBUG
    QByteArray dump(const int nestingLevel) const;
#endif

    FetchingState m_childrenState;
    MessagePart *m_parent;
    std::map<int, MessagePart::Ptr> m_children;
    Ptr m_headerPart, m_textPart, m_mimePart, m_rawPart;
    int m_row;

    friend class MessageModel;
    friend class TopLevelMessage; // due to that lambda in TopLevelMessage::fetchChildren
#ifdef MIME_TREE_DEBUG
    friend QDebug operator<<(QDebug dbg, const MessagePart &part);
private:
    virtual QByteArray dumpLocalInfo() const = 0;
#endif
};

#ifdef MIME_TREE_DEBUG
QDebug operator<<(QDebug dbg, const MessagePart &part);
#endif

/** @short Represent the root of the message with a valid index

This is a werid class, but the rest of the code really wants to operate on valid indexes, including the message's root.
*/
class TopLevelMessage : public MessagePart
{
public:
    TopLevelMessage(const QModelIndex &messageRoot, MessageModel *model);
    ~TopLevelMessage();

    void fetchChildren(MessageModel *model) override;
    QVariant data(int role) const override;
private:
#ifdef MIME_TREE_DEBUG
    virtual QByteArray dumpLocalInfo() const override;
#endif

    QPersistentModelIndex m_root;
    MessageModel *m_model;
};

class ProxyMessagePart : public MessagePart
{
public:
    ProxyMessagePart(MessagePart *parent, const int row, const QModelIndex &sourceIndex, MessageModel *model);
    ~ProxyMessagePart();

    void fetchChildren(MessageModel *model) override;

    QVariant data(int role) const override;

private:
#ifdef MIME_TREE_DEBUG
    virtual QByteArray dumpLocalInfo() const override;
#endif

    QPersistentModelIndex m_sourceIndex;
    MessageModel *m_model;
};

class LocalMessagePart : public MessagePart
{
public:
    LocalMessagePart(MessagePart *parent, const int row, const QByteArray &mimetype);
    ~LocalMessagePart();

    void fetchChildren(MessageModel *model) override;

    QVariant data(int role) const override;

    void setData(const QByteArray &data);

    void setCharset(const QByteArray &charset);
    void setContentFormat(const QByteArray &format);
    void setDelSp(const QByteArray &delSp);
    void setFilename(const QString &filename);
    void setEncoding(const QByteArray &encoding);
    void setBodyFldId(const QByteArray &bodyFldId);
    void setBodyDisposition(const QByteArray &bodyDisposition);
    void setMultipartRelatedStartPart(const QByteArray &startPart);
    void setOctets(uint octets);
    void setEnvelope(std::unique_ptr<Imap::Message::Envelope> envelope);
    void setHdrReferences(const QList<QByteArray> &references);
    void setHdrListPost(const QList<QUrl> &listPost);
    void setHdrListPostNo(const bool listPostNo);
    void setBodyFldParam(const QMap<QByteArray, QByteArray> &bodyFldParam);

    void setChild(int row, Ptr part);

private:
    bool isTopLevelMultipart() const;
    QByteArray partId() const;
    QByteArray pathToPart() const;
#ifdef MIME_TREE_DEBUG
    QByteArray dumpLocalInfo() const override;
#endif

protected:
    FetchingState m_localState;
    std::unique_ptr<Imap::Message::Envelope> m_envelope;
    QList<QByteArray> m_hdrReferences;
    QList<QUrl> m_hdrListPost;
    bool m_hdrListPostNo;
    QByteArray m_charset;
    QByteArray m_contentFormat;
    QByteArray m_delSp;
    QByteArray m_data;
    QByteArray m_mimetype;
    QByteArray m_encoding;
    QByteArray m_bodyFldId;
    QByteArray m_bodyDisposition;
    QByteArray m_multipartRelatedStartPart;
    QMap<QByteArray, QByteArray> m_bodyFldParam;
    QString m_filename;
    uint m_octets;

    friend class MessageModel;
};

}

#endif /* CRYPTOGRAPHY_MESSAGEPART_H */
