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

#ifndef CRYPTOGRAPHY_OPENPGPHELPER_H_
#define CRYPTOGRAPHY_OPENPGPHELPER_H_

#include <QDateTime>
#include <QModelIndex>

#include "Cryptography/MessagePart.h"
#include "Cryptography/PartReplacer.h"

namespace QCA {
class Initializer;
class KeyStore;
class KeyStoreManager;
class OpenPGP;
class SecureMessage;
}

namespace Cryptography {

class OpenPGPReplacer: public PartReplacer {
public:
    OpenPGPReplacer();
    virtual ~OpenPGPReplacer() override;

    MessagePart::Ptr createPart(MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex) override;
private:
    std::unique_ptr<QCA::Initializer> m_qcaInit;
    std::unique_ptr<QCA::OpenPGP> m_pgp;
    std::unique_ptr<QCA::KeyStoreManager> m_keystoreManager;
    std::unique_ptr<QCA::KeyStore> m_keystore;
    QMetaObject::Connection m_keystoreWaiting;
};

/** @short Wrapper for asynchronous PGP related operations using QCA */
class OpenPGPPart : public QObject, public LocalMessagePart {
    Q_OBJECT
public:
    OpenPGPPart(QCA::OpenPGP *pgp, QCA::KeyStore *keystore, MessageModel *model, MessagePart *parentPart,
                const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);
    ~OpenPGPPart();
    virtual QVariant data(int role) const override;

protected slots:
    void forwardFailure(const QString &statusTLDR, const QString &statusLong, const QString &statusIcon);
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) = 0;

protected:
    void updateSignedStatus();
    void emitDataChanged();

    MessageModel *m_model;
    QCA::OpenPGP *m_pgp;
    QCA::SecureMessage *m_msg;
    QCA::KeyStore *m_keystore;
    QMetaObject::Connection m_dataChanged;
    QPersistentModelIndex m_proxyParentIndex;
    bool m_waitingForData;
    bool m_wasSigned;
    /** @short Does this message look like an encrypted one? */
    bool m_isAllegedlyEncrypted;
    QString m_statusTLDR, m_statusLong, m_statusIcon;
    bool m_signatureOkDisregardingTrust;
    bool m_signatureValidTrusted;
    QString m_signatureIdentityName;
    QDateTime m_signDate;
};

/** @short MessageModel wrapper for OpenPGP encrypted message */
class OpenPGPEncryptedPart : public OpenPGPPart {
    Q_OBJECT

public:
    OpenPGPEncryptedPart(QCA::OpenPGP *pgp, QCA::KeyStore *keystore, MessageModel *model, MessagePart *parentPart, Ptr original,
                         const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);
    ~OpenPGPEncryptedPart();
    virtual QVariant data(int role) const override;

private slots:
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) override;
    void handleDecryptionFinished();

private:
    QPersistentModelIndex m_versionPart, m_encPart;
    bool m_decryptionSupported;
    bool m_decriptionFailed;
};

/** @short MessageModel wrapper for OpenPGP signed message */
class OpenPGPSignedPart : public OpenPGPPart {
    Q_OBJECT

public:
    OpenPGPSignedPart(QCA::OpenPGP *pgp, QCA::KeyStore *keystore, MessageModel *model, MessagePart *parentPart, Ptr original,
                      const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);
    ~OpenPGPSignedPart();

private slots:
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) override;
    void handleVerifyFinished();

private:
    QPersistentModelIndex m_plaintextPart, m_plaintextMimePart, m_signaturePart;
};

}

#endif /* CRYPTOGRAPHY_OPENPGPHELPER_H_ */
