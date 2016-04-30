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

#ifndef TROJITA_CRYPTO_GPGMEPP_H
#define TROJITA_CRYPTO_GPGMEPP_H

#include <future>
#include <memory>
#include <QDateTime>
#include <QModelIndex>
#include "Cryptography/MessagePart.h"
#include "Cryptography/PartReplacer.h"

namespace GpgME {
class Context;
class Signature;
}

namespace Cryptography {

struct SignatureDataBundle {
    bool wasSigned;
    bool isValidDisregardingTrust;
    bool isValidTrusted;
    QString tldrStatus;
    QString longStatus;
    QString statusIcon;
    QString signatureUid;
    QDateTime signatureDate;
};

/** @short What sort of crypto messages is this? */
enum class Protocol {
    OpenPGP,
    SMime,
};

class GpgMeReplacer: public PartReplacer {
public:
    GpgMeReplacer();
    virtual ~GpgMeReplacer() override;

    MessagePart::Ptr createPart(MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex) override;

    void registerOrhpanedCryptoTask(std::future<void> task);
private:
    std::vector<std::future<void>> m_orphans;
};

/** @short Wrapper for asynchronous PGP related operations using GpgME++ */
class GpgMePart : public QObject, public LocalMessagePart {
    Q_OBJECT
public:
    GpgMePart(const Protocol protocol, GpgMeReplacer *replacer, MessageModel *model, MessagePart *parentPart,
              const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);
    ~GpgMePart();
    virtual QVariant data(int role) const override;

protected slots:
    void forwardFailure(const QString &statusTLDR, const QString &statusLong, const QString &statusIcon);
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) = 0;
    void internalUpdateState(const Cryptography::SignatureDataBundle &d);

protected:
    void emitDataChanged();
    static void extractSignatureStatus(std::shared_ptr<GpgME::Context> ctx, const GpgME::Signature &sig,
                                       const std::vector<std::string> messageUids, const bool wasSigned, const bool wasEncrypted,
                                       bool &sigOkDisregardingTrust, bool &sigValidVerified, bool &uidMatched, QString &tldr, QString &longStatus, QString &icon, QString &signer, QDateTime &signDate);
    static void submitVerifyResult(QPointer<QObject> p, const SignatureDataBundle &data);
    std::vector<std::string> extractMessageUids();

    GpgMeReplacer *m_replacer;
    MessageModel *m_model;
    QMetaObject::Connection m_dataChanged;
    QPersistentModelIndex m_sourceIndex;
    QPersistentModelIndex m_proxyParentIndex;
    QPersistentModelIndex m_enclosingMessage;
    bool m_waitingForData;
    bool m_wasSigned;
    /** @short Does this message look like an encrypted one? */
    bool m_isAllegedlyEncrypted;
    bool m_signatureOkDisregardingTrust;
    bool m_signatureValidVerifiedTrusted;
    QString m_statusTLDR, m_statusLong, m_statusIcon;
    QString m_signatureIdentityName;
    QDateTime m_signDate;

    std::shared_ptr<GpgME::Context> m_ctx;
    std::future<void> m_crypto;
};

class GpgMeSigned : public GpgMePart {
    Q_OBJECT
public:
    GpgMeSigned(const Protocol protocol, GpgMeReplacer *replacer, MessageModel *model, MessagePart *parentPart, Ptr original,
                const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);
    ~GpgMeSigned();

private slots:
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) override;

private:
    QPersistentModelIndex m_plaintextPart, m_plaintextMimePart, m_signaturePart;
};

class GpgMeEncrypted : public GpgMePart {
    Q_OBJECT
public:
    GpgMeEncrypted(const Protocol protocol, GpgMeReplacer *replacer, MessageModel *model, MessagePart *parentPart, Ptr original,
                   const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);
    ~GpgMeEncrypted();

private slots:
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) override;
    void processDecryptedData(const bool ok, const QByteArray &data);

private:
    QPersistentModelIndex m_versionPart, m_encPart;
    bool m_decryptionSupported;
    bool m_decryptionFailed;
};
}

Q_DECLARE_METATYPE(Cryptography::SignatureDataBundle)

#endif
