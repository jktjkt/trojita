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
#include "SqlStorage.h"

namespace XtConnect {

MailSynchronizer::MailSynchronizer( QObject *parent, Imap::Mailbox::Model *model, MailboxFinder *finder, MessageDownloader *downloader, SqlStorage *storage ) :
    QObject(parent), m_model(model), m_finder(finder), m_downloader(downloader), m_storage(storage)
{
    Q_ASSERT(m_model);
    Q_ASSERT(m_finder);
    Q_ASSERT(m_downloader);
    Q_ASSERT(m_storage);
    connect( m_model, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(slotRowsInserted(QModelIndex,int,int)) );
    connect( m_finder, SIGNAL(mailboxFound(QString,QModelIndex)), this, SLOT(slotMailboxFound(QString,QModelIndex)) );
    connect( m_downloader, SIGNAL(messageDownloaded(QModelIndex,QByteArray,QByteArray,QString)),
             this, SLOT(slotMessageDataReady(QModelIndex,QByteArray,QByteArray,QString)) );
    m_deferredTimer = new QTimer(this);
    m_deferredTimer->setSingleShot(true);
    m_deferredTimer->setInterval(5000);
    connect(m_deferredTimer, SIGNAL(timeout()), this, SLOT(slotWalkDeferredMessages()));
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
    switchHere();
    walkThroughMessages( -1, -1 );
}

void MailSynchronizer::slotRowsInserted(const QModelIndex &parent, int start, int end)
{
    // check the mailbox's list, if it's the same index as the "parent" we just got passed
    if ( parent.parent() != m_index )
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
        end = m_model->rowCount( list ) - 1;
    } else {
        start = qMax( start, 0 );
        end = qMin( end, m_model->rowCount( list ) - 1 );
    }

    for ( int i = start; i <= end; ++i ) {
        QModelIndex message = m_model->index( i, 0, list );
        Q_ASSERT( message.isValid() );
        QVariant uid = m_model->data( message, Imap::Mailbox::RoleMessageUid );

        if ( uid.isValid() && uid.toUInt() != 0 ) {
            bool shouldLoad = true;
            emit aboutToRequestMessage( m_mailbox, message, &shouldLoad );
            if ( shouldLoad ) {
                m_downloader->requestDownload( message );
            }
        } else {
            m_deferredMessages << message;
            if ( ! m_deferredTimer->isActive() )
                m_deferredTimer->start();
            //qDebug() << "Message not synced yet, will check in a while" << m_mailbox << i;
        }
    }
}

bool MailSynchronizer::renewMailboxIndex()
{
    if ( ! m_index.isValid() ) {
        qDebug() << "Oops, we've lost the mailbox" << m_mailbox <<", will ask for it again in a while.";
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

    m_model->switchToMailbox( m_index );
}

void MailSynchronizer::slotMessageDataReady( const QModelIndex &message, const QByteArray &headers, const QByteArray &body, const QString &mainPart )
{
    Common::SqlTransactionAutoAborter guard = m_storage->transactionGuard();

    QVariant dateTimeVariant = message.data( Imap::Mailbox::RoleMessageDate );
    QVariant subject = message.data( Imap::Mailbox::RoleMessageSubject );
    Q_ASSERT(dateTimeVariant.isValid());
    Q_ASSERT(subject.isValid());
    QDateTime dateTime = dateTimeVariant.toDateTime();
    if ( dateTime.isNull() ) {
        m_model->logTrace(message, Common::LOG_OTHER, QLatin1String("MailSynchronizer"),
                          QString::fromUtf8("Warning: unknown timestamp for UID %1 in %2 - using current one").arg(
                              message.data(Imap::Mailbox::RoleMessageUid).toString(),
                              message.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString()));
        dateTime = QDateTime::currentDateTimeUtc();
    }

    quint64 emlId;
    SqlStorage::ResultType res = m_storage->insertMail( dateTime, subject.toString(),
                                                        mainPart, headers, body, emlId );

    if ( res == SqlStorage::RESULT_DUPLICATE ) {
        m_model->logTrace(message, Common::LOG_OTHER, QLatin1String("MailSynchronizer"), QLatin1String("Duplicate message"));
        emit messageIsDuplicate( m_mailbox, message );
        return;
    }

    if ( res == SqlStorage::RESULT_ERROR ) {
        m_model->logTrace(message, Common::LOG_OTHER, QLatin1String("MailSynchronizer"), QLatin1String("Cannot store into Postgres"));
        qWarning() << "Inserting failed";
        return;
    }

    _saveAddrList( emlId, message.data( Imap::Mailbox::RoleMessageFrom ), QLatin1String("FROM") );
    _saveAddrList( emlId, message.data( Imap::Mailbox::RoleMessageTo ), QLatin1String("TO") );
    _saveAddrList( emlId, message.data( Imap::Mailbox::RoleMessageCc ), QLatin1String("CC") );
    _saveAddrList( emlId, message.data( Imap::Mailbox::RoleMessageBcc ), QLatin1String("BCC") );

    if ( m_storage->markMailReady( emlId ) != SqlStorage::RESULT_OK ) {
        m_model->logTrace(message, Common::LOG_OTHER, QLatin1String("MailSynchronizer"),
                          QLatin1String("Failed to mark mail ready"));
        qWarning() << "Failed to mark mail ready";
        return;
    }

    if ( ! guard.commit() ) {
        m_model->logTrace(message, Common::LOG_OTHER, QLatin1String("MailSynchronizer"),
                          QLatin1String("Failed to commit current transaction"));
        m_storage->fail( QLatin1String("Failed to commit current transaction") );
        return;
    }

    emit messageSaved( m_mailbox, message );
}

void MailSynchronizer::_saveAddrList( const quint64 emlId, const QVariant &addresses, const QLatin1String kind )
{
    // FIXME: batched updates would be great...
    Q_ASSERT( addresses.type() == QVariant::List );
    Q_FOREACH( const QVariant &item, addresses.toList() ) {
        Q_ASSERT( item.isValid() );
        Q_ASSERT( item.type() == QVariant::StringList );
        QStringList expanded = item.toStringList();
        Q_ASSERT( expanded.size() == 4 );
        QString name = expanded[0];

        QString address;
        if ( expanded[2].isEmpty() && expanded[3].isEmpty() ) {
            address = QLatin1String("undisclosed-recipients;");
        } else if ( expanded[2].isEmpty() ) {
            address = expanded[3];
        } else if ( expanded[3].isEmpty() ) {
            address = expanded[2];
        } else {
            address = expanded[2] + QLatin1Char('@') + expanded[3];
        }

        if ( m_storage->insertAddress( emlId, name, address, kind ) != SqlStorage::RESULT_OK ) {
            m_model->logTrace(0, Common::LOG_OTHER, QLatin1String("MailSynchronizer"),
                              QLatin1String("Failed to insert address"));
            qWarning() << "Failed to insert address";
        }
    }
}

void MailSynchronizer::debugStats() const
{
    if ( m_index.isValid() ) {
        qDebug() << "Mailbox" << m_mailbox <<
                ( m_index.data(Imap::Mailbox::RoleMailboxItemsAreLoading).toBool() ? "[loading]" : "" ) <<
                "total" << m_index.data( Imap::Mailbox::RoleTotalMessageCount ).toUInt() <<
                ", active" << m_downloader->activeMessages() << ", queued" << m_downloader->pendingMessages() <<
                ", uid_wait" << m_deferredMessages.count();
    } else {
        qDebug() << "Mailbox" << m_mailbox << ": waiting for sync.";
    }
}

void MailSynchronizer::slotWalkDeferredMessages()
{
    QList<QPersistentModelIndex>::iterator it = m_deferredMessages.begin();
    while (it != m_deferredMessages.end()) {
        if ( ! it->isValid() ) {
            qDebug() << "A message got removed between the moment we saw it for the first time and the time we tried to process it";
            it = m_deferredMessages.erase(it);
            continue;
        }
        QVariant uid = it->data(Imap::Mailbox::RoleMessageUid);
        Q_ASSERT(uid.isValid());
        if ( uid.toUInt() == 0 ) {
            qDebug() << m_mailbox << ": still no UID for message #" << it->row() + 1;
            ++it;
        } else {
            bool shouldLoad = true;
            QModelIndex message = *it;
            it = m_deferredMessages.erase(it);
            emit aboutToRequestMessage( m_mailbox, message, &shouldLoad );
            if ( shouldLoad ) {
                m_downloader->requestDownload(message);
            }
        }
    }
    if ( ! m_deferredMessages.isEmpty() )
        m_deferredTimer->start();
}

}
