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
#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QFontInfo>
#include <QModelIndex>
#include <QPair>
#include <QStack>
#include "PlainTextFormatter.h"
#include "Common/Paths.h"
#include "Imap/Model/ItemRoles.h"
#include "UiUtils/Color.h"

namespace UiUtils {

/** @short Helper for plainTextToHtml for applying the HTML formatting

This function recognizes http and https links, e-mail addresses, *bold*, /italic/ and _underline_ text.
*/
QString helperHtmlifySingleLine(QString line)
{
    // Static regexps for the engine construction.
    // Warning, these operate on the *escaped* HTML!
#define HTML_RE_INTRO "(^|[\\s\\(\\[\\{])"
#define HTML_RE_EXTRO "($|[\\s\\),;.\\]\\}])"
    static const QRegExp patternRe(QLatin1String(
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
                HTML_RE_INTRO /* cap(4) */ "\\*((?!\\*)\\S+)\\*" /* cap(5) */ HTML_RE_EXTRO /* cap(6) */
                "|"
                // italics
                HTML_RE_INTRO /* cap(7) */ "/((?!/)\\S+)/" /* cap(8) */ HTML_RE_EXTRO /* cap(9) */
                "|"
                // underline
                HTML_RE_INTRO /* cap(10) */ "_((?!_)\\S+)_" /* cap(11) */ HTML_RE_EXTRO /* cap(12) */
                ")"
                // end of the formatting markup
                ), Qt::CaseSensitive, QRegExp::RegExp2
                );

    // RE instances to work on
    QRegExp pattern(patternRe);

    // Escape the HTML entities
    line = line.toHtmlEscaped();

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
            replacement = QStringLiteral("<a href=\"%1\">%1</a>").arg(linkText);
        } else if (!mailText.isEmpty()) {
            replacement = QStringLiteral("<a href=\"mailto:%1\">%1</a>").arg(mailText);
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
            replacement = QStringLiteral("%1<%2><span class=\"markup\">%3</span>%4<span class=\"markup\">%3</span></%2>%5")
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
    static QString closingLabel(QStringLiteral("<label for=\"q%1\"></label>"));
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

static QString lineWithoutTrailingCr(const QString &line)
{
    return line.endsWith(QLatin1Char('\r')) ? line.left(line.size() - 1) : line;
}

QString plainTextToHtml(const QString &plaintext, const FlowedFormat flowed)
{
    QRegExp quotemarks;
    switch (flowed) {
    case FlowedFormat::FLOWED:
    case FlowedFormat::FLOWED_DELSP:
        quotemarks = QRegExp(QLatin1String("^>+"));
        break;
    case FlowedFormat::PLAIN:
        // Also accept > interleaved by spaces. That's what KMail happily produces.
        // A single leading space is accepted, too. That's what Gerrit produces.
        quotemarks = QRegExp(QLatin1String("^( >|>)+"));
        break;
    }
    const int SIGNATURE_SEPARATOR = -2;

    auto lines = plaintext.split(QLatin1Char('\n'));
    std::vector<TextInfo> lineBuffer;
    lineBuffer.reserve(lines.size());

    // First pass: determine the quote level for each source line.
    // The quote level is ignored for the signature.
    bool signatureSeparatorSeen = false;
    Q_FOREACH(const QString &line, lines) {

        // Fast path for empty lines
        if (line.isEmpty()) {
            lineBuffer.emplace_back(0, line);
            continue;
        }

        // Special marker for the signature separator
        if (signatureSeparator().exactMatch(line)) {
            lineBuffer.emplace_back(SIGNATURE_SEPARATOR, lineWithoutTrailingCr(line));
            signatureSeparatorSeen = true;
            continue;
        }

        // Determine the quoting level
        int quoteLevel = 0;
        if (!signatureSeparatorSeen && quotemarks.indexIn(line) == 0) {
            quoteLevel = quotemarks.cap(0).count(QLatin1Char('>'));
        }

        lineBuffer.emplace_back(quoteLevel, lineWithoutTrailingCr(line));
    }

    // Second pass:
    // - Remove the quotemarks for everything prior to the signature separator.
    // - Collapse the lines with the same quoting level into a single block
    //   (optionally into a single line if format=flowed is active)
    auto it = lineBuffer.begin();
    while (it < lineBuffer.end() && it->depth != SIGNATURE_SEPARATOR) {

        // Remove the quotemarks
        it->text.remove(quotemarks);

        switch (flowed) {
        case FlowedFormat::FLOWED:
        case FlowedFormat::FLOWED_DELSP:
            if (flowed == FlowedFormat::FLOWED || flowed == FlowedFormat::FLOWED_DELSP) {
                // check for space-stuffing
                if (it->text.startsWith(QLatin1Char(' '))) {
                    it->text.remove(0, 1);
                }

                // quirk: fix a flowed line which actually isn't flowed
                if (it->text.endsWith(QLatin1Char(' ')) && (
                        it+1 == lineBuffer.end() || // end-of-document
                        (it+1)->depth == SIGNATURE_SEPARATOR || // right in front of the separator
                        (it+1)->depth != it->depth // end of paragraph
                   )) {
                    it->text.chop(1);
                }
            }
            break;
        case FlowedFormat::PLAIN:
            if (it->depth > 0 && it->text.startsWith(QLatin1Char(' '))) {
                // Because the space is re-added when we prepend the quotes. Adding that space is done
                // in order to make it look nice, i.e. to prevent lines like ">>something".
                it->text.remove(0, 1);
            }
            break;
        }


        if (it == lineBuffer.begin()) {
            // No "previous line"
            ++it;
            continue;
        }

        // Check for the line joining
        auto prev = it - 1;
        if (prev->depth == it->depth) {

            QString separator = QStringLiteral("\n");
            switch (flowed) {
            case FlowedFormat::PLAIN:
                // nothing fancy to do here, we cannot really join lines
                break;
            case FlowedFormat::FLOWED:
            case FlowedFormat::FLOWED_DELSP:
                // CR LF trailing is stripped already (LFs by the split into lines, CRs by lineWithoutTrailingCr in pass #1),
                // so we only have to check for the trailing space
                if (prev->text.endsWith(QLatin1Char(' '))) {

                    // implement the DelSp thingy
                    if (flowed == FlowedFormat::FLOWED_DELSP) {
                        prev->text.chop(1);
                    }

                    if (it->text.isEmpty() || prev->text.isEmpty()) {
                        // This one or the previous line is a blank one, so we cannot really join them
                    } else {
                        separator = QString();
                    }
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
            markup << QStringLiteral("\n");
            continue;
        }

        if (signatureSeparatorSeen) {
            // Just copy the data
            markup << helperHtmlifySingleLine(it->text);
            if (it+1 != lineBuffer.end())
                markup << QStringLiteral("\n");
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
                            omittedStuff = QStringLiteral("<span class=\"quotemarks\">");
                            for (int i = 0; i < closestDepth; ++i) {
                                omittedStuff += QLatin1String("&gt;");
                            }
                            for (int i = runner->depth; i < closestDepth; ++i) {
                                omittedPrefix += QLatin1String("<blockquote>");
                                omittedSuffix += QLatin1String("</blockquote>");
                            }
                            omittedStuff += QStringLiteral(" </span><label for=\"q%1\">...</label>").arg(interactiveControlsId);

                            // Now produce the proper quotation for the preview itself
                            for (int i = quoteLevel; i < runner->depth; ++i) {
                                previewPrefix.append(QLatin1String("<blockquote>"));
                                previewSuffix.append(QLatin1String("</blockquote>"));
                            }
                        }

                        previewQuotemarks = QStringLiteral("<span class=\"quotemarks\">");
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
                    line += QStringLiteral("<span class=\"level\"><input type=\"checkbox\" id=\"q%1\"/>").arg(interactiveControlsId)
                            + QLatin1String("<span class=\"shortquote\"><blockquote>") + quotemarks
                            + helperHtmlifySingleLine(it->text).replace(QLatin1String("\n"), QLatin1String("\n") + quotemarks);
                } else {
                    bool collapsed = nothingButQuotesAndSpaceTillSignature
                            || quoteLevel > 1
                            || currentLevelCharCount >= charsPerLineEquivalent * forceCollapseAfterLines
                            || currentLevelLineCount >= forceCollapseAfterLines;

                    line += QStringLiteral("<span class=\"level\"><input type=\"checkbox\" id=\"q%1\" %2/>")
                            .arg(QString::number(interactiveControlsId),
                                 collapsed ? QStringLiteral("checked=\"checked\"") : QString())
                            + QLatin1String("<span class=\"short\"><blockquote>")
                              + preview
                              + QStringLiteral(" <label for=\"q%1\">...</label>").arg(interactiveControlsId)
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
                markup << QStringLiteral("\n");
            } else if (it->depth == 0) {
                // Non-quoted block which is not enclosed in a <blockquote>
                markup << QStringLiteral("\n");
            }
        }
    }

    if (signatureSeparatorSeen) {
        // Terminate the signature
        markup << QStringLiteral("</span>");
    }

    if (quoteLevel) {
        // Terminate the quotes
        closeQuotesUpTo(markup, controlStack, quoteLevel, 0);
    }

    Q_ASSERT(controlStack.isEmpty());

    return markup.join(QString());
}

QString htmlizedTextPart(const QModelIndex &partIndex, const QFontInfo &font, const QColor &backgroundColor, const QColor &textColor,
                         const QColor &linkColor, const QColor &visitedLinkColor)
{
    static const QString defaultStyle = QString::fromUtf8(
        "pre{word-wrap: break-word; white-space: pre-wrap;}"
        // The following line, sadly, produces a warning "QFont::setPixelSize: Pixel size <= 0 (0)".
        // However, if it is not in place or if the font size is set higher, even to 0.1px, WebKit reserves space for the
        // quotation characters and therefore a weird white area appears. Even width: 0px doesn't help, so it looks like
        // we will have to live with this warning for the time being.
        ".quotemarks{color:transparent;font-size:0px;}"

        // Cannot really use the :dir(rtl) selector for putting the quote indicator to the "correct" side.
        // It's CSS4 and it isn't supported yet.
        "blockquote{font-size:90%; margin: 4pt 0 4pt 0; padding: 0 0 0 1em; border-left: 2px solid %1; unicode-bidi: -webkit-plaintext}"

        // Stop the font size from getting smaller after reaching two levels of quotes
        // (ie. starting on the third level, don't make the size any smaller than what it already is)
        "blockquote blockquote blockquote {font-size: 100%}"
        ".signature{opacity: 0.6;}"

        // Dynamic quote collapsing via pure CSS, yay
        "input {display: none}"
        "input ~ span.full {display: block}"
        "input ~ span.short {display: none}"
        "input:checked ~ span.full {display: none}"
        "input:checked ~ span.short {display: block}"
        "label {border: 1px solid %2; border-radius: 5px; padding: 0px 4px 0px 4px; white-space: nowrap}"
        // BLACK UP-POINTING SMALL TRIANGLE (U+25B4)
        // BLACK DOWN-POINTING SMALL TRIANGLE (U+25BE)
        "span.full > blockquote > label:before {content: \"\u25b4\"}"
        "span.short > blockquote > label:after {content: \" \u25be\"}"
        "span.shortquote > blockquote > label {display: none}"
    );

    QString fontSpecification(QStringLiteral("pre{"));
    if (font.italic())
        fontSpecification += QLatin1String("font-style: italic; ");
    if (font.bold())
        fontSpecification += QLatin1String("font-weight: bold; ");
    fontSpecification += QStringLiteral("font-size: %1px; font-family: \"%2\", monospace }").arg(
                QString::number(font.pixelSize()), font.family());

    QString textColors = QString::fromUtf8("body { background-color: %1; color: %2 }"
                                           "a:link { color: %3 } a:visited { color: %4 } a:hover { color: %3 }").arg(
                backgroundColor.name(), textColor.name(), linkColor.name(), visitedLinkColor.name());
    // looks like there's no special color for hovered links in Qt

    // build stylesheet and html header
    QColor tintForQuoteIndicator = backgroundColor;
    tintForQuoteIndicator.setAlpha(0x66);
    static QString stylesheet = defaultStyle.arg(linkColor.name(),
                                                 tintColor(textColor, tintForQuoteIndicator).name());
    static QFile file(Common::writablePath(Common::LOCATION_DATA) + QLatin1String("message.css"));
    static QDateTime lastVersion;
    QDateTime lastTouched(file.exists() ? QFileInfo(file).lastModified() : QDateTime());
    if (lastVersion < lastTouched) {
        stylesheet = defaultStyle;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString userSheet = QString::fromLocal8Bit(file.readAll().data());
            lastVersion = lastTouched;
            stylesheet += QLatin1Char('\n') + userSheet;
            file.close();
        }
    }

    // The dir="auto" is required for WebKit to treat all paragraphs as entities with possibly different text direction.
    // The individual paragraphs unfortunately share the same text alignment, though, as per
    // https://bugs.webkit.org/show_bug.cgi?id=71194 (fixed in Blink already).
    QString htmlHeader(QLatin1String("<html><head><style type=\"text/css\"><!--") + textColors + fontSpecification + stylesheet +
                       QLatin1String("--></style></head><body><pre dir=\"auto\">"));
    static QString htmlFooter(QStringLiteral("\n</pre></body></html>"));


    // We cannot rely on the QWebFrame's toPlainText because of https://bugs.kde.org/show_bug.cgi?id=321160
    QString markup = plainTextToHtml(partIndex.data(Imap::Mailbox::RolePartUnicodeText).toString(), flowedFormatForPart(partIndex));

    return htmlHeader + markup + htmlFooter;
}

FlowedFormat flowedFormatForPart(const QModelIndex &partIndex)
{
    FlowedFormat flowedFormat = FlowedFormat::PLAIN;
    if (partIndex.data(Imap::Mailbox::RolePartContentFormat).toString().toLower() == QLatin1String("flowed")) {
        flowedFormat = FlowedFormat::FLOWED;

        if (partIndex.data(Imap::Mailbox::RolePartContentDelSp).toString().toLower() == QLatin1String("yes"))
            flowedFormat = FlowedFormat::FLOWED_DELSP;
    }
    return flowedFormat;
}

}
