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
#ifndef MSA_FAKE_H
#define MSA_FAKE_H

#include <QPointer>
#include "AbstractMSA.h"

namespace MSA
{
class FakeFactory;

class Fake : public AbstractMSA
{
    Q_OBJECT
public:
    Fake(QObject *parent, FakeFactory *factory, const bool supportsBurl, const bool supportsImap);
    virtual ~Fake();
    virtual void sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data);
    virtual void sendBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl);
public slots:
    virtual void cancel();
private:
    Fake(const Fake &); // don't implement
    Fake &operator=(const Fake &); // don't implement
    FakeFactory *m_factory;
    bool m_supportsBurl;
    bool m_supportsImap;
    friend class FakeFactory;
};

/** @short Produce fake MSAs

This class is a QObject because we need signals and slots. These are required because it would otherwise
be rather hard to connect to a just-created AbstractMSA instance.
*/
class FakeFactory: public QObject, public MSAFactory
{
    Q_OBJECT
public:
    FakeFactory();
    virtual ~FakeFactory();
    virtual AbstractMSA *create(QObject *parent) const;
    Fake *lastMSA() const;

    void setBurlSupport(const bool enabled);
    void setImapSupport(const bool enabled);
public slots:
    void doEmitConnecting();
    void doEmitSending();
    void doEmitSent();
    void doEmitError(const QString &message);
    void doEmitProgressMax(int max);
    void doEmitProgress(int num);
signals:
    void requestedSending(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data);
    void requestedBurlSending(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl);
    void requestedCancelling();

    // forwarded from AbstractMSA
    void connecting();
    void sending();
    void sent();
    void error(const QString &message);
    void progressMax(int max);
    void progress(int num);

private:
    mutable QPointer<Fake> m_lastOne;
    bool m_supportsBurl;
    bool m_supportsImap;
    friend class Fake;
};

}

#endif // MSA_FAKE_H
