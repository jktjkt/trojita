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
#ifndef SENDMAIL_H
#define SENDMAIL_H

#include "AbstractMSA.h"

#include <QProcess>
#include <QStringList>

namespace MSA {

class Sendmail : public AbstractMSA
{
    Q_OBJECT
public:
    Sendmail( QObject* parent, const QString& _command, const QStringList& _args );
    virtual ~Sendmail();
    virtual void sendMail( const QString& from, const QStringList& to, const QByteArray& data );
private slots:
    void handleError( QProcess::ProcessError e );
    void handleBytesWritten( qint64 bytes );
    void handleStarted();
public slots:
    virtual void cancel();
private:
    QProcess* proc;
    QString command;
    QStringList args;
    QByteArray dataToSend;
    int writtenSoFar;

    Sendmail(const Sendmail&); // don't implement
    Sendmail& operator=(const Sendmail&); // don't implement
};

}

#endif // SENDMAIL_H
