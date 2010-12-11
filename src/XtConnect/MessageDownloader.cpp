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
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"

namespace XtConnect {

MessageDownloader::MessageDownloader(QObject *parent) :
    QObject(parent), lastModel(0)
{
}

void MessageDownloader::requestDownload( const QModelIndex &message )
{
    if ( ! lastModel ) {
        lastModel = message.model();
        connect( lastModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(slotDataChanged(QModelIndex,QModelIndex)) );
    }
    Q_ASSERT(lastModel == message.model());

    MessageMetadata metaData;
    metaData.header = lastModel->index( 0, Imap::Mailbox::TreeItem::OFFSET_HEADER, message );
    QVariant headerData = metaData.header.data( Imap::Mailbox::RolePartData );
    metaData.hasHeader = headerData.isValid();
    metaData.body = lastModel->index( 0, Imap::Mailbox::TreeItem::OFFSET_TEXT, message );
    QVariant textData = metaData.body.data( Imap::Mailbox::RolePartData );
    metaData.hasBody = textData.isValid();
    metaData.hasMessage = message.data( Imap::Mailbox::RoleMessageMessageId ).isValid();

    QModelIndex mainPart;
    QString partData;
    MainPartReturnCode mainPartStatus = findMainPartOfMessage( message, mainPart, metaData.partMessage, partData );
    metaData.mainPart = mainPart;
    metaData.hasMainPart = ( mainPartStatus == MAINPART_FOUND || mainPartStatus == MAINPART_PART_CANNOT_DETERMINE );

    if ( metaData.hasHeader && metaData.hasBody && metaData.hasMessage && metaData.hasMainPart ) {
        emit messageDownloaded( message, headerData.toByteArray(), textData.toByteArray(),
                                mainPartStatus == MAINPART_FOUND ? partData : metaData.partMessage );
        return;
    }

    m_parts[ message ] = metaData;
}

void MessageDownloader::slotDataChanged( const QModelIndex &a, const QModelIndex &b )
{
    if ( ! a.isValid() )
        return;

    if ( a != b )
        return;

    QModelIndex message = Imap::Mailbox::Model::findMessageForItem( a );
    if ( ! message.isValid() )
        return;

    QMap<QPersistentModelIndex,MessageMetadata>::iterator it = m_parts.find( message );
    if ( it == m_parts.end() )
        return;

    if ( a == it->header ) {
        it->hasHeader = true;
    } else if ( a == it->body ) {
        it->hasBody = true;
    } else if ( a == it.key() && ! it->hasMessage ) {
        it->hasMessage = true;

        QModelIndex mainPart;
        QString partData;
        MainPartReturnCode mainPartStatus = findMainPartOfMessage( message, mainPart, it->partMessage, partData );
        it->mainPart = mainPart;
        switch( mainPartStatus ) {
        case MAINPART_FOUND:
            it->hasMainPart = true;
            break;
        case MAINPART_MESSAGE_NOT_LOADED:
            Q_ASSERT(false);
            break;
        case MAINPART_PART_CANNOT_DETERMINE:
            it->mainPartFailed = true;
            it->hasMainPart = true;
            break;
        case MAINPART_PART_LOADING:
            // nothing needed here
            ;
        }
    } else if ( it->mainPart.isValid() && a == it->mainPart ) {
        it->hasMainPart = true;
    } else {
        return;
    }

    if ( it->hasHeader && it->hasBody && it->hasMessage && it->hasMainPart ) {
        QVariant headerData = it->header.data( Imap::Mailbox::RolePartData );
        QVariant textData = it->body.data( Imap::Mailbox::RolePartData );
        QVariant mainPartData = it->mainPart.data( Imap::Mailbox::RolePartData );
        QString mainPart;
        Q_ASSERT(headerData.isValid());
        Q_ASSERT(textData.isValid());
        if ( it->mainPartFailed ) {
            mainPart = it->partMessage;
        } else {
            Q_ASSERT(mainPartData.isValid());
            mainPart = mainPartData.toString();
        }
        Q_ASSERT(it.key().data( Imap::Mailbox::RoleMessageMessageId ).isValid());
        Q_ASSERT(it.key().data( Imap::Mailbox::RoleMessageSubject ).isValid());
        Q_ASSERT(it.key().data( Imap::Mailbox::RoleMessageDate ).isValid());
        emit messageDownloaded( message, headerData.toByteArray(), textData.toByteArray(), mainPart );
        m_parts.erase( it );
    }
}

QString MessageDownloader::findMainPart( QModelIndex &part )
{
    if ( ! part.isValid() )
        return QString::fromAscii("Invalid index");

    QString mimeType = part.data( Imap::Mailbox::RolePartMimeType ).toString().toLower();

    if ( mimeType == QLatin1String("text/plain") ) {
        // found it, no reason to do anything else
        return QString();
    }

    if ( mimeType == QLatin1String("text/html") ) {
        // HTML without a text/plain counterpart is not supported
        part = QModelIndex();
        return QString::fromAscii("A HTML message without a plaintext counterpart");
    }

    if ( mimeType == QLatin1String("message/rfc822") ) {
        if ( part.model()->rowCount( part ) != 1 ) {
            part = QModelIndex();
            return QString::fromAscii("Unsupported message/rfc822 formatting");
        }
        part = part.child( 0, 0 );
        return findMainPart( part );
    }

    if ( mimeType.startsWith( QLatin1String("multipart/") ) ) {
        QModelIndex target;
        QString str;
        for ( int i = 0; i < part.model()->rowCount( part ); ++i ) {
            // Walk through all children, try to find a first usable item
            target = part.child( i, 0 );
            str = findMainPart( target );
            if ( target.isValid() ) {
                // Found a usable item
                part = target;
                return QString();
            }

        }
        part = QModelIndex();
        return QString::fromAscii("This is a %1 formatted message whose parts are not suitable for diplaying here").arg(mimeType);
    }

    part = QModelIndex();
    return QString::fromAscii("MIME type %1 is not supported").arg(mimeType);
}

MessageDownloader::MainPartReturnCode MessageDownloader::findMainPartOfMessage(
        const QModelIndex &message, QModelIndex &mainPartIndex, QString &partMessage, QString &partData )
{
    mainPartIndex = message.child( 0, 0 );
    if ( ! mainPartIndex.isValid() ) {
        return MAINPART_MESSAGE_NOT_LOADED;
    }

    partMessage = findMainPart( mainPartIndex );
    if ( ! mainPartIndex.isValid() ) {
        return MAINPART_PART_CANNOT_DETERMINE;
    }

    QVariant data = mainPartIndex.data( Imap::Mailbox::RolePartData );
    if ( ! data.isValid() ) {
        return MAINPART_PART_LOADING;
    }

    partData = data.toString();
    return MAINPART_FOUND;
}

}
