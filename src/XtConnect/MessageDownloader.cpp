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

#include "MessageDownloader.h"
#include "Imap/Model/FindInterestingPart.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"

//#define DEBUG_PENDING_MESSAGES
//#define DEBUG_PENDING_MESSAGES_2

namespace XtConnect {

MessageDownloader::MessageDownloader(QObject *parent, const QString &mailboxName ):
    QObject(parent), lastModel(0), registeredMailbox(mailboxName)
{
    m_releasingTimer = new QTimer(this);
    m_releasingTimer->setSingleShot(true);
    connect(m_releasingTimer, SIGNAL(timeout()), this, SLOT(slotFreeProcessedMessages()));
}

void MessageDownloader::requestDownload( const QModelIndex &message )
{
    if ( ! lastModel ) {
        lastModel = message.model();
        connect( lastModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(slotDataChanged(QModelIndex,QModelIndex)) );
    }
    Q_ASSERT(lastModel == message.model());

    Q_ASSERT( message.parent().parent().data( Imap::Mailbox::RoleMailboxName ).toString() == registeredMailbox );

    MessageMetadata metaData;

    QModelIndex header = lastModel->index( 0, Imap::Mailbox::TreeItem::OFFSET_HEADER, message );
    QVariant headerData = header.data( Imap::Mailbox::RolePartData );
    metaData.hasHeader = headerData.isValid();
    if ( metaData.hasHeader )
        metaData.headerData = headerData.toByteArray();

    QModelIndex text = lastModel->index( 0, Imap::Mailbox::TreeItem::OFFSET_TEXT, message );
    QVariant textData = text.data( Imap::Mailbox::RolePartData );
    metaData.hasBody = textData.isValid();
    if ( metaData.hasBody )
        metaData.bodyData = textData.toByteArray();


    metaData.hasMessage = message.data( Imap::Mailbox::RoleMessageMessageId ).isValid();

    QModelIndex mainPart;
    QString partData;
    Imap::Mailbox::FindInterestingPart::MainPartReturnCode mainPartStatus =
            Imap::Mailbox::FindInterestingPart::findMainPartOfMessage(message, mainPart, metaData.partMessage, &partData);
    metaData.mainPart = mainPart;
    metaData.hasMainPart = ( mainPartStatus == Imap::Mailbox::FindInterestingPart::MAINPART_FOUND ||
                             mainPartStatus == Imap::Mailbox::FindInterestingPart::MAINPART_PART_CANNOT_DETERMINE );
    metaData.mainPartFailed = mainPartStatus == Imap::Mailbox::FindInterestingPart::MAINPART_PART_CANNOT_DETERMINE;

#ifdef DEBUG_PENDING_MESSAGES_2
    qDebug() << "requestDownload:" << message.row() << message.data( Imap::Mailbox::RoleMessageUid ).toUInt() <<
            metaData.hasHeader << metaData.hasBody << metaData.hasMessage << metaData.hasMainPart;
#endif

    if ( metaData.hasHeader && metaData.hasBody && metaData.hasMessage && metaData.hasMainPart ) {
        emit messageDownloaded( message, metaData.headerData, metaData.bodyData,
                                mainPartStatus == Imap::Mailbox::FindInterestingPart::MAINPART_FOUND ? partData : metaData.partMessage );
        return;
    }

    const uint uid = message.data( Imap::Mailbox::RoleMessageUid ).toUInt();
    Q_ASSERT(uid);
    m_parts[ uid ] = metaData;
}

void MessageDownloader::slotDataChanged( const QModelIndex &a, const QModelIndex &b )
{
    if ( ! a.isValid() ) {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "MessageDownloader::slotDataChanged: a not valid" << a;
#endif
        return;
    }

    if ( a != b ) {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "MessageDownloader::slotDataChanged: a != b" << a;
#endif
        return;
    }

    QModelIndex message = Imap::Mailbox::Model::findMessageForItem( a );
    if ( ! message.isValid() ) {
#ifdef DEBUG_PENDING_MESSAGES_2
        qDebug() << "MessageDownloader::slotDataChanged: message not valid" << a;
#endif
        return;
    }

    if ( message.parent().parent().data( Imap::Mailbox::RoleMailboxName ).toString() != registeredMailbox ) {
#ifdef DEBUG_PENDING_MESSAGES_2
        qDebug() << "MessageDownloader::slotDataChanged: not this mailbox" << a <<
                    message.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString() << registeredMailbox;
#endif
        return;
    }

    const uint uid = message.data( Imap::Mailbox::RoleMessageUid ).toUInt();
    Q_ASSERT(uid);

    QMap<uint,MessageMetadata>::iterator it = m_parts.find( uid );
    if ( it == m_parts.end() ) {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "We are not interested in message with UID" << uid;
#endif
        return;
    }

    const QAbstractItemModel *model = message.model();

    QModelIndex header = model->index( 0, Imap::Mailbox::TreeItem::OFFSET_HEADER, message );
    QModelIndex text = model->index( 0, Imap::Mailbox::TreeItem::OFFSET_TEXT, message );

    if ( a == header ) {
        it->hasHeader = true;
        QVariant data = header.data( Imap::Mailbox::RolePartData );
        Q_ASSERT(data.isValid());
        it->headerData = data.toByteArray();
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got header for" << uid;
#endif
    } else if ( a == text ) {
        it->hasBody = true;
        QVariant data = text.data( Imap::Mailbox::RolePartData );
        Q_ASSERT(data.isValid());
        it->bodyData = data.toByteArray();
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got body for" << uid;
#endif
    } else if ( a == message && ! it->hasMessage ) {
        it->hasMessage = true;
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got message for" << uid;
#endif

        QModelIndex mainPart;
        QString partData;
        Imap::Mailbox::FindInterestingPart::MainPartReturnCode mainPartStatus =
                Imap::Mailbox::FindInterestingPart::findMainPartOfMessage(message, mainPart, it->partMessage, &partData);
        it->mainPart = mainPart;
        switch( mainPartStatus ) {
        case Imap::Mailbox::FindInterestingPart::MAINPART_FOUND:
            it->hasMainPart = true;
#ifdef DEBUG_PENDING_MESSAGES
            qDebug() << "  ...and MAINPART_FOUND for" << uid;
#endif
            break;
        case Imap::Mailbox::FindInterestingPart::MAINPART_MESSAGE_NOT_LOADED:
#ifdef DEBUG_PENDING_MESSAGES
            qDebug() << "  ...and MAINPART_MESSAGE_NOT_LOADED for" << uid <<
                        "in" << message.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString();
#endif
            Q_ASSERT(false);
            break;
        case Imap::Mailbox::FindInterestingPart::MAINPART_PART_CANNOT_DETERMINE:
            it->mainPartFailed = true;
            it->hasMainPart = true;
#ifdef DEBUG_PENDING_MESSAGES
            qDebug() << "  ...and MAINPART_PART_CANNOT_DETERMINE for" << uid;
#endif
            break;
        case Imap::Mailbox::FindInterestingPart::MAINPART_PART_LOADING:
            // nothing needed here
#ifdef DEBUG_PENDING_MESSAGES
            qDebug() << "  ...and MAINPART_PART_LOADING for" << uid;
#endif
            break;
        }
    } else if ( it->mainPart.isValid() && a == it->mainPart ) {
        it->hasMainPart = true;
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got main part for" << uid;
#endif
    } else {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "  Got something else for" << uid;
#endif
        return;
    }

    if ( it->hasHeader && it->hasBody && it->hasMessage && it->hasMainPart ) {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "We've got everything for" << uid << "-> saving";
#endif
        QVariant mainPartData = it->mainPart.data( Imap::Mailbox::RolePartData );
        QString mainPart;
        if ( it->mainPartFailed ) {
            mainPart = it->partMessage;
        } else {
            Q_ASSERT(mainPartData.isValid());
            mainPart = mainPartData.toString();
        }
        Q_ASSERT(message.data( Imap::Mailbox::RoleMessageMessageId ).isValid());
        Q_ASSERT(message.data( Imap::Mailbox::RoleMessageSubject ).isValid());
        Q_ASSERT(message.data( Imap::Mailbox::RoleMessageDate ).isValid());
        emit messageDownloaded( message, it->headerData, it->bodyData, mainPart );
        m_parts.erase( it );

        m_messagesToBeFreed << message;
        if (!m_releasingTimer->isActive())
            m_releasingTimer->start();
    } else {
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "Something is missing for" << uid << it->hasHeader << it->hasBody << it->hasMessage << it->hasMainPart;
#endif
    }
}

int MessageDownloader::pendingMessages() const
{
    return m_parts.size();
}

/** @short Instruct the Model that the data it has cached for a particular message is no longer needed */
void MessageDownloader::slotFreeProcessedMessages()
{
    Q_FOREACH(QPersistentModelIndex index, m_messagesToBeFreed) {
        if (!index.isValid())
            continue;
        // The const_cast should be safe here -- this action is certainly not going to invalidate the index,
        // and even the releaseMessageData() won't (directly) touch its members anyway...
        Imap::Mailbox::Model *model = qobject_cast<Imap::Mailbox::Model*>(const_cast<QAbstractItemModel*>(index.model()));
        Q_ASSERT(model);
#ifdef DEBUG_PENDING_MESSAGES
        qDebug() << "Freeing memory for" << index.parent().parent().data(Imap::Mailbox::RoleMailboxName).toString() <<
                    "UID" << index.data(Imap::Mailbox::RoleMessageUid).toUInt();
#endif
        model->releaseMessageData(index);
    }
    m_messagesToBeFreed.clear();
}

}
