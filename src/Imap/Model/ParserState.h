/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@gentoo.org>

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
    QList<ImapTask*> activeTasks;
    /** @short An active KeepMailboxOpenTask, if one exists */
    KeepMailboxOpenTask* maintainingTask;
    /** @short A list of cepabilities, as advertised by the server */
    QStringList capabilities;
    /** @short Is the @arg capabilities usable? */
    bool capabilitiesFresh;
    /** @short LIST responses which were not processed yet */
    QList<Responses::List> listResponses;

    /** @short Is the connection currently being processed? */
    bool beingProcessed;

    ParserState(Parser *parser);
    ParserState();
};

/** @short Guards access to the ParserState

Slots which are connected to signals directly or indirectly connected to this Model could, unfortunately, re-enter the event
loop.  When this happens, other events could possibly get delivered leading to activation of Model's "dangerous" slots:

    - responseReceived()
    - slotParserDisconnected()
    - slotParseError()

These slots are dangerous because they have the potential of re-entering the event loop and also could delete the Parsers/Tasks.
Thhat's why we have to use a crude reference counter to guarantee that our objects aren't deleted on return from a nested event
loop until the slots triggered by the *outer* event loops have finished.

See https://projects.flaska.net/issues/467 for a tiny bit of detail.

*/
struct ParserStateGuard {
    ParserState &m_s;
    bool wasActive;
    ParserStateGuard(ParserState &s);
    ~ParserStateGuard();
};

}

}

#endif /* IMAP_MODEL_PARSERSTATE_H */
