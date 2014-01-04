/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#include "AbstractMSA.h"

/** @short Implementations of the Mail Submission Agent interface */
namespace MSA
{

AbstractMSA::AbstractMSA(QObject *parent): QObject(parent)
{
}

AbstractMSA::~AbstractMSA()
{
}

bool AbstractMSA::supportsBurl() const
{
    return false;
}

bool AbstractMSA::supportsImapSending() const
{
    return false;
}

void AbstractMSA::sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data)
{
    Q_UNUSED(from);
    Q_UNUSED(to);
    Q_UNUSED(data);
    emit error(tr("Sending mail plaintext is not supported by %1").arg(metaObject()->className()));
}

void AbstractMSA::sendBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl)
{
    Q_UNUSED(from);
    Q_UNUSED(to);
    Q_UNUSED(imapUrl);
    emit error(tr("BURL is not supported by %1").arg(metaObject()->className()));
}

void AbstractMSA::sendImap(const QString &mailbox, const int uidValidity, const int uid, const Imap::Mailbox::UidSubmitOptionsList options)
{
    Q_UNUSED(mailbox);
    Q_UNUSED(uidValidity);
    Q_UNUSED(uid);
    Q_UNUSED(options);
    emit error(tr("IMAP sending is not supported by %1").arg(metaObject()->className()));
}

void AbstractMSA::setPassword(const QString &password)
{
    Q_UNUSED(password);
    emit error(tr("Setting password is not supported by %1").arg(metaObject()->className()));
}

MSAFactory::~MSAFactory()
{
}

}
