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
#ifndef IMAP_COMMAND_H
#define IMAP_COMMAND_H

#include <QDateTime>
#include <QList>
#include <QTextStream>

/** @short Namespace for IMAP interaction */
namespace Imap
{

// Forward required for friend declaration
class Parser;
class CommandResult;

QTextStream &operator<<(QTextStream &stream, const CommandResult &r);

/** @short Namespace holding all supported IMAP commands and various helpers */
namespace Commands
{

/** Enumeration that specifies required method of transmission of this string */
enum TokenType {
    ATOM /**< Don't use any extra encoding, just send it directly, perhaps because it's already encoded */,
    QUOTED_STRING /**< Transmit using double-quotes */,
    LITERAL /**< Don't bother with checking this data, always use literal form */,
    IDLE, /**< Special case: IDLE command */
    IDLE_DONE, /**< Special case: the DONE for finalizing the IDLE command */
    STARTTLS, /**< Special case: STARTTLS */
    COMPRESS_DEFLATE, /**< Special case: COMPRESS DEFLATE */
    ATOM_NO_SPACE_AROUND /**< Do not add extra space either before or after this part */
};

/** @short Checks if we can use a quoted-string form for transmitting this string.
 *
 * We have to use literals for transmitting strings that are either too
 * long (as that'd cause problems with servers using too small line buffers),
 * contains CR, LF, zero byte or any characters outside of 7-bit ASCII range.
 */
TokenType howToTransmit(const QByteArray &str);

/** @short A part of the actual command.
 *
 * This is used by Parser to decide whether
 * to send the string as-is, to quote them or use a literal form for them.
 */
class PartOfCommand
{
    TokenType kind; /**< What encoding to use for this item */
    QByteArray text; /**< Actual text to send */
    bool numberSent;

    friend QTextStream &operator<<(QTextStream &stream, const PartOfCommand &c);
    friend class ::Imap::Parser;

public:
    /** Default constructor */
    PartOfCommand(const TokenType kind, const QByteArray &text): kind(kind), text(text), numberSent(false) {}
    /** Constructor that guesses correct type for passed string */
    PartOfCommand(const QByteArray &text): kind(howToTransmit(text)), text(text), numberSent(false) {}
};

/** @short Abstract class for specifying what command to execute */
class Command
{
    friend QTextStream &operator<<(QTextStream &stream, const Command &c);
    friend class ::Imap::Parser;
    QList<PartOfCommand> cmds;
    int currentPart;
public:
    Command &operator<<(const PartOfCommand &part) { cmds << part; return *this; }
    Command &operator<<(const QByteArray &text) { cmds << PartOfCommand(text); return *this; }
    Command(): currentPart(0) {}
    explicit Command(const QByteArray &name): currentPart(0) { cmds << PartOfCommand(ATOM, name); }
    void addTag(const QByteArray &tag) { cmds.insert(0, PartOfCommand(ATOM, tag)); }
};

/** @short Used for dumping a command to debug stream */
QTextStream &operator<<(QTextStream &stream, const Command &cmd);

}
}
#endif /* IMAP_COMMAND_H */
