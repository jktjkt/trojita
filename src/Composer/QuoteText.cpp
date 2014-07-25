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

#include "Composer/QuoteText.h"
#include "UiUtils/PlainTextFormatter.h"

namespace Composer {

/** @short Take the initial text and mark it as a quotation */
QStringList quoteText(QStringList inputLines)
{
    QStringList quote;
    for (QStringList::iterator line = inputLines.begin(); line != inputLines.end(); ++line) {
        if (UiUtils::signatureSeparator().exactMatch(*line)) {
            // This is the signature separator, we should not include anything below that in the quote
            break;
        }
        // rewrap - we need to keep the quotes at < 79 chars, yet the grow with every level
        if (line->length() < 79-2) {
            // this line is short enough, prepend quote mark and continue
            if (line->isEmpty() || line->at(0) == QLatin1Char('>'))
                line->prepend(QLatin1Char('>'));
            else
                line->prepend(QLatin1String("> "));
            quote << *line;
            continue;
        }
        // long line -> needs to be wrapped
        // 1st, detect the quote depth and eventually stript the quotes from the line
        int quoteLevel = 0;
        int contentStart = 0;
        if (line->at(0) == QLatin1Char('>')) {
            quoteLevel = 1;
            while (quoteLevel < line->length() && line->at(quoteLevel) == QLatin1Char('>'))
                ++quoteLevel;
            contentStart = quoteLevel;
            if (quoteLevel < line->length() && line->at(quoteLevel) == QLatin1Char(' '))
                ++contentStart;
        }

        // 2nd, build a quote string
        QString quotemarks;
        for (int i = 0; i < quoteLevel; ++i)
            quotemarks += QLatin1Char('>');
        quotemarks += QLatin1String("> ");

        // 3rd, wrap the line, prepend the quotemarks to each line and add it to the quote text
        int space(contentStart), lastSpace(contentStart), pos(contentStart), length(0);
        while (pos < line->length()) {
            if (line->at(pos) == QLatin1Char(' '))
                space = pos+1;
            ++length;
            if (length > 65-quotemarks.length() && space != lastSpace) {
                // wrap
                quote << quotemarks + line->mid(lastSpace, space - lastSpace);
                lastSpace = space;
                length = pos - space;
            }
            ++pos;
        }
        quote << quotemarks + line->mid(lastSpace);
    }
    return quote;
}

}
