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

#include <QTimer>
#include "IdleLauncher.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

IdleLauncher::IdleLauncher( Model* model, Parser* ptr ):
        QObject(model), m(model), parser(ptr), _idling(false)
{
    delayedEnter = new QTimer( this );
    delayedEnter->setObjectName( QString::fromAscii("IdleLauncher-%1").arg( model->objectName() ) );
    delayedEnter->setSingleShot( true );
    delayedEnter->setInterval( 5000 );
    connect( delayedEnter, SIGNAL(timeout()), this, SLOT(slotEnterIdleNow()) );
}

void IdleLauncher::slotEnterIdleNow()
{
    if ( ! parser ) {
        delayedEnter->stop();
        return;
    }
    m->enterIdle( parser );
    _idling = true;
}

void IdleLauncher::slotIdlingTerminated()
{
    _idling = false;
}

void IdleLauncher::enterIdleLater()
{
    delayedEnter->start();
}

bool IdleLauncher::idling()
{
    return _idling;
}

}
}
