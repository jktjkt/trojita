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

#include <QtTest>
#include <QAction>
#include <QTextDocument>
#include <QWebFrame>
#include <QWebView>
#include "test_Html_formatting.h"
#include "Utils/headless_test.h"
#include "Composer/Recipients.h"
#include "Composer/ReplaceSignature.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Composer/SubjectMangling.h"

Q_DECLARE_METATYPE(QList<QUrl>)

static QString visualizeWhitespace(QString s)
{
    return s.replace(QLatin1Char(' '),
                     // U+00B7, a middle dot
                     QChar(0x00b7));
}

/** @short Test that conversion of plaintext mail to HTML works reasonably well */
void HtmlFormattingTest::testPlainTextFormattingFlowed()
{
    QFETCH(QString, plaintext);
    QFETCH(QString, htmlFlowed);
    QFETCH(QString, htmlNotFlowed);

    QCOMPARE(visualizeWhitespace(UiUtils::plainTextToHtml(plaintext, UiUtils::FlowedFormat::FLOWED)),
             visualizeWhitespace(htmlFlowed));
    QCOMPARE(visualizeWhitespace(UiUtils::plainTextToHtml(plaintext, UiUtils::FlowedFormat::PLAIN)),
             visualizeWhitespace(htmlNotFlowed));
}

/** @short Data for testPlainTextFormattingFlowed */
void HtmlFormattingTest::testPlainTextFormattingFlowed_data()
{
    QTest::addColumn<QString>("plaintext");
    QTest::addColumn<QString>("htmlFlowed");
    QTest::addColumn<QString>("htmlNotFlowed");

    QTest::newRow("empty-1") << QString() << QString() << QString();
    QTest::newRow("empty-2") << QString("") << QString("") << QString("");
    QTest::newRow("empty-3") << QString("\n") << QString("\n") << QString("\n");
    QTest::newRow("empty-4") << QString("\n\n") << QString("\n\n") << QString("\n\n");

    QTest::newRow("minimal") << QString("ahoj") << QString("ahoj") << QString("ahoj");

    QTest::newRow("multiline-trivial-LF") << QString("Sample \ntext") << QString("Sample text") << QString("Sample \ntext");
    QTest::newRow("multiline-trivial-CR") << QString("Sample \rtext") << QString("Sample \rtext") << QString("Sample \rtext");
    QTest::newRow("multiline-trivial-CRLF") << QString("Sample \r\ntext") << QString("Sample text") << QString("Sample \ntext");
    QTest::newRow("multiline-with-empty-lines")
            << QString("Sample \ntext.\n\nYay!")
            << QString("Sample text.\n\nYay!")
            << QString("Sample \ntext.\n\nYay!");

    QTest::newRow("signature-LF")
            << QString("Yay.\n-- \nMeh.\n")
            << QString("Yay.\n<span class=\"signature\">-- \nMeh.\n</span>")
            << QString("Yay.\n<span class=\"signature\">-- \nMeh.\n</span>");
    QTest::newRow("signature-CRLF")
            << QString("Yay.\r\n-- \r\nMeh.\r\n")
            << QString("Yay.\n<span class=\"signature\">-- \nMeh.\n</span>")
            << QString("Yay.\n<span class=\"signature\">-- \nMeh.\n</span>");

    QTest::newRow("gerrit-extra-leading-space")
            << QString("Patch Set 2: Code-Review+2\r\n"
                       "\r\n"
                       "> There is no limit on the number of flags which could be present for\r\n"
                       " > a message\r\n"
                       "\r\n"
                       "Ok, checked ::normalizedFlags() - indeed *everything* is implicitly shared =)")
            << QString("Patch Set 2: Code-Review+2\n"
                       "\n"
                       "<span class=\"level\"><input type=\"checkbox\" id=\"q1\"/><span class=\"shortquote\"><blockquote><span class=\"quotemarks\">&gt; </span>There is no limit on the number of flags which could be present for\n"
                       "<label for=\"q1\"></label></blockquote></span></span>&gt; a message\n"
                       "\n"
                       "Ok, checked ::normalizedFlags() - indeed <b><span class=\"markup\">*</span>everything<span class=\"markup\">*</span></b> is implicitly shared =)")
            << QString("Patch Set 2: Code-Review+2\n"
                       "\n"
                       "<span class=\"level\"><input type=\"checkbox\" id=\"q1\"/><span class=\"shortquote\"><blockquote><span class=\"quotemarks\">&gt; </span>There is no limit on the number of flags which could be present for\n"
                       "<span class=\"quotemarks\">&gt; </span>a message\n"
                       "<label for=\"q1\"></label></blockquote></span></span>\n"
                       "Ok, checked ::normalizedFlags() - indeed <b><span class=\"markup\">*</span>everything<span class=\"markup\">*</span></b> is implicitly shared =)");
}

/** @short Corner cases of the DelSp formatting */
void HtmlFormattingTest::testPlainTextFormattingFlowedDelSp()
{
    QFETCH(QString, plaintext);
    QFETCH(QString, htmlFlowedDelSp);

    QCOMPARE(visualizeWhitespace(UiUtils::plainTextToHtml(plaintext, UiUtils::FlowedFormat::FLOWED_DELSP)),
             visualizeWhitespace(htmlFlowedDelSp));
}

/** @short Data for testPlainTextFormattingFlowedDelSp */
void HtmlFormattingTest::testPlainTextFormattingFlowedDelSp_data()
{
    QTest::addColumn<QString>("plaintext");
    QTest::addColumn<QString>("htmlFlowedDelSp");

    QTest::newRow("delsp-canonical") << QString("abc  \r\ndef") << QString("abc def");
    QTest::newRow("delsp-just-lf") << QString("abc  \ndef") << QString("abc def");
    QTest::newRow("delsp-borked-crlf") << QString("abc\r\ndef") << QString("abc\ndef");
    QTest::newRow("delsp-borked-lf") << QString("abc\ndef") << QString("abc\ndef");
    QTest::newRow("delsp-single-line-no-crlf") << QString("abc ") << QString("abc");
    QTest::newRow("delsp-single-line-crlf") << QString("abc \r\n") << QString("abc\n");
    QTest::newRow("delsp-single-line-lf") << QString("abc \n") << QString("abc\n");
    QTest::newRow("delsp-single-line-cr") << QString("abc \r") << QString("abc");
}

void HtmlFormattingTest::testPlainTextFormattingViaHtml()
{
    QFETCH(QString, plaintext);
    QFETCH(QString, html);

    QCOMPARE(UiUtils::plainTextToHtml(plaintext, UiUtils::FlowedFormat::FLOWED), html);
}

void HtmlFormattingTest::testPlainTextFormattingViaHtml_data()
{
    QTest::addColumn<QString>("plaintext");
    QTest::addColumn<QString>("html");

    QTest::newRow("containing-html")
            << QString("<p>ahoj &amp; blesmrt</p>")
            << QString("&lt;p&gt;ahoj &amp;amp; blesmrt&lt;/p&gt;");
    QTest::newRow("basic-formatting-1") << QString("foo bar") << QString("foo bar");
    QTest::newRow("basic-formatting-2")
            << QString("ahoj *cau* nazdar")
            << QString("ahoj <b><span class=\"markup\">*</span>cau<span class=\"markup\">*</span></b> nazdar");
    QTest::newRow("basic-formatting-3")
            << QString("/ahoj/ *cau*")
            << QString("<i><span class=\"markup\">/</span>ahoj<span class=\"markup\">/</span></i> <b><span class=\"markup\">*</span>cau<span class=\"markup\">*</span></b>");
    QTest::newRow("basic-formatting-4")
            << QString("ahoj *_cau_* nazdar")
            << QString("ahoj <b><span class=\"markup\">*</span><u><span class=\"markup\">_</span>cau"
                       "<span class=\"markup\">_</span></u><span class=\"markup\">*</span></b> nazdar");
    QTest::newRow("basic-formatting-666")
            << QString("foo *bar* _baz_ /pwn/ yay foo@ @bar @ blesmrt")
            << QString("foo <b><span class=\"markup\">*</span>bar<span class=\"markup\">*</span></b> "
                       "<u><span class=\"markup\">_</span>baz<span class=\"markup\">_</span></u> "
                       "<i><span class=\"markup\">/</span>pwn<span class=\"markup\">/</span></i> yay foo@ @bar @ blesmrt");
    QTest::newRow("formatting-and-newlines")
            << QString("*blesmrt*\ntrojita")
            << QString("<b><span class=\"markup\">*</span>blesmrt<span class=\"markup\">*</span></b>\ntrojita");
    QTest::newRow("links")
            << QString("ahoj http://pwn:123/foo?bar&baz#nope")
            << QString("ahoj <a href=\"http://pwn:123/foo?bar&amp;baz#nope\">http://pwn:123/foo?bar&amp;baz#nope</a>");
    // Test our escaping
    QTest::newRow("escaping-1")
            << QString::fromUtf8("<>&&gt; § §gt; §para;\n")
            << QString::fromUtf8("&lt;&gt;&amp;&amp;gt; § §gt; §para;\n");
    // A plaintext actually containing some HTML code -- bug 323390
    QTest::newRow("escaping-html-url-bug-323390")
            << QString("<a href=\"http://trojita.flaska.net\">Trojita</a>")
            << QString("&lt;a href=&quot;<a href=\"http://trojita.flaska.net\">http://trojita.flaska.net</a>&quot;&gt;Trojita&lt;/a&gt;");
    QTest::newRow("escaping-html-mail-bug-323390")
            << QString("some-mail&ad'dr@foo")
            << QString("<a href=\"mailto:some-mail&amp;ad'dr@foo\">some-mail&amp;ad'dr@foo</a>");

    QTest::newRow("mailto-1")
            << QString("ble.smrt-1_2+3@example.org")
            << QString("<a href=\"mailto:ble.smrt-1_2+3@example.org\">ble.smrt-1_2+3@example.org</a>");

    QTest::newRow("multiple-links-on-line")
            << QString::fromUtf8("Hi,\n"
                                 "http://meh/ http://pwn/now foo@bar http://wtf\n"
                                 "nothing x@y.org\n"
                                 "foo@example.org else\n"
                                 "test@domain"
                                 )
            << QString::fromUtf8("Hi,\n"
                                 "<a href=\"http://meh/\">http://meh/</a> <a href=\"http://pwn/now\">http://pwn/now</a> "
                                    "<a href=\"mailto:foo@bar\">foo@bar</a> <a href=\"http://wtf\">http://wtf</a>\n"
                                 "nothing <a href=\"mailto:x@y.org\">x@y.org</a>\n"
                                 "<a href=\"mailto:foo@example.org\">foo@example.org</a> else\n"
                                 "<a href=\"mailto:test@domain\">test@domain</a>");

    QTest::newRow("http-link-with-nested-mail-and-formatting-chars")
            << QString::fromUtf8("http://example.org/meh/yay/?foo=test@example.org\n"
                                 "http://example.org/(*checkout*)/pwn\n"
                                 "*https://domain.org/yay*")
            << QString::fromUtf8("<a href=\"http://example.org/meh/yay/?foo=test@example.org\">http://example.org/meh/yay/?foo=test@example.org</a>\n"
                                 "<a href=\"http://example.org/(*checkout*)/pwn\">http://example.org/(*checkout*)/pwn</a>\n"
                                 "<b><span class=\"markup\">*</span><a href=\"https://domain.org/yay\">https://domain.org/yay</a><span class=\"markup\">*</span></b>");

    QTest::newRow("just-underscores")
            << QString::fromUtf8("___________")
            << QString::fromUtf8("___________");

    QTest::newRow("duplicated-formatters")
            << QString::fromUtf8("__meh__ **blah** //boo//")
            << QString::fromUtf8("__meh__ **blah** //boo//");

    QTest::newRow("two-but-different")
            << QString::fromUtf8("_/meh/_ *_blah_* /*boo*/")
            << QString::fromUtf8("<u><span class=\"markup\">_</span><i><span class=\"markup\">/</span>meh"
                                 "<span class=\"markup\">/</span></i><span class=\"markup\">_</span></u> "
                                 "<b><span class=\"markup\">*</span><u><span class=\"markup\">_</span>blah"
                                 "<span class=\"markup\">_</span></u><span class=\"markup\">*</span></b> "
                                 "<i><span class=\"markup\">/</span><b><span class=\"markup\">*</span>boo"
                                 "<span class=\"markup\">*</span></b><span class=\"markup\">/</span></i>");
}

WebRenderingTester::WebRenderingTester()
{
    m_web = new QWebView(0);
    m_loop = new QEventLoop(this);
    connect(m_web, SIGNAL(loadFinished(bool)), m_loop, SLOT(quit()));
}

WebRenderingTester::~WebRenderingTester()
{
    delete m_web;
}

QString WebRenderingTester::asPlainText(const QString &input, const UiUtils::FlowedFormat format,
                                        const CollapsingFlags collapsing)
{
    // FIXME: bad pasted thing!
    static const QString stylesheet = QString::fromUtf8(
        "pre{word-wrap: break-word; white-space: pre-wrap;}"
        ".quotemarks{color:transparent;font-size:0px;}"
        "blockquote{font-size:90%; margin: 4pt 0 4pt 0; padding: 0 0 0 1em; border-left: 2px solid blue;}"
        "blockquote blockquote blockquote {font-size: 100%}"
        ".signature{opacity: 0.6;}"
        "input {display: none}"
        "input ~ span.full {display: block}"
        "input ~ span.short {display: none}"
        "input:checked ~ span.full {display: none}"
        "input:checked ~ span.short {display: block}"
        "label {border: 1px solid #333333; border-radius: 5px; padding: 0px 4px 0px 4px; margin-left: 8px; white-space: nowrap}"
        "span.full > blockquote > label:before {content: \"\u25b4\"}"
        "span.short > blockquote > label:after {content: \" \u25be\"}"
        "span.shortquote > blockquote > label {display: none}"
    );
    static const QString htmlHeader("<html><head><style type=\"text/css\"><!--" + stylesheet + "--></style></head><body><pre>");
    static const QString htmlFooter("\n</pre></body></html>");

    sourceData = htmlHeader + UiUtils::plainTextToHtml(input, format) + htmlFooter;
    if (collapsing == RenderExpandEverythingCollapsed)
        sourceData = sourceData.replace(" checked=\"checked\"", QString());
    QTimer::singleShot(0, this, SLOT(doDelayedLoad()));
    m_loop->exec();
    m_web->page()->action(QWebPage::SelectAll)->trigger();
    return m_web->page()->selectedText();
}

void WebRenderingTester::doDelayedLoad()
{
    m_web->page()->mainFrame()->setHtml(sourceData);
}

// ...because QCOMPARE uses a fixed buffer for 1024 bytes for the debug printing...
#define LONG_STR_QCOMPARE(WHAT, EXPECTED) \
{ \
    if (EXPECTED.size() < 350) { \
        QCOMPARE(WHAT, EXPECTED); \
    } else { \
        QString actual = WHAT; \
        if (actual != EXPECTED) {\
            qDebug() << actual; \
            qDebug() << EXPECTED; \
            qDebug() << #WHAT; \
        }; \
        QVERIFY(actual == EXPECTED); \
    } \
}

void HtmlFormattingTest::testPlainTextFormattingViaPaste()
{
    QFETCH(QString, source);
    QFETCH(QString, formattedFlowed);
    QFETCH(QString, formattedPlain);
    QFETCH(QString, expandedFlowed);

    // Allow specifying "the same" by just passing null QStrings along
    if (formattedPlain.isEmpty())
        formattedPlain = formattedFlowed;
    if (expandedFlowed.isEmpty())
        expandedFlowed = formattedFlowed;

#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
    {
        WebRenderingTester tester;
        LONG_STR_QCOMPARE(visualizeWhitespace(tester.asPlainText(source, UiUtils::FlowedFormat::FLOWED)),
                          visualizeWhitespace(formattedFlowed));
    }

    {
        WebRenderingTester tester;
        LONG_STR_QCOMPARE(visualizeWhitespace(tester.asPlainText(source, UiUtils::FlowedFormat::PLAIN)),
                          visualizeWhitespace(formattedPlain));
    }
#endif

    {
        WebRenderingTester tester;
        LONG_STR_QCOMPARE(visualizeWhitespace(tester.asPlainText(source, UiUtils::FlowedFormat::FLOWED, WebRenderingTester::RenderExpandEverythingCollapsed)),
                 visualizeWhitespace(expandedFlowed));
    }
}

void HtmlFormattingTest::testPlainTextFormattingViaPaste_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("formattedFlowed");
    QTest::addColumn<QString>("formattedPlain");
    QTest::addColumn<QString>("expandedFlowed");

    QTest::newRow("no-quotes")
            << QString("Sample mail message.\n")
            << QString("Sample mail message.\n")
            << QString() << QString();

    QTest::newRow("no-quotes-flowed")
            << QString("This is something which is split \namong a few lines \n  like \n   this. ")
            << QString("This is something which is split among a few lines  like   this.")
            << QString("This is something which is split \namong a few lines \n  like \n   this. ")
            << QString();

    QTest::newRow("quote-1")
            << QString("Foo bar.\n> blesmrt\n>>trojita\nomacka")
            << QString("Foo bar.\n> blesmrt\n>> trojita\nomacka")
            << QString() << QString();

    QTest::newRow("quote-levels")
            << QString("Zero.\n>One\n>> Two\n>>>> Four-0\n>>>> Four-1\n>>>> Four-2\n>>>> Four-3\n>>>Three\nZeroB")
            << QString("Zero.\n> One\n>> Two ...\nZeroB")
            << QString()
            << QString("Zero.\n> One\n>> Two\n>>>> Four-0\n>>>> Four-1\n>>>> Four-2\n>>>> Four-3\n>>> Three\nZeroB");

    QTest::newRow("quoted-no-spacing")
            << QString("> foo\nbar\n> baz")
            << QString("> foo\nbar\n> baz")
            << QString() << QString();

    QTest::newRow("bottom-quoting")
            << QString::fromUtf8("Foo bar.\n> blesmrt\n>> 333")
            << QString::fromUtf8("Foo bar.\n> blesmrt\n>> 333")
            << QString()
            << QString();

    QTest::newRow("bottom-quoting-toobig")
            << QString::fromUtf8("Foo bar.\n> blesmrt\n>> 333\n>> 666\n>> 666-2\n>> 666-3\n>> 666-4")
            << QString::fromUtf8("Foo bar.\n> blesmrt ...\n")
            << QString()
            << QString::fromUtf8("Foo bar.\n> blesmrt\n>> 333\n>> 666\n>> 666-2\n>> 666-3\n>> 666-4\n");

    QTest::newRow("different-quote-levels-not-flowed-together")
            << QString::fromUtf8("Foo bar. \n> blesmrt \n>> 333")
            << QString::fromUtf8("Foo bar.\n> blesmrt\n>> 333")
            << QString::fromUtf8("Foo bar. \n> blesmrt \n>> 333")
            << QString::fromUtf8("Foo bar.\n> blesmrt\n>> 333");

    QTest::newRow("different-quote-levels-not-flowed-together-toobig")
            << QString::fromUtf8("Foo bar. \n> blesmrt \n>> 333\n>> 666\n>> 666-2\n>> 666-3\n>> 666-4")
            // The space right in front of "..." is a separator for copy-paste. The original space from the input
            // is interpretted as a line mistakenly marked as flowed, but since the line of a paragraph cannot be
            // flowed, it's a bug which we detect and throw it away.
            << QString::fromUtf8("Foo bar.\n> blesmrt ...\n")
            // On the other hand, we *do* expect two spaces here -- one for the non-flowed space,
            // and the other one for our separator.
            << QString::fromUtf8("Foo bar. \n> blesmrt  ...\n")
            << QString::fromUtf8("Foo bar.\n> blesmrt\n>> 333\n>> 666\n>> 666-2\n>> 666-3\n>> 666-4\n");

    QTest::newRow("nested-quotes-correct-indicator")
            << QString::fromUtf8(">>> Three levels down.\n>> Two levels down.\n> One level down.\nReal mail.")
            << QString::fromUtf8(">>> ...\n>> Two levels down. ...\n> One level down.\nReal mail.")
            << QString()
            << QString::fromUtf8(">>> Three levels down.\n>> Two levels down.\n> One level down.\nReal mail.");

    QString lipsum = QString::fromUtf8("Lorem ipsum dolor sit amet, consectetur adipisici elit, sed eiusmod tempor incidunt ut "
                                       "labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
                                       "laboris nisi ut aliquid ex ea commodi consequat. Quis aute iure reprehenderit in "
                                       "voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint obcaecat "
                                       "cupiditat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");
    QString shortLipsum = (lipsum + QLatin1Char(' ') + lipsum).left(5*160);

    QTest::newRow("collapsed-last-quote")
            << QString::fromUtf8("Some real text.\n> ") + lipsum + QLatin1Char(' ') + lipsum
            << QString::fromUtf8("Some real text.\n> ") + shortLipsum + " ...\n"
            << QString()
            << QString::fromUtf8("Some real text.\n> ") + lipsum + QLatin1Char(' ') + lipsum + "\n";

    QTest::newRow("quoted-common")
            << QString::fromUtf8("On quinta-feira, 4 de outubro de 2012 15.46.57, André Somers wrote:\n"
                                 "> If you think that running 21 threads on an 8 core system will run make \n"
                                 "> your task go faster, then Thiago is right: you don't understand your \n"
                                 "> problem.\n"
                                 "If you run 8 threads on an 8-core system and they use the CPU fully, then \n"
                                 "you're running as fast as you can.\n"
                                 "\n"
                                 "If you have more threads than the number of processors and if all threads are \n"
                                 "ready to be executed, then the OS will schedule timeslices to each thread. \n"
                                 "That means threads get executed and suspended all the time, sometimes \n"
                                 "migrating between processors. That adds overhead.\n"
                                 // yes, some parts have been removed here.
                                 "\n"
                                 "-- \n"
                                 "Thiago's name goes here.\n")
            << QString::fromUtf8("On quinta-feira, 4 de outubro de 2012 15.46.57, André Somers wrote:\n"
                                 "> If you think that running 21 threads on an 8 core system will run make "
                                 "your task go faster, then Thiago is right: you don't understand your "
                                 "problem.\n"
                                 "If you run 8 threads on an 8-core system and they use the CPU fully, then "
                                 "you're running as fast as you can.\n"
                                 "\n"
                                 "If you have more threads than the number of processors and if all threads are "
                                 "ready to be executed, then the OS will schedule timeslices to each thread. "
                                 "That means threads get executed and suspended all the time, sometimes "
                                 "migrating between processors. That adds overhead.\n"
                                 "\n"
                                 "-- \n"
                                 "Thiago's name goes here.\n")
            << QString::fromUtf8("On quinta-feira, 4 de outubro de 2012 15.46.57, André Somers wrote:\n"
                                 "> If you think that running 21 threads on an 8 core system will run make \n"
                                 "> your task go faster, then Thiago is right: you don't understand your \n"
                                 "> problem.\n"
                                 "If you run 8 threads on an 8-core system and they use the CPU fully, then \n"
                                 "you're running as fast as you can.\n"
                                 "\n"
                                 "If you have more threads than the number of processors and if all threads are \n"
                                 "ready to be executed, then the OS will schedule timeslices to each thread. \n"
                                 "That means threads get executed and suspended all the time, sometimes \n"
                                 "migrating between processors. That adds overhead.\n"
                                 "\n"
                                 "-- \n"
                                 "Thiago's name goes here.\n")
            << QString();

    QTest::newRow("small-quotes-arent-collapsible")
            << QString::fromUtf8("On Wednesday 09 January 2013 08:56:25 Jekyll Wu wrote:\n"
                                 "> If you plan to \n"
                                 "> use bugs.kde.org as the tracker, then you don't need to call \n"
                                 "> setBugAddress() at all. The default value just works.\n"
                                 "\n"
                                 "Fixed.\n"
                                 "\n"
                                 "\n"
                                 "> And don't forget to ask sysadmins to create a \"mangonel\" product on \n"
                                 "> bugs.kde.org :)\n"
                                 "\n"
                                 "Done.\n"
                                 "\n"
                                 "Thanks for the review! :D\n"
                                 "\n"
                                 "-- \n"
                                 "Martin Sandsmark\n"
                                 "KDE\n")
            << QString::fromUtf8("On Wednesday 09 January 2013 08:56:25 Jekyll Wu wrote:\n"
                                 "> If you plan to use bugs.kde.org as the tracker, then you don't need to call setBugAddress() "
                                 "at all. The default value just works.\n"
                                 "\n"
                                 "Fixed.\n"
                                 "\n"
                                 "\n"
                                 "> And don't forget to ask sysadmins to create a \"mangonel\" product on bugs.kde.org :)\n"
                                 "\n"
                                 "Done.\n"
                                 "\n"
                                 "Thanks for the review! :D\n"
                                 "\n"
                                 "-- \n"
                                 "Martin Sandsmark\n"
                                 "KDE\n")
            << QString::fromUtf8("On Wednesday 09 January 2013 08:56:25 Jekyll Wu wrote:\n"
                                 "> If you plan to \n"
                                 "> use bugs.kde.org as the tracker, then you don't need to call \n"
                                 "> setBugAddress() at all. The default value just works.\n"
                                 "\n"
                                 "Fixed.\n"
                                 "\n"
                                 "\n"
                                 "> And don't forget to ask sysadmins to create a \"mangonel\" product on \n"
                                 "> bugs.kde.org :)\n"
                                 "\n"
                                 "Done.\n"
                                 "\n"
                                 "Thanks for the review! :D\n"
                                 "\n"
                                 "-- \n"
                                 "Martin Sandsmark\n"
                                 "KDE\n")
            << QString();

    // https://bugs.kde.org/show_bug.cgi?id=337919
    QTest::newRow("blank-line-separator")
            << QString("First para. \n\nSecond para.\n\n\nThird para.")
            << QString("First para. \n\nSecond para.\n\n\nThird para.")
            << QString("First para. \n\nSecond para.\n\n\nThird para.")
            << QString("First para. \n\nSecond para.\n\n\nThird para.");

    QTest::newRow("blank-line-separator-of-quoted")
            << QString("> First para. \n>\n> Second para.\n>\n>\n>Third para.")
            << QString("> First para. \n> \n> Second para.\n> \n> \n> Third para. ...\n")
            << QString("> First para. \n> \n> Second para.\n> \n> \n> Third para. ...\n")
            << QString("> First para. \n> \n> Second para.\n> \n> \n> Third para.\n");

    QTest::newRow("blanks-in-different-levels")
            << QString("Yesterday: \n>> Test2A. \n>>\n>> Test2B \n>>\nTest0. \n> Test1A. \n> \n>Test 1B. ")
            << QString("Yesterday:\n>> Test2A. \n>> \n>> Test2B \n>> \nTest0.\n> Test1A. \n> \n> Test 1B.")
            << QString("Yesterday: \n>> Test2A. \n>> \n>> Test2B \n>> \nTest0. \n> Test1A. \n> \n> Test 1B. ")
            << QString("Yesterday:\n>> Test2A. \n>> \n>> Test2B \n>> \nTest0.\n> Test1A. \n> \n> Test 1B.");

    QTest::newRow("kmail-spaces-in-quotes")
            << QString("On Wednesday 12 of November 2014 06:54:53 John Layt wrote:\r\n"
                       "> On 8 November 2014 08:45, John Layt <jlayt@kde.org> wrote:\r\n"
                       "> > Hi,\r\n"
                       "> > \r\n"
                       "> > Just checking, has the sponsorship budget been approved (so I can book\r\n"
                       "> > flights before they cost silly amounts), and has accommodation been\r\n"
                       "> > sorted yet or do we need to do it ourselves?\r\n"
                       "> \r\n"
                       "> Ping?\r\n"
                       "\r\n"
                       "CC'd Michael directly in case he missed the emails between all the jenkins \r\n"
                       "spam. I'd like to have this sorted out soon too  :)\r\n")
            << QString("On Wednesday 12 of November 2014 06:54:53 John Layt wrote:\n"
                       "> On 8 November 2014 08:45, John Layt <jlayt@kde.org> wrote:\n"
                       "> > Hi,\n"
                       // This might be surprising at first, but "> > > " is actually correct here.
                       // The first ">" is the quote level, second ">" is the second ">" from the first line,
                       // and because that line ends with a space, it's a flowed line, and therefore the next
                       // line should be joined in there. The next line has a quote depth of one (the matching
                       // stops at first space), and then again actually starts with another ">".
                       // Therefore ">" for a quote prefix, " " as a spearator, and "> > Just..." as actual content.
                       "> > > Just checking, has the sponsorship budget been approved (so I can book\n"
                       "> > flights before they cost silly amounts), and has accommodation been\n"
                       "> > sorted yet or do we need to do it ourselves?\n"
                       "> \n"
                       "> Ping?\n"
                       "\n"
                       "CC'd Michael directly in case he missed the emails between all the jenkins spam. I'd like to have this sorted out soon too  :)\n")
            << QString("On Wednesday 12 of November 2014 06:54:53 John Layt wrote:\n"
                       "> On 8 November 2014 08:45, John Layt <jlayt@kde.org> wrote:\n"
                       ">> Hi,\n"
                       ">> \n"
                       ">> Just checking, has the sponsorship budget been approved (so I can book\n"
                       ">> flights before they cost silly amounts), and has accommodation been\n"
                       ">> sorted yet or do we need to do it ourselves?\n"
                       "> \n"
                       "> Ping?\n"
                       "\n"
                       "CC'd Michael directly in case he missed the emails between all the jenkins \n"
                       "spam. I'd like to have this sorted out soon too  :)\n")
            << QString("On Wednesday 12 of November 2014 06:54:53 John Layt wrote:\n"
                       "> On 8 November 2014 08:45, John Layt <jlayt@kde.org> wrote:\n"
                       "> > Hi,\n"
                       // See above for that "weird" "> > > ".
                       "> > > Just checking, has the sponsorship budget been approved (so I can book\n"
                       "> > flights before they cost silly amounts), and has accommodation been\n"
                       "> > sorted yet or do we need to do it ourselves?\n"
                       "> \n"
                       "> Ping?\n"
                       "\n"
                       "CC'd Michael directly in case he missed the emails between all the jenkins spam. I'd like to have this sorted out soon too  :)\n");
}

void HtmlFormattingTest::testPlainTextFormattingViaPasteDelSp()
{
    QFETCH(QString, source);
    QFETCH(QString, expandedFlowed);
    QFETCH(QString, expandedFlowedDelSp);

    {
        WebRenderingTester tester;
        LONG_STR_QCOMPARE(visualizeWhitespace(tester.asPlainText(source, UiUtils::FlowedFormat::FLOWED, WebRenderingTester::RenderExpandEverythingCollapsed)),
                 visualizeWhitespace(expandedFlowed));
    }
    {
        WebRenderingTester tester;
        LONG_STR_QCOMPARE(visualizeWhitespace(tester.asPlainText(source, UiUtils::FlowedFormat::FLOWED_DELSP, WebRenderingTester::RenderExpandEverythingCollapsed)),
                 visualizeWhitespace(expandedFlowedDelSp));
    }
}


void HtmlFormattingTest::testPlainTextFormattingViaPasteDelSp_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("expandedFlowed");
    QTest::addColumn<QString>("expandedFlowedDelSp");

    QTest::newRow("no-quotes")
            << QString("Sample mail message.\n")
            << QString("Sample mail message.\n")
            << QString("Sample mail message.\n");

    QTest::newRow("no-quotes-flowed")
            << QString("This is something which is split \namong a \n few lines \n  like \n   this. ")
            << QString("This is something which is split among a few lines  like   this.")
            // yes, "afew" -- that's because of space stuffing.
            << QString("This is something which is splitamong afew lines like  this.");

    QTest::newRow("quote-1")
            << QString("Foo bar. \n> blesmrt\n>> trojita \n omacka")
            << QString("Foo bar.\n> blesmrt\n>> trojita\nomacka")
            << QString("Foo bar.\n> blesmrt\n>> trojita\nomacka");

    // https://bugs.kde.org/show_bug.cgi?id=337919
    QTest::newRow("blank-line-separator")
            << QString("First para. \n\nSecond para.\n\n\nThird para.")
            << QString("First para. \n\nSecond para.\n\n\nThird para.")
            << QString("First para.\n\nSecond para.\n\n\nThird para.");

    QTest::newRow("blank-line-separator-of-quoted")
            << QString("> First para. \n>\n> Second para.\n>\n>\n>Third para.")
            << QString("> First para. \n> \n> Second para.\n> \n> \n> Third para.\n")
            << QString("> First para.\n> \n> Second para.\n> \n> \n> Third para.\n");

    QTest::newRow("blanks-in-different-levels")
            << QString("Yesterday: \n>> Test2A. \n>>\n>> Test2B \n>>\nTest0. \n> Test1A. \n> \n>Test 1B. ")
            << QString("Yesterday:\n>> Test2A. \n>> \n>> Test2B \n>> \nTest0.\n> Test1A. \n> \n> Test 1B.")
            << QString("Yesterday:\n>> Test2A.\n>> \n>> Test2B\n>> \nTest0.\n> Test1A.\n> \n> Test 1B.");
}

/** @short Test that the link recognition in plaintext -> HTML formatting recognizes the interesting links */
void HtmlFormattingTest::testLinkRecognition()
{
    QFETCH(QString, prefix);
    QFETCH(QString, link);
    QFETCH(QString, suffix);

    QString input = prefix + link + suffix;
    QString expected = prefix + QString::fromUtf8("<a href=\"%1\">%1</a>").arg(link) + suffix;

    QCOMPARE(UiUtils::plainTextToHtml(input, UiUtils::FlowedFormat::PLAIN), expected);
}

/** @short Test data for testLinkRecognition */
void HtmlFormattingTest::testLinkRecognition_data()
{
    QTest::addColumn<QString>("prefix");
    QTest::addColumn<QString>("link");
    QTest::addColumn<QString>("suffix");

    QString empty;
    QString space(QLatin1String(" "));

    QTest::newRow("basic-http") << empty << QString::fromUtf8("http://blesmrt") << empty;
    QTest::newRow("basic-https") << empty << QString::fromUtf8("https://blesmrt") << empty;
    QTest::newRow("parentheses") << QString::fromUtf8("(") << QString::fromUtf8("https://blesmrt") << QString::fromUtf8(")");
    QTest::newRow("url-query") << empty << QString::fromUtf8("https://blesmrt.trojita/?foo=bar") << empty;
    QTest::newRow("url-fragment") << empty << QString::fromUtf8("https://blesmrt.trojita/#pwn") << empty;

    QTest::newRow("trailing-dot") << empty << QString::fromUtf8("http://blesmrt") << QString::fromUtf8(".");
    QTest::newRow("trailing-dot-2") << empty << QString::fromUtf8("http://blesmrt") << QString::fromUtf8(". Foo");
    QTest::newRow("trailing-comma") << empty << QString::fromUtf8("http://blesmrt") << QString::fromUtf8(",");
    QTest::newRow("trailing-comma-2") << empty << QString::fromUtf8("http://blesmrt") << QString::fromUtf8(", foo");
    QTest::newRow("trailing-semicolon") << empty << QString::fromUtf8("http://blesmrt") << QString::fromUtf8(";");
    QTest::newRow("trailing-semicolon-2") << empty << QString::fromUtf8("http://blesmrt") << QString::fromUtf8("; foo");
    QTest::newRow("trailing-colon") << empty << QString::fromUtf8("http://blesmrt") << QString::fromUtf8(":");
    QTest::newRow("trailing-colon-2") << empty << QString::fromUtf8("http://blesmrt") << QString::fromUtf8(": blah");

    QTest::newRow("trailing-sentence-1") << QString::fromUtf8("meh ") << QString::fromUtf8("http://blesmrt") << QString::fromUtf8("?");
    QTest::newRow("trailing-sentence-2") << QString::fromUtf8("meh ") << QString::fromUtf8("http://blesmrt") << QString::fromUtf8("!");
    QTest::newRow("trailing-sentence-3") << QString::fromUtf8("meh ") << QString::fromUtf8("http://blesmrt") << QString::fromUtf8(".");
}

/** @short Test data which should not be recognized as links */
void HtmlFormattingTest::testUnrecognizedLinks()
{
    QFETCH(QString, input);

    QCOMPARE(UiUtils::plainTextToHtml(input, UiUtils::FlowedFormat::PLAIN), input);
}

/** @short Test data for testUnrecognizedLinks */
void HtmlFormattingTest::testUnrecognizedLinks_data()
{
    QTest::addColumn<QString>("input");

    QTest::newRow("basic-ftp") << QString::fromUtf8("ftp://blesmrt");
    QTest::newRow("at-sign-start") << QString::fromUtf8("@foo");
    QTest::newRow("at-sign-end") << QString::fromUtf8("foo@");
    QTest::newRow("at-sign-standalone-1") << QString::fromUtf8("@");
    QTest::newRow("at-sign-standalone-2") << QString::fromUtf8(" @ ");
    QTest::newRow("http-standalone") << QString::fromUtf8("http://");
    QTest::newRow("http-standalone-stuff") << QString::fromUtf8("http:// foo");
}

void HtmlFormattingTest::testSignatures()
{
    QFETCH(QString, original);
    QFETCH(QString, signature);
    QFETCH(QString, result);

    QTextDocument doc;
    doc.setPlainText(original);
    Composer::Util::replaceSignature(&doc, signature);
    QCOMPARE(doc.toPlainText(), result);
}

void HtmlFormattingTest::testSignatures_data()
{
    QTest::addColumn<QString>("original");
    QTest::addColumn<QString>("signature");
    QTest::addColumn<QString>("result");

    QTest::newRow("empty-all") << QString() << QString() << QString();
    QTest::newRow("empty-signature-1") << QString("foo") << QString() << QString("foo");
    QTest::newRow("empty-signature-2") << QString("foo\n") << QString() << QString("foo\n");
    QTest::newRow("empty-signature-3") << QString("foo\n-- ") << QString() << QString("foo");
    QTest::newRow("empty-signature-4") << QString("foo\n-- \n") << QString() << QString("foo");
    QTest::newRow("empty-signature-5") << QString("foo\n\n-- \n") << QString() << QString("foo\n");

    QTest::newRow("no-signature-1") << QString("foo") << QString("meh") << QString("foo\n-- \nmeh");
    QTest::newRow("no-signature-2") << QString("foo\n") << QString("meh") << QString("foo\n\n-- \nmeh");
    QTest::newRow("no-signature-3") << QString("foo\n\n") << QString("meh") << QString("foo\n\n\n-- \nmeh");
    QTest::newRow("no-signature-4") << QString("foo\nbar\nbaz") << QString("meh") << QString("foo\nbar\nbaz\n-- \nmeh");
    QTest::newRow("no-signature-5") << QString("foo\n--") << QString("meh") << QString("foo\n--\n-- \nmeh");

    QTest::newRow("replacement") << QString("foo\n-- \njohoho") << QString("sig") << QString("foo\n-- \nsig");
    QTest::newRow("replacement-of-multiline") << QString("foo\n-- \njohoho\nwtf\nbar") << QString("sig") << QString("foo\n-- \nsig");
}

QTEST_MAIN(HtmlFormattingTest)
