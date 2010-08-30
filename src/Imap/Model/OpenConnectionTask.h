/* Copyright (C) 2007 - 2010 Jan Kundrát <jkt@flaska.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef IMAP_OPENCONNECTIONTASK_H
#define IMAP_OPENCONNECTIONTASK_H

#include "ImapTask.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

class TreeItemMailbox;

/** @short Open new connection to the IMAP server, determine capabilities and login

This Task is responsible for creating new connection to the remote server, optionally
establishing encryption if requested or needed, "determining capabilities" (using what
the server tells us, or asking for an update if we aren't satisfied) and finally taking
care of proper authentication.
*/
class OpenConnectionTask : public ImapTask
{
Q_OBJECT
public:
    OpenConnectionTask( Model* _model );
    virtual void perform();

    virtual bool handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp );

public:
    Parser* parser;
    CommandHandle startTlsCmd;
    CommandHandle capabilityCmd;
};

}
}

#endif // IMAP_OPENCONNECTIONTASK_H
