/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#ifndef SMTP_H
#define SMTP_H

#include "AbstractMSA.h"
#include "qwwsmtpclient/qwwsmtpclient.h"

namespace MSA
{

class SMTP : public AbstractMSA
{
    Q_OBJECT
public:
    SMTP(QObject *parent, const QString &host, quint16 port, bool encryptedConnect, bool startTls, bool auth,
         const QString &user, const QString &pass);
    virtual void sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data);

    virtual bool supportsBurl() const;
    virtual void sendBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl);
public slots:
    virtual void cancel();
    void handleDone(bool ok);
    void handleError(QAbstractSocket::SocketError err, const QString &msg);
private:
    QwwSmtpClient *qwwSmtp;
    QString host;
    quint16 port;
    bool encryptedConnect;
    bool startTls;
    bool auth;
    QString user;
    QString pass;
    bool failed;

    SMTP(const SMTP &); // don't implement
    SMTP &operator=(const SMTP &); // don't implement
};

}

#endif // SMTP_H
