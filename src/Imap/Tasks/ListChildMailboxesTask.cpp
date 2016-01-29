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
    ImapTask(model), mailboxIndex(mailbox), mailboxIsRootMailbox(!mailbox.isValid())
{
    Q_ASSERT(!mailbox.isValid() || dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailbox.internalPointer())));
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

    if (!mailboxIsRootMailbox && !mailboxIndex.isValid()) {
        // FIXME: add proper fix
        _failed(tr("Mailbox vanished before we could ask for its children"));
        return;
    }
    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(model->translatePtr(mailboxIndex)));
    Q_ASSERT(mailbox);

    QString mailboxName = mailbox->mailbox();
    if (mailboxName.isNull())
        mailboxName = QStringLiteral("%");
    else
        mailboxName += mailbox->separator() + QLatin1Char('%');

    QStringList returnOptions;
    if (model->accessParser(parser).capabilitiesFresh) {
        if (model->accessParser(parser).capabilities.contains(QStringLiteral("LIST-EXTENDED"))) {
            returnOptions << QStringLiteral("SUBSCRIBED") << QStringLiteral("CHILDREN");
        }
        if (model->accessParser(parser).capabilities.contains(QStringLiteral("LIST-STATUS"))) {
            returnOptions << QStringLiteral("STATUS (%1)").arg(NumberOfMessagesTask::requestedStatusOptions().join(QStringLiteral(" ")));
        }
    }
    // empty string, not a null string
    tag = parser->list(QLatin1String(""), mailboxName, returnOptions);
}

bool ListChildMailboxesTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    if (resp->tag.isEmpty())
        return false;

    if (resp->tag == tag) {

        if (mailboxIndex.isValid() || mailboxIsRootMailbox) {
            TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(model->translatePtr(mailboxIndex)));
            Q_ASSERT(mailbox);

            if (resp->kind == Responses::OK) {
                model->finalizeList(parser, mailbox);
                applyCachedStatus();
                _completed();
            } else {
                applyCachedStatus();
                _failed(tr("LIST failed"));
            }
        } else {
            applyCachedStatus();
            _failed(tr("Mailbox no longer available -- weird timing?"));
        }
        return true;
    } else {
        return false;
    }
}

/** @short Defer processing of the STATUS responses until after all of the LISTs are processed */
bool ListChildMailboxesTask::handleStatus(const Imap::Responses::Status *const resp)
{
    if (!mailboxIndex.isValid() && !mailboxIsRootMailbox)
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
    if (!mailboxIndex.isValid() && !mailboxIsRootMailbox)
        return QStringLiteral("[invalid mailboxIndex]");

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(model->translatePtr(mailboxIndex)));
    Q_ASSERT(mailbox);
    return QStringLiteral("Listing stuff below mailbox %1").arg(mailbox->mailbox());
}

QVariant ListChildMailboxesTask::taskData(const int role) const
{
    return role == RoleTaskCompactName ? QVariant(tr("Listing mailboxes")) : QVariant();
}

void ListChildMailboxesTask::_failed(const QString &errorMessage)
{
    if (mailboxIsRootMailbox || mailboxIndex.isValid()) {
        TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(model->translatePtr(mailboxIndex)));
        mailbox->setFetchStatus(TreeItem::UNAVAILABLE);
        QModelIndex index = mailbox->toIndex(model);
        emit model->dataChanged(index, index);
    }
    ImapTask::_failed(errorMessage);
}


}
}
