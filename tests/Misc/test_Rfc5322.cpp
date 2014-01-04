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

#include <QDebug>
#include <QTest>
#include "test_Rfc5322.h"
#include "Utils/headless_test.h"
#include "Common/MetaTypes.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/Rfc5322HeaderParser.h"

namespace QTest {

/** @short Debug data dumper for QList<uint> */
template<>
char *toString(const QList<QByteArray> &list)
{
    QString buf;
    QDebug d(&buf);
    d << list;
    return qstrdup(buf.toUtf8().constData());
}

}


void Rfc5322Test::initTestCase()
{
    Common::registerMetaTypes();
}

void Rfc5322Test::testHeaders()
{
    QFETCH(QByteArray, input);
    QFETCH(bool, ok);
    QFETCH(QList<QByteArray>, references);
    QFETCH(QList<QByteArray>, listPost);
    QFETCH(bool, listPostNo);
    QFETCH(QList<QByteArray>, messageId);
    QFETCH(QList<QByteArray>, inReplyTo);

    Imap::LowLevelParser::Rfc5322HeaderParser parser;
    bool res = parser.parse(input);
    QCOMPARE(res, ok);

    QCOMPARE(parser.references, references);
    QCOMPARE(parser.listPost, listPost);
    QCOMPARE(parser.listPostNo, listPostNo);
    QCOMPARE(parser.messageId, messageId);
    QCOMPARE(parser.inReplyTo, inReplyTo);
}

void Rfc5322Test::testHeaders_data()
{
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<QList<QByteArray> >("references");
    QTest::addColumn<QList<QByteArray> >("listPost");
    QTest::addColumn<bool>("listPostNo");
    QTest::addColumn<QList<QByteArray> >("messageId");
    QTest::addColumn<QList<QByteArray> >("inReplyTo");

    QList<QByteArray> refs;
    QList<QByteArray> lp;
    QList<QByteArray> mi;
    QList<QByteArray> irt;

    QTest::newRow("empty-1") << QByteArray() << true << refs << lp << false << mi << irt;
    QTest::newRow("empty-2") << QByteArray("  ") << true << refs << lp << false << mi << irt;
    QTest::newRow("empty-3") << QByteArray("\r\n  \r\n") << true << refs << lp << false << mi << irt;

    refs << "foo@bar";
    QTest::newRow("trivial") << QByteArray("reFerences: <foo@bar>\r\n") << true << refs << lp << false << mi << irt;

    refs.clear();
    refs << "a@b" << "x@[aaaa]" << "bar@baz";
    QTest::newRow("folding-squares-phrases-other-headers-etc")
        << QByteArray("references: <a@b>   <x@[aaaa]> foo <bar@\r\n baz>\r\nfail: foo\r\n\r\nsmrt")
        << true << refs << lp << false << mi << irt;

    QTest::newRow("broken-following-headers")
        << QByteArray("references: <a@b>   <x@[aaaa]> foo <bar@\r\n baz>\r\nfail: foo\r")
        << false << refs << lp << false << mi << irt;

    refs.clear();
    lp << "mailto:list@host.com";
    QTest::newRow("list-post-1")
        << QByteArray("List-Post: <mailto:list@host.com>\r\n")
        << true << refs << lp << false << mi << irt;

    lp.clear();
    lp << "mailto:moderator@host.com";
    QTest::newRow("list-post-2")
        << QByteArray("List-Post: <mailto:moderator@host.com> (Postings are Moderated)\r\n")
        << true << refs << lp << false << mi << irt;

    lp.clear();
    lp << "mailto:moderator@host.com?subject=list%20posting";
    QTest::newRow("list-post-3")
        << QByteArray("List-Post: <mailto:moderator@host.com?subject=list%20posting>\r\n")
        << true << refs << lp << false << mi << irt;

    lp.clear();
    QTest::newRow("list-post-no")
        << QByteArray("List-Post: NO (posting not allowed on this list)\r\n")
        << true << refs << lp << true << mi << irt;

    lp << "ftp://ftp.host.com/list.txt" << "mailto:list@host.com?subject=help";
    QTest::newRow("list-post-4")
        << QByteArray("List-Post: <ftp://ftp.host.com/list.txt> (FTP),\r\n  <mailto:list@host.com?subject=help>\r\n")
        << true << refs << lp << false << mi << irt;

    refs.clear();
    lp.clear();
    refs << "20121031120002.5C37D5807C@linuxized.com" << "CAKmKYaDZtfZ9wzKML8WgJ=evVhteyOG0RVfsASpBGViwncsaiQ@mail.gmail.com"
        << "50911AE6.8060402@gmail.com";
    lp << "mailto:gentoo-dev@lists.gentoo.org";
    QTest::newRow("realworld-1")
        << QByteArray("List-Post: <mailto:gentoo-dev@lists.gentoo.org>\r\n"
                      "References: <20121031120002.5C37D5807C@linuxized.com> "
                      "<CAKmKYaDZtfZ9wzKML8WgJ=evVhteyOG0RVfsASpBGViwncsaiQ@mail.gmail.com>\r\n"
                      " <50911AE6.8060402@gmail.com>\r\n"
                      "\r\n")
        << true << refs << lp << false << mi << irt;

    refs.clear();
    lp.clear();
    mi << "ea758af6-e6ff-44d3-964d-cb61289d1d3a@flaska.net";
    QTest::newRow("message-id-1")
        << QByteArray("Message-ID: <ea758af6-e6ff-44d3-964d-cb61289d1d3a@flaska.net>\r\n")
        << true << refs << lp << false << mi << irt;

    mi.clear();
    mi << "a@b";
    irt << "x@y" << "foo@[666]";
    QTest::newRow("message-id-and-in-reply-to")
        << QByteArray("MESSAGE-id: <a@b>\r\n"
                      "In-REPLY-to:   <x@y > <foo@[666]>\r\n")
        << true << refs << lp << false << mi << irt;

    refs.clear();
    lp.clear();
    lp << "http://groups.google.com/group/MEH/post?hl=cs_US" << "mailto:CENSORED@googlegroups.com";
    mi.clear();
    irt.clear();
    QTest::newRow("list-post-http-and-mailto")
        << QByteArray("List-Post: <http://groups.google.com/group/MEH/post?hl=cs_US>, <mailto:CENSORED@googlegroups.com>\r\n")
        << true << refs << lp << false << mi << irt;
}


TROJITA_HEADLESS_TEST(Rfc5322Test)
