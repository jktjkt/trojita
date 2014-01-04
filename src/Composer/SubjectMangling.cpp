/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#include <QObject>
#include <QRegExp>
#include <QStringList>

#include "SubjectMangling.h"

namespace Composer {
namespace Util {

/** @short Prepare a subject to be used in a reply message */
QString replySubject(const QString &subject)
{
    // These operations should *not* check for internationalized variants of "Re"; these are evil.

#define RE_PREFIX_RE "(?:(?:Re:\\s*)*)"
#define RE_PREFIX_ML "(?:(\\[[^\\]]+\\]\\s*)?)"

    static QRegExp rePrefixMatcher(QLatin1String("^"
                                                 RE_PREFIX_RE // a sequence of "Re: " prefixes
                                                 RE_PREFIX_ML // something like a mailing list prefix
                                                 RE_PREFIX_RE // a sequence of "Re: " prefixes
                                                 ), Qt::CaseInsensitive);
    rePrefixMatcher.setPatternSyntax(QRegExp::RegExp2);
    QLatin1String correctedPrefix("Re: ");

    if (rePrefixMatcher.indexIn(subject) == -1) {
        // Our regular expression has failed, so better play it safe and blindly prepend "Re: "
        return correctedPrefix + subject;
    } else {
        QStringList listPrefixes;
        int pos = 0;
        int oldPos = 0;
        while ((pos = rePrefixMatcher.indexIn(subject, pos, QRegExp::CaretAtOffset)) != -1) {
            if (rePrefixMatcher.matchedLength() == 0)
                break;
            pos += rePrefixMatcher.matchedLength();
            if (!listPrefixes.contains(rePrefixMatcher.cap(1)))
                listPrefixes << rePrefixMatcher.cap(1);
            oldPos = pos;
        }

        QString mlPrefix = listPrefixes.join(QString()).trimmed();
        QString baseSubject = subject.mid(oldPos + qMax(0, rePrefixMatcher.matchedLength()));

        if (!mlPrefix.isEmpty() && !baseSubject.isEmpty())
            mlPrefix += QLatin1Char(' ');

        return correctedPrefix + mlPrefix + baseSubject;
    }
}

}
}


