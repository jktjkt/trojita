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

#ifndef IMAP_OPENMAILBOXTASK_H
#define IMAP_OPENMAILBOXTASK_H

#include "ImapTask.h"

namespace Imap {
namespace Mailbox {

/** @short A task responsible for providing a fully synchronized connection to the IMAP server

When this class emits its completed() signal, the provided Imap::Parser* is guaranteed to
have a valid mailbox opened and the state of the connection should be fully synchronized,
that is, all UIDs are already known etc.

*/
class StartTlsTask: public ImapTask
{
Q_OBJECT
public:
    StartTlsTask( Model* _model, Imap::Parser* _parser, const CommandHandle& _parentTask );
    virtual void perform();
public slots:
    virtual void abort();
    virtual void upstreamFailed();
private:
};

}
}

#endif // IMAP_OPENMAILBOXTASK_H
