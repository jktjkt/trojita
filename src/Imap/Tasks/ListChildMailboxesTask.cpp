/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "ListChildMailboxesTask.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "GetAnyConnectionTask.h"
#include "NumberOfMessagesTask.h"

namespace Imap
{
namespace Mailbox
{


ListChildMailboxesTask::ListChildMailboxesTask(Model *model, const QModelIndex &mailbox):
    ImapTask(model), mailboxIndex(mailbox)
{
    Q_ASSERT(dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailbox.internalPointer())));
    conn = model->m_taskFactory->createGetAnyConnectionTask(model);
    conn->addDependentTask(this);
}

ListChildMailboxesTask::~ListChildMailboxesTask()
{
    qDeleteAll(m_pendingStatusResponses);
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
        mailboxName = QLatin1String("%");
    else
        mailboxName += mailbox->separator() + QLatin1Char('%');

    QStringList returnOptions;
    if (model->accessParser(parser).capabilitiesFresh) {
        if (model->accessParser(parser).capabilities.contains(QLatin1String("LIST-EXTENDED"))) {
            returnOptions << QLatin1String("SUBSCRIBED") << QLatin1String("CHILDREN");
        }
        if (model->accessParser(parser).capabilities.contains(QLatin1String("LIST-STATUS"))) {
            returnOptions << QString("STATUS (%1)").arg(NumberOfMessagesTask::requestedStatusOptions().join(QLatin1String(" ")));
        }
    }
    tag = parser->list("", mailboxName, returnOptions);
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
                model->finalizeList(parser, mailbox);
                applyCachedStatus();
                _completed();
            } else {
                applyCachedStatus();
                _failed("LIST failed");
                // FIXME: error handling
            }
        } else {
            applyCachedStatus();
            _failed("Mailbox no longer available -- weird timing?");
            // FIXME: error handling
        }
        return true;
    } else {
        return false;
    }
}

/** @short Defer processing of the STATUS responses until after all of the LISTs are processed */
bool ListChildMailboxesTask::handleStatus(const Imap::Responses::Status *const resp)
{
    if (!mailboxIndex.isValid())
        return false;

    if (!resp->mailbox.startsWith(mailboxIndex.data(RoleMailboxName).toString())) {
        // not our data
        return false;
    }

    // Got to cache these responses until we can apply them
    m_pendingStatusResponses << new Imap::Responses::Status(*resp);
    return true;
}

/** @short Send the cached STATUS responses to the Model */
void ListChildMailboxesTask::applyCachedStatus()
{
    Q_FOREACH(Imap::Responses::Status *resp, m_pendingStatusResponses) {
        resp->plug(parser, model);
        delete resp;
    }
    m_pendingStatusResponses.clear();
}

QString ListChildMailboxesTask::debugIdentification() const
{
    if (! mailboxIndex.isValid())
        return QLatin1String("[invalid mailboxIndex]");

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    return QString::fromUtf8("Listing stuff below mailbox %1").arg(mailbox->mailbox());
}

QVariant ListChildMailboxesTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Listing mailboxes")) : QVariant();
}


}
}
