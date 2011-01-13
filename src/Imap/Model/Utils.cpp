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
#include "Utils.h"
#include <cmath>

namespace Imap {
namespace Mailbox {

QString PrettySize::prettySize( uint bytes )
{
    if ( bytes == 0 )
        return tr("0");
    int order = std::log( static_cast<double>(bytes) ) / std::log( 1024.0 );
    QString suffix;
    if ( order <= 0 )
        return QString::number( bytes );
    else if ( order == 1 )
        suffix = tr("kB");
    else if ( order == 2 )
        suffix = tr("MB");
    else if ( order == 3 )
        suffix = tr("GB");
    else
        suffix = tr("TB"); // shame on you for such mails
    return tr("%1 %2").arg( QString::number(
            bytes / ( std::pow( 1024.0, order ) ),
            'f', 1 ), suffix );
}


}

}
