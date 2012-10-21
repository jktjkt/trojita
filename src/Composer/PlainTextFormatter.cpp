/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>
   Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QObject>

#include <QDebug> // FIXME: jkt: remove this

#include "PlainTextFormatter.h"

namespace Composer {
namespace Util {

QStringList plainTextToHtml(const QString &plaintext)
{
    static const QRegExp link("(https*://[;/?:@=&$\\-_.+!'(),0-9a-zA-Z%#]*)");
    static const QRegExp mail("([a-zA-Z0-9\\.\\-_]*@[a-zA-Z0-9\\.\\-_]*)");
    static QString intro("(^|[\\s\\(\\[\\{])");
    static QString extro("([\\s\\),;.\\]\\}])");
    static const QRegExp bold(intro + "\\*(\\S*)\\*" + extro);
    static const QRegExp italic(intro + "/(\\S*)/" + extro);
    static const QRegExp underline(intro + "_(\\S*)_" + extro);

    // Processing:
    // the plain text is split into lines
    // leading quotemarks are counted and stripped
    // next, the line is marked up (*bold*, /italic/, _underline_ and active link support)
    // if the last line ended with a space, the result is appended, otherwise canonical quotemarkes are
    // prepended and the line appended to the markup list (see http://tools.ietf.org/html/rfc3676)
    // whenever the quote level grows, a <blockquote> is opened and closed when it shrinks

    int quoteLevel = 0;
    QStringList plain(plaintext.split('\n'));
    QStringList markup;
    for (int i = 0; i < plain.count(); ++i) {
        QString *line = const_cast<QString*>(&plain.at(i));

        // ignore empty lines
        if (line->isEmpty()) {
            markup << *line;
            continue;
        }
        // determine quotelevel
        int cQuoteLevel = 0;
        if (line->at(0) == '>') {
            int j = 1;
            cQuoteLevel = 1;
            while (j < line->length() && (line->at(j) == '>' || line->at(j) == ' '))
                cQuoteLevel += line->at(j++) == '>';
        }
        // strip quotemarks
        if (cQuoteLevel) {
            static QRegExp quotemarks("^[>\\s]*");
            line->remove(quotemarks);
        }
        // markup *bold*, /italic/, _underline_ and active links
        line->replace(">", "§gt;"); // we cannot escape them after we added actual tags
        line->replace("<", "§lt;"); // and we cannot use the amps "&" since we'll have to escape it to &amp; later on as well
        line->replace(link, "<a href=\"\\1\">\\1</a>");
        line->replace(mail, "<a href=\"mailto:\\1\">\\1</a>");
#define MARKUP(_item_) "<span class=\"markup\">"#_item_"</span>"
        line->replace(bold, "\\1<b>" MARKUP(*) "\\2" MARKUP(*) "</b>\\3");
        line->replace(italic, "\\1<i>" MARKUP(/) "\\2" MARKUP(/) "</i>\\3");
        line->replace(underline, "\\1<u>" MARKUP(_) "\\2" MARKUP(_) "</u>\\3");
#undef MARKUP
        line->replace("&", "&amp;");
        line->replace("§gt;", "&gt;");
        line->replace("§lt;", "&lt;");

        // if this is a non floating new line, prepend canonical quotemarks
        if (cQuoteLevel && !(cQuoteLevel == quoteLevel && markup.last().endsWith(' '))) {
            QString quotemarks("<span class=\"quotemarks\">");
            for (int i = 0; i < cQuoteLevel; ++i)
                quotemarks += "&gt;";
            quotemarks += " </span>";
            line->prepend(quotemarks);
        }

        // handle quotelevel depending blockquotes
        cQuoteLevel -= quoteLevel;
        quoteLevel += cQuoteLevel; // quoteLevel is now what cQuoteLevel was two lines before
        while (cQuoteLevel > 0) {
            line->prepend("<blockquote>");
            --cQuoteLevel;
        }
        while (cQuoteLevel < 0) {
            line->prepend("</blockquote>");
            ++cQuoteLevel;
        }
        // appaned or join the line
        if (!markup.isEmpty() && markup.last().endsWith(' ') && markup.last() != QLatin1String("-- "))
            markup.last().append(*line);
        else
            markup << *line;
    }
    // close open blockquotes
    // (bottom quoters, we're unfortunately -yet- not permittet to shoot them, so we need to deal with them ;-)
    QString quoteCloser;
    while (quoteLevel > 0) {
        quoteCloser.append("</blockquote>");
        --quoteLevel;
    }
    markup << quoteCloser;
    return markup;
}

}
}


