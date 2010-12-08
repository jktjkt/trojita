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

#ifndef IMAP_OBTAINSYNCHRONIZEDMAILBOXTASK_H
#define IMAP_OBTAINSYNCHRONIZEDMAILBOXTASK_H

#include "ImapTask.h"
#include <QModelIndex>
#include "../Model/Model.h"

namespace Imap {
namespace Mailbox {

class TreeItemMailbox;

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
    ObtainSynchronizedMailboxTask( Model* _model, const QModelIndex& _mailboxIndex, ImapTask* parentTask );
    virtual void perform();
    virtual bool handleStateHelper( Imap::Parser* ptr, const Imap::Responses::State* const resp );
    virtual bool handleNumberResponse( Imap::Parser* ptr, const Imap::Responses::NumberResponse* const resp );
    virtual bool handleFlags( Imap::Parser* ptr, const Imap::Responses::Flags* const resp );
    virtual bool handleSearch( Imap::Parser* ptr, const Imap::Responses::Search* const resp );
    virtual bool handleFetch( Imap::Parser* ptr, const Imap::Responses::Fetch* const resp );

    typedef enum { UID_SYNC_ALL, UID_SYNC_ONLY_NEW } UidSyncingMode;

    virtual QString debugIdentification() const;

private:
    void _finalizeSelect();
    void _fullMboxSync( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState );
    void _syncNoNewNoDeletions( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState, const QList<uint>& seqToUid );
    void _syncOnlyDeletions( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState );
    void _syncOnlyAdditions( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState, const SyncState& oldState );
    void _syncGeneric( TreeItemMailbox* mailbox, TreeItemMsgList* list, const SyncState& syncState );

    void _finalizeUidSyncAll( TreeItemMailbox* mailbox );
    static void _finalizeUidSyncOnlyNew( Model *model, TreeItemMailbox* mailbox, const uint oldExists, QList<uint> &uidMap );

    void syncUids( TreeItemMailbox* mailbox, const uint lowestUidToQuery=0 );
    void syncFlags( TreeItemMailbox* mailbox );

    bool handleResponseCodeInsideState( const Imap::Responses::State* const resp );

private:
    ImapTask* conn;
    QPersistentModelIndex mailboxIndex;
    CommandHandle selectCmd;
    CommandHandle uidSyncingCmd;
    CommandHandle flagsCmd;
    Imap::Mailbox::MailboxSyncingProgress status;
    UidSyncingMode uidSyncingMode;
    QList<uint> uidMap;

    friend class KeepMailboxOpenTask; // needs access to conn because it wants to re-use its parser, yay
};

}
}

#endif // IMAP_OBTAINSYNCHRONIZEDMAILBOXTASK_H
