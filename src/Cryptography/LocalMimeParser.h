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

#ifndef TROJITA_CRYPTO_LOCAL_MIME_PARSER_H
#define TROJITA_CRYPTO_LOCAL_MIME_PARSER_H

#include <QObject>
#include "Cryptography/PartReplacer.h"

namespace Cryptography {

/** @short Local parsing of MIME messages */
class LocalMimeMessageParser: public PartReplacer {
public:
    LocalMimeMessageParser();
    virtual ~LocalMimeMessageParser() override;

    MessagePart::Ptr createPart(MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex) override;

};

class LocallyParsedMimePart: public QObject, public LocalMessagePart {
    Q_OBJECT
public:
    LocallyParsedMimePart(MessageModel *model, MessagePart *parentPart, Ptr originalPart,
                          const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex);

    QVariant data(int role) const override;

public slots:
    void messageMaybeAvailable(const QModelIndex &topLeft, const QModelIndex &bottomRight);
private:
#ifdef MIME_TREE_DEBUG
    QByteArray dumpLocalInfo() const override;
#endif

    MessageModel *m_model;
    QPersistentModelIndex m_sourceHeaderIndex, m_sourceTextIndex, m_proxyParentIndex;
};

}
#endif
