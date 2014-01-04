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

#ifndef IMAP_MODEL_PARSERSTATE_H
#define IMAP_MODEL_PARSERSTATE_H

#include <QPointer>
#include "../ConnectionState.h"
#include "../Parser/Parser.h"

namespace Imap {
class Parser;
namespace Mailbox {
class ImapTask;
class KeepMailboxOpenTask;

/** @short Helper structure for keeping track of each parser's state */
struct ParserState {
    /** @short Which parser are we talking about here */
    QPointer<Parser> parser;
    /** @short The mailbox which we'd like to have selected */
    ConnectionState connState;
    /** @short The logout command */
    CommandHandle logoutCmd;
    /** @short List of tasks which are active already, and should therefore receive events */
    QList<ImapTask *> activeTasks;
    /** @short An active KeepMailboxOpenTask, if one exists */
    KeepMailboxOpenTask *maintainingTask;
    /** @short A list of cepabilities, as advertised by the server */
    QStringList capabilities;
    /** @short Is the @arg capabilities usable? */
    bool capabilitiesFresh;
    /** @short LIST responses which were not processed yet */
    QList<Responses::List> listResponses;

    /** @short Is the connection currently being processed? */
    int processingDepth;

    ParserState(Parser *parser);
    ParserState();
};

}
}

#endif /* IMAP_MODEL_PARSERSTATE_H */
