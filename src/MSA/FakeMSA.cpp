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
#include "FakeMSA.h"

namespace MSA
{

Fake::Fake(QObject *parent, FakeFactory *factory, const bool supportsBurl, const bool supportsImap):
    AbstractMSA(parent), m_factory(factory), m_supportsBurl(supportsBurl), m_supportsImap(supportsImap)
{
}

Fake::~Fake()
{
}

void Fake::sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data)
{
    emit m_factory->requestedSending(from, to, data);
}

void Fake::sendBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl)
{
    emit m_factory->requestedBurlSending(from, to, imapUrl);
}

void Fake::cancel()
{
    emit m_factory->requestedCancelling();
}

FakeFactory::FakeFactory(): m_supportsBurl(false), m_supportsImap(false)
{
}

FakeFactory::~FakeFactory()
{
}

AbstractMSA *FakeFactory::create(QObject *parent) const
{
    m_lastOne = new Fake(parent, const_cast<FakeFactory *>(this), m_supportsBurl, m_supportsImap);
    connect(m_lastOne.data(), &AbstractMSA::connecting, this, &FakeFactory::connecting);
    connect(m_lastOne.data(), &AbstractMSA::sending, this, &FakeFactory::sending);
    connect(m_lastOne.data(), &AbstractMSA::sent, this, &FakeFactory::sent);
    connect(m_lastOne.data(), &AbstractMSA::error, this, &FakeFactory::error);
    connect(m_lastOne.data(), &AbstractMSA::progressMax, this, &FakeFactory::progressMax);
    connect(m_lastOne.data(), &AbstractMSA::progress, this, &FakeFactory::progress);
    return m_lastOne;
}

Fake *FakeFactory::lastMSA() const
{
    return m_lastOne;
}

void FakeFactory::doEmitConnecting()
{
    emit lastMSA()->connecting();
}

void FakeFactory::doEmitSending()
{
    emit lastMSA()->sending();
}

void FakeFactory::doEmitSent()
{
    emit lastMSA()->sent();
}

void FakeFactory::doEmitError(const QString &message)
{
    emit lastMSA()->error(message);
}

void FakeFactory::doEmitProgressMax(int max)
{
    emit lastMSA()->progressMax(max);
}

void FakeFactory::doEmitProgress(int num)
{
    emit lastMSA()->progress(num);
}

void FakeFactory::setBurlSupport(const bool enabled)
{
    m_supportsBurl = enabled;
}

void FakeFactory::setImapSupport(const bool enabled)
{
    m_supportsImap = enabled;
}

}
