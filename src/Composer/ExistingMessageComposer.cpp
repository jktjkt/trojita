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

#include <algorithm>
#include <QBuffer>
#include <QDateTime>
#include <QUrl>
#include "Composer/ExistingMessageComposer.h"
#include "Imap/Model/FullMessageCombiner.h"
#include "Imap/Model/ItemRoles.h"

namespace Composer {

ExistingMessageComposer::ExistingMessageComposer(const QModelIndex& messageRoot)
    : m_root(messageRoot)
{
    m_combiner.reset(new Imap::Mailbox::FullMessageCombiner(messageRoot, nullptr));
    QObject::connect(m_combiner.get(), &Imap::Mailbox::FullMessageCombiner::failed,
                     [this](const QString &message) { m_errorMessage = message; });
}

ExistingMessageComposer::~ExistingMessageComposer() = default;

bool ExistingMessageComposer::isReadyForSerialization() const
{
    return m_combiner->loaded();
}

void ExistingMessageComposer::setPreloadEnabled(const bool preload)
{
    if (preload) {
        m_combiner->load();
    }
}

bool ExistingMessageComposer::asRawMessage(QIODevice *target, QString *errorMessage) const
{
    m_combiner->load();
    if (!m_combiner->loaded()) {
        *errorMessage = m_errorMessage;
        return false;
    }

    target->write(m_combiner->data());
    return true;
}

bool ExistingMessageComposer::asCatenateData(QList<Imap::Mailbox::CatenatePair> &target, QString *errorMessage) const
{
    target.clear();
    QByteArray url = m_root.data(Imap::Mailbox::RoleIMAPRelativeUrl).toByteArray();
    if (!url.isEmpty()) {
        target.append(qMakePair(Imap::Mailbox::CATENATE_URL, url));
        return true;
    } else {
        *errorMessage = Imap::Mailbox::FullMessageCombiner::tr("Cannot obtain IMAP URL for CATENATE");
        return false;
    }
}

QDateTime ExistingMessageComposer::timestamp() const
{
    return m_root.data(Imap::Mailbox::RoleMessageDate).toDateTime();
}

QByteArray ExistingMessageComposer::rawFromAddress() const
{
    return m_rawFrom;
}

QList<QByteArray> ExistingMessageComposer::rawRecipientAddresses() const
{
    return m_rawRecipients;
}

void ExistingMessageComposer::setRecipients(const QList<QPair<Composer::RecipientKind, Imap::Message::MailAddress>> &recipients)
{
    m_rawRecipients.clear();
    std::transform(recipients.cbegin(), recipients.cend(), std::back_inserter(m_rawRecipients),
                   [](const QPair<Composer::RecipientKind, Imap::Message::MailAddress> &addr) {
        return addr.second.asSMTPMailbox();
    });
}

void ExistingMessageComposer::setFrom(const Imap::Message::MailAddress &from)
{
    m_rawFrom = from.asSMTPMailbox();
}

}
