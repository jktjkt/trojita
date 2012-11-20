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

#include "MailboxFinder.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/ItemRoles.h"

namespace XtConnect {

MailboxFinder::MailboxFinder( QObject *parent, Imap::Mailbox::Model *model ) :
    QObject(parent), m_model(model)
{
    Q_ASSERT(m_model);
    connect( m_model, SIGNAL(layoutChanged()), this, SLOT(checkArrivals()) );
    connect( m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(slotRowsInserted(QModelIndex,int,int)) );
}

void MailboxFinder::addMailbox( const QString &mailbox )
{
    m_watchedNames.append(mailbox);
    QTimer::singleShot(0, this, SLOT(checkArrivals()));
}

void MailboxFinder::checkArrivals()
{
    Q_FOREACH( const QString &mailbox, m_watchedNames ) {
        QModelIndex root;
        bool cont = false;

        // Simply use the MVC API to find an interesting object
        do {
            cont = false;
            int rowCount = m_model->rowCount( root );
            if ( rowCount < 2 ) {
                // remember, the first one is list of messages!
                break;
            }
            // ...so the iteration really starts at 1. We go over all mailboxes which are children of current root.
            for ( int i = 1; i < rowCount; ++i ) {
                const QModelIndex index = m_model->index( i, 0, root );
                const QString possibleName = m_model->data( index, Imap::Mailbox::RoleMailboxName ).toString();
                const QString separator = m_model->data( index, Imap::Mailbox::RoleMailboxSeparator ).toString();

                if ( possibleName.isEmpty() && separator.isEmpty() ) {
                    // This shoudln't really happen
                    m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("MailboxFinder"),
                                      QLatin1String("Weird, there's a mailbox with no name and no separator. Avoiding!"));
                    continue;
                }

                if ( possibleName == mailbox ) {
                    // found it
                    m_watchedNames.removeAll( mailbox );
                    emit mailboxFound( mailbox, index );
                    break;
                } else if ( mailbox.startsWith( possibleName + separator ) ) {
                    // we know where to go
                    root = index;
                    cont = true;
                    break;
                }
            }
        } while ( cont );
    }
}

void MailboxFinder::slotRowsInserted( const QModelIndex &parent, int start, int end )
{
    // "something got inserted". It's enough to simply trigger the finder in checkArrivals().
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);
    checkArrivals();
}

}
