/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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

#ifndef IMAP_TASK_IDLELAUNCHER_H
#define IMAP_TASK_IDLELAUNCHER_H

#include <QPointer>

class QTimer;

namespace Imap {

class Parser;

namespace Mailbox {

class KeepMailboxOpenTask;

/** @short Automatically launch and maintain the IDLE command

This class servers as a helper for the KeepMailboxOpenTask task. Its responsibility
is to watch the parser for being "idle" (ie. no commands flowing from upper layers)
and automatically launching the IDLE command, watching for its interruption (perhaps
by other commands) and restarting it when the parser is idling again.
*/
class IdleLauncher: public QObject {
    Q_OBJECT
public:
    /** @short Create the IdleLauncher

This function should only be called when the KeepMailboxOpenTask has selected the
target mailbox and all member variables are set up.
*/
    IdleLauncher( KeepMailboxOpenTask* parent );

    /** @short Register the interest in launching the IDLE command after a delay */
    void enterIdleLater();

    /** @short Prevent any further idling */
    void die();

    /** @short Break the IDLE command */
    void finishIdle();

    /** @short Inform the launcher that IDLE got terminated. It won't be restarted yet. */
    void idleTerminated();
public slots:
    /** @short Immediately send the IDLE command to the parser */
    void slotEnterIdleNow();
    /** @short Abort the IDLE command because it's been running for too long */
    void slotTerminateLongIdle();
private:
    KeepMailboxOpenTask* task;
    QTimer* delayedEnter;
    QTimer* renewal;
    bool _idling;

    IdleLauncher(const Imap::Mailbox::IdleLauncher&); // don't implement
    IdleLauncher& operator=(const Imap::Mailbox::IdleLauncher&); // don't implement
};

}

}

#endif /* IMAP_TASK_IDLELAUNCHER_H */
