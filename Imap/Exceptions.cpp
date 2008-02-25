/* Copyright (C) 2008 Jan Kundrát <jkt@gentoo.org>

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
#include "Exceptions.h"

namespace Imap {

const char* Exception::what( const int context ) const throw()
{
    if ( _offset == -1 )
        return _msg.c_str();
    else {
        QByteArray out(_msg.c_str());
        out += " when parsing this:\n…";
        out += _line.mid( qMax(0, _offset - context), 2 * context );
        out += "…\n";
        out += QByteArray( 1+qMax(0, _offset - context) + _offset, ' ' );
        out += "^ here\n";
        return out.constData();
    }
}

}
