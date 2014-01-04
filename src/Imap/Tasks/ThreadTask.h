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
    void incrementalThreadingAvailable(const Responses::ESearch::IncrementalThreadingData_t &update);
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
