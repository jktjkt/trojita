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
    , m_keystoreManager(new QCA::KeyStoreManager(nullptr))
{
    // If the QCA::OpenPGP was initialized/destroyed within the OpenPGPEncryptedPart, the CI
    // hit some segfaults in Qt 5.2.1 and 5.4.0, see the comments at Gerrit

    const QString gpgKeystore = QStringLiteral("qca-gnupg");
    QCA::KeyStoreManager::start(gpgKeystore);
    m_keystoreWaiting = QObject::connect(m_keystoreManager.get(), &QCA::KeyStoreManager::keyStoreAvailable,
                                         [this, gpgKeystore](const QString &name){
        if (name != gpgKeystore) {
            return;
        }
        QObject::disconnect(m_keystoreWaiting);
        m_keystore = std::unique_ptr<QCA::KeyStore>(new QCA::KeyStore(gpgKeystore, m_keystoreManager.get()));
        m_keystore->startAsynchronousMode();
        m_keystore->entryList();
    });
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
            return MessagePart::Ptr(new OpenPGPEncryptedPart(m_pgp.get(), m_keystore.get(), model, parentPart, std::move(original),
                                                             sourceItemIndex, proxyParentIndex));
        }
    } else if (mimeType == "multipart/signed") {
        const auto bodyFldParam = sourceItemIndex.data(RolePartBodyFldParam).value<bodyFldParam_t>();
        if (bodyFldParam[QByteArray("PROTOCOL")].toLower() == QByteArrayLiteral("application/pgp-signature")) {
            return MessagePart::Ptr(new OpenPGPSignedPart(m_pgp.get(), m_keystore.get(), model, parentPart, std::move(original),
                                                          sourceItemIndex, proxyParentIndex));
        }
    }

    return original;
}

OpenPGPPart::OpenPGPPart(QCA::OpenPGP *pgp, QCA::KeyStore *keystore, MessageModel *model, MessagePart *parentPart,
                         const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : QObject(model)
    , LocalMessagePart(parentPart, sourceItemIndex.row(), sourceItemIndex.data(RolePartMimeType).toByteArray())
    , m_model(model)
    , m_pgp(pgp)
    , m_msg(nullptr)
    , m_keystore(keystore)
    , m_proxyParentIndex(proxyParentIndex)
    , m_waitingForData(true)
    , m_wasSigned(false)
    , m_isAllegedlyEncrypted(false)
    , m_statusTLDR(tr("Waiting for data..."))
    , m_statusIcon(QStringLiteral("clock"))
    , m_signatureOkDisregardingTrust(false)
    , m_signatureValidTrusted(false)
{
    Q_ASSERT(sourceItemIndex.isValid());
    m_dataChanged = connect(sourceItemIndex.model(), &QAbstractItemModel::dataChanged, this, &OpenPGPPart::handleDataChanged);
}

OpenPGPPart::~OpenPGPPart()
{
}

void OpenPGPPart::forwardFailure(const QString &statusTLDR, const QString &statusLong, const QString &statusIcon)
{
    disconnect(m_dataChanged);
    m_waitingForData = false;
    m_wasSigned = false;
    m_statusTLDR = statusTLDR;
    m_statusLong = statusLong;
    m_statusIcon = statusIcon;

    // This forward is needed because we migth be emitting this indirectly, from the item's constructor.
    // At the time the ctor runs, the multipart/encrypted has not been inserted into the proxy model yet,
    // so we cannot obtain its index.
    emit m_model->error(m_proxyParentIndex.child(m_row, 0), m_statusTLDR, m_statusLong);
    emitDataChanged();
}

void OpenPGPPart::emitDataChanged()
{
    QModelIndex idx = m_proxyParentIndex.child(m_row, 0);
    emit m_model->dataChanged(idx, idx);
}

void OpenPGPPart::updateSignedStatus()
{
    const QString iconEncVerified = QStringLiteral("emblem-success");
    const QString iconSignedVerified = QStringLiteral("emblem-success");
    const QString iconEncSignedUntrusted = QStringLiteral("emblem-encrypted-unlocked");
    const QString iconSignedUntrusted = QStringLiteral("emblem-information");
    const QString iconEncSignedBroken = QStringLiteral("emblem-warning");
    const QString iconSignedBroken = QStringLiteral("emblem-warning");
    const QString iconEncSignedUnknown = QStringLiteral("emblem-encrypted-unlocked");
    const QString iconSignedUnknown = QStringLiteral("emblem-information");

    m_wasSigned = m_msg->wasSigned();
    m_signatureOkDisregardingTrust = m_msg->verifySuccess();
    auto signer = m_msg->signer();
    m_signDate = signer.timestamp();

    // Extract the list of candidate e-mail addresses based on Sender and From headers.
    // This will be used to check if the signer and the author of the message are the same later on.
    // We're checking against the nearest parent message, so we support forwarding just fine.
    QModelIndex index = m_proxyParentIndex;
    std::vector<QString> messageIdentities;
    while (index.isValid()) {
        if (index.data(RolePartMimeType).toByteArray() == "message/rfc822") {
            auto envelopeVariant = index.data(RoleMessageEnvelope);
            Q_ASSERT(envelopeVariant.isValid());
            const auto envelope = envelopeVariant.value<Imap::Message::Envelope>();
            messageIdentities.reserve(envelope.from.size() + envelope.sender.size());
            auto storeSenderIdentity = [&messageIdentities](const Imap::Message::MailAddress &identity) {
                messageIdentities.emplace_back(QString::fromUtf8(identity.asSMTPMailbox()));
            };
            std::for_each(envelope.from.begin(), envelope.from.end(), storeSenderIdentity);
            std::for_each(envelope.sender.begin(), envelope.sender.end(), storeSenderIdentity);
            break;
        }
        index = index.parent();
    }

    if (m_wasSigned) {
        // Well, we cannot access something like signer.key().name() because the keys are in some semi-constructed state
        // and the list of userIds is empty, so this either asserts when trying to access an item[0] in an empty QList,
        // or segfaults later. This is obviously a bug in QCA; it is not documented anywhere.
        //
        // https://bugs.kde.org/show_bug.cgi?id=359464
        //
        // The key ID is safe, though.
        QString keyId = signer.key().pgpPublicKey().keyId();

        enum { Untrusted, SemiTrusted, Trusted } uidTrustLevel = Untrusted;
        bool identityMatches = false;

#define TROJITA_QCA_SHINY_TRUST_LEVEL_WHICH_DEPENDS_ON_UNRELEASED_QCA
#ifdef TROJITA_QCA_SHINY_TRUST_LEVEL_WHICH_DEPENDS_ON_UNRELEASED_QCA
        // this needs QCA patched with https://git.reviewboard.kde.org/r/127262/
        switch (signer.keyValidity()) {
        case QCA::Validity::ValidityGood: // TRUST_ULTIMATE, TRUST_FULLY
            uidTrustLevel = Trusted;
            break;
        case QCA::Validity::ErrorPathLengthExceeded: // TRUST_MARGINAL
            uidTrustLevel = SemiTrusted;
            break;
        case QCA::Validity::ErrorRejected: // TRUST_NEVER
        case QCA::Validity::ErrorInvalidCA: // TRUST_UNDEFINED
        default:
            break;
        }
#endif

        // Verify whether the key which signed the data matches someone from the message's headers
        if (m_keystore) {
            const auto &allKeys = m_keystore->entryList();
            auto matchingKey = std::find_if(allKeys.begin(), allKeys.end(), [keyId](const QCA::KeyStoreEntry &k) {
                return k.id() == keyId;
            });
            if (matchingKey != allKeys.end()) {
                // just indicate the signer's key name; the UID validity comes later
                m_signatureIdentityName = matchingKey->pgpPublicKey().primaryUserId();

                if (matchingKey->pgpPublicKey().isTrusted()) {
                    // QCA does this is for "my own keys", let's show a green tick on these
                    uidTrustLevel = Trusted;
                }

                // validate the UID
                for (const auto &userId: matchingKey->pgpPublicKey().userIds()) {
                    Imap::Message::MailAddress candidate;
                    bool ok = Imap::Message::MailAddress::fromPrettyString(candidate, userId);
                    if (!ok)
                        continue;
                    if (std::find(messageIdentities.begin(), messageIdentities.end(),
                                  QString::fromUtf8(candidate.asSMTPMailbox()))
                            != messageIdentities.end()) {
                        m_signatureIdentityName = userId;
                        identityMatches = true;
                        break;
                    }
                }
            }
        }

        m_signatureValidTrusted = m_signatureOkDisregardingTrust && identityMatches && uidTrustLevel >= SemiTrusted;
        QString reason;
        if (!m_keystore) {
            reason = tr("The PGP keystore is not available yet");
        } else if (!identityMatches) {
            reason = tr("Signer and author of the message are not the same");
        } else if (uidTrustLevel == Untrusted) {
            reason = tr("Key is not trusted");
        }

        switch (signer.identityResult()) {
        case QCA::SecureMessageSignature::Valid:
            if (m_signatureValidTrusted) {
                m_statusIcon = m_isAllegedlyEncrypted ? iconEncVerified : iconSignedVerified;
                m_statusTLDR = m_isAllegedlyEncrypted ?
                            tr("Encrypted, verified signature") :
                            tr("Verified signature");
                if (m_signatureIdentityName.isEmpty()) {
                    m_statusLong = tr("Signed by trusted key %1 on %2").arg(keyId, m_signDate.toString());
                } else {
                    m_statusLong = tr("Trusted signature by %1 (%2) on %3").arg(m_signatureIdentityName, keyId, m_signDate.toString());
                }
            } else if (!identityMatches && m_keystore) {
                // that's a good enough reason for screaming wildly
                m_statusIcon = m_isAllegedlyEncrypted ? iconEncSignedBroken : iconSignedBroken;
                m_statusTLDR = m_isAllegedlyEncrypted ?
                            tr("Encrypted, signature by a stranger") :
                            tr("Some signature by a stranger");
                m_statusLong = m_signatureIdentityName.isEmpty() ?
                            tr("Signed by a stranger, untrusted key %1 on %2; %3").arg(keyId, m_signDate.toString(), reason) :
                            tr("Untrusted signature by a stranger %1 (%2) on %3; %4").arg(m_signatureIdentityName, keyId, m_signDate.toString(), reason);
            } else {
                m_statusIcon = m_isAllegedlyEncrypted ? iconEncSignedUntrusted : iconSignedUntrusted;
                m_statusTLDR = m_isAllegedlyEncrypted ?
                            tr("Encrypted, some signature") :
                            tr("Some signature");
                m_statusLong = m_signatureIdentityName.isEmpty() ?
                            tr("Signed by untrusted key %1 on %2; %3").arg(keyId, m_signDate.toString(), reason) :
                            tr("Untrusted signature by %1 (%2) on %3; %4").arg(m_signatureIdentityName, keyId, m_signDate.toString(), reason);
            }
            break;
        case QCA::SecureMessageSignature::InvalidSignature:
            m_statusIcon = m_isAllegedlyEncrypted ? iconEncSignedBroken : iconSignedBroken;
            m_statusTLDR = m_isAllegedlyEncrypted ?
                        tr("Encrypted, broken signature") :
                        tr("Broken signature");
            m_statusLong = tr("Invalid signature by key %1").arg(keyId);
            break;
        case QCA::SecureMessageSignature::InvalidKey:
            m_statusIcon = m_isAllegedlyEncrypted ? iconEncSignedUnknown : iconSignedUnknown;
            m_statusTLDR = m_isAllegedlyEncrypted ?
                        tr("Encrypted, some signature") :
                        tr("Some signature");
            m_statusLong = tr("Invalid key for this signature");
            break;
        case QCA::SecureMessageSignature::NoKey:
            m_statusIcon = m_isAllegedlyEncrypted ? iconEncSignedUnknown : iconSignedUnknown;
            m_statusTLDR = m_isAllegedlyEncrypted ?
                        tr("Encrypted, some signature") :
                        tr("Some signature");
            m_statusLong = tr("Cannot verify signature -- key %1 is not available in the keyring").arg(keyId);
            break;
        }
    }
    emitDataChanged();
}

QVariant OpenPGPPart::data(int role) const
{
    using namespace Imap::Mailbox;
    switch (role) {
    case Imap::Mailbox::RolePartSignatureVerifySupported:
        return m_wasSigned;
    case RolePartCryptoNotFinishedYet:
        return m_waitingForData || !!m_msg;
    case RolePartCryptoTLDR:
        return m_statusTLDR;
    case RolePartCryptoDetailedMessage:
        return m_statusLong;
    case RolePartCryptoStatusIconName:
        return m_statusIcon;
    case Imap::Mailbox::RolePartSignatureValidTrusted:
        return m_wasSigned ? QVariant(m_signatureValidTrusted) : QVariant();
    case Imap::Mailbox::RolePartSignatureValidDisregardingTrust:
        return m_wasSigned ? QVariant(m_signatureOkDisregardingTrust) : QVariant();
    case RolePartSignatureSignerName:
        return m_signatureIdentityName;
    case RolePartSignatureSignDate:
        return m_signDate;
    }
    return LocalMessagePart::data(role);
}


OpenPGPEncryptedPart::OpenPGPEncryptedPart(QCA::OpenPGP *pgp, QCA::KeyStore *keystore, MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                           const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : OpenPGPPart(pgp, keystore, model, parentPart, sourceItemIndex, proxyParentIndex)
    , m_versionPart(sourceItemIndex.child(0, 0))
    , m_encPart(sourceItemIndex.child(1, 0))
    , m_decryptionSupported(false)
{
    m_isAllegedlyEncrypted = true;
    if (QCA::isSupported("openpgp")) {
        const auto rowCount = sourceItemIndex.model()->rowCount(sourceItemIndex);
        if (rowCount == 2) {
            // Trigger lazy loading of the required message parts
            m_versionPart.data(RolePartData);
            m_encPart.data(RolePartData);
            CALL_LATER(this, handleDataChanged, Q_ARG(QModelIndex, m_encPart), Q_ARG(QModelIndex, m_encPart));
        } else {
            CALL_LATER(this, forwardFailure, Q_ARG(QString, tr("Malformed Encrypted Message")),
                       Q_ARG(QString, tr("Expected 2 parts, but found %1.").arg(rowCount)),
                       Q_ARG(QString, QStringLiteral("emblem-error")));
        }
    } else {
        CALL_LATER(this, forwardFailure, Q_ARG(QString, tr("Cannot Decrypt")),
                   Q_ARG(QString, tr("OpenPGP Failure. QCA: Working with OpenPGP not supported. Is the OpenPGP plugin available?")),
                   Q_ARG(QString, QStringLiteral("script-error")));
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
        m_statusTLDR = tr("Encrypted message is gone");
        m_statusLong = QString();
        m_statusIcon = QStringLiteral("state-offline");
        emitDataChanged();
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
    m_waitingForData = false;

    // Check compliance with RFC3156
    QString versionString = m_versionPart.data(RolePartData).toString();
    if (!versionString.contains(QLatin1String("Version: 1"))) {
        forwardFailure(tr("Malformed Encrypted Message"),
                       tr("Unsupported PGP/MIME version. Expected \"Version: 1\", got \"%2\".").arg(versionString),
                       QStringLiteral("emblem-error"));
        return;
    }
    Q_ASSERT(!m_msg);
    m_msg = new QCA::SecureMessage(m_pgp);
    m_statusTLDR = tr("Decrypting...");
    connect(m_msg, &QCA::SecureMessage::finished, this, &OpenPGPEncryptedPart::handleDecryptionFinished);
    m_msg->setFormat(QCA::SecureMessage::Ascii);
    m_msg->startDecrypt();
    m_msg->update(m_encPart.data(RolePartData).toByteArray().data());
    m_msg->end();
    emitDataChanged();
}

void OpenPGPEncryptedPart::handleDecryptionFinished()
{
    Q_ASSERT(m_msg);
    if (!m_msg->success()) {
        forwardFailure(tr("Cannot decrypt: %1").arg(QCAHelper::qcaErrorStrings(m_msg->errorCode())),
                       m_msg->diagnosticText(), QStringLiteral("script-error"));
    } else if (!m_versionPart.isValid() || !m_encPart.isValid() || !m_proxyParentIndex.isValid()) {
        forwardFailure(tr("Encrypted message is gone"), QString(), QStringLiteral("state-offline"));
    } else {
        QByteArray message;
        while (m_msg->bytesAvailable()) {
            message.append(m_msg->read());
        }
        m_decryptionSupported = true;
        updateSignedStatus();
        if (!m_wasSigned) {
            m_statusIcon = QStringLiteral("emblem-encrypted-unlocked");
            m_statusTLDR = tr("Encrypted");
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
    emitDataChanged();
}

QVariant OpenPGPEncryptedPart::data(int role) const
{
    switch (role) {
    case Imap::Mailbox::RolePartDecryptionSupported:
        return m_decryptionSupported;
    default:
        return OpenPGPPart::data(role);
    }
}


OpenPGPSignedPart::OpenPGPSignedPart(QCA::OpenPGP *pgp, QCA::KeyStore *keystore, MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                     const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : OpenPGPPart(pgp, keystore, model, parentPart, sourceItemIndex, proxyParentIndex)
    , m_plaintextPart(sourceItemIndex.child(0, 0).child(0, TreeItem::OFFSET_RAW_CONTENTS))
    , m_plaintextMimePart(sourceItemIndex.child(0, 0).child(0, TreeItem::OFFSET_MIME))
    , m_signaturePart(sourceItemIndex.child(1, 0))
{
    m_wasSigned = true;
    Q_ASSERT(sourceItemIndex.child(0, 0).isValid());
    Q_ASSERT(m_plaintextPart.isValid());
    Q_ASSERT(m_signaturePart.isValid());
    if (QCA::isSupported("openpgp")) {
        const auto rowCount = sourceItemIndex.model()->rowCount(sourceItemIndex);
        if (rowCount == 2) {
            // Trigger lazy loading of the required message parts
            m_plaintextPart.data(RolePartData);
            m_plaintextMimePart.data(RolePartData);
            m_signaturePart.data(RolePartData);
            CALL_LATER(this, handleDataChanged, Q_ARG(QModelIndex, m_plaintextPart), Q_ARG(QModelIndex, m_plaintextPart));
        } else {
            CALL_LATER(this, forwardFailure, Q_ARG(QString, tr("Malformed Signed Message")),
                       Q_ARG(QString, tr("Expected 2 parts, but found %1.").arg(rowCount)),
                       Q_ARG(QString, QStringLiteral("emblem-error")));
        }
    } else {
        CALL_LATER(this, forwardFailure, Q_ARG(QString, tr("Cannot verify signature")),
                   Q_ARG(QString, tr("OpenPGP Failure. QCA: Working with OpenPGP not supported. Is the OpenPGP plugin available?")),
                   Q_ARG(QString, QStringLiteral("script-error")));
    }

    // we might change this in future; maybe having access to the OFFSET_RAW_CONTENTS makes some sense
    setSpecialParts(nullptr, nullptr, nullptr, nullptr);
}

OpenPGPSignedPart::~OpenPGPSignedPart()
{
}

void OpenPGPSignedPart::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_ASSERT(topLeft == bottomRight);
    if (!m_plaintextPart.isValid()) {
        forwardFailure(tr("Signed message is gone"), QString(), QStringLiteral("state-offline"));
        return;
    }
    if (topLeft != m_plaintextPart && topLeft != m_plaintextMimePart && topLeft != m_signaturePart) {
        return;
    }
    Q_ASSERT(m_plaintextPart.isValid());
    Q_ASSERT(m_plaintextMimePart.isValid());
    Q_ASSERT(m_signaturePart.isValid());
    if (!m_plaintextPart.data(RoleIsFetched).toBool() || !m_plaintextMimePart.data(RoleIsFetched).toBool() ||
            !m_signaturePart.data(RoleIsFetched).toBool()) {
        return;
    }

    // Now that we have the data, let's make the content of the message immediately visible.
    // There is no point in delaying this until the moment the signature gets checked.
    QByteArray rawData = m_plaintextMimePart.data(RolePartData).toByteArray() + m_plaintextPart.data(RolePartData).toByteArray();
    mimetic::MimeEntity me(rawData.begin(), rawData.end());
    auto idx = m_proxyParentIndex.child(m_row, 0);
    Q_ASSERT(idx.isValid());
    m_model->insertSubtree(idx, MimeticUtils::mimeEntityToPart(me, nullptr, 0));

    disconnect(m_dataChanged);
    m_waitingForData = false;

    Q_ASSERT(!m_msg);
    m_msg = new QCA::SecureMessage(m_pgp);
    m_statusTLDR = tr("Verifying signature...");
    connect(m_msg, &QCA::SecureMessage::finished, this, &OpenPGPSignedPart::handleVerifyFinished);
    m_msg->setFormat(QCA::SecureMessage::Ascii);
    m_msg->startVerify(m_signaturePart.data(RolePartData).toByteArray());
    m_msg->update(m_plaintextMimePart.data(RolePartData).toByteArray().data());
    m_msg->update(m_plaintextPart.data(RolePartData).toByteArray().data());
    m_msg->end();
    emitDataChanged();
}

void OpenPGPSignedPart::handleVerifyFinished()
{
    Q_ASSERT(m_msg);
    if (!m_msg->success()) {
        forwardFailure(tr("Cannot verify signature: %1").arg(QCAHelper::qcaErrorStrings(m_msg->errorCode())),
                       m_msg->diagnosticText(), QStringLiteral("script-error"));
    } else if (!m_plaintextPart.isValid() || !m_signaturePart.isValid() || !m_proxyParentIndex.isValid()) {
        forwardFailure(tr("Signed message is gone"), QString(), QStringLiteral("state-offline"));
    } else {
        updateSignedStatus();
    }
    delete m_msg;
    m_msg = nullptr;
    emitDataChanged();
}

}
