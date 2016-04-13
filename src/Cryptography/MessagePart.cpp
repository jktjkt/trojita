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

#include <QtGui/QFont>

#include "MessagePart.h"

#include "MessageModel.h"
#include "Cryptography/PartReplacer.h"
#include "Common/MetaTypes.h"
#include "Imap/Encoders.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/Formatting.h"

namespace Cryptography {

#define PART_FETCH_CHILDREN(MODEL) const_cast<MessagePart *>(this)->fetchChildren(const_cast<MessageModel *>(MODEL))

MessagePart::MessagePart(MessagePart *parent, const int row)
    : m_childrenState(FetchingState::NONE)
    , m_parent(parent)
    , m_children()
    , m_row(row)
{
}

MessagePart::~MessagePart()
{
}

MessagePart *MessagePart::parent() const
{
    return m_parent;
}

int MessagePart::row() const
{
    return m_row;
}

int MessagePart::rowCount(MessageModel *model) const
{
    PART_FETCH_CHILDREN(model);
    return m_children.size();
}

int MessagePart::columnCount(MessageModel *model) const
{
    PART_FETCH_CHILDREN(model);
    return 1;
}

MessagePart *MessagePart::child(MessageModel *model, int row, int column) const
{
    PART_FETCH_CHILDREN(model);
    if (row == 0) {
        switch (column) {
        case Imap::Mailbox::TreeItem::OFFSET_HEADER:
            return m_headerPart.get();
        case Imap::Mailbox::TreeItem::OFFSET_TEXT:
            return m_textPart.get();
        case Imap::Mailbox::TreeItem::OFFSET_MIME:
            return m_mimePart.get();
        case Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS:
            return m_rawPart.get();
        }
    }
    if (column != 0) {
        return nullptr;
    }

    PART_FETCH_CHILDREN(model);
    auto it = m_children.find(row);
    return it == m_children.end() ? nullptr : it->second.get();
}

void MessagePart::setSpecialParts(Ptr headerPart, Ptr textPart, Ptr mimePart, Ptr rawPart)
{
    Q_ASSERT(!m_headerPart);
    Q_ASSERT(!m_textPart);
    Q_ASSERT(!m_mimePart);
    Q_ASSERT(!m_rawPart);
    m_headerPart = std::move(headerPart);
    m_textPart = std::move(textPart);
    m_mimePart = std::move(mimePart);
    m_rawPart = std::move(rawPart);
}

#ifdef MIME_TREE_DEBUG
QDebug operator<<(QDebug dbg, const MessagePart &part)
{
    return dbg << part.dump(0).data();
}

QByteArray MessagePart::dump(const int nestingLevel) const
{
    const QByteArray spacing(nestingLevel, ' ');
    QByteArray res = spacing + QByteArray::number(row()) + ": " + dumpLocalInfo() +
            " (0x" + QByteArray::number(reinterpret_cast<const qulonglong>(this), 16) + ", " +
            data(Imap::Mailbox::RolePartMimeType).toByteArray() + ", " +
            data(Imap::Mailbox::RolePartPathToPart).toByteArray()
            + ") {\n";
    for (const auto &child: m_children) {
        if (child.second->parent() != this) {
            res += spacing + "  v-- next child has wrong parent "
                             "0x" + QByteArray::number(reinterpret_cast<const qulonglong>(child.second->parent()), 16) + " --v \n";
        }
        res += child.second->dump(nestingLevel + 2);
    }
    res += spacing + "}\n";
    return res;
}

QByteArray MessagePart::dumpLocalInfo() const
{
    return "MessagePart";
}
#endif

TopLevelMessage::TopLevelMessage(const QModelIndex &messageRoot, MessageModel *model)
    : MessagePart(nullptr, 0)
    , m_root(messageRoot)
    , m_model(model)
{
    Q_ASSERT(m_root.isValid());
    m_model->m_map[m_root] = this;
}

TopLevelMessage::~TopLevelMessage()
{
    for (auto it = m_model->m_map.begin(); it != m_model->m_map.end(); /* nothing */) {
        if (*it == this) {
            it = m_model->m_map.erase(it);
        } else {
            ++it;
        }
    }
}

QVariant TopLevelMessage::data(int role) const
{
    return m_root.data(role);
}

void TopLevelMessage::fetchChildren(MessageModel *model)
{
    // This is where the magic happens -- this class is just a dumb, fake node, so we're inserting something real below.
    // The stuff below still points to the message root, *not* to an actual message part, and everybody is happy that way.

    if (m_childrenState == FetchingState::NONE) {
        // trigger a fetch within the original model
        m_root.model()->rowCount(m_root);

        if (m_root.child(0, 0).isValid()) {
            // OK, we can do it synchronously.
            // Note that we are *not* guarding this based on a RoleIsFetched because that thing might not be true
            // when the rowsInserted() is called.
            m_children[0] = Ptr(new ProxyMessagePart(this, 0, m_root, model));
            m_childrenState = FetchingState::DONE;
        } else {
            // The upstream model is loading its stuff, okay, let's do it asynchronously.
            m_childrenState = FetchingState::LOADING;
            model->m_insertRows = model->connect(model->m_message.model(), &QAbstractItemModel::rowsInserted,
                                                 model, [this, model](const QModelIndex &idx) {
                if (idx == model->m_message) {
                    model->disconnect(model->m_insertRows);
                    Q_ASSERT(m_root.isValid());
                    m_childrenState = MessagePart::FetchingState::NONE;
                    model->beginInsertRows(QModelIndex(), 0, 0);
                    fetchChildren(model);
                    model->endInsertRows();
                }
            });
        }
    }
}

#ifdef MIME_TREE_DEBUG
QByteArray TopLevelMessage::dumpLocalInfo() const
{
    return "TopLevelMessage";
}
#endif


ProxyMessagePart::ProxyMessagePart(MessagePart *parent, const int row, const QModelIndex &sourceIndex, MessageModel *model)
    : MessagePart(parent, row)
    , m_sourceIndex(sourceIndex)
    , m_model(model)
{
    m_model->m_map[sourceIndex] = this;
}

ProxyMessagePart::~ProxyMessagePart()
{
    for (auto it = m_model->m_map.begin(); it != m_model->m_map.end(); /* nothing */) {
        if (*it == this) {
            it = m_model->m_map.erase(it);
        } else {
            ++it;
        }
    }
}

QVariant ProxyMessagePart::data(int role) const
{
    if (auto msg = dynamic_cast<TopLevelMessage *>(parent())) {
        switch (role) {
        case Qt::DisplayRole:
            return QStringLiteral("[fake message root: UID %1]").arg(
                        msg->data(Imap::Mailbox::RoleMessageUid).toString());
        }
    }
    return m_sourceIndex.data(role);
}

void ProxyMessagePart::fetchChildren(MessageModel *model)
{
    QModelIndex index = parent() ? QModelIndex(m_sourceIndex) : model->message();
    if (!index.isValid()) {
        return;
    }
    switch (m_childrenState) {
    case FetchingState::DONE:
    case FetchingState::LOADING:
    case FetchingState::UNAVAILABLE:
        return;
    case FetchingState::NONE:
        break;
    }

    auto headerIdx = index.child(0, Imap::Mailbox::TreeItem::OFFSET_HEADER);
    auto textIdx = index.child(0, Imap::Mailbox::TreeItem::OFFSET_TEXT);
    auto mimeIdx = index.child(0, Imap::Mailbox::TreeItem::OFFSET_MIME);
    auto rawIdx = index.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);

    setSpecialParts(
                std::unique_ptr<ProxyMessagePart>(headerIdx.isValid() ? new ProxyMessagePart(this, 0, headerIdx, model) : nullptr),
                std::unique_ptr<ProxyMessagePart>(textIdx.isValid() ? new ProxyMessagePart(this, 0, textIdx, model) : nullptr),
                std::unique_ptr<ProxyMessagePart>(mimeIdx.isValid() ? new ProxyMessagePart(this, 0, mimeIdx, model) : nullptr),
                std::unique_ptr<ProxyMessagePart>(rawIdx.isValid() ? new ProxyMessagePart(this, 0, rawIdx, model) : nullptr)
                );

    m_childrenState = FetchingState::LOADING;
    int row = 0;
    while (true) {
        auto childIndex = index.child(row, 0);
        if (!childIndex.isValid()) {
            break;
        }

        auto part = Ptr(new ProxyMessagePart(this, row, childIndex, model));

        // Find out if something wants to override our choice
        for (const auto &module: model->m_partHandlers) {
            part = module->createPart(model, this, std::move(part), childIndex, model->createIndex(this->row(), 0, this));
        }

        m_children[row] = std::move(part);
        ++row;
    }
    m_childrenState = FetchingState::DONE;
}

#ifdef MIME_TREE_DEBUG
QByteArray ProxyMessagePart::dumpLocalInfo() const
{
    return "ProxyMessagePart";
}
#endif


LocalMessagePart::LocalMessagePart(MessagePart *parent, const int row, const QByteArray &mimetype)
    : MessagePart(parent, row)
    , m_localState(FetchingState::NONE)
    , m_hdrListPostNo(false)
    , m_mimetype(mimetype)
    , m_octets(0)
{
}

LocalMessagePart::~LocalMessagePart()
{
}

void LocalMessagePart::fetchChildren(MessageModel *model)
{
    return;
}

void LocalMessagePart::setChild(int row, Ptr part)
{
    Q_ASSERT((int)m_children.size() >= row);
    Q_ASSERT(part);
    Q_ASSERT(row == part->row());
    m_children[row] = std::move(part);
}

void LocalMessagePart::setData(const QByteArray &data)
{
    m_data = data;
    m_localState = FetchingState::DONE;
}

void LocalMessagePart::setCharset(const QByteArray &charset)
{
    m_charset = charset;
}

void LocalMessagePart::setContentFormat(const QByteArray &format)
{
    m_contentFormat = format;
}

void LocalMessagePart::setDelSp(const QByteArray &delSp)
{
    m_delSp = delSp;
}

void LocalMessagePart::setFilename(const QString &filename)
{
    m_filename = filename;
}

void LocalMessagePart::setEncoding(const QByteArray &encoding)
{
    m_encoding = encoding;
}

void LocalMessagePart::setBodyFldId(const QByteArray &bodyFldId)
{
    m_bodyFldId = bodyFldId;
}

void LocalMessagePart::setBodyDisposition(const QByteArray &bodyDisposition)
{
    m_bodyDisposition = bodyDisposition;
}

void LocalMessagePart::setMultipartRelatedStartPart(const QByteArray &startPart)
{
    m_multipartRelatedStartPart = startPart;
}

void LocalMessagePart::setOctets(uint octets)
{
    m_octets = octets;
}

void LocalMessagePart::setEnvelope(std::unique_ptr<Imap::Message::Envelope> envelope)
{
    m_envelope = std::move(envelope);
}

void LocalMessagePart::setHdrReferences(const QList<QByteArray> &references)
{
    m_hdrReferences = references;
}

void LocalMessagePart::setHdrListPost(const QList<QUrl> &listPost)
{
    m_hdrListPost = listPost;
}

void LocalMessagePart::setHdrListPostNo(const bool listPostNo)
{
    m_hdrListPostNo = listPostNo;
}

void LocalMessagePart::setBodyFldParam(const QMap<QByteArray, QByteArray> &bodyFldParam)
{
    m_bodyFldParam = bodyFldParam;
}

bool LocalMessagePart::isTopLevelMultipart() const
{
    return m_mimetype.startsWith("multipart/") && (!parent()->parent() || parent()->data(Imap::Mailbox::RolePartMimeType).toByteArray().startsWith("message/"));
}

QByteArray LocalMessagePart::partId() const
{
    if (isTopLevelMultipart())
        return QByteArray();

    QByteArray id = QByteArray::number(row() + 1);
    if (parent() && !parent()->data(Imap::Mailbox::RolePartId).toByteArray().isEmpty())
        id = parent()->data(Imap::Mailbox::RolePartId).toByteArray() + '.' + id;

    return id;
}

QByteArray LocalMessagePart::pathToPart() const {
    QByteArray parentPath;
    if (parent()) {
        parentPath = parent()->data(Imap::Mailbox::RolePartPathToPart).toByteArray();
    }

    return parentPath + '/' + QByteArray::number(row());
}

QVariant LocalMessagePart::data(int role) const
{
    // The first three roles are for debugging purposes only
    switch (role) {
    case Qt::DisplayRole:
        if (isTopLevelMultipart()) {
            return QString::fromUtf8(m_mimetype);
        } else {
            return QString(QString::fromUtf8(partId()) + QStringLiteral(": ") + QString::fromUtf8(m_mimetype));
        }
    case Qt::ToolTipRole:
        if (m_mimetype == "message/rfc822") {
            Q_ASSERT(m_envelope);
            QString buf;
            QTextStream stream(&buf);
            stream << *m_envelope;
            return UiUtils::Formatting::htmlEscaped(buf);
        } else {
            return m_octets > 10000 ? QStringLiteral("%1 bytes of data").arg(m_octets) : QString::fromUtf8(m_data);
        }
    case Qt::FontRole:
    {
        QFont f;
        f.setItalic(true);
        return f;
    }
    case Imap::Mailbox::RoleIsFetched:
        return m_localState == FetchingState::DONE;
    case Imap::Mailbox::RoleMessageEnvelope:
        if (m_envelope) {
            return QVariant::fromValue<Imap::Message::Envelope>(*m_envelope);
        } else {
            return QVariant();
        }
    case Imap::Mailbox::RoleMessageHeaderReferences:
        return QVariant::fromValue(m_hdrReferences);
    case Imap::Mailbox::RoleMessageHeaderListPost:
    {
        QVariantList res;
        Q_FOREACH(const QUrl &url, m_hdrListPost)
            res << url;
        return res;
    }
    case Imap::Mailbox::RoleMessageHeaderListPostNo:
        return m_hdrListPostNo;
    case Imap::Mailbox::RoleIsUnavailable:
        return m_localState == FetchingState::UNAVAILABLE;
    case Imap::Mailbox::RolePartData:
        return m_data;
    case Imap::Mailbox::RolePartUnicodeText:
        if (m_mimetype.startsWith("text/")) {
            return Imap::decodeByteArray(m_data, m_charset);
        } else {
            return QVariant();
        }
    case Imap::Mailbox::RolePartMimeType:
        return m_mimetype;
    case Imap::Mailbox::RolePartCharset:
        return m_charset;
    case Imap::Mailbox::RolePartContentFormat:
        return m_contentFormat;
    case Imap::Mailbox::RolePartContentDelSp:
        return m_delSp;
    case Imap::Mailbox::RolePartEncoding:
        return m_encoding;
    case Imap::Mailbox::RolePartBodyFldId:
        return m_bodyFldId;
    case Imap::Mailbox::RolePartBodyDisposition:
        return m_bodyDisposition;
    case Imap::Mailbox::RolePartFileName:
        return m_filename;
    case Imap::Mailbox::RolePartOctets:
        return m_octets;
    case Imap::Mailbox::RolePartId:
        return partId();
    case Imap::Mailbox::RolePartPathToPart:
        return pathToPart();
    case Imap::Mailbox::RolePartMultipartRelatedMainCid:
        if (m_multipartRelatedStartPart.isEmpty()) {
            return m_multipartRelatedStartPart;
        } else {
            return QVariant();
        }
    case Imap::Mailbox::RolePartIsTopLevelMultipart:
        return isTopLevelMultipart();
    case Imap::Mailbox::RolePartForceFetchFromCache:
        return QVariant(); // Nothing to do here
    case Imap::Mailbox::RolePartBufferPtr:
        return QVariant::fromValue(const_cast<QByteArray*>(&m_data));
    case Imap::Mailbox::RoleMessageDate:
        return m_envelope ? m_envelope->date : QDateTime();
    case Imap::Mailbox::RoleMessageSubject:
        return m_envelope ? m_envelope->subject : QString();
    case Imap::Mailbox::RoleMessageFrom:
        return m_envelope ? Imap::Mailbox::TreeItemMessage::addresListToQVariant(m_envelope->from) : QVariant();
    case Imap::Mailbox::RoleMessageSender:
        return m_envelope ? Imap::Mailbox::TreeItemMessage::addresListToQVariant(m_envelope->sender) : QVariant();
    case Imap::Mailbox::RoleMessageReplyTo:
        return m_envelope ? Imap::Mailbox::TreeItemMessage::addresListToQVariant(m_envelope->replyTo) : QVariant();
    case Imap::Mailbox::RoleMessageTo:
        return m_envelope ? Imap::Mailbox::TreeItemMessage::addresListToQVariant(m_envelope->to) : QVariant();
    case Imap::Mailbox::RoleMessageCc:
        return m_envelope ? Imap::Mailbox::TreeItemMessage::addresListToQVariant(m_envelope->cc) : QVariant();
    case Imap::Mailbox::RoleMessageBcc:
        return m_envelope ? Imap::Mailbox::TreeItemMessage::addresListToQVariant(m_envelope->bcc) : QVariant();
    case Imap::Mailbox::RoleMessageMessageId:
        return m_envelope ? m_envelope->messageId : QByteArray();
    case Imap::Mailbox::RoleMessageInReplyTo:
        return m_envelope ? QVariant::fromValue(m_envelope->inReplyTo) : QVariant();
    case Imap::Mailbox::RoleMessageFlags:
        return QVariant();
    case Imap::Mailbox::RolePartBodyFldParam:
        return QVariant::fromValue<decltype(m_bodyFldParam)>(m_bodyFldParam);
    default:
        break;
    }

    return QVariant();
}

#ifdef MIME_TREE_DEBUG
QByteArray LocalMessagePart::dumpLocalInfo() const
{
    return "LocalMessagePart";
}
#endif


}
