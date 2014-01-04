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
    virtual void setPassword(const QString &password);
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
    QByteArray from;
    QList<QByteArray> to;
    QByteArray data;
    bool isWaitingForPassword;
    enum { MODE_SMTP_INVALID, MODE_SMTP_DATA, MODE_SMTP_BURL } sendingMode;

    void sendContinueGotPassword();

    SMTP(const SMTP &); // don't implement
    SMTP &operator=(const SMTP &); // don't implement
};

class SMTPFactory: public MSAFactory
{
public:
    SMTPFactory(const QString &host, quint16 port, bool encryptedConnect, bool startTls, bool auth,
         const QString &user, const QString &pass);
    virtual ~SMTPFactory();
    virtual AbstractMSA *create(QObject *parent) const;
private:
    QString m_host;
    quint16 m_port;
    bool m_encryptedConnect;
    bool m_startTls;
    bool m_auth;
    QString m_user;
    QString m_pass;
};

}

#endif // SMTP_H
