/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

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

#include <mimetic/mimetic.h>
#include <QBrush>
#include <QFont>
#include "Common/InvokeMethod.h"
#include "Cryptography/LocalMimeParser.h"
#include "Cryptography/MessageModel.h"
#include "Cryptography/MimeticUtils.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

namespace Cryptography {

LocalMimeMessageParser::LocalMimeMessageParser()
    : PartReplacer()
{
}

LocalMimeMessageParser::~LocalMimeMessageParser()
{
}

MessagePart::Ptr LocalMimeMessageParser::createPart(MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                                    const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
{
    auto mimeType = sourceItemIndex.data(Imap::Mailbox::RolePartMimeType).toByteArray();
    if (mimeType == "message/rfc822") {
        return MessagePart::Ptr(new LocallyParsedMimePart(model, parentPart, std::move(original),
                                                          sourceItemIndex, proxyParentIndex));
    }
    return original;
}

LocallyParsedMimePart::LocallyParsedMimePart(MessageModel *model, MessagePart *parentPart, Ptr originalPart,
                                             const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : QObject(0)
    , LocalMessagePart(parentPart, originalPart->row(), sourceItemIndex.data(Imap::Mailbox::RolePartMimeType).toByteArray())
    , m_model(model)
    , m_sourceHeaderIndex(sourceItemIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_HEADER))
    , m_sourceTextIndex(sourceItemIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_TEXT))
    , m_proxyParentIndex(proxyParentIndex)
{
    Q_ASSERT(m_proxyParentIndex.isValid());
    Q_ASSERT(m_proxyParentIndex.model() == model);
    Q_ASSERT(m_sourceHeaderIndex.isValid());
    Q_ASSERT(m_sourceHeaderIndex.model() != model);
    Q_ASSERT(m_sourceTextIndex.isValid());
    Q_ASSERT(m_sourceTextIndex.model() != model);
    connect(m_sourceHeaderIndex.model(), &QAbstractItemModel::dataChanged, this, &LocallyParsedMimePart::messageMaybeAvailable);

    // request the data
    m_sourceHeaderIndex.data(Imap::Mailbox::RolePartData);
    m_sourceTextIndex.data(Imap::Mailbox::RolePartData);
    m_localState = FetchingState::LOADING;
    // ...and speculatively check if they're already there
    CALL_LATER(this, messageMaybeAvailable, Q_ARG(QModelIndex, m_sourceHeaderIndex), Q_ARG(QModelIndex, m_sourceHeaderIndex));
}

void LocallyParsedMimePart::messageMaybeAvailable(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_ASSERT(m_children.empty());
    Q_ASSERT(topLeft == bottomRight);
    Q_ASSERT(m_sourceHeaderIndex.isValid() == m_sourceTextIndex.isValid());

    if (!m_sourceHeaderIndex.isValid()) {
        if (auto qaim = qobject_cast<QAbstractItemModel *>(sender())) {
            disconnect(qaim, &QAbstractItemModel::dataChanged, this, &LocallyParsedMimePart::messageMaybeAvailable);
        }
        m_localState = FetchingState::UNAVAILABLE;
        return;
    }
    Q_ASSERT(m_proxyParentIndex.isValid());

    if (topLeft != m_sourceHeaderIndex && topLeft != m_sourceTextIndex) {
        return;
    }

    if (m_sourceHeaderIndex.data(Imap::Mailbox::RoleIsFetched).toBool() && m_sourceTextIndex.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        disconnect(m_sourceHeaderIndex.model(), &QAbstractItemModel::dataChanged, this, &LocallyParsedMimePart::messageMaybeAvailable);
        Q_ASSERT(m_localState == FetchingState::LOADING);

        // This part is rather fugly because we do not really have access to the full MIME plaintext of the "entire attachment",
        // including its *own* MIME headers such as Content-Type. In other words, the knowledge that the item's Content-Type
        // happens to be "message/rfc822" is communicated to us through an out-of-band channel, the IMAP's BODYSTRUCTURE.
        //
        // We're simply reconstructing this piece of information by adding a hand-crafted header.
        const QByteArray header = QByteArrayLiteral("Content-Type: ") + m_mimetype + QByteArrayLiteral("\r\n\r\n");
        const QByteArray data = header +
                m_sourceHeaderIndex.data(Imap::Mailbox::RolePartData).toByteArray() +
                m_sourceTextIndex.data(Imap::Mailbox::RolePartData).toByteArray();
        auto part = MimeticUtils::mimeEntityToPart(mimetic::MimeEntity(data.begin(), data.end()), this, row());
        auto rawPart = dynamic_cast<LocalMessagePart *>(part.get());
        Q_ASSERT(rawPart);

        // Do not store our artificial header, though!
        rawPart->setData(data.mid(header.length()));
        m_localState = FetchingState::DONE;

        m_model->replaceMeWithSubtree(m_proxyParentIndex, this, std::move(part));
        //m_model->insertSubtree(m_model->index(row(), 0, m_proxyParentIndex), std::move(part));
    }
}

QVariant LocallyParsedMimePart::data(int role) const
{
    switch (role) {
    case Qt::FontRole:
    {
        QFont f;
        f.setItalic(true);
        return f;
    }
    case Qt::BackgroundRole:
        return QBrush(QColor(0x80, 0x80, 0xff));
    default:
        return LocalMessagePart::data(role);
    }
}

#ifdef MIME_TREE_DEBUG
QByteArray LocallyParsedMimePart::dumpLocalInfo() const
{
    return "LocallyParsedMimePart";
}
#endif


}
