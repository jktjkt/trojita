/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2018 Erik Quaeghebeur <kde@equaeghe.nospammail.net>

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
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QStringList>

#include "SubjectMangling.h"

namespace Composer {
namespace Util {

/** @short Prepare a subject to be used in a reply message */
QString replySubject(const QString &subject)
{
    // These operations should *not* check for internationalized variants of "Re"; these are evil.
    static const QRegularExpression rePrefixMatcher(QLatin1String(
                              /* initial whitespace */ "\\s*"
                  /* either Re: or mailing list tag */ "(?:Re:|(\\[.+?\\]))"
                        /* repetitions of the above */ "(?:\\s|Re:|\\1)*"),
                                              QRegularExpression::CaseInsensitiveOption);

    QStringList reply_subject(QStringLiteral("Re:"));
    int start = 0;

    // extract mailing list tags & find start of ‘base’ subject
    QRegularExpressionMatchIterator i = rePrefixMatcher.globalMatch(subject);
    while (i.hasNext() && i.peekNext().capturedStart() == start) {
        if (!i.peekNext().captured(1).isEmpty())
            reply_subject << i.peekNext().captured(1);
        start = i.next().capturedEnd();
    }

    // no trailing space after last mailing list tag before empty ‘base’ subject
    // TODO: this is for test-suite compliance only; remove?
    if (start < subject.length() || reply_subject.length() == 1)
        reply_subject << subject.mid(start);

    return reply_subject.join(QLatin1Char(' '));
}

/** @short Prepare a subject to be used in a message to be forwarded */
QString forwardSubject(const QString &subject)
{
    QLatin1String forwardPrefix("Fwd: ");
    return forwardPrefix + subject;
}

}
}


