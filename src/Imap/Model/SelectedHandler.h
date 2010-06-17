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
#ifndef SELECTEDHANDLER_H
#define SELECTEDHANDLER_H

#include "Model.h"

namespace Imap {
namespace Mailbox {

class SelectedHandler : public ModelStateHandler
{
    Q_OBJECT
public:
    SelectedHandler( Model* _m );
    virtual void handleState( Imap::Parser* ptr, const Imap::Responses::State* const resp );
    virtual void handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp );
    virtual void handleList( Imap::Parser* ptr, const Imap::Responses::List* const resp );
    virtual void handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp );
    virtual void handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp );
    virtual void handleSort( Imap::Parser* ptr, const Imap::Responses::Sort* const resp );
    virtual void handleThread( Imap::Parser* ptr, const Imap::Responses::Thread* const resp );
    virtual void handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp );
};

}
}

#endif // SELECTEDHANDLER_H
