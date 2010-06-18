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
    void enterIdleLater();
    bool idling();
public slots:
    void slotEnterIdleNow();
    void slotIdlingTerminated();
private:
    Model* m;
    QPointer<Parser> parser;
    QTimer* delayedEnter;
    bool _idling;

    IdleLauncher(const Imap::Mailbox::IdleLauncher&); // don't implement
    IdleLauncher& operator=(const Imap::Mailbox::IdleLauncher&); // don't implement
};

}

}

#endif /* IMAP_MODEL_IDLELAUNCHER_H */
