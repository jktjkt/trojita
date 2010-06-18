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

#ifndef IMAP_MODEL_IDLELAUNCHER_H
#define IMAP_MODEL_IDLELAUNCHER_H

#include <QPointer>

class QTimer;

/** @short Namespace for IMAP interaction */
namespace Imap {

class Parser;

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

class Model;

class IdleLauncher: public QObject {
    Q_OBJECT
public:
    IdleLauncher( Model* model, Parser* ptr );

    /** @short Register the interest in launching the IDLE command after a delay */
    void enterIdleLater();
    /** @short Delay the IDLE invocation for a while

This function should be called when further communication via the socket suggests
that the IDLE command should be postponed
*/
    void postponeIdleIfActive();
public slots:
    /** @short Immediately send the IDLE command to the parser */
    void slotEnterIdleNow();
    /** @short Inform the IDLE launcher that the IDLE command got terminated */
    void idlingTerminated();
    /** @short Re-start the IDLE command which we had to abort because of a timeout */
    void resumeIdlingAfterTimeout();
private:
    Model* m;
    QPointer<Parser> parser;
    QTimer* delayedEnter;
    QTimer* renewal;
    bool _idling;
    bool _restartInProgress;

    IdleLauncher(const Imap::Mailbox::IdleLauncher&); // don't implement
    IdleLauncher& operator=(const Imap::Mailbox::IdleLauncher&); // don't implement
};

}

}

#endif /* IMAP_MODEL_IDLELAUNCHER_H */
