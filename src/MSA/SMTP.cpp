/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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
#include "SMTP.h"

namespace MSA
{

SMTP::SMTP(QObject *parent, const QString &host, quint16 port, bool encryptedConnect, bool startTls, bool auth,
           const QString &user, const QString &pass):
    AbstractMSA(parent), host(host), port(port),
    encryptedConnect(encryptedConnect), startTls(startTls), auth(auth),
    user(user), pass(pass), failed(false)
{
    qwwSmtp = new QwwSmtpClient(this);
    // FIXME: handle SSL errors properly
    connect(qwwSmtp, SIGNAL(sslErrors(QList<QSslError>)), qwwSmtp, SLOT(ignoreSslErrors()));
    connect(qwwSmtp, SIGNAL(connected()), this, SIGNAL(sending()));
    connect(qwwSmtp, SIGNAL(done(bool)), this, SLOT(handleDone(bool)));
    connect(qwwSmtp, SIGNAL(socketError(QAbstractSocket::SocketError,QString)),
            this, SLOT(handleError(QAbstractSocket::SocketError,QString)));
}

void SMTP::cancel()
{
    qwwSmtp->disconnectFromHost();
}

void SMTP::handleDone(bool ok)
{
    if (ok)
        emit sent();
    else if (! failed) {
        if (qwwSmtp->errorString().isEmpty())
            emit error(tr("Sending of the message failed."));
        else
            emit error(tr("Sending of the message failed with the following error: %1").arg(qwwSmtp->errorString()));
    }
}

void SMTP::handleError(QAbstractSocket::SocketError err, const QString &msg)
{
    Q_UNUSED(err);
    failed = true;
    emit error(msg);
}

void SMTP::sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data)
{
    emit progressMax(data.size());
    emit progress(0);
    emit connecting();
    if (encryptedConnect)
        qwwSmtp->connectToHostEncrypted(host, port);
    else
        qwwSmtp->connectToHost(host, port);
    if (startTls)
        qwwSmtp->startTls();
    if (auth)
        qwwSmtp->authenticate(user, pass,
                               (startTls || encryptedConnect) ?
                               QwwSmtpClient::AuthPlain :
                               QwwSmtpClient::AuthAny);
    emit sending(); // FIXME: later
    qwwSmtp->sendMail(from, to, QString::fromUtf8(data));
    qwwSmtp->disconnectFromHost();
}

bool SMTP::supportsBurl() const
{
    return true;
}

void SMTP::sendBurl(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &imapUrl)
{
    emit progressMax(1);
    emit progress(0);
    emit connecting();
    if (encryptedConnect)
        qwwSmtp->connectToHostEncrypted(host, port);
    else
        qwwSmtp->connectToHost(host, port);
    if (startTls)
        qwwSmtp->startTls();
    if (auth)
        qwwSmtp->authenticate(user, pass,
                               (startTls || encryptedConnect) ?
                               QwwSmtpClient::AuthPlain :
                               QwwSmtpClient::AuthAny);
    emit sending(); // FIXME: later
    qwwSmtp->sendMailBurl(from, to, imapUrl);
    qwwSmtp->disconnectFromHost();
}

}
