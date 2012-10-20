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
}

TROJITA_HEADLESS_TEST(ComposerResponsesTest)
