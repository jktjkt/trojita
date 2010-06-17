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

#include "ModelUpdaters.h"
#include "Model.h"

namespace Imap {
namespace Mailbox {

_MailboxListUpdater::_MailboxListUpdater( Model* model, TreeItemMailbox* mailbox, const QList<TreeItem*>& children ):
        QObject(model), _model(model), _mailbox(mailbox), _children(children)
{
}

void _MailboxListUpdater::perform()
{
    _model->replaceChildMailboxes( _mailbox, _children );
    deleteLater();
}

_NumberOfMessagesUpdater::_NumberOfMessagesUpdater( Model* model, TreeItemMailbox* mailbox ):
        QObject(model), _model(model), _mailbox(mailbox)
{
}

void _NumberOfMessagesUpdater::perform()
{
    _model->emitMessageCountChanged( _mailbox );
}


}
}
