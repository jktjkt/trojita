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

#include <QModelIndex>

#include "Cryptography/MessagePart.h"
#include "Cryptography/PartReplacer.h"

namespace QCA {
class Initializer;
class OpenPGP;
class SecureMessage;
}

namespace Cryptography {

class OpenPGPReplacer: public PartReplacer {
public:
    OpenPGPReplacer(MessageModel *model);
    virtual ~OpenPGPReplacer() override;

    MessagePart::Ptr createPart(MessagePart *parentPart, MessagePart::Ptr original,
                                const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex) override;
private:
    std::unique_ptr<QCA::Initializer> m_qcaInit;
    std::unique_ptr<QCA::OpenPGP> m_pgp;
};

/** @short Wrapper for asynchronous PGP related operations using QCA */
class OpenPGPPart : public QObject, public LocalMessagePart {
    Q_OBJECT
public:
    OpenPGPPart(QCA::OpenPGP *pgp, MessageModel *model, MessagePart *parentPart,
                const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);
    ~OpenPGPPart();

protected slots:
    void forwardFailure(const QString &error, const QString &details);
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) = 0;

protected:
    MessageModel *m_model;
    QCA::OpenPGP *m_pgp;
    QCA::SecureMessage *m_msg;
    QMetaObject::Connection m_dataChanged;
    QPersistentModelIndex m_proxyParentIndex;
};

/** @short MessageModel wrapper for OpenPGP encrypted message */
class OpenPGPEncryptedPart : public OpenPGPPart {
    Q_OBJECT

public:
    OpenPGPEncryptedPart(QCA::OpenPGP *pgp, MessageModel *model, MessagePart *parentPart, Ptr original,
                         const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);
    ~OpenPGPEncryptedPart();

private slots:
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) override;
    void handleDecryptionFinished();

private:
    QPersistentModelIndex m_versionPart, m_encPart;
};

}

#endif /* CRYPTOGRAPHY_OPENPGPHELPER_H_ */
