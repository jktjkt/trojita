/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_MODEL_TASKFACTORY_H
#define IMAP_MODEL_TASKFACTORY_H

#include <memory>
#include <QMap>
#include <QModelIndex>
#include "CatenateData.h"
#include "CopyMoveOperation.h"
#include "FlagsOperation.h"
#include "SubscribeUnSubscribeOperation.h"
#include "UidSubmitData.h"

namespace Imap
{
class Parser;
namespace Mailbox
{

class AppendTask;
class CopyMoveMessagesTask;
class CreateMailboxTask;
class DeleteMailboxTask;
class EnableTask;
class ExpungeMailboxTask;
class FetchMsgMetadataTask;
class FetchMsgPartTask;
class GetAnyConnectionTask;
class IdTask;
class ImapTask;
class KeepMailboxOpenTask;
class ListChildMailboxesTask;
class NumberOfMessagesTask;
class ObtainSynchronizedMailboxTask;
class OpenConnectionTask;
class UpdateFlagsTask;
class ThreadTask;
class NoopTask;
class UnSelectTask;
class SortTask;
class SubscribeUnsubscribeTask;
class GenUrlAuthTask;
class UidSubmitTask;

class Model;
class TreeItemMailbox;
class TreeItemPart;

class TaskFactory
{
public:
    virtual ~TaskFactory();

    virtual CopyMoveMessagesTask *createCopyMoveMessagesTask(Model *model, const QModelIndexList &messages,
            const QString &targetMailbox, const CopyMoveOperation op);
    virtual CreateMailboxTask *createCreateMailboxTask(Model *model, const QString &mailbox);
    virtual DeleteMailboxTask *createDeleteMailboxTask(Model *model, const QString &mailbox);
    virtual EnableTask *createEnableTask(Model *model, ImapTask *dependingTask, const QList<QByteArray> &extensions);
    virtual ExpungeMailboxTask *createExpungeMailboxTask(Model *model, const QModelIndex &mailbox);
    virtual FetchMsgMetadataTask *createFetchMsgMetadataTask(Model *model, const QModelIndex &mailbox, const QList<uint> &uid);
    virtual FetchMsgPartTask *createFetchMsgPartTask(Model *model, const QModelIndex &mailbox, const QList<uint> &uids, const QStringList &parts);
    virtual GetAnyConnectionTask *createGetAnyConnectionTask(Model *model);
    virtual IdTask *createIdTask(Model *model, ImapTask *dependingTask);
    virtual KeepMailboxOpenTask *createKeepMailboxOpenTask(Model *model, const QModelIndex &mailbox, Parser *oldParser);
    virtual ListChildMailboxesTask *createListChildMailboxesTask(Model *model, const QModelIndex &mailbox);
    virtual NumberOfMessagesTask *createNumberOfMessagesTask(Model *model, const QModelIndex &mailbox);
    virtual ObtainSynchronizedMailboxTask *createObtainSynchronizedMailboxTask(Model *model, const QModelIndex &mailboxIndex,
            ImapTask *parentTask, KeepMailboxOpenTask *keepTask);
    virtual OpenConnectionTask *createOpenConnectionTask(Model *model);
    virtual UpdateFlagsTask *createUpdateFlagsTask(Model *model, const QModelIndexList &messages, const FlagsOperation flagOperation,
            const QString &flags);
    virtual UpdateFlagsTask *createUpdateFlagsTask(Model *model, CopyMoveMessagesTask *copyTask,
            const QList<QPersistentModelIndex> &messages, const FlagsOperation flagOperation,
            const QString &flags);
    virtual ThreadTask *createThreadTask(Model *model, const QModelIndex &mailbox, const QByteArray &algorithm, const QStringList &searchCriteria);
    virtual NoopTask *createNoopTask(Model *model, ImapTask *parentTask);
    virtual UnSelectTask *createUnSelectTask(Model *model, ImapTask *parentTask);
    virtual SortTask *createSortTask(Model *model, const QModelIndex &mailbox, const QStringList &searchConditions, const QStringList &sortCriteria);
    virtual AppendTask *createAppendTask(Model *model, const QString &targetMailbox, const QByteArray &rawMessageData,
                                         const QStringList &flags, const QDateTime &timestamp);
    virtual AppendTask *createAppendTask(Model *model, const QString &targetMailbox, const QList<CatenatePair> &data,
                                         const QStringList &flags, const QDateTime &timestamp);
    virtual SubscribeUnsubscribeTask *createSubscribeUnsubscribeTask(Model *model, const QModelIndex &mailbox,
                                                                     const SubscribeUnsubscribeOperation operation);
    virtual GenUrlAuthTask *createGenUrlAuthTask(Model *model, const QString &host, const QString &user, const QString &mailbox,
                                                 const uint uidValidity, const uint uid, const QString &part, const QString &access);
    virtual UidSubmitTask *createUidSubmitTask(Model *model, const QString &mailbox, const uint uidValidity, const uint uid,
                                               const UidSubmitOptionsList &submitOptions);
};

class TestingTaskFactory: public TaskFactory
{
public:
    TestingTaskFactory();
    virtual OpenConnectionTask *createOpenConnectionTask(Model *model);
    virtual ListChildMailboxesTask *createListChildMailboxesTask(Model *model, const QModelIndex &mailbox);
    bool fakeOpenConnectionTask;
    bool fakeListChildMailboxes;
    QMap<QString,QStringList> fakeListChildMailboxesMap;
private:
    Parser *newParser(Model *model);
};

typedef std::auto_ptr<TaskFactory> TaskFactoryPtr;

}
}

#endif // IMAP_MODEL_TASKFACTORY_H
