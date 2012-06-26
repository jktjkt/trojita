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
#include <ctype.h>
#include <QStringList>
#include "Command.h"

namespace Imap
{
namespace Commands
{

QTextStream &operator<<(QTextStream &stream, const Command &cmd)
{
    for (QList<PartOfCommand>::const_iterator it = cmd.cmds.begin(); it != cmd.cmds.end(); ++it) {
        if (it != cmd.cmds.begin()) {
            stream << " ";
        }
        stream << *it;
    }
    return stream << endl;
}

TokenType howToTransmit(const QByteArray &str)
{
    if (str.length() > 100)
        return LITERAL;

    if (str.isEmpty())
        return QUOTED_STRING;

    TokenType res = ATOM;

    for (int i = 0; i < str.size(); ++i) {
        char c = str.at(i);

        if (!isalnum(c) && c != '-' && c != '_')
            res = QUOTED_STRING;

        if (!isascii(c) || c == '\r' || c == '\n' || c == '\0' || c == '"') {
            return LITERAL;
        }
    }
    return res;
}

QTextStream &operator<<(QTextStream &stream, const PartOfCommand &part)
{
    switch (part.kind) {
    case ATOM:
        stream << part.text;
        break;
    case QUOTED_STRING: {
        QByteArray item = part.text;
        item.replace('\\', "\\\\");
        stream << '"' << item << '"';
    }
    break;
    case LITERAL:
        stream << "{" << part.text.length() << "}" << endl << part.text;
        break;
    case IDLE:
        stream << "IDLE" << endl << "[Entering IDLE mode...]";
        break;
    case IDLE_DONE:
        stream << "DONE" << endl << "[Leaving IDLE mode...]";
        break;
    case STARTTLS:
        stream << "STARTTLS" << endl << "[Starting TLS...]";
        break;
    case COMPRESS_DEFLATE:
        stream << "COMPRESS DELFATE" << endl << "[Starting deflate...]";
        break;
    case ATOM_NO_SPACE_AROUND:
        stream << part.text;
    }
    return stream;
}

}
}
