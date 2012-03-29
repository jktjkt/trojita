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
#include "Sendmail.h"

namespace MSA
{

Sendmail::Sendmail(QObject *parent, const QString &command, const QStringList &args):
    AbstractMSA(parent), command(command), args(args)
{
    proc = new QProcess(this);
    connect(proc, SIGNAL(started()), this, SLOT(handleStarted()));
    connect(proc, SIGNAL(finished(int)), this, SIGNAL(sent()));
    connect(proc, SIGNAL(error(QProcess::ProcessError)),
            this, SLOT(handleError(QProcess::ProcessError)));
    connect(proc, SIGNAL(bytesWritten(qint64)), this, SLOT(handleBytesWritten(qint64)));
}

Sendmail::~Sendmail()
{
    proc->kill();
    proc->waitForFinished();
}

void Sendmail::sendMail(const QString &from, const QStringList &to, const QByteArray &data)
{
    // FIXME: from
    emit progressMax(data.size());
    emit progress(0);
    // FIXME: support for passing the from argument, perhaps via the formatting options?
    QStringList myArgs = args;
    for (QStringList::const_iterator it = to.begin(); it != to.end(); ++it) {
        myArgs << *it;
    }
    writtenSoFar = 0;
    emit connecting();
    proc->start(command, myArgs);
    dataToSend = data;
}

void Sendmail::cancel()
{
    proc->terminate();
}

void Sendmail::handleStarted()
{
    emit sending();
    proc->write(dataToSend);
    proc->closeWriteChannel();
}

void Sendmail::handleError(QProcess::ProcessError e)
{
    Q_UNUSED(e);
    emit error(tr("The sendmail process has failed: %1").arg(proc->errorString()));
}

void Sendmail::handleBytesWritten(qint64 bytes)
{
    writtenSoFar += bytes;
    emit progress(writtenSoFar);
}

}
