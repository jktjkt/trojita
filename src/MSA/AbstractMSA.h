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
#ifndef ABSTRACTMSA_H
#define ABSTRACTMSA_H

#include <QByteArray>
#include <QObject>
#include "Imap/Model/UidSubmitData.h"

namespace MSA
{

class AbstractMSA : public QObject
{
    Q_OBJECT
public:
    explicit AbstractMSA(QObject *parent);
    virtual ~AbstractMSA();
    virtual bool supportsBurl() const;
    virtual bool supportsImapSending() const;
    virtual void sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data);
    virtual void sendBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl);
    virtual void sendImap(const QString &mailbox, const int uidValidity, const int uid,
                          const Imap::Mailbox::UidSubmitOptionsList options);
public slots:
    virtual void cancel() = 0;
    virtual void setPassword(const QString &password);
signals:
    void connecting();
    void passwordRequested(const QString &user, const QString &host);
    void sending();
    void sent();
    void error(const QString &message);
    void progressMax(int max);
    void progress(int num);
};

/** @short Factory producing AbstractMSA instances */
class MSAFactory {
public:
    virtual ~MSAFactory();
    virtual AbstractMSA *create(QObject *parent) const = 0;
};

}

#endif // ABSTRACTMSA_H
