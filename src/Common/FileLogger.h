/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef COMMON_FILELOGGER_H
#define COMMON_FILELOGGER_H

#include "Logging.h"

class QTextStream;

namespace Common
{

class FileLogger : public QObject
{
    Q_OBJECT
public:
    explicit FileLogger(QObject *parent = 0);
    virtual ~FileLogger();

public slots:
    /** @short An IMAP model wants to log something */
    void slotImapLogged(uint parser, Common::LogMessage message);

    /** @short Enable/disable persistent logging */
    void setFileLogging(const bool enabled, const QString &fileName);

    void setConsoleLogging(const bool enabled);

    void setAutoFlush(const bool autoFlush);

protected:
    QString formatMessage(uint parser, const Common::LogMessage &message) const;
    void escapeCrLf(QString &s);

    QTextStream *m_fileLog;

    bool m_consoleLog;
    bool m_autoFlush;
};

}

#endif // COMMON_FILELOGGER_H
