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

#include <QDebug> // FIXME: remove me

namespace Composer {
namespace Util {

/** @short Helper for plainTextToHtml for applying the HTML formatting

This funciton recognizes http and https links, e-mail addresses, *bold*, /italic/ and _underline_ text.
*/
QString helperHtmlifySingleLine(QString line)
{
    // Static regexps for the engine construction
    static const QRegExp linkRe("("
                              "https?://" // scheme prefix
                              "[;/?:@=&$\\-_.+!',0-9a-zA-Z%#~\\[\\]\\(\\)*]+" // allowed characters
                              "[/@=&$\\-_+'0-9a-zA-Z%#~]" // termination
                              ")");
    static const QRegExp mailRe("([a-zA-Z0-9\\.\\-_\\+]+@[a-zA-Z0-9\\.\\-_]+)");
    static QString intro("(^|[\\s\\(\\[\\{])");
    static QString extro("($|[\\s\\),;.\\]\\}])");
#define TROJITA_RE_BOLD "\\*(\\S*)\\*"
#define TROJITA_RE_ITALIC "/(\\S*)/"
#define TROJITA_RE_UNDERLINE "_(\\S*)_"
    static const QRegExp boldRe(intro + TROJITA_RE_BOLD + extro);
    static const QRegExp italicRe(intro + TROJITA_RE_ITALIC + extro);
    static const QRegExp underlineRe(intro + TROJITA_RE_UNDERLINE + extro);
    static const QRegExp anyFormattingRe(intro + "(" TROJITA_RE_BOLD "|" TROJITA_RE_ITALIC "|" TROJITA_RE_UNDERLINE ")" + extro);
#undef TROJITA_RE_BOLD
#undef TROJITA_RE_ITALIC
#undef TROJITA_RE_UNDERLINE

    // RE instances to work on
    QRegExp link(linkRe), mail(mailRe), bold(boldRe), italic(italicRe), underline(underlineRe), anyFormatting(anyFormattingRe);

    // Now prepare markup *bold*, /italic/ and _underline_ and also turn links into HTML.
    // This is a bit more involved because we want to apply the regular expressions in a certain order and also at the same
    // time prevent the lower-priority regexps from clobbering the output of the previous stages.
    int start = 0;
    while (start < line.size()) {
        qDebug() << "Main loop:" << start << line.size() << line;
        // Find the position of the first thing which matches
        int posLink = link.indexIn(line, start, QRegExp::CaretAtOffset);
        if (posLink == -1)
            posLink = line.size();

        int posMail = mail.indexIn(line, start, QRegExp::CaretAtOffset);
        if (posMail == -1)
            posMail = line.size();

        int posFormatting = anyFormatting.indexIn(line, start, QRegExp::CaretAtOffset);
        if (posFormatting == -1)
            posFormatting = line.size();

        const int firstSpecial = qMin(qMin(posLink, posMail), posFormatting);
        if (firstSpecial == line.size()) {
            qDebug() << "nothing else";
            // No further matches for this line -> we're done
            break;
        }
        qDebug() << "some RE has matched";

        if (firstSpecial == posLink) {
            QString replacement = QString::fromUtf8("<a href=\"%1\">%1</a>").arg(link.cap(1));
            line = line.left(firstSpecial) + replacement + line.mid(firstSpecial + link.matchedLength());
            start = firstSpecial + replacement.size();
        } else if (firstSpecial == posMail) {
            QString replacement = QString::fromUtf8("<a href=\"mailto:%1\">%1</a>").arg(mail.cap(1));
            line = line.left(firstSpecial) + replacement + line.mid(firstSpecial + mail.matchedLength());
            start = firstSpecial + replacement.size();
        } else if (firstSpecial == posFormatting) {
            // Careful here; the inner contents of the current match shall be formatted as well which is why we need recursion
            QChar elementName;
            QChar markupChar;
            const QRegExp *re = 0;

            if (posFormatting == bold.indexIn(line, start, QRegExp::CaretAtOffset)) {
                elementName = QLatin1Char('b');
                markupChar = QLatin1Char('*');
                re = &bold;
            } else if (posFormatting == italic.indexIn(line, start, QRegExp::CaretAtOffset)) {
                elementName = QLatin1Char('i');
                markupChar = QLatin1Char('/');
                re = &italic;
            } else if (posFormatting == underline.indexIn(line, start, QRegExp::CaretAtOffset)) {
                elementName = QLatin1Char('u');
                markupChar = QLatin1Char('_');
                re = &underline;
            }
            Q_ASSERT(re);
            qDebug() << "Got formatting";
            qDebug() << " old line:" << line;
            qDebug() << " at:" << line.mid(start);
            qDebug() << " prefix:" << line.left(firstSpecial);
            qDebug() << " suffix:" << line.mid(firstSpecial + re->matchedLength());
            QString replacement = QString::fromUtf8("%1<%2><span class=\"markup\">%3</span>%4<span class=\"markup\">%3</span></%2>%5")
                        .arg(re->cap(1), elementName, markupChar, helperHtmlifySingleLine(re->cap(2)), re->cap(3));

            qDebug() << " replacement:" << replacement;
            line = line.left(firstSpecial) + replacement + line.mid(firstSpecial + re->matchedLength());
            start = firstSpecial + replacement.size();
            qDebug() << " chunk to be still processed:" << line.mid(start);
        } else {
            Q_ASSERT(false);
        }
    }

    return line;
}

QStringList plainTextToHtml(const QString &plaintext, const FlowedFormat flowed)
{


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

        line = helperHtmlifySingleLine(line);

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


