/* Copyright (C) 2014-2015 Stephan Platz <trojita@paalsteek.de>
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

#include <mimetic/mimetic.h>
#include <QtCrypto>
#include "Common/InvokeMethod.h"
#include "Cryptography/MessagePart.h"
#include "Cryptography/MessageModel.h"
#include "Cryptography/MimeticUtils.h"
#include "Cryptography/OpenPGPHelper.h"
#include "Cryptography/QCAHelper.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

using namespace Imap::Mailbox;

namespace Cryptography {

OpenPGPReplacer::OpenPGPReplacer()
    : PartReplacer()
    , m_qcaInit(new QCA::Initializer())
    , m_pgp(new QCA::OpenPGP(nullptr))
{
    // If the QCA::OpenPGP was initialized/destroyed within the OpenPGPEncryptedPart, the CI
    // hit some segfaults in Qt 5.2.1 and 5.4.0, see the comments at Gerrit
}

OpenPGPReplacer::~OpenPGPReplacer()
{
}

MessagePart::Ptr OpenPGPReplacer::createPart(MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                             const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
{
    // Just - say - wow, because this is for sure much easier then typing QMap<QByteArray, QByteArray>, isn't it.
    // Yep, I just got a new toy.
    using bodyFldParam_t = std::result_of<decltype(&TreeItemPart::bodyFldParam)(TreeItemPart)>::type;

    auto mimeType = sourceItemIndex.data(RolePartMimeType).toByteArray();
    if (mimeType == "multipart/encrypted") {
        const auto bodyFldParam = sourceItemIndex.data(RolePartBodyFldParam).value<bodyFldParam_t>();
        if (bodyFldParam[QByteArrayLiteral("PROTOCOL")].toLower() == QByteArrayLiteral("application/pgp-encrypted")) {
            return MessagePart::Ptr(new OpenPGPEncryptedPart(m_pgp.get(), model, parentPart, std::move(original),
                                                             sourceItemIndex, proxyParentIndex));
        }
    }

    return original;
}

OpenPGPPart::OpenPGPPart(QCA::OpenPGP *pgp, MessageModel *model, MessagePart *parentPart,
                         const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : QObject(model)
    , LocalMessagePart(parentPart, sourceItemIndex.row(), sourceItemIndex.data(RolePartMimeType).toByteArray())
    , m_model(model)
    , m_pgp(pgp)
    , m_msg(nullptr)
    , m_proxyParentIndex(proxyParentIndex)
{
    Q_ASSERT(sourceItemIndex.isValid());
    m_dataChanged = connect(sourceItemIndex.model(), &QAbstractItemModel::dataChanged, this, &OpenPGPPart::handleDataChanged);
}

OpenPGPPart::~OpenPGPPart()
{
}

void OpenPGPPart::forwardFailure(const QString &error, const QString &details)
{
    disconnect(m_dataChanged);

    // This forward is needed because we are emitting this indirectly, from the item's constructor.
    // At the time the ctor runs, the multipart/encrypted has not been inserted into the proxy model yet,
    // so we cannot obtain its index.
    emit m_model->error(m_proxyParentIndex.child(m_row, 0), error, details);
}


OpenPGPEncryptedPart::OpenPGPEncryptedPart(QCA::OpenPGP *pgp, MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                           const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : OpenPGPPart(pgp, model, parentPart, sourceItemIndex, proxyParentIndex)
    , m_versionPart(sourceItemIndex.child(0, 0))
    , m_encPart(sourceItemIndex.child(1, 0))
{
    if (QCA::isSupported("openpgp")) {
        const auto rowCount = sourceItemIndex.model()->rowCount(sourceItemIndex);
        if (rowCount == 2) {
            // Trigger lazy loading of the required message parts
            m_versionPart.data(RolePartData);
            m_encPart.data(RolePartData);
            CALL_LATER(this, handleDataChanged, Q_ARG(QModelIndex, m_encPart), Q_ARG(QModelIndex, m_encPart));
        } else {
            CALL_LATER(this, forwardFailure, Q_ARG(QString, tr("Malformed Message")),
                       Q_ARG(QString, tr("Expected 2 parts, but found %1.").arg(rowCount)));
        }
    } else {
        CALL_LATER(this, forwardFailure, Q_ARG(QString, tr("OpenPGP Failure")),
                   Q_ARG(QString, tr("QCA: Working with OpenPGP not supported. Is the OpenPGP plugin available?")));
    }

    auto oldState = m_localState;
    // the raw data are available on the part itself
    setData(original->data(Imap::Mailbox::RolePartData).toByteArray());
    m_localState = oldState;
    // we might change this in future; maybe having access to the OFFSET_RAW_CONTENTS makes some sense
    setSpecialParts(nullptr, nullptr, nullptr, nullptr);
}

OpenPGPEncryptedPart::~OpenPGPEncryptedPart()
{
}

void OpenPGPEncryptedPart::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_ASSERT(topLeft == bottomRight);
    if (!m_encPart.isValid()) {
        forwardFailure(tr("Message is gone"), QString());
        return;
    }
    if (topLeft != m_versionPart && topLeft != m_encPart) {
        return;
    }
    Q_ASSERT(m_versionPart.isValid());
    Q_ASSERT(m_encPart.isValid());
    if (!m_versionPart.data(RoleIsFetched).toBool() || !m_encPart.data(RoleIsFetched).toBool()) {
        return;
    }

    disconnect(m_dataChanged);

    // Check compliance with RFC3156
    QString versionString = m_versionPart.data(RolePartData).toString();
    if (!versionString.contains(QLatin1String("Version: 1"))) {
        forwardFailure(tr("Malformed Message"),
                       tr("Unsupported PGP/MIME version. Expected \"Version: 1\", got \"%2\".").arg(versionString));
        return;
    }
    Q_ASSERT(!m_msg);
    m_msg = new QCA::SecureMessage(m_pgp);
    connect(m_msg, &QCA::SecureMessage::finished, this, &OpenPGPEncryptedPart::handleDecryptionFinished);
    m_msg->setFormat(QCA::SecureMessage::Ascii);
    m_msg->startDecrypt();
    m_msg->update(m_encPart.data(RolePartData).toByteArray().data());
    m_msg->end();
}

void OpenPGPEncryptedPart::handleDecryptionFinished()
{
    Q_ASSERT(m_msg);
    if (!m_msg->success()) {
        forwardFailure(QCAHelper::qcaErrorStrings(m_msg->errorCode()), m_msg->diagnosticText());
    } else if (!m_versionPart.isValid() || !m_encPart.isValid() || !m_proxyParentIndex.isValid()) {
        forwardFailure(tr("Message is gone"), QString());
    } else {
        QByteArray message;
        while (m_msg->bytesAvailable()) {
            message.append(m_msg->read());
        }
        // FIXME: it would be nice to extract more information from GnuPG about this encrypted message,
        // but QCA doesn't really provide *anything* of value in this context.
        // Sounds like patching time, I guess.
        mimetic::MimeEntity me(message.begin(), message.end());
        auto idx = m_proxyParentIndex.child(m_row, 0);
        Q_ASSERT(idx.isValid());
        m_model->insertSubtree(idx, MimeticUtils::mimeEntityToPart(me, nullptr, 0));
    }
    delete m_msg;
    m_msg = nullptr;
}

}
