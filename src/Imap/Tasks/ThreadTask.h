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

#ifndef IMAP_THREAD_TASK_H
#define IMAP_THREAD_TASK_H

#include <QPersistentModelIndex>
#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Send the THREAD command with requested parameters  */
class ThreadTask : public ImapTask
{
    Q_OBJECT
public:

    /** @short Is this a regular threading request, or an incremental operation? */
    typedef enum {
        /** @short Regular threading request */
        THREADING_REGULAR,
        /** @short Incremental request */
        THREADING_INCREMENTAL
    } ThreadingIncrementalMode;

    ThreadTask(Model *model, const QModelIndex &mailbox, const QByteArray &algorithm, const QStringList &searchCriteria,
               const ThreadingIncrementalMode incrementalMode = THREADING_REGULAR);
    virtual void perform();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual bool handleThread(const Imap::Responses::Thread *const resp);
    virtual bool handleESearch(const Responses::ESearch *const resp);
    virtual QVariant taskData(const int role) const;
    virtual bool needsMailbox() const {return true;}
signals:
    /** @short An incremental update to the threading as per draft-imap-incthread */
    void incrementalThreadingAvailable(const uint previousThreadRoot, const QVector<Imap::Responses::ThreadingNode> &mapping);
protected:
    virtual void _failed(const QString &errorMessage);
private:
    CommandHandle tag;
    ImapTask *conn;
    QPersistentModelIndex mailboxIndex;
    QByteArray algorithm;
    QStringList searchCriteria;
    QVector<Imap::Responses::ThreadingNode> mapping;
    bool m_incrementalMode;
};

}
}

#endif // IMAP_THREAD_TASK_H
