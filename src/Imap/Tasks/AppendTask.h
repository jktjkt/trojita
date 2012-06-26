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

#ifndef IMAP_APPENDTASK_H
#define IMAP_APPENDTASK_H

#include "ImapTask.h"

namespace Imap
{
namespace Mailbox
{

/** @short Append message to a mailbox */
class AppendTask : public ImapTask
{
    Q_OBJECT
public:
    AppendTask(Model *model, const QString &targetMailbox, const QByteArray &rawMessageData, const QStringList &flags,
               const QDateTime &timestamp);
    virtual void perform();

    virtual bool handleStateHelper(const Imap::Responses::State *const resp);
    virtual bool needsMailbox() const {return false;}
    virtual QVariant taskData(const int role) const;

private:
    ImapTask *conn;
    CommandHandle tag;
    QString targetMailbox;
    QByteArray rawMessageData;
    QStringList flags;
    QDateTime timestamp;
};

}
}

#endif // IMAP_APPENDTASK_H
