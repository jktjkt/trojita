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

#include <limits>
#include <QObject>
#include <QPair>
#include <QStack>
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
#define TROJITA_RE_BOLD "\\*((?!\\*)\\S+)\\*"
#define TROJITA_RE_ITALIC "/((?!/)\\S+)/"
#define TROJITA_RE_UNDERLINE "_((?!_)\\S+)_"
    static const QRegExp boldRe(intro + TROJITA_RE_BOLD + extro);
    static const QRegExp italicRe(intro + TROJITA_RE_ITALIC + extro);
    static const QRegExp underlineRe(intro + TROJITA_RE_UNDERLINE + extro);
    static const QRegExp anyFormattingRe(intro + "(" TROJITA_RE_BOLD "|" TROJITA_RE_ITALIC "|" TROJITA_RE_UNDERLINE ")" + extro);
#undef TROJITA_RE_BOLD
#undef TROJITA_RE_ITALIC
#undef TROJITA_RE_UNDERLINE

    // RE instances to work on
    QRegExp link(linkRe), mail(mailRe), bold(boldRe), italic(italicRe), underline(underlineRe), anyFormatting(anyFormattingRe);

    // Escape the HTML entities
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    line = Qt::escape(line);
#else
    line = line.toHtmlEscaped();
#endif

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


/** @short Return a preview of the quoted text

The goal is to produce "roughly N" lines of non-empty text. Blanks are ignored and
lines longer than charsPerLine are broken into multiple chunks.
*/
QString firstNLines(const QString &input, int numLines, const int charsPerLine)
{
    Q_ASSERT(numLines >= 2);
    QString out = input.section(QLatin1Char('\n'), 0, numLines - 1, QString::SectionSkipEmpty);
    const int cutoff = numLines * charsPerLine;
    if (out.size() >= cutoff) {
        int pos = input.indexOf(QLatin1Char(' '), cutoff);
        if (pos != -1)
            return out.left(pos - 1);
    }
    return out;
}

/** @short Helper for closing blockquotes and adding the interactive control elements at the right places */
void closeQuotesUpTo(QStringList &markup, QStack<QPair<int, int> > &controlStack, int &quoteLevel, const int finalQuoteLevel)
{
    static QString closingLabel("<label for=\"q%1\"></label>");
    static QLatin1String closeSingleQuote("</blockquote>");
    static QLatin1String closeQuoteBlock("</span></span>");

    Q_ASSERT(quoteLevel >= finalQuoteLevel);

    while (quoteLevel > finalQuoteLevel) {
        // Check whether an interactive control element is supposed to be present here
        bool controlBlock = !controlStack.isEmpty() && (quoteLevel == controlStack.top().first);
        if (controlBlock) {
            markup.last().append(closingLabel.arg(controlStack.pop().second));
        }
        markup.last().append(closeSingleQuote);
        --quoteLevel;
        if (controlBlock) {
            markup.last().append(closeQuoteBlock);
        }
    }
}

/** @short Returna a regular expression which matches the signature separators */
QRegExp signatureSeparator()
{
    // "-- " is the standards-compliant signature separator.
    // "Line of underscores" is non-standard garbage which Mailman happily generates. Yes, it's nasty and ugly.
    return QRegExp(QLatin1String("-- |_{45,}"));
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
        if (signatureSeparator().exactMatch(line)) {
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

    // Third pass: HTML escaping, formatting and adding fancy markup
    signatureSeparatorSeen = false;
    int quoteLevel = 0;
    QStringList markup;
    int interactiveControlsId = 0;
    QStack<QPair<int,int> > controlStack;
    for (it = lineBuffer.begin(); it != lineBuffer.end(); ++it) {

        if (it->first == SIGNATURE_SEPARATOR && !signatureSeparatorSeen) {
            // The first signature separator
            signatureSeparatorSeen = true;
            closeQuotesUpTo(markup, controlStack, quoteLevel, 0);
            markup << QLatin1String("<span class=\"signature\">") + helperHtmlifySingleLine(it->second);
            continue;
        }

        if (signatureSeparatorSeen) {
            // Just copy the data
            markup << helperHtmlifySingleLine(it->second);
            continue;
        }

        Q_ASSERT(quoteLevel == 0 || quoteLevel != it->first);

        if (quoteLevel > it->first) {
            // going back in the quote hierarchy
            closeQuotesUpTo(markup, controlStack, quoteLevel, it->first);
        }

        // Pretty-formatted block of the ">>>" characters
        QString quotemarks;

        if (it->first) {
            quotemarks += QLatin1String("<span class=\"quotemarks\">");
            for (int i = 0; i < it->first; ++i) {
                quotemarks += QLatin1String("&gt;");
            }
            quotemarks += QLatin1String(" </span>");
        }

        static const int previewLines = 5;
        static const int charsPerLineEquivalent = 160;
        static const int forceCollapseAfterLines = 10;

        if (quoteLevel < it->first) {
            // We're going deeper in the quote hierarchy
            QString line;
            while (quoteLevel < it->first) {
                ++quoteLevel;

                // Check whether there is anything at the newly entered level of nesting
                bool anythingOnJustThisLevel = false;

                // A short summary of the quotation
                QString preview;

                QList<QPair<int, QString> >::iterator runner = it;
                while (runner != lineBuffer.end()) {
                    if (runner->first == quoteLevel) {
                        anythingOnJustThisLevel = true;

                        ++interactiveControlsId;
                        controlStack.push(qMakePair(quoteLevel, interactiveControlsId));

                        QString omittedStuff;
                        QString previewPrefix, previewSuffix;
                        QString currentChunk = firstNLines(runner->second, previewLines, charsPerLineEquivalent);
                        QString omittedPrefix, omittedSuffix;
                        QString previewQuotemarks;

                        if (runner != it ) {
                            // we have skipped something, make it obvious to the user

                            // Find the closest level which got collapsed
                            int closestDepth = std::numeric_limits<int>::max();
                            QList<QPair<int, QString> >::const_iterator depthRunner = it;
                            while (depthRunner != runner) {
                                closestDepth = std::min(closestDepth, depthRunner->first);
                                ++depthRunner;
                            }

                            // The [...] marks shall be prefixed by the closestDepth quote markers
                            omittedStuff = QLatin1String("<span class=\"quotemarks\">");
                            for (int i = 0; i < closestDepth; ++i) {
                                omittedStuff += QLatin1String("&gt;");
                            }
                            for (int i = runner->first; i < closestDepth; ++i) {
                                omittedPrefix += QLatin1String("<blockquote>");
                                omittedSuffix += QLatin1String("</blockquote>");
                            }
                            omittedStuff += QString::fromUtf8(" </span><label for=\"q%1\">...</label>").arg(interactiveControlsId);

                            // Now produce the proper quotation for the preview itself
                            for (int i = quoteLevel; i < runner->first; ++i) {
                                previewPrefix.append(QLatin1String("<blockquote>"));
                                previewSuffix.append(QLatin1String("</blockquote>"));
                            }
                        }

                        previewQuotemarks = QLatin1String("<span class=\"quotemarks\">");
                        for (int i = 0; i < runner->first; ++i) {
                            previewQuotemarks += QLatin1String("&gt;");
                        }
                        previewQuotemarks += QLatin1String(" </span>");

                        preview = previewPrefix
                                    + omittedPrefix + omittedStuff + omittedSuffix
                                + previewQuotemarks
                                + helperHtmlifySingleLine(currentChunk)
                                    .replace(QLatin1String("\n"), QLatin1String("\n") + previewQuotemarks)
                                + previewSuffix;

                        break;
                    }
                    if (runner->first < quoteLevel) {
                        // This means that we have left the current level of nesting, so there cannot possible be anything else
                        // at the current level of nesting *and* in the current quote block
                        break;
                    }
                    ++runner;
                }

                // Is there nothing but quotes until the end of mail or until the signature separator?
                bool nothingButQuotesAndSpaceTillSignature = true;
                runner = it;
                while (++runner != lineBuffer.end()) {
                    if (runner->first == SIGNATURE_SEPARATOR)
                        break;
                    if (runner->first > 0)
                        continue;
                    if (runner->first == 0 && !runner->second.isEmpty()) {
                        nothingButQuotesAndSpaceTillSignature = false;
                        break;
                    }
                }

                // Size of the current level, including the nested stuff
                int currentLevelCharCount = 0;
                int currentLevelLineCount = 0;
                runner = it;
                while (runner != lineBuffer.end() && runner->first >= quoteLevel) {
                    currentLevelCharCount += runner->second.size();
                    // one for the actual block
                    currentLevelLineCount += runner->second.count(QLatin1Char('\n')) + 1;
                    ++runner;
                }


                if (!anythingOnJustThisLevel) {
                    // no need for fancy UI controls
                    line += QLatin1String("<blockquote>");
                    continue;
                }

                if (quoteLevel == it->first
                        && currentLevelCharCount <= charsPerLineEquivalent * previewLines
                        && currentLevelLineCount <= previewLines) {
                    // special case: the quote is very short, no point in making it collapsible
                    line += QString::fromUtf8("<span class=\"level\"><input type=\"checkbox\" id=\"q%1\"/>").arg(interactiveControlsId)
                            + QLatin1String("<span class=\"shortquote\"><blockquote>") + quotemarks
                            + helperHtmlifySingleLine(it->second).replace(QLatin1String("\n"), QLatin1String("\n") + quotemarks);
                } else {
                    bool collapsed = nothingButQuotesAndSpaceTillSignature
                            || quoteLevel > 1
                            || currentLevelCharCount >= charsPerLineEquivalent * forceCollapseAfterLines
                            || currentLevelLineCount >= forceCollapseAfterLines;

                    line += QString::fromUtf8("<span class=\"level\"><input type=\"checkbox\" id=\"q%1\" %2/>")
                            .arg(QString::number(interactiveControlsId),
                                 collapsed ? QString::fromUtf8("checked=\"checked\"") : QString())
                            + QLatin1String("<span class=\"short\"><blockquote>")
                              + preview
                              + QString::fromUtf8("<label for=\"q%1\">...</label>").arg(interactiveControlsId)
                              + QLatin1String("</blockquote></span>")
                            + QLatin1String("<span class=\"full\"><blockquote>");
                    if (quoteLevel == it->first) {
                        // We're now finally on the correct level of nesting so we can output the current line
                        line += quotemarks + helperHtmlifySingleLine(it->second)
                                .replace(QLatin1String("\n"), QLatin1String("\n") + quotemarks);
                    }
                }
            }
            markup << line;
        } else {
            // Either no quotation or we're continuing an old quote block and there was a nested quotation before
            markup << quotemarks + helperHtmlifySingleLine(it->second)
                      .replace(QLatin1String("\n"), QLatin1String("\n") + quotemarks);
        }
    }

    if (signatureSeparatorSeen) {
        // Terminate the signature
        markup.last().append(QLatin1String("</span>"));
    }

    if (quoteLevel) {
        // Terminate the quotes
        closeQuotesUpTo(markup, controlStack, quoteLevel, 0);
    }

    Q_ASSERT(controlStack.isEmpty());

    return markup;
}

}
}


