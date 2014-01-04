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
#ifndef TROJITA_IMAP_LOGGING_H
#define TROJITA_IMAP_LOGGING_H

#include <QDateTime>
#include <QVector>

namespace Common
{

/** @short What is that message related to? */
enum LogKind {
    LOG_IO_READ, /**< Data read from the server */
    LOG_IO_WRITTEN, /**< Data written to the server */
    LOG_PARSE_ERROR, /**< Error when parsing data */
    LOG_MAILBOX_SYNC, /**< Tracing of mailbox resynchronization */
    LOG_TASKS, /**< Tracing related to Tasks */
    LOG_MESSAGES, /**< Manipulating messages */
    LOG_OTHER /**< Something else */
};

/** @short Representaiton of one message */
struct LogMessage {
    /** @short When did it occur? */
    QDateTime timestamp;
    /** @short What's it related to */
    LogKind kind;
    /** @short Detailed identification of the origin */
    QString source;
    /** @short Actual message */
    QString message;
    /** @short Was it truncated? */
    uint truncatedBytes;

    LogMessage(const QDateTime &timestamp_, const LogKind kind_, const QString &source_, const QString &message_, const uint truncated_):
        timestamp(timestamp_), kind(kind_), source(source_), message(message_), truncatedBytes(truncated_)
    {
    }

    // default constructor for QVector
    LogMessage() {}
};

}

// Both QString and QDateTime are movable, so our combination is movable as well
Q_DECLARE_TYPEINFO(Common::LogMessage, Q_MOVABLE_TYPE);

#endif // TROJITA_IMAP_LOGGING_H
