/* Copyright (C) 2014-2015 Stephan Platz <trojita@paalsteek.de>

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

#include <mimetic/mimetic.h>

#include <sstream>
#include "QCAHelper.h"
#include "MessagePart.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/Message.h"
#include "Imap/Parser/Rfc5322HeaderParser.h"

namespace Cryptography {

QString QCAHelper::qcaErrorStrings(const int e)
{
    switch (e) {
    case QCA::SecureMessage::ErrorPassphrase:
        return tr("passphrase was either wrong or not provided");
    case QCA::SecureMessage::ErrorFormat:
        return tr("input format was bad");
    case QCA::SecureMessage::ErrorSignerExpired:
        return tr("signing key is expired");
    case QCA::SecureMessage::ErrorSignerInvalid:
        return tr("signing key is invalid in some way");
    case QCA::SecureMessage::ErrorEncryptExpired:
        return tr("encrypting key is expired");
    case QCA::SecureMessage::ErrorEncryptUntrusted:
        return tr("encrypting key is untrusted");
    case QCA::SecureMessage::ErrorEncryptInvalid:
        return tr("encrypting key is invalid in some way");
    case QCA::SecureMessage::ErrorNeedCard:
        return tr("pgp card is missing");
#if QCA_VERSION >= 0x020000 //v2.0.0
    case QCA::SecureMessage::ErrorCertKeyMismatch:
        return tr("certificate and private key don't match");
#endif
#if QCA_VERSION >= 0x020100 //v2.1.0
    case QCA::SecureMessage::ErrorSignerRevoked:
        return tr("signing key is revoked");
    case QCA::SecureMessage::ErrorSignatureExpired:
        return tr("signature is expired");
    case QCA::SecureMessage::ErrorEncryptRevoked:
        return tr("encrypting key is revoked");
#endif
    case QCA::SecureMessage::ErrorUnknown:
    default:
        return tr("other error (%1)").arg(e);
    }
}

}
