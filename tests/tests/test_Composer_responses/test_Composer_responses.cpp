/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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
#include <QTextDocument>
#include "test_Composer_responses.h"
#include "../headless_test.h"
#include "Composer/PlainTextFormatter.h"
#include "Composer/Recipients.h"
#include "Composer/ReplaceSignature.h"
#include "Composer/SubjectMangling.h"

Q_DECLARE_METATYPE(Composer::RecipientList)
Q_DECLARE_METATYPE(QList<QUrl>)

QString recipientKindtoString(const Composer::RecipientKind kind)
{
    switch (kind) {
    case Composer::ADDRESS_BCC:
        return QLatin1String("Bcc:");
    case Composer::ADDRESS_CC:
        return QLatin1String("Cc:");
    case Composer::ADDRESS_FROM:
        return QLatin1String("From:");
    case Composer::ADDRESS_REPLY_TO:
        return QLatin1String("Reply-To:");
    case Composer::ADDRESS_SENDER:
        return QLatin1String("Sender:");
    case Composer::ADDRESS_TO:
        return QLatin1String("To:");
    }
    Q_ASSERT(false);
    return QString();
}

namespace QTest {

    template <>
    char *toString(const Composer::RecipientList &list)
    {
        QString buf;
        QDebug d(&buf);
        Q_FOREACH(const Composer::RecipientList::value_type &item, list) {
            d << recipientKindtoString(item.first).toUtf8().constData() << item.second.asSMTPMailbox().constData() << ",";
        }
        return qstrdup(buf.toUtf8().constData());
    }
}

/** @short Test that subjects remain sane in replied/forwarded messages */
void ComposerResponsesTest::testSubjectMangling()
{
    QFETCH(QString, original);
    QFETCH(QString, replied);

    QCOMPARE(Composer::Util::replySubject(original), replied);
}

/** @short Data for testSubjectMangling */
void ComposerResponsesTest::testSubjectMangling_data()
{
    QTest::addColumn<QString>("original");
    QTest::addColumn<QString>("replied");

    QTest::newRow("no-subject") << QString() << QString::fromUtf8("Re: ");
    QTest::newRow("simple") << QString::fromUtf8("ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("already-replied") << QString::fromUtf8("Re: ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("already-replied-no-space") << QString::fromUtf8("re:ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("already-replied-case") << QString::fromUtf8("RE: ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("multiple-re") << QString::fromUtf8("Re:RE: re: ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("trailing-re") << QString::fromUtf8("ahoj re:") << QString::fromUtf8("Re: ahoj re:");
    QTest::newRow("leading-trailing-re") << QString::fromUtf8("re: ahoj re:") << QString::fromUtf8("Re: ahoj re:");

    // mailing list stuff
    QTest::newRow("ml-empty") << QString::fromUtf8("[foo]") << QString::fromUtf8("[foo] Re: ");
    QTest::newRow("ml-simple") << QString::fromUtf8("[foo] bar") << QString::fromUtf8("[foo] Re: bar");
    QTest::newRow("ml-simple-no-space") << QString::fromUtf8("[foo]bar") << QString::fromUtf8("[foo] Re: bar");
    QTest::newRow("ml-broken") << QString::fromUtf8("[foo bar") << QString::fromUtf8("Re: [foo bar");
    QTest::newRow("ml-two-words") << QString::fromUtf8("[foo bar] x") << QString::fromUtf8("[foo bar] Re: x");
    QTest::newRow("ml-re-empty") << QString::fromUtf8("[foo] Re:") << QString::fromUtf8("[foo] Re: ");
    QTest::newRow("re-ml-re-empty") << QString::fromUtf8("Re: [foo] Re:") << QString::fromUtf8("[foo] Re: ");
    QTest::newRow("re-ml-re-empty-no-spaces") << QString::fromUtf8("Re:[foo]Re:") << QString::fromUtf8("[foo] Re: ");
    QTest::newRow("ml-ml") << QString::fromUtf8("[foo] [bar] blah") << QString::fromUtf8("[foo] [bar] Re: blah");
    QTest::newRow("ml-ml-re") << QString::fromUtf8("[foo] [bar] Re: blah") << QString::fromUtf8("[foo] [bar] Re: blah");
    QTest::newRow("ml-re-ml") << QString::fromUtf8("[foo] Re: [bar] blah") << QString::fromUtf8("[foo] [bar] Re: blah");
    QTest::newRow("ml-re-ml-re") << QString::fromUtf8("[foo] Re: [bar] Re: blah") << QString::fromUtf8("[foo] [bar] Re: blah");
    QTest::newRow("re-ml-ml") << QString::fromUtf8("Re: [foo] [bar] blah") << QString::fromUtf8("[foo] [bar] Re: blah");
    QTest::newRow("re-ml-ml-re") << QString::fromUtf8("Re: [foo] [bar] Re: blah") << QString::fromUtf8("[foo] [bar] Re: blah");
    QTest::newRow("re-ml-re-ml") << QString::fromUtf8("Re: [foo] Re: [bar] blah") << QString::fromUtf8("[foo] [bar] Re: blah");
    QTest::newRow("re-ml-re-ml-re") << QString::fromUtf8("Re: [foo] Re: [bar] Re: blah") << QString::fromUtf8("[foo] [bar] Re: blah");

    // test removing duplicate items
    QTest::newRow("M-M") << QString::fromUtf8("[foo] [foo] blah") << QString::fromUtf8("[foo] Re: blah");
    QTest::newRow("M-M-re") << QString::fromUtf8("[foo] [foo] Re: blah") << QString::fromUtf8("[foo] Re: blah");
    QTest::newRow("M-M-re-re") << QString::fromUtf8("[foo] [foo] Re: Re: blah") << QString::fromUtf8("[foo] Re: blah");
    QTest::newRow("M-re-M") << QString::fromUtf8("[foo] Re: [foo] blah") << QString::fromUtf8("[foo] Re: blah");
    QTest::newRow("re-M-re-M") << QString::fromUtf8("Re: [foo] Re: [foo] blah") << QString::fromUtf8("[foo] Re: blah");
    QTest::newRow("re-M-re-M-re") << QString::fromUtf8("Re: [foo] Re: [foo] Re: blah") << QString::fromUtf8("[foo] Re: blah");

    // stuff which should not be subject to subject sanitization
    QTest::newRow("brackets-end") << QString::fromUtf8("blesmrt [test]") << QString::fromUtf8("Re: blesmrt [test]");
    QTest::newRow("re-brackets-end") << QString::fromUtf8("Re: blesmrt [test]") << QString::fromUtf8("Re: blesmrt [test]");
    QTest::newRow("re-brackets-re-end") << QString::fromUtf8("Re: blesmrt Re: [test]") << QString::fromUtf8("Re: blesmrt Re: [test]");
    QTest::newRow("brackets-re-end") << QString::fromUtf8("blesmrt Re: [test]") << QString::fromUtf8("Re: blesmrt Re: [test]");
}

/** @short Test that conversion of plaintext mail to HTML works reasonably well */
void ComposerResponsesTest::testPlainTextFormatting()
{
    QFETCH(QString, plaintext);
    QFETCH(QString, htmlFlowed);
    QFETCH(QString, htmlNotFlowed);

    QCOMPARE(Composer::Util::plainTextToHtml(plaintext, Composer::Util::FORMAT_FLOWED).join(QLatin1String("\n")), htmlFlowed);
    QCOMPARE(Composer::Util::plainTextToHtml(plaintext, Composer::Util::FORMAT_PLAIN).join(QLatin1String("\n")), htmlNotFlowed);
}

/** @short Data for testPlainTextFormatting */
void ComposerResponsesTest::testPlainTextFormatting_data()
{
    QTest::addColumn<QString>("plaintext");
    QTest::addColumn<QString>("htmlFlowed");
    QTest::addColumn<QString>("htmlNotFlowed");

    QTest::newRow("empty-1") << QString() << QString() << QString();
    QTest::newRow("empty-2") << QString("") << QString("") << QString("");
    QTest::newRow("empty-3") << QString("\n") << QString("\n") << QString("\n");
    QTest::newRow("empty-4") << QString("\n\n") << QString("\n\n") << QString("\n\n");

    QTest::newRow("minimal") << QString("ahoj") << QString("ahoj") << QString("ahoj");
    QTest::newRow("containing-html")
            << QString("<p>ahoj &amp; blesmrt</p>")
            << QString("&lt;p&gt;ahoj &amp;amp; blesmrt&lt;/p&gt;")
            << QString("&lt;p&gt;ahoj &amp;amp; blesmrt&lt;/p&gt;");
    QTest::newRow("basic-formatting")
            << QString("foo *bar* _baz_ /pwn/ yay foo@ @bar @ blesmrt")
            << QString("foo <b><span class=\"markup\">*</span>bar<span class=\"markup\">*</span></b> "
                       "<u><span class=\"markup\">_</span>baz<span class=\"markup\">_</span></u> "
                       "<i><span class=\"markup\">/</span>pwn<span class=\"markup\">/</span></i> yay foo@ @bar @ blesmrt")
            << QString("foo <b><span class=\"markup\">*</span>bar<span class=\"markup\">*</span></b> "
                       "<u><span class=\"markup\">_</span>baz<span class=\"markup\">_</span></u> "
                       "<i><span class=\"markup\">/</span>pwn<span class=\"markup\">/</span></i> yay foo@ @bar @ blesmrt");
    QTest::newRow("formatting-and-newlines")
            << QString("*blesmrt*\ntrojita")
            << QString("<b><span class=\"markup\">*</span>blesmrt<span class=\"markup\">*</span></b>\ntrojita")
            << QString("<b><span class=\"markup\">*</span>blesmrt<span class=\"markup\">*</span></b>\ntrojita");
    QTest::newRow("links")
            << QString("ahoj http://pwn:123/foo?bar&baz#nope")
            << QString("ahoj <a href=\"http://pwn:123/foo?bar&amp;baz#nope\">http://pwn:123/foo?bar&amp;baz#nope</a>")
            << QString("ahoj <a href=\"http://pwn:123/foo?bar&amp;baz#nope\">http://pwn:123/foo?bar&amp;baz#nope</a>");
    // Test our escaping
    QTest::newRow("escaping-1")
            << QString::fromUtf8("&gt; § §gt; §para;\n")
            << QString::fromUtf8("&amp;gt; § §gt; §para;\n")
            << QString::fromUtf8("&amp;gt; § §gt; §para;\n");

    QTest::newRow("mailto-1")
            << QString("ble.smrt-1_2+3@example.org")
            << QString("<a href=\"mailto:ble.smrt-1_2+3@example.org\">ble.smrt-1_2+3@example.org</a>")
            << QString("<a href=\"mailto:ble.smrt-1_2+3@example.org\">ble.smrt-1_2+3@example.org</a>");

    // Test how the quoted bits are represented
    QTest::newRow("quoted-1")
            << QString::fromUtf8("Foo bar.\n"
                                 "> blesmrt\n"
                                 ">>trojita\n"
                                 "omacka")
            << QString::fromUtf8("Foo bar.\n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>blesmrt\n"
                                 "<blockquote><span class=\"quotemarks\">&gt;&gt; </span>trojita"
                                 "</blockquote></blockquote>\n"
                                 "omacka")
            << QString::fromUtf8("Foo bar.\n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>blesmrt\n"
                                 "<blockquote><span class=\"quotemarks\">&gt;&gt; </span>trojita"
                                 "</blockquote></blockquote>\n"
                                 "omacka");

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
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>If you think that running 21 threads "
                                 "on an 8 core system will run make your task go faster, then Thiago is right: you don't understand your problem.</blockquote>\n"
                                 "If you run 8 threads on an 8-core system and they use the CPU fully, then you're running as fast as you can.\n"
                                 "\n"
                                 "If you have more threads than the number of processors and if all threads are ready "
                                 "to be executed, then the OS will schedule timeslices to each thread. That means "
                                 "threads get executed and suspended all the time, sometimes migrating between "
                                 "processors. That adds overhead.\n"
                                 "\n"
                                 "<span class=\"signature\">-- \n"
                                 "Thiago's name goes here.\n</span>")
            << QString::fromUtf8("On quinta-feira, 4 de outubro de 2012 15.46.57, André Somers wrote:\n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>If you think that running 21 threads on an 8 core system will run make \n"
                                 "your task go faster, then Thiago is right: you don't understand your \n"
                                 "problem.</blockquote>\n"
                                 "If you run 8 threads on an 8-core system and they use the CPU fully, then \n"
                                 "you're running as fast as you can.\n"
                                 "\n"
                                 "If you have more threads than the number of processors and if all threads are \n"
                                 "ready to be executed, then the OS will schedule timeslices to each thread. \n"
                                 "That means threads get executed and suspended all the time, sometimes \n"
                                 "migrating between processors. That adds overhead.\n"
                                 "\n"
                                 "<span class=\"signature\">-- \n"
                                 "Thiago's name goes here.\n</span>");

    QTest::newRow("quoted-no-spacing")
            << QString::fromUtf8("> foo\nbar\n> baz")
            << QString::fromUtf8("<blockquote><span class=\"quotemarks\">&gt; </span>foo</blockquote>\n"
                                 "bar\n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>baz</blockquote>")
            << QString::fromUtf8("<blockquote><span class=\"quotemarks\">&gt; </span>foo</blockquote>\n"
                                 "bar\n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>baz</blockquote>");

    QTest::newRow("bottom-quoting")
            << QString::fromUtf8("Foo bar.\n"
                                 "> blesmrt\n"
                                 ">> 333")
            << QString::fromUtf8("Foo bar.\n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>blesmrt\n"
                                 "<blockquote><span class=\"quotemarks\">&gt;&gt; </span>333</blockquote></blockquote>")
            << QString::fromUtf8("Foo bar.\n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>blesmrt\n"
                                 "<blockquote><span class=\"quotemarks\">&gt;&gt; </span>333</blockquote></blockquote>");

    QTest::newRow("different-quote-levels-not-flowed-together")
            << QString::fromUtf8("Foo bar. \n"
                                 "> blesmrt \n"
                                 ">> 333")
            << QString::fromUtf8("Foo bar. \n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>blesmrt \n"
                                 "<blockquote><span class=\"quotemarks\">&gt;&gt; </span>333</blockquote></blockquote>")
            << QString::fromUtf8("Foo bar. \n"
                                 "<blockquote><span class=\"quotemarks\">&gt; </span>blesmrt \n"
                                 "<blockquote><span class=\"quotemarks\">&gt;&gt; </span>333</blockquote></blockquote>");

}

/** @short Test that the link recognition in plaintext -> HTML formatting recognizes the interesting links */
void ComposerResponsesTest::testLinkRecognition()
{
    QFETCH(QString, prefix);
    QFETCH(QString, link);
    QFETCH(QString, suffix);

    QString input = prefix + link + suffix;
    QString expected = prefix + QString::fromUtf8("<a href=\"%1\">%1</a>").arg(link) + suffix;

    QCOMPARE(Composer::Util::plainTextToHtml(input, Composer::Util::FORMAT_PLAIN).join(QLatin1String("\n")), expected);
}

/** @short Test data for testLinkRecognition */
void ComposerResponsesTest::testLinkRecognition_data()
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
void ComposerResponsesTest::testUnrecognizedLinks()
{
    QFETCH(QString, input);

    QCOMPARE(Composer::Util::plainTextToHtml(input, Composer::Util::FORMAT_PLAIN).join(QLatin1String("\n")), input);
}

/** @short Test data for testUnrecognizedLinks */
void ComposerResponsesTest::testUnrecognizedLinks_data()
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

void ComposerResponsesTest::testSignatures()
{
    QFETCH(QString, original);
    QFETCH(QString, signature);
    QFETCH(QString, result);

    QTextDocument doc;
    doc.setPlainText(original);
    Composer::Util::replaceSignature(&doc, signature);
    QCOMPARE(doc.toPlainText(), result);
}

void ComposerResponsesTest::testSignatures_data()
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

/** @short Test different means of responding ("private", "to all", "to list") */
void ComposerResponsesTest::testResponseAddresses()
{
    QFETCH(Composer::RecipientList, original);
    QFETCH(QList<QUrl>, headerListPost);
    QFETCH(bool, headerListPostNo);
    QFETCH(bool, privateOk);
    QFETCH(Composer::RecipientList, privateRecipients);
    QFETCH(bool, allOk);
    QFETCH(Composer::RecipientList, allRecipients);
    QFETCH(bool, listOk);
    QFETCH(Composer::RecipientList, listRecipients);

    Composer::RecipientList canary;
    canary << qMakePair(Composer::ADDRESS_REPLY_TO,
                        Imap::Message::MailAddress(QString(), QString(), QString(), QLatin1String("fail")));
    Composer::RecipientList res;

    res = canary;
    QCOMPARE(Composer::Util::replyRecipientList(Composer::REPLY_PRIVATE, original, headerListPost, headerListPostNo, res),
             privateOk);
    if (!privateOk)
        QVERIFY(res == canary);
    else
        QCOMPARE(res, privateRecipients);

    res = canary;
    QCOMPARE(Composer::Util::replyRecipientList(Composer::REPLY_ALL, original, headerListPost, headerListPostNo, res),
             allOk);
    if (!allOk)
        QVERIFY(res == canary);
    else
        QCOMPARE(res, allRecipients);

    res = canary;
    QCOMPARE(Composer::Util::replyRecipientList(Composer::REPLY_LIST, original, headerListPost, headerListPostNo, res),
             listOk);
    if (!listOk)
        QVERIFY(res == canary);
    else
        QCOMPARE(res, listRecipients);
}

Imap::Message::MailAddress mail(const char * addr)
{
    QStringList list = QString(QLatin1String(addr)).split(QLatin1Char('@'));
    Q_ASSERT(list.size() == 2);
    return Imap::Message::MailAddress(QString(), QString(), list[0], list[1]);
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailFrom(const char * addr)
{
    return qMakePair(Composer::ADDRESS_FROM, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailSender(const char * addr)
{
    return qMakePair(Composer::ADDRESS_SENDER, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailReplyTo(const char * addr)
{
    return qMakePair(Composer::ADDRESS_REPLY_TO, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailTo(const char * addr)
{
    return qMakePair(Composer::ADDRESS_TO, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailCc(const char * addr)
{
    return qMakePair(Composer::ADDRESS_CC, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailBcc(const char * addr)
{
    return qMakePair(Composer::ADDRESS_BCC, mail(addr));
}

void ComposerResponsesTest::testResponseAddresses_data()
{
    using namespace Composer;

    QTest::addColumn<RecipientList>("original");
    QTest::addColumn<QList<QUrl> >("headerListPost");
    QTest::addColumn<bool>("headerListPostNo");
    QTest::addColumn<bool>("privateOk");
    QTest::addColumn<RecipientList>("privateRecipients");
    QTest::addColumn<bool>("allOk");
    QTest::addColumn<RecipientList>("allRecipients");
    QTest::addColumn<bool>("listOk");
    QTest::addColumn<RecipientList>("listRecipients");

    RecipientList empty;
    QList<QUrl> listPost;

    QTest::newRow("from")
        << (RecipientList() << mailFrom("a@b"))
        << listPost << false
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b"))
        << false << empty;

    QTest::newRow("sender")
        << (RecipientList() << mailSender("a@b"))
        << listPost << false
        << false << empty
        << false << empty
        << false << empty;

    QTest::newRow("to")
        << (RecipientList() << mailTo("a@b"))
        << listPost << false
        << false << empty
        << true << (RecipientList() << mailTo("a@b"))
        << false << empty;

    QTest::newRow("cc")
        << (RecipientList() << mailCc("a@b"))
        << listPost << false
        << false << empty
        << true << (RecipientList() << mailTo("a@b"))
        << false << empty;

    QTest::newRow("bcc")
        << (RecipientList() << mailBcc("a@b"))
        << listPost << false
        << false << empty
        << true << (RecipientList() << mailBcc("a@b"))
        << false << empty;

    QTest::newRow("from-to")
        << (RecipientList() << mailFrom("a@b") << mailTo("c@d"))
        << listPost << false
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b") << mailCc("c@d"))
        << false << empty;

    QTest::newRow("from-sender-to")
        << (RecipientList() << mailFrom("a@b") << mailSender("x@y") << mailTo("c@d"))
        << listPost << false
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b") << mailCc("c@d"))
        << false << empty;

    QTest::newRow("list-munged")
        << (RecipientList() << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"));

    QTest::newRow("list-unmunged")
        << (RecipientList() << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"));

    QTest::newRow("list-munged-bcc")
        << (RecipientList() << mailBcc("bcc@example.org") << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"));

    QTest::newRow("list-unmunged-bcc")
        << (RecipientList() << mailBcc("bcc@example.org") << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"));
}

QTEST_MAIN(ComposerResponsesTest)
