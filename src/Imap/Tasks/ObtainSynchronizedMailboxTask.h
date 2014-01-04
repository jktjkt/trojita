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

#ifndef IMAP_OBTAINSYNCHRONIZEDMAILBOXTASK_H
#define IMAP_OBTAINSYNCHRONIZEDMAILBOXTASK_H

#include "ImapTask.h"
#include <QModelIndex>
#include "../Model/Model.h"

namespace Imap
{
namespace Mailbox
{

class UnSelectTask;

/** @short Create a synchronized connection to the IMAP server

Upon creation, this class will obtain a connection to the IMAP
server (either by creating new one, or simply stealing one from
an already established one, open a mailbox, fully synchronize it
and when all of the above is done, simply declare itself completed.
*/
class ObtainSynchronizedMailboxTask : public ImapTask
{
    Q_OBJECT
public:
    ObtainSynchronizedMailboxTask(Model *model, const QModelIndex &mailboxIndex, ImapTask *parentTask, KeepMailboxOpenTask *keepTask);
    virtual void perform();
    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual bool handleNumberResponse(const Imap::Responses::NumberResponse *const resp);
    virtual bool handleFlags(const Imap::Responses::Flags *const resp);
    virtual bool handleSearch(const Imap::Responses::Search *const resp);
    virtual bool handleESearch(const Imap::Responses::ESearch *const resp);
    virtual bool handleFetch(const Imap::Responses::Fetch *const resp);
    virtual bool handleVanished(const Imap::Responses::Vanished *const resp);
    virtual bool handleEnabled(const Responses::Enabled * const resp);

    typedef enum { UID_SYNC_ALL, UID_SYNC_ONLY_NEW } UidSyncingMode;

    virtual void addDependentTask(ImapTask *task);

    virtual QString debugIdentification() const;
    virtual QVariant taskData(const int role) const;
    virtual bool needsMailbox() const {return false;}

private:
    void finalizeSelect();
    void fullMboxSync(TreeItemMailbox *mailbox, TreeItemMsgList *list);
    void syncNoNewNoDeletions(TreeItemMailbox *mailbox, TreeItemMsgList *list);
    void syncOnlyAdditions(TreeItemMailbox *mailbox, TreeItemMsgList *list);
    void syncGeneric(TreeItemMailbox *mailbox, TreeItemMsgList *list);

    void applyUids(TreeItemMailbox *mailbox);
    void finalizeSearch();

    void syncUids(TreeItemMailbox *mailbox, const uint lowestUidToQuery=0);
    void syncFlags(TreeItemMailbox *mailbox);
    void updateHighestKnownUid(TreeItemMailbox *mailbox, const TreeItemMsgList *list) const;

    void notifyInterestingMessages(TreeItemMailbox *mailbox);

    bool handleResponseCodeInsideState(const Imap::Responses::State *const resp);

    /** @short Check current mailbox for validty, and take an evasive action if it disappeared

      There's a problem when going online after an outage, where the underlying TreeItemMailbox could disappear.
      This function checks the index for validity, and queues a fake "unselect" task just to make sure that
      we get out of that mailbox as soon as possible. This task will also die() in such situation.

      See issue #88 for details.

      @returns true if the current response shall be consumed
    */
    bool dieIfInvalidMailbox();

private slots:
    /** @short We're now out of that mailbox, hurray! */
    void slotUnSelectCompleted();

private:
    ImapTask *conn;
    QPersistentModelIndex mailboxIndex;
    CommandHandle selectCmd;
    CommandHandle uidSyncingCmd;
    CommandHandle flagsCmd;
    QList<CommandHandle> newArrivalsFetch;
    Imap::Mailbox::MailboxSyncingProgress status;
    UidSyncingMode uidSyncingMode;
    QList<uint> uidMap;
    uint firstUnknownUidOffset;
    SyncState oldSyncState;
    bool m_usingQresync;

    /** @short An UNSELECT task, if active */
    UnSelectTask *unSelectTask;

    /** @short The KeepMailboxOpenTask for which we're working */
    KeepMailboxOpenTask *keepTaskChild;

    friend class KeepMailboxOpenTask; // needs access to conn because it wants to re-use its parser, yay
};

}
}

#endif // IMAP_OBTAINSYNCHRONIZEDMAILBOXTASK_H
