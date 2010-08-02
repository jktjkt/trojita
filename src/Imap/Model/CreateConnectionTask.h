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

#ifndef IMAP_CREATECONNECTIONTASK_H
#define IMAP_CREATECONNECTIONTASK_H

#include "ImapTask.h"

namespace Imap {
namespace Mailbox {

class TreeItemMailbox;

/** @short Establish an authenticated connection to the IMAP server

Upon its construction, this class will obtain a connection to the specified
mailbox. If it subsequently emits the completed() slide, you can use its parser
argument as a handle to a parser pointing to a fully synchronized mailbox.

FIXME: this is actually still not true ATM, the mailbox might be left in a
semi-synchronized state. Will be fixed later.
*/
class CreateConnectionTask : public ImapTask
{
Q_OBJECT
public:
    CreateConnectionTask( Model* _model, TreeItemMailbox* mailbox, bool forceResync=false );
    virtual void perform();

    virtual bool handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp );

private slots:
    void _hackSignalCompletion();

public:
    Parser* parser;

private:
    bool immediately;
    friend class Model; // temporary hack: for access to _completed();
};

}
}

#endif // IMAP_CREATECONNECTIONTASK_H
