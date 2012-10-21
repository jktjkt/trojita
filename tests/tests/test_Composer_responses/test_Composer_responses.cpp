/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#include <QtTest>
#include "test_Composer_responses.h"
#include "../headless_test.h"
#include "Composer/PlainTextFormatter.h"
#include "Composer/SubjectMangling.h"

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
    QTest::newRow("ml-ml") << QString::fromUtf8("[foo] [bar] blah") << QString::fromUtf8("[foo] Re: [bar] blah");
    QTest::newRow("ml-ml-re") << QString::fromUtf8("[foo] [bar] Re: blah") << QString::fromUtf8("[foo] Re: [bar] Re: blah");
    QTest::newRow("ml-re-ml") << QString::fromUtf8("[foo] Re: [bar] blah") << QString::fromUtf8("[foo] Re: [bar] blah");
    QTest::newRow("ml-re-ml-re") << QString::fromUtf8("[foo] Re: [bar] Re: blah") << QString::fromUtf8("[foo] Re: [bar] Re: blah");
    QTest::newRow("re-ml-ml") << QString::fromUtf8("Re: [foo] [bar] blah") << QString::fromUtf8("[foo] Re: [bar] blah");
    QTest::newRow("re-ml-ml-re") << QString::fromUtf8("Re: [foo] [bar] Re: blah") << QString::fromUtf8("[foo] Re: [bar] Re: blah");
    QTest::newRow("re-ml-re-ml") << QString::fromUtf8("Re: [foo] Re: [bar] blah") << QString::fromUtf8("[foo] Re: [bar] blah");
    QTest::newRow("re-ml-re-ml-re") << QString::fromUtf8("Re: [foo] Re: [bar] Re: blah") << QString::fromUtf8("[foo] Re: [bar] Re: blah");
}

/** @short Test that conversion of plaintext mail to HTML works reasonably well */
void ComposerResponsesTest::testPlainTextFormatting()
{
    QFETCH(QString, plaintext);
    QFETCH(QString, html);

    QCOMPARE(Composer::Util::plainTextToHtml(plaintext).join(QLatin1String("\n")), html);
}

/** @short Data for testPlainTextFormatting */
void ComposerResponsesTest::testPlainTextFormatting_data()
{
    QTest::addColumn<QString>("plaintext");
    QTest::addColumn<QString>("html");

    QTest::newRow("empty-1") << QString() << QString("\n");
    QTest::newRow("empty-2") << QString("") << QString("\n");
    QTest::newRow("empty-3") << QString("\n") << QString("\n\n");
    QTest::newRow("empty-4") << QString("\n\n") << QString("\n\n\n");

    QTest::newRow("minimal") << QString("ahoj") << QString("ahoj\n");
    QTest::newRow("containing-html") << QString("<p>ahoj &amp; blesmrt</p>") << QString("&lt;p&gt;ahoj &amp;amp; blesmrt&lt;/p&gt;\n");
    QTest::newRow("basic-formatting") << QString("foo *bar* _baz_ /pwn/ yay") <<
                                         QString("foo <b><span class=\"markup\">*</span>bar<span class=\"markup\">*</span></b> "
                                                 "<u><span class=\"markup\">_</span>baz<span class=\"markup\">_</span></u> "
                                                 "<i><span class=\"markup\">/</span>pwn<span class=\"markup\">/</span></i> yay\n");
    QTest::newRow("links") << QString("ahoj http://pwn:123/foo?bar&baz#nope") <<
                              QString("ahoj <a href=\"http://pwn:123/foo?bar&amp;baz#nope\">http://pwn:123/foo?bar&amp;baz#nope</a>\n");
    // Test our escaping
    QTest::newRow("escaping-1") << QString::fromUtf8("&gt; § §gt; §para;\n") << QString::fromUtf8("&amp;gt; § §gt; §para;\n\n");

    // FIXME: more tests, including the format=flowed bits
    //QTest::newRow("long line") << QString("Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.") << QString("ahoj\n");
}

TROJITA_HEADLESS_TEST(ComposerResponsesTest)
