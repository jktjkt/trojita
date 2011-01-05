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

#ifndef MAILBOXFINDER_H
#define MAILBOXFINDER_H

#include <QModelIndex>
#include <QStringList>

namespace Imap {
namespace Mailbox {
class Model;
}
}

namespace XtConnect {

/** @short Find model indexes for mailboxes

The API of the Model doesn't currently have a way of asking for a QModelIndex for a named mailbox.
This class fills that gap.

To start, call the addMailbox() method for each mailbox that you want to look up. Then, after
several invocations of the event loop, the IMAP server will be asked for incremental mailbox
listing, nesting as needed. When the mailbox is found, the mailboxFound() slot will fire.

When there's a problem finding that mailbox, no signal will be emitted. This is related to how the
Model currently works, because the Tasks have no way of informing the user that they failed.

In addition, the MailboxFinder works purely on MVC layer, so it has no IMAP knowledge besides custom
item roles.
*/
class MailboxFinder : public QObject
{
    Q_OBJECT
public:
    explicit MailboxFinder( QObject *parent, Imap::Mailbox::Model *model );

    /** @short Specify an interest in obtaining an index for this mailbox */
    void addMailbox( const QString &mailbox );

signals:
    /** @short Fires when the mapping has been found

If the mailbox isn't found, no signal is emitted.
*/
    void mailboxFound( const QString &mailbox, const QModelIndex &index );

private slots:
    /** @short Walk the internal list of names that we're interested in, ask the model for data if needed */
    void checkArrivals();
    /** @short Handler for the rowsInserted() signal */
    void slotRowsInserted( const QModelIndex &parent, int start, int end );

private:
    Imap::Mailbox::Model *m_model;
    QStringList m_watchedNames;
};

}

#endif // MAILBOXFINDER_H
