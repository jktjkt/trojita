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
#include "Exceptions.h"

#include <QTextStream>
#include "Parser/Response.h"

namespace Imap
{

const char *ImapException::what() const throw()
{
    if (m_offset == -1)
        return m_msg.c_str();
    else {
        QByteArray out(m_msg.c_str());
        out += " when parsing this:\n";
        out += m_line;
        out += QByteArray(m_offset, ' ');
        out += "^ here (offset " + QByteArray::number(m_offset) + ")\n";
        return out.constData();
    }
}


MailboxException::MailboxException(const char *const msg,
                                   const Imap::Responses::AbstractResponse &response)
{
    QByteArray buf;
    QTextStream s(&buf);
    s << msg << "\r\n" << response;
    s.flush();
    m_msg = buf.constData();
    m_exceptionClass = "MailboxException";
}

MailboxException::MailboxException(const char *const msg)
{
    m_msg = msg;
    m_exceptionClass = "MailboxException";
}

}
