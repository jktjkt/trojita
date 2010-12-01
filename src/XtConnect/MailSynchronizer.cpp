/*
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

#include <QDebug>
#include "MailSynchronizer.h"
#include "Imap/Model/Model.h"
#include "MailboxFinder.h"

namespace XtConnect {

MailSynchronizer::MailSynchronizer( QObject *parent, Imap::Mailbox::Model *model, MailboxFinder *finder ) :
    QObject(parent), m_model(model), m_finder(finder)
{
    Q_ASSERT(m_model);
    connect( m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(slotRowsInserted(QModelIndex,int,int)) );
    connect( m_finder, SIGNAL(mailboxFound(QString,QModelIndex)), this, SLOT(slotMailboxFound(QString,QModelIndex)) );
}

void MailSynchronizer::setMailbox( const QString &mailbox )
{
    Q_ASSERT(m_finder);
    m_mailbox = mailbox;
    qDebug() << "Will watch mailbox" << mailbox;
    m_finder->addMailbox( mailbox );
}

void MailSynchronizer::slotMailboxFound( const QString &mailbox, const QModelIndex &index )
{
    if ( mailbox != m_mailbox )
        return;

    m_index = index;
    // FIXME: request list of messages, go through them
}


void MailSynchronizer::slotRowsInserted(const QModelIndex &parent, int start, int end)
{
    // check the mailbox's list, if it's the same index as the "parent" we just got passed
    if ( parent.parent() != m_index )
        return;

    qDebug() << Q_FUNC_INFO << parent << start << end;
}

}
