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


#include "Fake_ListChildMailboxesTask.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/TaskFactory.h"
#include "GetAnyConnectionTask.h"

namespace Imap
{
namespace Mailbox
{


Fake_ListChildMailboxesTask::Fake_ListChildMailboxesTask(Model *model, const QModelIndex &mailbox):
    ListChildMailboxesTask(model, mailbox)
{
    Q_ASSERT(dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailbox.internalPointer())));
}

void Fake_ListChildMailboxesTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    parser = conn->parser;
    QList<Responses::List> &listResponses = model->accessParser(parser).listResponses;
    Q_ASSERT(listResponses.isEmpty());
    TestingTaskFactory *factory = dynamic_cast<TestingTaskFactory *>(model->m_taskFactory.get());
    Q_ASSERT(factory);
    for (QMap<QString, QStringList>::const_iterator it = factory->fakeListChildMailboxesMap.constBegin();
         it != factory->fakeListChildMailboxesMap.constEnd(); ++it) {
        if (it.key() != mailbox->mailbox())
            continue;
        for (QStringList::const_iterator childIt = it->begin(); childIt != it->end(); ++childIt) {
            QString childMailbox = mailbox->mailbox().isEmpty() ? *childIt : QString::fromUtf8("%1^%2").arg(mailbox->mailbox(), *childIt);
            listResponses.append(Responses::List(Responses::LIST, QStringList(), QLatin1String("^"), childMailbox, QMap<QByteArray, QVariant>()));
        }
    }
    model->finalizeList(parser, mailbox);
    _completed();
}

bool Fake_ListChildMailboxesTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    // This is a fake task inheriting from the "real one", so we have to reimplement functions which do real work with stubs
    Q_UNUSED(resp);
    return false;
}


}
}
