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
#include <QPair>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QTextDocument>
#endif
#include "PlainTextFormatter.h"

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
    static const QRegExp mailRe("([\\w!#$%&'*+-/=?^_`{|}~]+@[a-zA-Z0-9\\.\\-_]+)");
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
            // No further matches for this line -> we're done
            break;
        }

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
            QString replacement = QString::fromUtf8("%1<%2><span class=\"markup\">%3</span>%4<span class=\"markup\">%3</span></%2>%5")
                        .arg(re->cap(1), elementName, markupChar, helperHtmlifySingleLine(re->cap(2)), re->cap(3));

            line = line.left(firstSpecial) + replacement + line.mid(firstSpecial + re->matchedLength());
            start = firstSpecial + replacement.size();
        } else {
            Q_ASSERT(false);
        }
    }

    return line;
}

QStringList plainTextToHtml(const QString &plaintext, const FlowedFormat flowed)
{
    static QRegExp quotemarks("^>[>\\s]*");
    const int SIGNATURE_SEPARATOR = -2;

    QList<QPair<int, QString> > lineBuffer;

    // First pass: determine the quote level for each source line.
    // The quote level is ignored for the signature.
    bool signatureSeparatorSeen = false;
    Q_FOREACH(QString line, plaintext.split('\n')) {

        // Fast path for empty lines
        if (line.isEmpty()) {
            lineBuffer << qMakePair(0, line);
            continue;
        }

        // Special marker for the signature separator
        if (line == QLatin1String("-- ")) {
            lineBuffer << qMakePair(SIGNATURE_SEPARATOR, line);
            signatureSeparatorSeen = true;
            continue;
        }

        // Determine the quoting level
        int quoteLevel = 0;
        if (!signatureSeparatorSeen && line.at(0) == '>') {
            int j = 1;
            quoteLevel = 1;
            while (j < line.length() && (line.at(j) == '>' || line.at(j) == ' '))
                quoteLevel += line.at(j++) == '>';
        }

        lineBuffer << qMakePair(quoteLevel, line);
    }

    // Second pass:
    // - Remove the quotemarks for everything prior to the signature separator.
    // - Collapse the lines with the same quoting level into a single block
    //   (optionally into a single line if format=flowed is active)
    QList<QPair<int, QString> >::iterator it = lineBuffer.begin();
    while (it < lineBuffer.end() && it->first != SIGNATURE_SEPARATOR) {

        // Remove the quotemarks
        it->second.remove(quotemarks);

        if (it == lineBuffer.begin()) {
            // No "previous line"
            ++it;
            continue;
        }

        // Check for the line joining
        QList<QPair<int, QString> >::iterator prev = it - 1;
        if (prev->first == it->first) {
            // empty lines must not be removed

            QString separator;
            switch (flowed) {
            case FORMAT_PLAIN:
                separator = QLatin1Char('\n');
                break;
            case FORMAT_FLOWED:
                separator = prev->second.endsWith(QLatin1Char(' ')) ? QLatin1String("") : QLatin1String("\n");
                break;
            }
            prev->second += separator + it->second;
            it = lineBuffer.erase(it);
        } else {
            ++it;
        }
    }

    // Third pass: the HTML escaping
    for (it = lineBuffer.begin(); it != lineBuffer.end(); ++it) {
        it->second = helperHtmlifySingleLine(
        // Escape the HTML entities
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
                    Qt::escape(it->second)
#else
                    it->second.toHtmlEscaped()
#endif
                    );
    }

    // Fourth pass: adding fancy markup
    signatureSeparatorSeen = false;
    int quoteLevel = 0;
    QStringList markup;
    static QLatin1String closeSingleQuote("</blockquote>");
    for (it = lineBuffer.begin(); it != lineBuffer.end(); ++it) {

        if (it->first == SIGNATURE_SEPARATOR && !signatureSeparatorSeen) {
            // The first signature separator
            signatureSeparatorSeen = true;
            // Terminate the quotes
            while (quoteLevel) {
                --quoteLevel;
                markup.last().append(closeSingleQuote);
            }
            markup << QLatin1String("<span class=\"signature\">-- ");
            continue;
        }

        if (signatureSeparatorSeen) {
            // Just copy the data
            markup << it->second;
            continue;
        }

        while (quoteLevel > it->first) {
            --quoteLevel;
            markup.last().append(closeSingleQuote);
        }
        QString prefix;
        while (quoteLevel < it->first) {
            ++quoteLevel;
            prefix += QLatin1String("<blockquote>");
        }
        if (quoteLevel) {
            prefix += QLatin1String("<span class=\"quotemarks\">");
            for (int i = 0; i < quoteLevel; ++i) {
                prefix += QLatin1String("&gt;");
            }
            prefix += QLatin1String(" </span>");
        }
        markup << prefix + it->second;
    }
    // Terminate the signature
    if (signatureSeparatorSeen) {
        markup.last().append(QLatin1String("</span>"));
    }
    // Terminate the quotes
    while (quoteLevel) {
        --quoteLevel;
        markup.last().append(closeSingleQuote);
    }

    return markup;
}

}
}


