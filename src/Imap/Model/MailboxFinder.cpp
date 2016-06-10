/*  Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>
    Certain enhancements (www.xtuple.com/trojita-enhancements)
    are copyright © 2010 by OpenMFG LLC, dba xTuple.  All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    - Neither the name of xTuple nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior
    written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
    ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "Common/InvokeMethod.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxFinder.h"
#include "Imap/Model/Model.h"

namespace Imap {

namespace Mailbox {

MailboxFinder::MailboxFinder(QObject *parent, QAbstractItemModel *model)
    : QObject(parent)
    , m_model(model)
{
    Q_ASSERT(m_model);
    Q_ASSERT_X(!qobject_cast<Imap::Mailbox::Model *>(model),
               "MailboxFinder", "This requires a proxy model which exports a filtered view, not the original Model");
    connect(m_model, &QAbstractItemModel::layoutChanged, this, &MailboxFinder::checkArrivals);
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &MailboxFinder::checkArrivals);
}

void MailboxFinder::addMailbox(const QString &mailbox)
{
    m_pending.insert(mailbox);
    EMIT_LATER_NOARG(this, checkArrivals);
}

void MailboxFinder::checkArrivals()
{
    Q_FOREACH(const QString &mailbox, m_pending) {
        QModelIndex root;
        bool cont = false;

        // Simply use the MVC API to find an interesting object
        do {
            cont = false;
            int rowCount = m_model->rowCount(root);

            for (int i = 0; i < rowCount; ++i) {
                const QModelIndex index = m_model->index(i, 0, root);
                const QString possibleName = m_model->data(index, Imap::Mailbox::RoleMailboxName).toString();
                const QString separator = m_model->data(index, Imap::Mailbox::RoleMailboxSeparator).toString();

                if (possibleName.isEmpty() && separator.isEmpty()) {
                    // This shouldn't really happen
                    qDebug() << "Skipping a mailbox with no name and no separator";
                    continue;
                }

                if (possibleName == mailbox) {
                    // found it
                    m_pending.remove(mailbox);
                    emit mailboxFound(mailbox, index);
                    break;
                } else if (mailbox.startsWith(possibleName + separator)) {
                    // we know where to go
                    root = index;
                    cont = true;
                    break;
                }
            }
        } while (cont);
    }
}

}
}
