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

#include "FindInterestingPart.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"

namespace Imap {
namespace Mailbox {

QString FindInterestingPart::findMainPart( QModelIndex &part )
{
    if ( ! part.isValid() )
        return QLatin1String("Invalid index");

    QString mimeType = part.data( Imap::Mailbox::RolePartMimeType ).toString().toLower();

    if ( mimeType == QLatin1String("text/plain") ) {
        // found it, no reason to do anything else
        return QString();
    }

    if ( mimeType == QLatin1String("text/html") ) {
        // HTML without a text/plain counterpart is not supported
        part = QModelIndex();
        return QLatin1String("A HTML message without a plaintext counterpart");
    }

    if ( mimeType == QLatin1String("message/rfc822") ) {
        if ( part.model()->rowCount( part ) != 1 ) {
            part = QModelIndex();
            return QLatin1String("Unsupported message/rfc822 formatting");
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
        return QString::fromUtf8("This is a %1 formatted message whose parts are not suitable for diplaying here").arg(mimeType);
    }

    part = QModelIndex();
    return QString::fromUtf8("MIME type %1 is not supported").arg(mimeType);
}

FindInterestingPart::MainPartReturnCode FindInterestingPart::findMainPartOfMessage(
        const QModelIndex &message, QModelIndex &mainPartIndex, QString &partMessage, QString *partData)
{
    mainPartIndex = message.child( 0, 0 );
    if ( ! mainPartIndex.isValid() ) {
        return MAINPART_MESSAGE_NOT_LOADED;
    }

    partMessage = findMainPart( mainPartIndex );
    if ( ! mainPartIndex.isValid() ) {
        return MAINPART_PART_CANNOT_DETERMINE;
    }

    if (partData) {
        QVariant data = mainPartIndex.data( Imap::Mailbox::RolePartData );
        if ( ! data.isValid() ) {
            return MAINPART_PART_LOADING;
        }

        *partData = data.toString();
        return MAINPART_FOUND;
    } else {
        return mainPartIndex.data(Imap::Mailbox::RoleIsFetched).toBool() ? MAINPART_FOUND : MAINPART_PART_LOADING;
    }
}

}
}
