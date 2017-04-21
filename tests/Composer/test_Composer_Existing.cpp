/* Copyright (C) 2006 - 2017 Jan Kundrát <jkt@kde.org>

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
#include "test_Composer_Existing.h"
#include "Utils/FakeCapabilitiesInjector.h"
#include "Imap/data.h"
#include "Imap/Model/ItemRoles.h"
#include "Streams/FakeSocket.h"

ComposerExistingTest::ComposerExistingTest()
{
}

void ComposerExistingTest::init()
{
    LibMailboxSync::init();

    // There's a default delay of 50ms which made the cEmpty() and justKeepTask() ignore these pending actions...
    model->setProperty("trojita-imap-delayed-fetch-part", QVariant(0u));

    existsA = 5;
    uidValidityA = 333666;
    uidMapA << 10 << 11 << 12 << 13 << 14;
    uidNextA = 15;
    helperSyncAWithMessagesEmptyState();

    QCOMPARE(msgListA.child(0, 0).data(Imap::Mailbox::RoleMessageSubject), QVariant());
    cClient(t.mk("UID FETCH 10:14 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer("* 1 FETCH (BODYSTRUCTURE "
            "(\"text\" \"plain\" (\"charset\" \"UTF-8\" \"format\" \"flowed\") NIL NIL \"8bit\" 362 15 NIL NIL NIL)"
            " ENVELOPE (NIL \"subj\" NIL NIL NIL NIL NIL NIL NIL \"<msgid>\")"
            ")\r\n" +
            t.last("OK fetched\r\n"));
}

/** @short Not requesting anything should not result in network activity */
void ComposerExistingTest::testDoNothing()
{
    Composer::ExistingMessageComposer composer(msgListA.child(0, 0));
    QVERIFY(!composer.isReadyForSerialization());
    cEmpty();
}

/** @short A successful and simple composing */
void ComposerExistingTest::testSimpleCompose()
{
    Composer::ExistingMessageComposer composer(msgListA.child(0, 0));
    QVERIFY(!composer.isReadyForSerialization());
    QString errorMessage;
    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    QCOMPARE(composer.asRawMessage(&buf, &errorMessage), false);
    QVERIFY(data.isEmpty());
    cClientRegExp(t.mk("UID FETCH 10 \\(BODY\\.PEEK\\["
                       "("
                       "TEXT\\] BODY\\.PEEK\\[HEADER"
                       "|"
                       "HEADER\\] BODY\\.PEEK\\[TEXT"
                       ")"
                       "\\]\\)"));
    QByteArray headers("Date: Wed, 12 Apr 2017 09:28:35 +0200\r\n\r\n");
    QByteArray body("this is the body!");
    cServer("* 1 FETCH (BODY[TEXT] {" + QByteArray::number(body.size()) + "}\r\n" + body + ")\r\n");
    cServer("* 1 FETCH (BODY[HEADER] {" + QByteArray::number(headers.size()) + "}\r\n" + headers + ")\r\n");
    cServer(t.last("OK fetched\r\n"));
    QCOMPARE(composer.asRawMessage(&buf, &errorMessage), true);
    QVERIFY(data.startsWith("Resent-Date: "));
    QVERIFY(data.endsWith("\r\n" + headers + body));
    QCOMPARE(errorMessage, QString());
    QVERIFY(composer.isReadyForSerialization());
    cEmpty();
}

void ComposerExistingTest::testSimpleNo()
{
    Composer::ExistingMessageComposer composer(msgListA.child(0, 0));
    QString errorMessage;
    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    QCOMPARE(composer.asRawMessage(&buf, &errorMessage), false);
    QVERIFY(data.isEmpty());
    cClientRegExp(t.mk("UID FETCH 10 \\(BODY\\.PEEK\\["
                       "("
                       "TEXT\\] BODY\\.PEEK\\[HEADER"
                       "|"
                       "HEADER\\] BODY\\.PEEK\\[TEXT"
                       ")"
                       "\\]\\)"));
    cServer(t.last("NO go away\r\n"));
    QCOMPARE(composer.asRawMessage(&buf, &errorMessage), false);
    QCOMPARE(data, QByteArray());
    QCOMPARE(errorMessage, QStringLiteral("Offline mode: uncached message data not available"));
    QVERIFY(!composer.isReadyForSerialization());
    cEmpty();
}

void ComposerExistingTest::testCatenate()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("CATENATE"));
    Composer::ExistingMessageComposer composer(msgListA.child(0, 0));
    QString errorMessage;
    QVERIFY(!composer.isReadyForSerialization());
    QList<Imap::Mailbox::CatenatePair> data;
    QVERIFY(composer.asCatenateData(data, &errorMessage));
    QCOMPARE(data.size(), 2);
    QCOMPARE(data[0].first, Imap::Mailbox::CATENATE_TEXT);
    QVERIFY(data[0].second.startsWith("Resent-Date: "));
    QVERIFY(data[0].second.endsWith("\r\n"));
    QCOMPARE(data[1].first, Imap::Mailbox::CATENATE_URL);
    QCOMPARE(data[1].second, QByteArrayLiteral("/a;UIDVALIDITY=333666/;UID=10"));
    QVERIFY(!composer.isReadyForSerialization()); // FIXME: this is actually a bug, but one which is hard to fix without changing the API
    cEmpty();
}

QTEST_GUILESS_MAIN(ComposerExistingTest)
