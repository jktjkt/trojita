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

#ifndef IMAP_MODEL_FINDINTERESTINGPART_H
#define IMAP_MODEL_FINDINTERESTINGPART_H

#include <QModelIndex>

namespace Imap {
namespace Mailbox {

/** @short Find the most interesting part of a given message */
struct FindInterestingPart
{
    /** @short Manipulate the index to find a "main part" of a message

    This function will try to find the most interesting part of a message, ie. something which can be
    stored as representative data of a message in an environment which doesn't support MIME. If the
    main part can't be found, this function will return a string message mentioning what has happened.
    */
    static QString findMainPart( QModelIndex &part );

    /** @short Status of finding the main part */
    enum MainPartReturnCode {
        MAINPART_FOUND, /**< It was found and data are available right now */
        MAINPART_MESSAGE_NOT_LOADED, /**< The bodystructure is not known yet */
        MAINPART_PART_LOADING, /**< @short It was found, but the part data themselves weren't fetched yet */
        MAINPART_PART_CANNOT_DETERMINE /**< @short There's no supported MIME part in this message */
    };

    /** @short Try to find a usable "main part" of a message

    @see findMainPart(), MainPartReturnCode
    */
    static MainPartReturnCode findMainPartOfMessage(const QModelIndex &message, QModelIndex &mainPartIndex, QString &partMessage,
                                                    QString *partData);
};

}
}

#endif // IMAP_MODEL_FINDINTERESTINGPART_H
