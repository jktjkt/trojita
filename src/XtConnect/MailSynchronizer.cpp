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
#include "Imap/Model/ItemRoles.h"
#include "MailboxFinder.h"
#include "MessageDownloader.h"

namespace XtConnect {

MailSynchronizer::MailSynchronizer( QObject *parent, Imap::Mailbox::Model *model, MailboxFinder *finder, MessageDownloader *downloader ) :
    QObject(parent), m_model(model), m_finder(finder), m_downloader(downloader), ignoreArrivals(true)
{
    Q_ASSERT(m_model);
    Q_ASSERT(m_finder);
    Q_ASSERT(m_downloader);
    connect( m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(slotRowsInserted(QModelIndex,int,int)) );
    connect( m_finder, SIGNAL(mailboxFound(QString,QModelIndex)), this, SLOT(slotMailboxFound(QString,QModelIndex)) );
    connect( m_model, SIGNAL(mailboxSyncingProgress(QModelIndex,Imap::Mailbox::MailboxSyncingProgress)),
             this, SLOT(slotMailboxSyncStateProgress(QModelIndex,Imap::Mailbox::MailboxSyncingProgress)) );
    connect( m_downloader, SIGNAL(messageDownloaded(QModelIndex,QByteArray)), this, SLOT(slotMessageDataReady(QModelIndex,QByteArray)) );
}

void MailSynchronizer::setMailbox( const QString &mailbox )
{
    m_mailbox = mailbox;
    qDebug() << "Will watch mailbox" << mailbox;
    slotGetMailboxIndexAgain();
}

void MailSynchronizer::slotGetMailboxIndexAgain()
{
    Q_ASSERT(m_finder);
    m_finder->addMailbox( m_mailbox );
}

void MailSynchronizer::slotMailboxFound( const QString &mailbox, const QModelIndex &index )
{
    if ( mailbox != m_mailbox )
        return;

    m_index = index;
    QModelIndex list = m_index.child( 0, 0 );
    Q_ASSERT( list.isValid() );
    // Check if something is already there, and if it is, enable rowsInserted() tracking
    if ( m_model->rowCount( list ) > 0 )
        ignoreArrivals = false;

    switchHere();
}


void MailSynchronizer::slotRowsInserted(const QModelIndex &parent, int start, int end)
{
    // check the mailbox's list, if it's the same index as the "parent" we just got passed
    if ( parent.parent() != m_index )
        return;

    if ( ignoreArrivals )
        return;

    walkThroughMessages( start, end );
}

void MailSynchronizer::walkThroughMessages( int start, int end )
{
    if ( renewMailboxIndex() )
        return;

    QModelIndex list = m_index.child( 0, 0 );
    Q_ASSERT( list.isValid() );

    if ( start == -1 && end == -1 ) {
        start = 0;
        end = m_model->rowCount( list );
    } else {
        start = qMax( start, 0 );
        end = qMin( end, m_model->rowCount( list ) );
    }

    for ( int i = start; i < end; ++i ) {
        QModelIndex message = m_model->index( i, 0, list );
        Q_ASSERT( message.isValid() );
        QVariant uid = m_model->data( message, Imap::Mailbox::RoleMessageUid );
        if ( uid.isValid() && uid.toUInt() != 0 ) {
            m_downloader->requestDownload( message );
        } else {
            qDebug() << m_mailbox << i << "[unsynced message, huh?]";
        }
    }
}

bool MailSynchronizer::renewMailboxIndex()
{
    if ( ! m_index.isValid() ) {
        qDebug() << "Oops, we've lost the mailbox" << m_mailbox <<", will ask for it again in a while.";
        ignoreArrivals = true;
        QTimer::singleShot( 14*1000 /*1000 * 60 * 2*/, this, SLOT(slotGetMailboxIndexAgain()) );
        return true;
    } else {
        return false;
    }
}

void MailSynchronizer::switchHere()
{
    if ( renewMailboxIndex() )
        return;

    qDebug() << "Switching to" << m_mailbox;
    m_model->switchToMailbox( m_index );
}

void MailSynchronizer::slotMailboxSyncStateProgress( const QModelIndex &mailbox, Imap::Mailbox::MailboxSyncingProgress state )
{
    if ( mailbox != m_index )
        return;

    if ( state == Imap::Mailbox::STATE_DONE ) {
        ignoreArrivals = false;
        walkThroughMessages( -1, -1 );
    } else {;
        ignoreArrivals = true;
    }
}

void MailSynchronizer::slotMessageDataReady( const QModelIndex &message, const QByteArray &data )
{
    qDebug() << "Received data for" << m_mailbox << message.data( Imap::Mailbox::RoleMessageUid ).toUInt() << data.size();
}

}
