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
#include "Imap/Model/MailboxTree.h"

namespace XtConnect {

MessageDownloader::MessageDownloader(QObject *parent) :
    QObject(parent)
{
}

void MessageDownloader::requestDownload( const QModelIndex &message )
{
    static const QAbstractItemModel *lastModel = 0;
    if ( ! lastModel ) {
        lastModel = message.model();
        connect( lastModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(slotDataChanged(QModelIndex,QModelIndex)) );
    }
    Q_ASSERT(lastModel == message.model());

    QModelIndex indexHeader = lastModel->index( 0, Imap::Mailbox::TreeItem::OFFSET_HEADER, message );
    QModelIndex indexText = lastModel->index( 0, Imap::Mailbox::TreeItem::OFFSET_TEXT, message );

    QVariant headerData = indexHeader.data( Imap::Mailbox::RolePartData );
    QVariant textData = indexText.data( Imap::Mailbox::RolePartData );
    if ( headerData.isValid() && textData.isValid() ) {
        QByteArray data = headerData.toByteArray() + textData.toByteArray();
        emit messageDownloaded( message, data );
    } else {
        MessageMetadata metaData;
        metaData.header = indexHeader;
        metaData.body = indexText;
        m_parts[ message ] = metaData;
    }
}

void MessageDownloader::slotDataChanged( const QModelIndex &a, const QModelIndex &b )
{
    if ( ! a.isValid() )
        return;

    if ( a != b )
        return;

    QModelIndex message = a.parent();

    if ( ! message.isValid() )
        return;

    QMap<QPersistentModelIndex,MessageMetadata>::iterator it = m_parts.find( message );
    if ( it == m_parts.end() )
        return;

    if ( a == it->header ) {
        it->hasHeader = true;
    } else if ( a == it->body ) {
        it->hasBody = true;
    } else {
        return;
    }

    if ( it->hasHeader && it->hasBody ) {
        QVariant headerData = it->header.data( Imap::Mailbox::RolePartData );
        QVariant textData = it->body.data( Imap::Mailbox::RolePartData );
        Q_ASSERT(headerData.isValid());
        Q_ASSERT(textData.isValid());
        QByteArray data = headerData.toByteArray() + textData.toByteArray();
        emit messageDownloaded( message, data );
        m_parts.erase( it );
    }
}

}
