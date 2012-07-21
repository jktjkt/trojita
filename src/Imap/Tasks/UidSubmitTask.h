/* Copyright (C) 2007 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_UIDSUBMIT_TASK_H
#define IMAP_UIDSUBMIT_TASK_H

#include <QPersistentModelIndex>
#include "ImapTask.h"
#include "Imap/Model/CatenateData.h"

namespace Imap
{
namespace Mailbox
{

/** @short Submit the specified message through the UID SUBMIT command */
class UidSubmitTask : public ImapTask
{
    Q_OBJECT
public:
    UidSubmitTask(Model *model, const QString &mailbox, const uint uidValidity, const uint uid,
                  const UidSubmitOptionsList &submitOptions);
    virtual void perform();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual bool needsMailbox() const {return true;}
    virtual QVariant taskData(const int role) const;

private:
    ImapTask *conn;
    CommandHandle tag;
    QPersistentModelIndex m_mailboxIndex;
    uint m_uidValidity;
    uint m_uid;
    UidSubmitOptionsList m_options;
};

}
}

#endif // IMAP_UIDSUBMIT_TASK_H
