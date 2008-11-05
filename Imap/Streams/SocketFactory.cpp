/* Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QProcess>
#include "SocketFactory.h"
#include "IODeviceSocket.h"

namespace Imap {
namespace Mailbox {

ProcessSocketFactory::ProcessSocketFactory(
        const QString& executable, const QStringList& args):
    _executable(executable), _args(args)
{
}

Imap::SocketPtr ProcessSocketFactory::create()
{
    // FIXME: this may leak memory if an exception strikes in this function
    // (before we return the pointer)
    QProcess* proc = new QProcess();
    proc->start( _executable, _args );
    if ( ! proc->waitForStarted() )
        return Imap::SocketPtr( 0 );

    return Imap::SocketPtr( new IODeviceSocket( proc ) );
}

}
}
