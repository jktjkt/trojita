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

#include <cstring>
#include <future>
#include <mimetic/mimetic.h>
#include <gpgme++/context.h>
#include <gpgme++/data.h>
#include <gpgme++/decryptionresult.h>
#include <gpgme++/key.h>
#include <gpgme++/interfaces/progressprovider.h>
#include <qgpgme/dataprovider.h>
#include <QElapsedTimer>
#include "Common/InvokeMethod.h"
#include "Cryptography/GpgMe++.h"
#include "Cryptography/MessagePart.h"
#include "Cryptography/MessageModel.h"
#include "Cryptography/MimeticUtils.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

using namespace Imap::Mailbox;

namespace {

#if 0
QByteArray getAuditLog(GpgME::Context *ctx)
{
    QByteArray buf;
    buf.reserve(666);
    GpgME::Data bufData(buf.data_ptr()->data(), buf.capacity(), false);
    auto err = ctx->getAuditLog(bufData);
    if (err) {
        return Cryptography::GpgMePart::tr("[getAuditLog failed: %1]").arg(QString::fromUtf8(err.asString())).toUtf8();
    }
    return buf;
}
#endif

bool is_running(const std::future<void> &future)
{
    return future.wait_for(std::chrono::duration_values<std::chrono::seconds>::zero()) == std::future_status::timeout;
}

}

namespace Cryptography {

QString protocolToString(const Protocol protocol)
{
    switch (protocol) {
    case Protocol::OpenPGP:
        return QStringLiteral("OpenPGP");
    case Protocol::SMime:
        return QStringLiteral("S/MIME");
    }
    Q_UNREACHABLE();
}

GpgMeReplacer::GpgMeReplacer()
    : PartReplacer()
{
    GpgME::initializeLibrary();
    qRegisterMetaType<SignatureDataBundle>();
}

GpgMeReplacer::~GpgMeReplacer()
{
    if (!m_orphans.empty()) {
        QElapsedTimer t;
        t.start();
        qDebug() << "Cleaning" << m_orphans.size() << "orphaned crypto task: ";
        for (auto &task: m_orphans) {
            if (is_running(task)) {
                qDebug() << " [waiting]";
                task.get();
            } else {
                qDebug() << " [already completed]";
            }
        }
        qDebug() << " ...finished after" << t.elapsed() << "ms.";
    }
}

MessagePart::Ptr GpgMeReplacer::createPart(MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                             const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
{
    // Just - say - wow, because this is for sure much easier then typing QMap<QByteArray, QByteArray>, isn't it.
    // Yep, I just got a new toy.
    using bodyFldParam_t = std::result_of<decltype(&TreeItemPart::bodyFldParam)(TreeItemPart)>::type;

    auto mimeType = sourceItemIndex.data(RolePartMimeType).toByteArray();
    if (mimeType == "multipart/encrypted") {
        const auto bodyFldParam = sourceItemIndex.data(RolePartBodyFldParam).value<bodyFldParam_t>();
        if (bodyFldParam[QByteArrayLiteral("PROTOCOL")].toLower() == QByteArrayLiteral("application/pgp-encrypted")) {
            return MessagePart::Ptr(new GpgMeEncrypted(Protocol::OpenPGP, this, model, parentPart, std::move(original),
                                                       sourceItemIndex, proxyParentIndex));
        }
    } else if (mimeType == "multipart/signed") {
        const auto bodyFldParam = sourceItemIndex.data(RolePartBodyFldParam).value<bodyFldParam_t>();
        auto protocol = bodyFldParam[QByteArray("PROTOCOL")].toLower();
        if (protocol == QByteArrayLiteral("application/pgp-signature")) {
            return MessagePart::Ptr(new GpgMeSigned(Protocol::OpenPGP, this, model, parentPart, std::move(original),
                                                    sourceItemIndex, proxyParentIndex));
        } else if (protocol == QByteArrayLiteral("application/pkcs7-signature")
                   || protocol == QByteArrayLiteral("application/x-pkcs7-signature")) {
            return MessagePart::Ptr(new GpgMeSigned(Protocol::SMime, this, model, parentPart, std::move(original),
                                                    sourceItemIndex, proxyParentIndex));
        }
    } else if (mimeType == "application/pkcs7-mime" || mimeType == "application/x-pkcs7-mime") {
        return MessagePart::Ptr(new GpgMeEncrypted(Protocol::SMime, this, model, parentPart, std::move(original),
                                                   sourceItemIndex, proxyParentIndex));
    }

    return original;
}

void GpgMeReplacer::registerOrhpanedCryptoTask(std::future<void> task)
{
    auto it = m_orphans.begin();
    while (it != m_orphans.end()) {
        if (it->valid() && is_running(*it)) {
            ++it;
        } else {
            qDebug() << "[cleaning an already-finished crypto orphan]";
            it = m_orphans.erase(it);
        }
    }
    if (task.valid() && is_running(task)) {
        m_orphans.emplace_back(std::move(task));
    }
    if (!m_orphans.empty()) {
        qDebug() << "We have" << m_orphans.size() << "orphaned crypto tasks";
    }
}

GpgMePart::GpgMePart(const Protocol protocol, GpgMeReplacer *replacer, MessageModel *model, MessagePart *parentPart,
                     const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : QObject(model)
    , LocalMessagePart(parentPart, sourceItemIndex.row(), sourceItemIndex.data(RolePartMimeType).toByteArray())
    , m_replacer(replacer)
    , m_model(model)
    , m_sourceIndex(sourceItemIndex)
    , m_proxyParentIndex(proxyParentIndex)
    , m_waitingForData(false)
    , m_wasSigned(false)
    , m_isAllegedlyEncrypted(false)
    , m_signatureOkDisregardingTrust(false)
    , m_signatureValidVerifiedTrusted(false)
    , m_statusTLDR(tr("Waiting for data..."))
    , m_statusIcon(QStringLiteral("clock"))
    , m_ctx(
          protocol == Protocol::OpenPGP ?
              (std::shared_ptr<GpgME::Context>(GpgME::checkEngine(GpgME::OpenPGP) ?
                                                   nullptr
                                                 : GpgME::Context::createForProtocol(GpgME::OpenPGP)))
            : (protocol == Protocol::SMime ?
                   (std::shared_ptr<GpgME::Context>(GpgME::checkEngine(GpgME::CMS) ?
                                                        nullptr
                                                      : GpgME::Context::createForProtocol(GpgME::CMS)))
                 : nullptr)
          )
{
    Q_ASSERT(sourceItemIndex.isValid());

    // Find "an email" which encloses the current part.
    // This will be used later for figuring our whether "the sender" and the key which signed this part correspond to each other.
    QModelIndex index = m_proxyParentIndex;
    while (index.isValid()) {
        if (index.data(RolePartMimeType).toByteArray() == "message/rfc822") {
            m_enclosingMessage = index;
            break;
        } else {
            index = index.parent();
        }
    }

    // do the periodic cleanup
    m_replacer->registerOrhpanedCryptoTask(std::future<void>());
}

GpgMePart::~GpgMePart()
{
    if (m_ctx && m_crypto.valid()) {
        // this is documented to be thread safe at all times
        m_ctx->cancelPendingOperation();

        // Because std::future's destructor blocks/joins, we have a problem if the operation that is running
        // in the std::async gets stuck for some reason. If we make no additional precautions, the GUI would freeze
        // at the destruction time, which means that everything will run "smoothly" (with the GUI remaining responsive)
        // until the time we move to another message -- and that's quite nasty surprise.
        //
        // To solve this thing, we send this background operation to a central registry in this destructor. Every now
        // and then, that registry is checked and those which have already finished are reaped. This happens whenever
        // a new crypto operation is started.
        m_replacer->registerOrhpanedCryptoTask(std::move(m_crypto));
    }
}

/** @short This slot is typically invoked from another thread, but we're always running in the "correct one" */
void GpgMePart::internalUpdateState(const SignatureDataBundle &d)
{
    m_wasSigned = d.wasSigned;
    m_signatureOkDisregardingTrust = d.isValidDisregardingTrust;
    m_signatureValidVerifiedTrusted = d.isValidTrusted;
    m_statusTLDR = d.tldrStatus;
    m_statusLong = d.longStatus;
    m_statusIcon = d.statusIcon;
    m_signatureIdentityName = d.signatureUid;
    m_signDate = d.signatureDate;
    m_crypto.get();
    emitDataChanged();
}

void GpgMePart::forwardFailure(const QString &statusTLDR, const QString &statusLong, const QString &statusIcon)
{
    disconnect(m_dataChanged);
    m_waitingForData = false;
    m_wasSigned = false;
    m_statusTLDR = statusTLDR;
    m_statusLong = statusLong;
    m_statusIcon = statusIcon;

    if (m_sourceIndex.isValid()) {
        std::vector<MessagePart::Ptr> children;
        for (int i = 0; i < m_sourceIndex.model()->rowCount(m_sourceIndex); ++i) {
            children.emplace_back(MessagePart::Ptr(new ProxyMessagePart(nullptr, 0, m_sourceIndex.child(i, 0), m_model)));
        }
        // This has to happen prior to emitting error()
        m_model->insertSubtree(m_proxyParentIndex.child(m_row, 0), std::move(children));
    }

    // This forward is needed because we migth be emitting this indirectly, from the item's constructor.
    // At the time the ctor runs, the multipart/encrypted has not been inserted into the proxy model yet,
    // so we cannot obtain its index.
    emit m_model->error(m_proxyParentIndex.child(m_row, 0), m_statusTLDR, m_statusLong);
    emitDataChanged();
}

void GpgMePart::emitDataChanged()
{
    auto idx = m_proxyParentIndex.child(m_row, 0);
    emit m_model->dataChanged(idx, idx);
}

QVariant GpgMePart::data(int role) const
{
    switch (role) {
    case Imap::Mailbox::RolePartSignatureVerifySupported:
        return m_wasSigned;
    case RolePartCryptoNotFinishedYet:
        return m_waitingForData ||
                (m_crypto.valid() &&
                 m_crypto.wait_for(std::chrono::duration_values<std::chrono::seconds>::zero()) == std::future_status::timeout);
    case RolePartCryptoTLDR:
        return m_statusTLDR;
    case RolePartCryptoDetailedMessage:
        return m_statusLong;
    case RolePartCryptoStatusIconName:
        return m_statusIcon;
    case Imap::Mailbox::RolePartSignatureValidTrusted:
        return m_wasSigned ? QVariant(m_signatureValidVerifiedTrusted) : QVariant();
    case Imap::Mailbox::RolePartSignatureValidDisregardingTrust:
        return m_wasSigned ? QVariant(m_signatureOkDisregardingTrust) : QVariant();
    case RolePartSignatureSignerName:
        return m_signatureIdentityName;
    case RolePartSignatureSignDate:
        return m_signDate;
    default:
        return LocalMessagePart::data(role);
    }
}

void GpgMePart::extractSignatureStatus(std::shared_ptr<GpgME::Context> ctx, const GpgME::Signature &sig,
                                       const std::vector<std::string> messageUids, const bool wasSigned, const bool wasEncrypted,
                                       bool &sigOkDisregardingTrust, bool &sigValidVerified,
                                       bool &uidMatched, QString &tldr, QString &longStatus, QString &icon, QString &signer, QDateTime &signDate)
{
    Q_UNUSED(wasSigned);

#if 0
    qDebug() << "signature summary" << sig.summary() << "status err code" << sig.status() << sig.status().asString()
             << "validity" << sig.validity() << "fingerprint" << sig.fingerprint();
#endif

    if (sig.summary() & GpgME::Signature::KeyMissing) {
        // that's right, there won't be any Green or Red labeling from GpgME; is we don't have the key, we cannot
        // do anything, period.
        tldr = wasEncrypted ? tr("Encrypted; some signature: missing key") : tr("Some signature: missing key");
        longStatus = tr("Key %1 is not available in the keyring.\n"
                        "Cannot verify signature validity or do anything else. "
                        "The message might or might not have been tampered with.")
                .arg(QString::fromUtf8(sig.fingerprint()));
        icon = QStringLiteral("emblem-information");
        return;
    }

    GpgME::Error keyError;
    auto key = ctx->key(sig.fingerprint(), keyError, false);
    if (keyError) {
        tldr = tr("Internal error");
        longStatus = tr("Error when verifying signature: cannot retrieve key %1: %2")
                .arg(QString::fromUtf8(sig.fingerprint()), QString::fromUtf8(keyError.asString()));
        icon = QStringLiteral("script-error");
        return;
    }

    const auto &uids = key.userIDs();
    auto needle = std::find_if(uids.begin(), uids.end(), [&messageUids](const GpgME::UserID &uid) {
        std::string email = uid.email();
        if (email.empty())
            return false;
        if (email[0] == '<' && email[email.size() - 1] == '>') {
            // this happens in the CMS, so let's kill the wrapping
            email = email.substr(1, email.size() - 2);
        }
        return std::find(messageUids.begin(), messageUids.end(), email) != messageUids.end();
    });

    if (needle != uids.end()) {
        uidMatched = true;
        switch (ctx->protocol()) {
        case GpgME::Protocol::CMS:
            signer = QString::fromUtf8(key.userID(0).id());
            break;
        case GpgME::Protocol::OpenPGP:
            signer = QStringLiteral("%1 (%2)").arg(QString::fromUtf8(needle->id()), QString::fromUtf8(sig.fingerprint()));
            break;
        case GpgME::Protocol::UnknownProtocol:
            Q_ASSERT(0);
        }
    } else if (!uids.empty()) {
        switch (ctx->protocol()) {
        case GpgME::Protocol::CMS:
            signer = QString::fromUtf8(key.userID(0).id());
            break;
        case GpgME::Protocol::OpenPGP:
            signer = QStringLiteral("%1 (%2)").arg(QString::fromUtf8(key.userID(0).id()), QString::fromUtf8(sig.fingerprint()));
            break;
        case GpgME::Protocol::UnknownProtocol:
            Q_ASSERT(0);
        }
    } else {
        signer = QString::fromUtf8(sig.fingerprint());
    }
    signDate = QDateTime::fromTime_t(sig.creationTime());

    if (sig.summary() & GpgME::Signature::Green) {
        // FIXME: change the above to GpgME::Signature::Valid and react to expired keys/signatures by checking the timestamp
        sigOkDisregardingTrust = true;
        if (uidMatched) {
            tldr = wasEncrypted ? tr("Encrypted, verified signature") : tr("Verified signature");
            longStatus = tr("Verified signature from %1 on %2")
                    .arg(signer, signDate.toString(Qt::DefaultLocaleShortDate));
            icon = QStringLiteral("emblem-success");
            sigValidVerified = true;
        } else {
            tldr = wasEncrypted ? tr("Encrypted, signed by stranger") : tr("Signed by stranger");
            longStatus = tr("Verified signature, but the signer is someone else:\n%1\nSignature was made on %2.")
                    .arg(signer, signDate.toString(Qt::DefaultLocaleShortDate));
            icon = QStringLiteral("emblem-warning");
        }
    } else if (sig.summary() & GpgME::Signature::Red) {
        if (uidMatched) {
            if (sig.status().code() == GPG_ERR_BAD_SIGNATURE) {
                tldr = wasEncrypted ? tr("Encrypted, bad signature") : tr("Bad signature");
            } else {
                tldr = wasEncrypted ?
                            tr("Encrypted, bad signature: %1").arg(QString::fromUtf8(sig.status().asString()))
                          : tr("Bad signature: %1").arg(QString::fromUtf8(sig.status().asString()));
            }
            longStatus = tr("Bad signature by %1 on %2").arg(signer, signDate.toString(Qt::DefaultLocaleShortDate));
        } else {
            if (sig.status().code() == GPG_ERR_BAD_SIGNATURE) {
                tldr = wasEncrypted ? tr("Encrypted, bad signature by stranger") : tr("Bad signature by stranger");
            } else {
                tldr = wasEncrypted ?
                            tr("Encrypted, bad signature by stranger: %1").arg(QString::fromUtf8(sig.status().asString()))
                          : tr("Bad signature by stranger: %1").arg(QString::fromUtf8(sig.status().asString()));
            }
            longStatus = tr("Bad signature by someone else: %1 on %2.")
                    .arg(signer, signDate.toString(Qt::DefaultLocaleShortDate));
        }
        icon = QStringLiteral("emblem-error");
    } else {
        switch (sig.validity()) {
        case GpgME::Signature::Full:
        case GpgME::Signature::Ultimate:
        case GpgME::Signature::Never:
            Q_ASSERT(false);
            // these are handled by GpgME by setting the appropriate Red/Green flags
            break;
        case GpgME::Signature::Unknown:
        case GpgME::Signature::Undefined:
            sigOkDisregardingTrust = true;
            if (uidMatched) {
                tldr = wasEncrypted ? tr("Encrypted, some signature") : tr("Some signature");
                longStatus = tr("Unknown signature from %1 on %2")
                        .arg(signer, signDate.toString(Qt::DefaultLocaleShortDate));
                icon = wasEncrypted ? QStringLiteral("emblem-encrypted-unlocked") : QStringLiteral("emblem-information");
            } else {
                tldr = wasEncrypted ? tr("Encrypted, some signature by stranger") : tr("Some signature by stranger");
                longStatus = tr("Unknown signature by somebody else: %1 on %2")
                        .arg(signer, signDate.toString(Qt::DefaultLocaleShortDate));
                icon = QStringLiteral("emblem-warning");
            }
            break;
        case GpgME::Signature::Marginal:
            sigOkDisregardingTrust = true;
            if (uidMatched) {
                tldr = wasEncrypted ? tr("Encrypted, semi-trusted signature") : tr("Semi-trusted signature");
                longStatus = tr("Semi-trusted signature from %1 on %2")
                        .arg(signer, signDate.toString(Qt::DefaultLocaleShortDate));
                icon = wasEncrypted ? QStringLiteral("emblem-encrypted-unlocked") : QStringLiteral("emblem-information");
            } else {
                tldr = wasEncrypted ?
                            tr("Encrypted, semi-trusted signature by stranger")
                          : tr("Semi-trusted signature by stranger");
                longStatus = tr("Semi-trusted signature by somebody else: %1 on %2")
                        .arg(signer, signDate.toString(Qt::DefaultLocaleShortDate));
                icon = QStringLiteral("emblem-warning");
            }
            break;
        }
    }

    const auto LF = QLatin1Char('\n');

    // extract the individual error bits
    if (sig.summary() & GpgME::Signature::KeyRevoked) {
        longStatus += LF + tr("The key or at least one certificate has been revoked.");
    }
    if (sig.summary() & GpgME::Signature::KeyExpired) {
        // FIXME: how to get the expiration date?
        longStatus += LF + tr("The key or one of the certificates has expired.");
    }
    if (sig.summary() & GpgME::Signature::SigExpired) {
        longStatus += LF + tr("Signature expired on %1.")
                .arg(QDateTime::fromTime_t(sig.expirationTime()).toString(Qt::DefaultLocaleShortDate));
    }
    if (sig.summary() & GpgME::Signature::KeyMissing) {
        longStatus += LF + tr("Can't verify due to a missing key or certificate.");
    }
    if (sig.summary() & GpgME::Signature::CrlMissing) {
        longStatus += LF + tr("The CRL (or an equivalent mechanism) is not available.");
    }
    if (sig.summary() & GpgME::Signature::CrlTooOld) {
        longStatus += LF + tr("Available CRL is too old.");
    }
    if (sig.summary() & GpgME::Signature::BadPolicy) {
        longStatus += LF + tr("A policy requirement was not met.");
    }
    if (sig.summary() & GpgME::Signature::SysError) {
        longStatus += LF + tr("A system error occurred. %1")
                .arg(QString::fromUtf8(sig.status().asString()));
    }

    if (sig.summary() & GpgME::Signature::Valid) {
        // Extract signature validity
        switch (sig.validity()) {
        case GpgME::Signature::Undefined:
            longStatus += LF + tr("Signature validity is undefined.");
            break;
        case GpgME::Signature::Never:
            longStatus += LF + tr("Signature validity is never to be trusted.");
            break;
        case GpgME::Signature::Marginal:
            longStatus += LF + tr("Signature validity is marginal.");
            break;
        case GpgME::Signature::Full:
            longStatus += LF + tr("Signature validity is full.");
            break;
        case GpgME::Signature::Ultimate:
            longStatus += LF + tr("Signature validity is ultimate.");
            break;
        case GpgME::Signature::Unknown:
            longStatus += LF + tr("Signature validity is unknown.");
            break;
        }
    }

    // Show the certificate chain -- which only applies to S/MIME cryptography
    if (ctx->protocol() == GpgME::Protocol::CMS) {
        auto parentCert = key;
        int depth = 1;

        longStatus += LF + LF + tr("Trust chain:");
        while (!parentCert.isNull()) {
            QString indent(depth, QLatin1Char(' '));
            const char *chainId = parentCert.chainID();
            const char *primaryFp = parentCert.primaryFingerprint();
            if (!chainId || !primaryFp) {
                longStatus += LF + tr("%1(Unavailable)").arg(indent);
                break;
            } else if (std::strcmp(chainId, primaryFp) == 0) {
                // a self-signed cert -> break
                longStatus += LF + tr("%1(self-signed)").arg(indent);
                break;
            }
            longStatus += LF + QStringLiteral("%1%2 (%3)").arg(indent,
                                                               QString::fromUtf8(parentCert.issuerName()),
                                                               QString::fromUtf8(parentCert.primaryFingerprint()));
            ++depth;
            parentCert = ctx->key(parentCert.chainID(), keyError, false);
            if (keyError) {
                longStatus += LF + tr("Error when retrieving key for the trust chain: %1")
                        .arg(QString::fromUtf8(keyError.asString()));
                break;
            }
        }

    }

#if 0
    // this always shows "Success" in my limited testing, so...
    longStatus += LF + tr("Signature invalidity reason: %1")
            .arg(QString::fromUtf8(sig.nonValidityReason().asString()));
#endif
}

void GpgMePart::submitVerifyResult(QPointer<QObject> p, const SignatureDataBundle &data)
{
    if (p) {
        bool ok = QMetaObject::invokeMethod(p, "internalUpdateState", Qt::QueuedConnection,
                                            // must use full namespace qualification
                                            Q_ARG(Cryptography::SignatureDataBundle, data));
        Q_ASSERT(ok); Q_UNUSED(ok);
    } else {
        qDebug() << "[async crypto: GpgMePart is gone, not doing anything]";
    }
}

std::vector<std::string> GpgMePart::extractMessageUids()
{
    // Extract the list of candidate e-mail addresses based on Sender and From headers.
    // This will be used to check if the signer and the author of the message are the same later on.
    // We're checking against the nearest parent message, so we support forwarding just fine.

    Q_ASSERT(m_enclosingMessage.isValid());
    std::vector<std::string> messageUids;
    auto envelopeVariant = m_enclosingMessage.data(RoleMessageEnvelope);
    Q_ASSERT(envelopeVariant.isValid());
    const auto envelope = envelopeVariant.value<Imap::Message::Envelope>();
    messageUids.reserve(envelope.from.size() + envelope.sender.size());
    auto storeSenderUid = [&messageUids](const Imap::Message::MailAddress &identity) {
        messageUids.emplace_back(identity.asSMTPMailbox().data());
    };
    std::for_each(envelope.from.begin(), envelope.from.end(), storeSenderUid);
    std::for_each(envelope.sender.begin(), envelope.sender.end(), storeSenderUid);
    return messageUids;
}



GpgMeSigned::GpgMeSigned(const Protocol protocol, GpgMeReplacer *replacer, MessageModel *model, MessagePart *parentPart, Ptr original,
                         const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : GpgMePart(protocol, replacer, model, parentPart, sourceItemIndex, proxyParentIndex)
    , m_plaintextPart(sourceItemIndex.child(0, 0).child(0, TreeItem::OFFSET_RAW_CONTENTS))
    , m_plaintextMimePart(sourceItemIndex.child(0, 0).child(0, TreeItem::OFFSET_MIME))
    , m_signaturePart(sourceItemIndex.child(1, 0))
{
    m_wasSigned = true;
    Q_ASSERT(sourceItemIndex.child(0, 0).isValid());

    if (m_ctx) {
        const auto rowCount = sourceItemIndex.model()->rowCount(sourceItemIndex);
        if (rowCount == 2) {
            Q_ASSERT(m_plaintextPart.isValid());
            Q_ASSERT(m_signaturePart.isValid());
            m_dataChanged = connect(sourceItemIndex.model(), &QAbstractItemModel::dataChanged, this, &GpgMeSigned::handleDataChanged);
            Q_ASSERT(m_dataChanged);
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
                   Q_ARG(QString, tr("Failed to initialize GpgME (%1)").arg(protocolToString(protocol))),
                   Q_ARG(QString, QStringLiteral("script-error")));
    }

    auto oldState = m_localState;
    // the raw data are available on the part itself
    setData(original->data(Imap::Mailbox::RolePartData).toByteArray());
    m_localState = oldState;
    // we might change this in future; maybe having access to the OFFSET_RAW_CONTENTS makes some sense
    setSpecialParts(nullptr, nullptr, nullptr, nullptr);
}

GpgMeSigned::~GpgMeSigned()
{
}

void GpgMeSigned::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_ASSERT(topLeft == bottomRight);
    if (!m_plaintextPart.isValid()) {
        forwardFailure(tr("Signed message is gone"), QString(), QStringLiteral("state-offline"));
        return;
    }
    if (topLeft != m_plaintextPart && topLeft != m_plaintextMimePart && topLeft != m_signaturePart &&
            topLeft != m_enclosingMessage) {
        return;
    }
    Q_ASSERT(m_plaintextPart.isValid());
    Q_ASSERT(m_plaintextMimePart.isValid());
    Q_ASSERT(m_signaturePart.isValid());
    if (!m_plaintextPart.data(RoleIsFetched).toBool() || !m_plaintextMimePart.data(RoleIsFetched).toBool() ||
            !m_signaturePart.data(RoleIsFetched).toBool() || !m_enclosingMessage.data(RoleMessageEnvelope).isValid()) {
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

    m_statusTLDR = tr("Verifying signature...");

    auto signatureData = m_signaturePart.data(RolePartData).toByteArray();
    bool wasEncrypted = m_isAllegedlyEncrypted;
    auto messageUids = extractMessageUids();
    auto ctx = m_ctx;

    m_crypto = std::async(std::launch::async, [this, ctx, rawData, signatureData, messageUids, wasEncrypted](){
        QPointer<QObject> p(this);

        GpgME::Data sigData(signatureData.data(), signatureData.size(), false);
        GpgME::Data msgData(rawData.data(), rawData.size(), false);

        auto verificationResult = ctx->verifyDetachedSignature(sigData, msgData);
        bool wasSigned = false;
        bool sigOkDisregardingTrust = false;
        bool sigValidVerified = false;
        bool uidMatched = false;
        QString tldr;
        QString longStatus;
        QString icon;
        QString signer;
        QDateTime signDate;

        if (verificationResult.numSignatures() == 0) {
            tldr = tr("No signatures in the signed message");
            icon = QStringLiteral("script-error");
        }

        for (const auto &sig: verificationResult.signatures()) {
            wasSigned = true;
            extractSignatureStatus(ctx, sig, messageUids, wasSigned, wasEncrypted,
                                   sigOkDisregardingTrust, sigValidVerified, uidMatched, tldr, longStatus, icon, signer, signDate);

            // FIXME: add support for multiple signatures at once. How do we want to handle them in the UI?
            break;
        }
        submitVerifyResult(p, {wasSigned, sigOkDisregardingTrust, sigValidVerified, tldr, longStatus, icon, signer, signDate});
    });
    emitDataChanged();
}


GpgMeEncrypted::GpgMeEncrypted(const Protocol protocol, GpgMeReplacer *replacer, MessageModel *model, MessagePart *parentPart, Ptr original,
                               const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex)
    : GpgMePart(protocol, replacer, model, parentPart, sourceItemIndex, proxyParentIndex)
    , m_decryptionSupported(false)
    , m_decryptionFailed(false)
{
    m_isAllegedlyEncrypted = true;
    if (m_ctx) {
        const auto rowCount = sourceItemIndex.model()->rowCount(sourceItemIndex);

        switch (m_ctx->protocol()) {
        case GpgME::Protocol::OpenPGP:
            m_versionPart = sourceItemIndex.child(0, 0);
            m_encPart = sourceItemIndex.child(1, 0);
            if (rowCount == 2) {
                m_dataChanged = connect(sourceItemIndex.model(), &QAbstractItemModel::dataChanged, this, &GpgMeEncrypted::handleDataChanged);
                Q_ASSERT(m_dataChanged);
                // Trigger lazy loading of the required message parts
                m_versionPart.data(RolePartData);
                m_encPart.data(RolePartData);
                CALL_LATER(this, handleDataChanged, Q_ARG(QModelIndex, m_encPart), Q_ARG(QModelIndex, m_encPart));
            } else {
                CALL_LATER(this, forwardFailure, Q_ARG(QString, tr("Malformed Encrypted Message")),
                           Q_ARG(QString, tr("Expected 2 parts for an encrypted OpenPGP message, but found %1.").arg(rowCount)),
                           Q_ARG(QString, QStringLiteral("emblem-error")));
            }
            break;

        case GpgME::Protocol::CMS:
            // We need to override the MIME type handling because the GUI only really expects encrypted messages using this type.
            // The application/pkcs7-mime is an opaque leaf node in a MIME tree, there are no other relevant parts.
            // This is very different from, say, an OpenPGP message.
            m_mimetype = QByteArrayLiteral("multipart/encrypted");
            m_versionPart = m_encPart = sourceItemIndex;
            m_dataChanged = connect(sourceItemIndex.model(), &QAbstractItemModel::dataChanged, this, &GpgMeEncrypted::handleDataChanged);
            Q_ASSERT(m_dataChanged);
            // Trigger lazy loading of the required message parts
            m_versionPart.data(RolePartData);
            m_encPart.data(RolePartData);
            CALL_LATER(this, handleDataChanged, Q_ARG(QModelIndex, m_encPart), Q_ARG(QModelIndex, m_encPart));
            break;

        case GpgME::Protocol::UnknownProtocol:
            Q_ASSERT(false);
            break;

        }
    } else {
        CALL_LATER(this, forwardFailure, Q_ARG(QString, tr("Cannot descrypt")),
                   Q_ARG(QString, tr("Failed to initialize GpgME (%1)").arg(protocolToString(protocol))),
                   Q_ARG(QString, QStringLiteral("script-error")));
    }

    auto oldState = m_localState;
    // the raw data are available on the part itself
    setData(original->data(Imap::Mailbox::RolePartData).toByteArray());
    m_localState = oldState;
    // we might change this in future; maybe having access to the OFFSET_RAW_CONTENTS makes some sense
    setSpecialParts(nullptr, nullptr, nullptr, nullptr);
}

GpgMeEncrypted::~GpgMeEncrypted()
{
}

void GpgMeEncrypted::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_ASSERT(topLeft == bottomRight);
    if (!m_encPart.isValid()) {
        forwardFailure(tr("Encrypted message is gone"), QString(), QStringLiteral("state-offline"));
        return;
    }
    if (topLeft != m_versionPart && topLeft != m_encPart && topLeft != m_enclosingMessage) {
        return;
    }
    Q_ASSERT(m_versionPart.isValid());
    Q_ASSERT(m_encPart.isValid());
    if (!m_versionPart.data(RoleIsFetched).toBool() || !m_encPart.data(RoleIsFetched).toBool()
            || !m_enclosingMessage.data(RoleMessageEnvelope).isValid()) {
        return;
    }

    disconnect(m_dataChanged);
    m_waitingForData = false;

    if (m_ctx->protocol() == GpgME::Protocol::OpenPGP) {
        // Check compliance with RFC3156
        QString versionString = m_versionPart.data(RolePartData).toString();
        if (!versionString.contains(QLatin1String("Version: 1"))) {
            forwardFailure(tr("Malformed Encrypted Message"),
                           tr("Unsupported PGP/MIME version. Expected \"Version: 1\", got \"%2\".").arg(versionString),
                           QStringLiteral("emblem-error"));
            return;
        }
    }

    m_statusTLDR = tr("Decrypting...");

    auto cipherData = m_encPart.data(RolePartData).toByteArray();
    auto messageUids = extractMessageUids();
    auto ctx = m_ctx;

    m_crypto = std::async(std::launch::async, [this, ctx, cipherData, messageUids ](){
        QPointer<QObject> p(this);

        GpgME::Data encData(cipherData.data(), cipherData.size(), false);
        QGpgME::QByteArrayDataProvider dp;
        GpgME::Data plaintextData(&dp);

        auto combinedResult = ctx->decryptAndVerify(encData, plaintextData);

        bool wasSigned = false;
        bool wasEncrypted = true;
        bool sigOkDisregardingTrust = false;
        bool sigValidVerified = false;
        bool uidMatched = false;
        QString tldr;
        QString longStatus;
        QString icon;
        QString signer;
        QDateTime signDate;

        if (combinedResult.second.numSignatures() != 0) {
            for (const auto &sig: combinedResult.second.signatures()) {
                wasSigned = true;
                extractSignatureStatus(ctx, sig, messageUids, wasSigned, wasEncrypted,
                                       sigOkDisregardingTrust, sigValidVerified, uidMatched, tldr, longStatus, icon, signer, signDate);

                // FIXME: add support for multiple signatures at once. How do we want to handle them in the UI?
                break;
            }
        }

        constexpr QChar LF = QLatin1Char('\n');

        bool decryptedOk = !combinedResult.first.error();

        if (combinedResult.first.error()) {
            if (tldr.isEmpty()) {
                tldr = tr("Broken encrypted message");
            }
            longStatus += LF + tr("Decryption error: %1").arg(QString::fromUtf8(combinedResult.first.error().asString()));
            icon = QStringLiteral("emblem-error");
        } else if (tldr.isEmpty()) {
            tldr = tr("Encrypted message");
            icon = QStringLiteral("emblem-encrypted-unlocked");
        }

        if (combinedResult.first.isWrongKeyUsage()) {
            longStatus += LF + tr("Wrong key usage, not for encryption");
        }
        if (auto msg = combinedResult.first.unsupportedAlgorithm()) {
            longStatus += LF + tr("Unsupported algorithm: %1").arg(QString::fromUtf8(msg));
        }

        for (const auto &recipient: combinedResult.first.recipients()) {
            GpgME::Error keyError;
            auto key = ctx->key(recipient.keyID(), keyError, false);
            if (keyError) {
                longStatus += LF + tr("Cannot extract recipient %1: %2")
                        .arg(QString::fromUtf8(recipient.keyID()), QString::fromUtf8(keyError.asString()));
            } else {
                if (key.numUserIDs()) {
                    longStatus += LF + tr("Encrypted to %1 (%2)")
                            .arg(QString::fromUtf8(key.userID(0).id()), QString::fromUtf8(recipient.keyID()));
                } else {
                    longStatus += LF + tr("Encrypted to %1").arg(QString::fromUtf8(recipient.keyID()));
                }
            }
        }
        if (auto fname = combinedResult.first.fileName()) {
            longStatus += LF + tr("Original filename: %1").arg(QString::fromUtf8(fname));
        }

        if (p) {
            bool ok = QMetaObject::invokeMethod(p, "processDecryptedData", Qt::QueuedConnection,
                                                Q_ARG(bool, decryptedOk),
                                                Q_ARG(QByteArray, dp.data()));
            Q_ASSERT(ok); Q_UNUSED(ok);
        } else {
            qDebug() << "[async crypto: GpgMeEncrypted is gone, not sending cleartext data]";
        }
        submitVerifyResult(p, {wasSigned, sigOkDisregardingTrust, sigValidVerified, tldr, longStatus, icon, signer, signDate});
    });

    emitDataChanged();
}

void GpgMeEncrypted::processDecryptedData(const bool ok, const QByteArray &data)
{
    if (!m_versionPart.isValid() || !m_encPart.isValid() || !m_proxyParentIndex.isValid()) {
        forwardFailure(tr("Encrypted message is gone"), QString(), QStringLiteral("state-offline"));
    } else {
        auto idx = m_proxyParentIndex.child(m_row, 0);
        Q_ASSERT(idx.isValid());
        if (ok) {
            mimetic::MimeEntity me(data.begin(), data.end());
            m_model->insertSubtree(idx, MimeticUtils::mimeEntityToPart(me, nullptr, 0));
        } else {
            // offer access to the original part
            std::unique_ptr<LocalMessagePart> part(new LocalMessagePart(nullptr, 0, m_encPart.data(RolePartMimeType).toByteArray()));
            part->setBodyDisposition("attachment");
            part->setFilename(m_encPart.data(RolePartFileName).toString());
            part->setOctets(m_encPart.data(RolePartOctets).toULongLong());
            part->setData(m_encPart.data(RolePartData).toByteArray());
            m_model->insertSubtree(idx, std::move(part));
        }
        emitDataChanged();
    }
}


}
