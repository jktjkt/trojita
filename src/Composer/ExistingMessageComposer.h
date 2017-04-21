/* Copyright (C) 2006 - 2017 Jan Kundrát <jkt@kde.org>

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

#pragma once

#include <memory>
#include <QDateTime>
#include <QPersistentModelIndex>
#include "Composer/AbstractComposer.h"

namespace Imap {
namespace Mailbox {
class FullMessageCombiner;
}
}

namespace Composer {

class ExistingMessageComposer : public AbstractComposer
{
public:

    explicit ExistingMessageComposer(const QModelIndex &messageRoot);
    ~ExistingMessageComposer();

    virtual bool isReadyForSerialization() const override;
    virtual bool asRawMessage(QIODevice *target, QString *errorMessage) const override;
    virtual bool asCatenateData(QList<Imap::Mailbox::CatenatePair> &target, QString *errorMessage) const override;
    virtual QDateTime timestamp() const override;
    virtual QByteArray rawFromAddress() const override;
    virtual QList<QByteArray> rawRecipientAddresses() const override;
    virtual void setRecipients(const QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress>> &recipients) override;
    virtual void setFrom(const Imap::Message::MailAddress &from) override;
    virtual void setPreloadEnabled(const bool preload) override;
private:
    QByteArray resentChunk() const;
    std::unique_ptr<Imap::Mailbox::FullMessageCombiner> m_combiner;
    QPersistentModelIndex m_root;
    QString m_errorMessage;
    Imap::Message::MailAddress m_resentFrom;
    QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress>> m_recipients;
    mutable QDateTime m_resentDate;
    mutable QByteArray m_messageId;
};

}
