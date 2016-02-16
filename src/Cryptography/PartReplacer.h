/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TROJITA_CRYPTO_PART_REPLACER_H
#define TROJITA_CRYPTO_PART_REPLACER_H

#include "Cryptography/MessagePart.h"

namespace Cryptography {

/** @short Interface for replacing specific part by a custom tree */
class PartReplacer {
public:
    PartReplacer();
    virtual ~PartReplacer();

    /** @short Mangle the MIME part if desirable

    The implementations of this interface checks if it is desirable to override the @arg original MIME part with
    a custom specification. If not, the @arg original shall be returned without modification.

    The MessageModel is in an inconsistent state at the time this method is called; the @arg original is actually
    *not* included within the list of its parent's children (the children of @arg parentPart are undefined). Care
    should be taken to not access the proxy indexes below the @arg proxyParentIndex. On the other hand, it is safe
    to use the @arg original to request data from the proxied part, including calling its row().

    If this module wants to override the MIME part (maybe based on its MIME type, maybe based on some ad-hoc criteria),
    it should return a replacement part. Note that other part replacers might override this decision by overwriting
    the returned objects. Resource management is taken care of by the unique_ptr interface.

    The @arg sourceItemIndex points to an index within the source model (that is *not* the MessageModel) which
    represents the current item (see @arg original).

    The @arg proxyParentIndex represents a parent of @arg original within the MessageModel. There is no index
    representing the current item within the MessageModel because this method is called at the time the model
    is not in a consistent shape, and therefore accessing its MVC methods for siblings of "this" item (i.e.,
    children of @arg proxyParentIndex) is not safe.
    */
    virtual MessagePart::Ptr createPart(MessageModel *model, MessagePart *parentPart, MessagePart::Ptr original,
                                        const QModelIndex &sourceItemIndex, const QModelIndex &proxyParentIndex) = 0;
};

}
#endif
