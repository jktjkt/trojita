/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>
   Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QTextDocument>
#endif

#include "PlainTextFormatter.h"

namespace Composer {
namespace Util {

QStringList plainTextToHtml(const QString &plaintext, const FlowedFormat flowed)
{
    static const QRegExp link("("
                              "https?://" // scheme prefix
                              "[;/?:@=&$\\-_.+!',0-9a-zA-Z%#~]+" // allowed characters
                              "[/@=&$\\-_+'0-9a-zA-Z%#~]" // termination
                              ")");
    static const QRegExp mail("([a-zA-Z0-9\\.\\-_\\+]+@[a-zA-Z0-9\\.\\-_]+)");
    static QString intro("(^|[\\s\\(\\[\\{])");
    static QString extro("($|[\\s\\),;.\\]\\}])");
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
    // have we seen the signature separator and should we therefore explicitly close that block later?
    bool shallCloseSignature = false;
    for (int i = 0; i < plain.count(); ++i) {
        QString &line = plain[i];

        // ignore empty lines
        if (line.isEmpty()) {
            markup << line;
            continue;
        }
        // determine quotelevel
        int cQuoteLevel = 0;
        if (line.at(0) == '>') {
            int j = 1;
            cQuoteLevel = 1;
            while (j < line.length() && (line.at(j) == '>' || line.at(j) == ' '))
                cQuoteLevel += line.at(j++) == '>';
        }
        // strip quotemarks
        if (cQuoteLevel) {
            static QRegExp quotemarks("^[>\\s]*");
            line.remove(quotemarks);
        }
        // Escape the HTML entities
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
        line = Qt::escape(line);
#else
        line = line.toHtmlEscaped();
#endif
        // markup *bold*, /italic/, _underline_ and active links
        line.replace(link, "<a href=\"\\1\">\\1</a>");
        line.replace(mail, "<a href=\"mailto:\\1\">\\1</a>");
#define MARKUP(_item_) "<span class=\"markup\">"#_item_"</span>"
        line.replace(bold, "\\1<b>" MARKUP(*) "\\2" MARKUP(*) "</b>\\3");
        line.replace(italic, "\\1<i>" MARKUP(/) "\\2" MARKUP(/) "</i>\\3");
        line.replace(underline, "\\1<u>" MARKUP(_) "\\2" MARKUP(_) "</u>\\3");
#undef MARKUP

        // if this is a non floating new line, prepend canonical quotemarks
        if (cQuoteLevel && !(cQuoteLevel == quoteLevel && markup.last().endsWith(' '))) {
            QString quotemarks("<span class=\"quotemarks\">");
            for (int i = 0; i < cQuoteLevel; ++i)
                quotemarks += "&gt;";
            quotemarks += " </span>";
            line.prepend(quotemarks);
        }

        if (cQuoteLevel < quoteLevel) {
            // this line is ascending in the quoted depth
            Q_ASSERT(!markup.isEmpty());
            for (int i = 0; i < quoteLevel - cQuoteLevel; ++i) {
                markup.last().append("</blockquote>");
            }
        } else if (cQuoteLevel > quoteLevel) {
            // even more nested quotations
            for (int i = 0; i < cQuoteLevel - quoteLevel; ++i) {
                line.prepend("<blockquote>");
            }
        }

        if (!shallCloseSignature && line == QLatin1String("-- ")) {
            // Only recognize the first signature separator
            shallCloseSignature = true;
            line.prepend(QLatin1String("<span class=\"signature\">"));
        }

        // appaned or join the line
        if (markup.isEmpty()) {
            markup << line;
        } else if (flowed == FORMAT_FLOWED) {
            if ((quoteLevel == cQuoteLevel) && markup.last().endsWith(QLatin1Char(' ')) &&
                    markup.last() != QLatin1String("<span class=\"signature\">-- "))
                markup.last().append(line);
            else
                markup << line;
        } else {
            markup << line;
        }

        quoteLevel = cQuoteLevel;
    }

    // close any open elements
    QString closer;
    if (shallCloseSignature)
        closer = QLatin1String("</span>");
    // close open blockquotes
    // (bottom quoters, we're unfortunately -yet- not permittet to shoot them, so we need to deal with them ;-)
    while (quoteLevel > 0) {
        closer.append("</blockquote>");
        --quoteLevel;
    }
    if (!closer.isEmpty()) {
        Q_ASSERT(!markup.isEmpty());
        markup.last().append(closer);
    }

    return markup;
}

}
}


