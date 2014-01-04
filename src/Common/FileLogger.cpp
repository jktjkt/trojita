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

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "FileLogger.h"
#include "../Imap/Model/Utils.h"

namespace Common
{

FileLogger::FileLogger(QObject *parent) :
    QObject(parent), m_fileLog(0), m_consoleLog(false), m_autoFlush(false)
{
}

void FileLogger::setFileLogging(const bool enabled, const QString &fileName)
{
    if (enabled) {
        if (m_fileLog)
            return;

        QFile *logFile = new QFile(fileName, this);
        logFile->open(QIODevice::Truncate | QIODevice::WriteOnly);
        m_fileLog = new QTextStream(logFile);
    } else {
        if (m_fileLog) {
            QIODevice *dev = m_fileLog->device();
            delete m_fileLog;
            delete dev;
            m_fileLog = 0;
        }
    }
}

FileLogger::~FileLogger()
{
    delete m_fileLog;
}

void FileLogger::escapeCrLf(QString &s)
{
    s.replace(QChar('\r'), 0x240d /* SYMBOL FOR CARRIAGE RETURN */)
            .replace(QChar('\n'), 0x240a /* SYMBOL FOR LINE FEED */);
}

void FileLogger::slotImapLogged(uint parser, Common::LogMessage message)
{
    if (!m_fileLog && !m_consoleLog)
        return;

    QString formatted = formatMessage(parser, message);
    escapeCrLf(formatted);

    if (m_fileLog) {
        *m_fileLog << formatted << "\n";
        if (m_autoFlush)
            m_fileLog->flush();
    }

    enum {CUTOFF=200};
    if (m_consoleLog) {
        if (message.message.size() > CUTOFF) {
            // Got to reformat the message
            message.truncatedBytes = message.message.size() - CUTOFF;
            message.message = message.message.left(CUTOFF);
            formatted = formatMessage(parser, message);
            escapeCrLf(formatted);
        }
        qDebug() << formatted.toUtf8().constData() << "\n";
    }
}

QString FileLogger::formatMessage(uint parser, const Common::LogMessage &message) const
{
    using namespace Common;
    QString direction;
    switch (message.kind) {
    case LOG_IO_READ:
        direction = QLatin1String(" <<< ");
        break;
    case LOG_IO_WRITTEN:
        direction = QLatin1String(" >>> ");
        break;
    case LOG_PARSE_ERROR:
        direction = QLatin1String(" [err] ");
        break;
    case LOG_MAILBOX_SYNC:
        direction = QLatin1String(" [sync] ");
        break;
    case LOG_TASKS:
        direction = QLatin1String(" [task] ");
        break;
    case LOG_MESSAGES:
        direction = QLatin1String(" [msg] ");
        break;
    case LOG_OTHER:
        direction = QLatin1String(" ");
        break;
    }
    if (message.truncatedBytes) {
        direction += QLatin1String("[truncated] ");
    }
    return message.timestamp.toString(QLatin1String("hh:mm:ss.zzz")) + QString::number(parser) + QLatin1Char(' ') +
            direction + message.source + QLatin1Char(' ') + message.message.trimmed();
}

/** @short Enable flushing the on-disk log after each message

Automatically flushing the log will make sure that all messages are actually stored in the log even in the event of a program
crash, but at the cost of a rather high overhead.
*/
void FileLogger::setAutoFlush(const bool autoFlush)
{
    m_autoFlush = autoFlush;
    if (m_autoFlush && m_fileLog)
        m_fileLog->flush();
}

void FileLogger::setConsoleLogging(const bool enabled)
{
    m_consoleLog = enabled;
}

}
