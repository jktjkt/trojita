/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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

namespace MSA {

class SMTP : public AbstractMSA
{
    Q_OBJECT
public:
    SMTP( QObject* parent, const QString& host, quint16 port,
          bool encryptedConnect, bool startTls, bool auth,
          const QString& user, const QString& pass );
    virtual void sendMail( const QString& from, const QStringList& to, const QByteArray& data );
public slots:
    virtual void cancel();
    void handleDone( bool ok );
    void handleError( QAbstractSocket::SocketError err, const QString& msg );
private:
    QwwSmtpClient* _qwwSmtp;
    QString _host;
    quint16 _port;
    bool _encryptedConnect;
    bool _startTls;
    bool _auth;
    QString _user;
    QString _pass;
    bool _failed;

    SMTP(const SMTP&); // don't implement
    SMTP& operator=(const SMTP&); // don't implement
};

}

#endif // SMTP_H
