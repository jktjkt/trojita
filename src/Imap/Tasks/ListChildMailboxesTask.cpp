/* Copyright (C) 2007 - 2011 Jan Kundrát <jkt@flaska.net>

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


#include "ListChildMailboxesTask.h"
#include "GetAnyConnectionTask.h"
#include "Model.h"
#include "MailboxTree.h"

namespace Imap
{
namespace Mailbox
{


ListChildMailboxesTask::ListChildMailboxesTask(Model *model, const QModelIndex &mailbox):
    ImapTask(model), mailboxIndex(mailbox)
{
    TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailbox.internalPointer()));
    Q_ASSERT(mailboxPtr);
    conn = model->_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

void ListChildMailboxesTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    if (! mailboxIndex.isValid()) {
        // FIXME: add proper fix
        _failed("Mailbox vanished before we could ask for its children");
        return;
    }
    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);

    QString mailboxName = mailbox->mailbox();
    if (mailboxName.isNull())
        mailboxName = QString::fromAscii("%");
    else
        mailboxName += mailbox->separator() + QChar('%');
    tag = parser->list("", mailboxName);
}

bool ListChildMailboxesTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (mailboxIndex.isValid()) {
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
            Q_ASSERT(mailbox);

            if (resp->kind == Responses::OK) {
                model->_finalizeList(parser, mailbox);
                _completed();
            } else {
                _failed("LIST failed");
                // FIXME: error handling
            }
        } else {
            _failed("Mailbox no longer available -- weird timing?");
            // FIXME: error handling
        }
        return true;
    } else {
        return false;
    }
}

QString ListChildMailboxesTask::debugIdentification() const
{
    if (! mailboxIndex.isValid())
        return QString::fromAscii("[invalid mailboxIndex]");

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    return QString::fromAscii("Listing stuff below mailbox %1").arg(mailbox->mailbox());
}


}
}
