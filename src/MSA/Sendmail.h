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
#ifndef SENDMAIL_H
#define SENDMAIL_H

#include "AbstractMSA.h"

#include <QProcess>
#include <QStringList>

namespace MSA
{

class Sendmail : public AbstractMSA
{
    Q_OBJECT
public:
    Sendmail(QObject *parent, const QString &command, const QStringList &args);
    virtual ~Sendmail();
    virtual void sendMail(const QByteArray &from, const QList<QByteArray> &to, const QByteArray &data);
private slots:
    void handleError(QProcess::ProcessError e);
    void handleBytesWritten(qint64 bytes);
    void handleStarted();
    void handleFinished(const int exitCode);
public slots:
    virtual void cancel();
private:
    QProcess *proc;
    QString command;
    QStringList args;
    QByteArray dataToSend;
    int writtenSoFar;

    Sendmail(const Sendmail &); // don't implement
    Sendmail &operator=(const Sendmail &); // don't implement
};

class SendmailFactory: public MSAFactory
{
public:
    SendmailFactory(const QString &command, const QStringList &args);
    virtual ~SendmailFactory();
    virtual AbstractMSA *create(QObject *parent) const;
private:
    QString m_command;
    QStringList m_args;
};

}

#endif // SENDMAIL_H
