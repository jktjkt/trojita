/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>
   Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

This function recognizes http and https links, e-mail addresses, *bold*, /italic/ and _underline_ text.
*/
QString helperHtmlifySingleLine(QString line)
{
    // Static regexps for the engine construction.
    // Warning, these operate on the *escaped* HTML!
    static QString intro("(^|[\\s\\(\\[\\{])");
    static QString extro("($|[\\s\\),;.\\]\\}])");
    static const QRegExp patternRe(
                // hyperlinks
                "(" // cap(1)
                "https?://" // scheme prefix
                "(?:[;/?:@=$\\-_.+!',0-9a-zA-Z%#~\\[\\]\\(\\)\\*]|&amp;)+" // allowed characters
                "(?:[/@=$\\-_+'0-9a-zA-Z%#~]|&amp;)" // termination
                ")"
                // end of hyperlink
                "|"
                // e-mail pattern
                "((?:[a-zA-Z0-9_\\.!#$%'\\*\\+\\-/=?^`\\{|\\}~]|&amp;)+@[a-zA-Z0-9\\.\\-_]+)" // cap(2)
                // end of e-mail pattern
                "|"
                // formatting markup
                "(" // cap(3)
                // bold text
                + intro /* cap(4) */ + "\\*((?!\\*)\\S+)\\*" /* cap(5) */ + extro /* cap(6) */
                + "|"
                // italics
                + intro /* cap(7) */ + "/((?!/)\\S+)/" /* cap(8) */ + extro /* cap(9) */
                + "|"
                // underline
                + intro /* cap(10) */ + "_((?!_)\\S+)_" /* cap(11) */ + extro /* cap(12) */
                + ")"
                // end of the formatting markup
                , Qt::CaseSensitive, QRegExp::RegExp2
                );

    // RE instances to work on
    QRegExp pattern(patternRe);

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
        int pos = pattern.indexIn(line, start, QRegExp::CaretAtOffset);
        if (pos == -1 || pos == line.size()) {
            // No further matches for this line -> we're done
            break;
        }

        const QString &linkText = pattern.cap(1);
        const QString &mailText = pattern.cap(2);
        const QString &boldText = pattern.cap(5);
        const QString &italicText = pattern.cap(8);
        const QString &underlineText = pattern.cap(11);
        bool isSpecialFormat = !boldText.isEmpty() || !italicText.isEmpty() || !underlineText.isEmpty();
        QString replacement;

        if (!linkText.isEmpty()) {
            replacement = QString::fromUtf8("<a href=\"%1\">%1</a>").arg(linkText);
        } else if (!mailText.isEmpty()) {
            replacement = QString::fromUtf8("<a href=\"mailto:%1\">%1</a>").arg(mailText);
        } else if (isSpecialFormat) {
            // Careful here; the inner contents of the current match shall be formatted as well which is why we need recursion
            QChar elementName;
            QChar markupChar;
            int whichOne = 0;

            if (!boldText.isEmpty()) {
                elementName = QLatin1Char('b');
                markupChar = QLatin1Char('*');
                whichOne = 3;
            } else if (!italicText.isEmpty()) {
                elementName = QLatin1Char('i');
                markupChar = QLatin1Char('/');
                whichOne = 6;
            } else if (!underlineText.isEmpty()) {
                elementName = QLatin1Char('u');
                markupChar = QLatin1Char('_');
                whichOne = 9;
            }
            Q_ASSERT(whichOne);
            replacement = QString::fromUtf8("%1<%2><span class=\"markup\">%3</span>%4<span class=\"markup\">%3</span></%2>%5")
                    .arg(pattern.cap(whichOne + 1), elementName, markupChar,
                         helperHtmlifySingleLine(pattern.cap(whichOne + 2)), pattern.cap(whichOne + 3));
        }
        Q_ASSERT(!replacement.isEmpty());
        line = line.left(pos) + replacement + line.mid(pos + pattern.matchedLength());
        start = pos + replacement.size();
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
            markup << closingLabel.arg(controlStack.pop().second);
        }
        markup << closeSingleQuote;
        --quoteLevel;
        if (controlBlock) {
            markup << closeQuoteBlock;
        }
    }
}

/** @short Returna a regular expression which matches the signature separators */
QRegExp signatureSeparator()
{
    // "-- " is the standards-compliant signature separator.
    // "Line of underscores" is non-standard garbage which Mailman happily generates. Yes, it's nasty and ugly.
    return QRegExp(QLatin1String("(-- |_{45,})(\\r)?"));
}

struct TextInfo {
    int depth;
    QString text;

    TextInfo(const int depth, const QString &text): depth(depth), text(text)
    {
    }
};

QString plainTextToHtml(const QString &plaintext, const FlowedFormat flowed)
{
    static QRegExp quotemarks("^>[>\\s]*");
    const int SIGNATURE_SEPARATOR = -2;

    QList<TextInfo> lineBuffer;

    // First pass: determine the quote level for each source line.
    // The quote level is ignored for the signature.
    bool signatureSeparatorSeen = false;
    Q_FOREACH(QString line, plaintext.split('\n')) {

        // Fast path for empty lines
        if (line.isEmpty()) {
            lineBuffer << TextInfo(0, line);
            continue;
        }

        // Special marker for the signature separator
        if (signatureSeparator().exactMatch(line)) {
            lineBuffer << TextInfo(SIGNATURE_SEPARATOR, line);
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

        lineBuffer << TextInfo(quoteLevel, line);
    }

    // Second pass:
    // - Remove the quotemarks for everything prior to the signature separator.
    // - Collapse the lines with the same quoting level into a single block
    //   (optionally into a single line if format=flowed is active)
    auto it = lineBuffer.begin();
    while (it < lineBuffer.end() && it->depth != SIGNATURE_SEPARATOR) {

        // Remove the quotemarks
        it->text.remove(quotemarks);

        if (flowed == FORMAT_FLOWED_DELSP) {
            if (it->text.endsWith(QLatin1String(" \r"))) {
                it->text.chop(2);
                it->text += QLatin1Char('\r');
            } else if (it->text.endsWith(QLatin1Char(' '))) {
                it->text.chop(1);
            }
        }

        if (it == lineBuffer.begin()) {
            // No "previous line"
            ++it;
            continue;
        }

        // Check for the line joining
        auto prev = it - 1;
        if (prev->depth == it->depth) {
            // empty lines must not be removed

            QString separator = QLatin1String("\n");
            switch (flowed) {
            case FORMAT_PLAIN:
                // nothing fancy to do here
                break;
            case FORMAT_FLOWED:
            case FORMAT_FLOWED_DELSP:
                // Now the trailing \n is striped already; we only have to check for stuff ending with " " or " \r".
                if (prev->text.endsWith(QLatin1Char(' '))) {
                    separator = QString();
                } else if (prev->text.endsWith(QLatin1String(" \r"))) {
                    separator = QString();
                    // Remove that extra \r
                    prev->text.chop(1);
                }
                break;
            }
            prev->text += separator + it->text;
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

        if (it->depth == SIGNATURE_SEPARATOR && !signatureSeparatorSeen) {
            // The first signature separator
            signatureSeparatorSeen = true;
            closeQuotesUpTo(markup, controlStack, quoteLevel, 0);
            markup << QLatin1String("<span class=\"signature\">") + helperHtmlifySingleLine(it->text);
            markup << QLatin1String("\n");
            continue;
        }

        if (signatureSeparatorSeen) {
            // Just copy the data
            markup << helperHtmlifySingleLine(it->text);
            if (it+1 != lineBuffer.end())
                markup << QLatin1String("\n");
            continue;
        }

        Q_ASSERT(quoteLevel == 0 || quoteLevel != it->depth);

        if (quoteLevel > it->depth) {
            // going back in the quote hierarchy
            closeQuotesUpTo(markup, controlStack, quoteLevel, it->depth);
        }

        // Pretty-formatted block of the ">>>" characters
        QString quotemarks;

        if (it->depth) {
            quotemarks += QLatin1String("<span class=\"quotemarks\">");
            for (int i = 0; i < it->depth; ++i) {
                quotemarks += QLatin1String("&gt;");
            }
            quotemarks += QLatin1String(" </span>");
        }

        static const int previewLines = 5;
        static const int charsPerLineEquivalent = 160;
        static const int forceCollapseAfterLines = 10;

        if (quoteLevel < it->depth) {
            // We're going deeper in the quote hierarchy
            QString line;
            while (quoteLevel < it->depth) {
                ++quoteLevel;

                // Check whether there is anything at the newly entered level of nesting
                bool anythingOnJustThisLevel = false;

                // A short summary of the quotation
                QString preview;

                auto runner = it;
                while (runner != lineBuffer.end()) {
                    if (runner->depth == quoteLevel) {
                        anythingOnJustThisLevel = true;

                        ++interactiveControlsId;
                        controlStack.push(qMakePair(quoteLevel, interactiveControlsId));

                        QString omittedStuff;
                        QString previewPrefix, previewSuffix;
                        QString currentChunk = firstNLines(runner->text, previewLines, charsPerLineEquivalent);
                        QString omittedPrefix, omittedSuffix;
                        QString previewQuotemarks;

                        if (runner != it ) {
                            // we have skipped something, make it obvious to the user

                            // Find the closest level which got collapsed
                            int closestDepth = std::numeric_limits<int>::max();
                            auto depthRunner(it);
                            while (depthRunner != runner) {
                                closestDepth = std::min(closestDepth, depthRunner->depth);
                                ++depthRunner;
                            }

                            // The [...] marks shall be prefixed by the closestDepth quote markers
                            omittedStuff = QLatin1String("<span class=\"quotemarks\">");
                            for (int i = 0; i < closestDepth; ++i) {
                                omittedStuff += QLatin1String("&gt;");
                            }
                            for (int i = runner->depth; i < closestDepth; ++i) {
                                omittedPrefix += QLatin1String("<blockquote>");
                                omittedSuffix += QLatin1String("</blockquote>");
                            }
                            omittedStuff += QString::fromUtf8(" </span><label for=\"q%1\">...</label>").arg(interactiveControlsId);

                            // Now produce the proper quotation for the preview itself
                            for (int i = quoteLevel; i < runner->depth; ++i) {
                                previewPrefix.append(QLatin1String("<blockquote>"));
                                previewSuffix.append(QLatin1String("</blockquote>"));
                            }
                        }

                        previewQuotemarks = QLatin1String("<span class=\"quotemarks\">");
                        for (int i = 0; i < runner->depth; ++i) {
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
                    if (runner->depth < quoteLevel) {
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
                    if (runner->depth == SIGNATURE_SEPARATOR)
                        break;
                    if (runner->depth > 0)
                        continue;
                    if (runner->depth == 0 && !runner->text.isEmpty()) {
                        nothingButQuotesAndSpaceTillSignature = false;
                        break;
                    }
                }

                // Size of the current level, including the nested stuff
                int currentLevelCharCount = 0;
                int currentLevelLineCount = 0;
                runner = it;
                while (runner != lineBuffer.end() && runner->depth >= quoteLevel) {
                    currentLevelCharCount += runner->text.size();
                    // one for the actual block
                    currentLevelLineCount += runner->text.count(QLatin1Char('\n')) + 1;
                    ++runner;
                }


                if (!anythingOnJustThisLevel) {
                    // no need for fancy UI controls
                    line += QLatin1String("<blockquote>");
                    continue;
                }

                if (quoteLevel == it->depth
                        && currentLevelCharCount <= charsPerLineEquivalent * previewLines
                        && currentLevelLineCount <= previewLines) {
                    // special case: the quote is very short, no point in making it collapsible
                    line += QString::fromUtf8("<span class=\"level\"><input type=\"checkbox\" id=\"q%1\"/>").arg(interactiveControlsId)
                            + QLatin1String("<span class=\"shortquote\"><blockquote>") + quotemarks
                            + helperHtmlifySingleLine(it->text).replace(QLatin1String("\n"), QLatin1String("\n") + quotemarks);
                } else {
#if QT_VERSION < QT_VERSION_CHECK(4, 8, 0)
                    // old WebKit doesn't really support the dynamic updates of the :checked pseudoclass
                    bool collapsed = false;
                    Q_UNUSED(forceCollapseAfterLines);
#else
                    bool collapsed = nothingButQuotesAndSpaceTillSignature
                            || quoteLevel > 1
                            || currentLevelCharCount >= charsPerLineEquivalent * forceCollapseAfterLines
                            || currentLevelLineCount >= forceCollapseAfterLines;
#endif

                    line += QString::fromUtf8("<span class=\"level\"><input type=\"checkbox\" id=\"q%1\" %2/>")
                            .arg(QString::number(interactiveControlsId),
                                 collapsed ? QString::fromUtf8("checked=\"checked\"") : QString())
                            + QLatin1String("<span class=\"short\"><blockquote>")
                              + preview
                              + QString::fromUtf8(" <label for=\"q%1\">...</label>").arg(interactiveControlsId)
                              + QLatin1String("</blockquote></span>")
                            + QLatin1String("<span class=\"full\"><blockquote>");
                    if (quoteLevel == it->depth) {
                        // We're now finally on the correct level of nesting so we can output the current line
                        line += quotemarks + helperHtmlifySingleLine(it->text)
                                .replace(QLatin1String("\n"), QLatin1String("\n") + quotemarks);
                    }
                }
            }
            markup << line;
        } else {
            // Either no quotation or we're continuing an old quote block and there was a nested quotation before
            markup << quotemarks + helperHtmlifySingleLine(it->text)
                      .replace(QLatin1String("\n"), QLatin1String("\n") + quotemarks);
        }

        auto next = it + 1;
        if (next != lineBuffer.end()) {
            if (next->depth >= 0 && next->depth < it->depth) {
                // Decreasing the quotation level -> no starting <blockquote>
                markup << QLatin1String("\n");
            } else if (it->depth == 0) {
                // Non-quoted block which is not enclosed in a <blockquote>
                markup << QLatin1String("\n");
            }
        }
    }

    if (signatureSeparatorSeen) {
        // Terminate the signature
        markup << QLatin1String("</span>");
    }

    if (quoteLevel) {
        // Terminate the quotes
        closeQuotesUpTo(markup, controlStack, quoteLevel, 0);
    }

    Q_ASSERT(controlStack.isEmpty());

    return markup.join(QString());
}

}
}


