/* Copyright (C) 2014 Stephan Platz <trojita@paalsteek.de>
   Copyright (C) 2014 Jan Kundrát <jkt@kde.org>

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

#include <QTest>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "data.h"
#include "test_Imap_MsgPartNetAccessManager.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "Imap/Network/ForbiddenReply.h"
#include "Imap/Network/MsgPartNetworkReply.h"
#include "Streams/FakeSocket.h"

void ImapMsgPartNetAccessManagerTest::init()
{
    LibMailboxSync::init();

    // By default, there's a 50ms delay between the time we request a part download and the time it actually happens.
    // That's too long for a unit test.
    model->setProperty("trojita-imap-delayed-fetch-part", 0);

    networkPolicy = new Imap::Mailbox::DummyNetworkWatcher(0, model);
    netAccessManager = new Imap::Network::MsgPartNetAccessManager(0);

    initialMessages(2);
    QModelIndex m1 = msgListA.child(0, 0);
    QVERIFY(m1.isValid());
    QCOMPARE(model->rowCount(m1), 0);
    cClient(t.mk("UID FETCH 1:2 (" FETCH_METADATA_ITEMS ")\r\n"));
    //cServer()

    QCOMPARE(model->rowCount(msgListA), 2);
    msg1 = msgListA.child(0, 0);
    QVERIFY(msg1.isValid());
    msg2 = msgListA.child(1, 0);
    QVERIFY(msg2.isValid());

    cEmpty();
}

void ImapMsgPartNetAccessManagerTest::cleanup()
{
    delete netAccessManager;
    netAccessManager = 0;
    delete networkPolicy;
    networkPolicy = 0;
    LibMailboxSync::cleanup();
}

void ImapMsgPartNetAccessManagerTest::testMessageParts()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QByteArray, partId);
    QFETCH(QString, url);
    QFETCH(bool, validity);
    QFETCH(QByteArray, text);

    cServer("* 1 FETCH (UID 1 BODYSTRUCTURE (" + bodystructure + "))\r\n" +
            "* 2 FETCH (UID 2 BODYSTRUCTURE (" + bodystructure + "))\r\n"
            + t.last("OK fetched\r\n"));
    QVERIFY(model->rowCount(msg1) > 0);

    netAccessManager->setModelMessage(msg1);
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    QNetworkReply *res = netAccessManager->get(req);
    if (validity) {
        QVERIFY(qobject_cast<Imap::Network::MsgPartNetworkReply*>(res));
        cClient(t.mk("UID FETCH 1 (BODY.PEEK[") + partId + "])\r\n");
        cServer("* 1 FETCH (UID 1 BODY[" + partId + "] " + asLiteral(text) + ")\r\n" + t.last("OK fetched\r\n"));
        cEmpty();
        QCOMPARE(text, res->readAll());
    } else {
        QVERIFY(qobject_cast<Imap::Network::ForbiddenReply*>(res));
    }
    QCOMPARE(res->isFinished(), true);
    cEmpty();
    QVERIFY(errorSpy->isEmpty());
}

void ImapMsgPartNetAccessManagerTest::testMessageParts_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<QByteArray>("partId");
    QTest::addColumn<QString>("url");
    QTest::addColumn<bool>("validity");
    QTest::addColumn<QByteArray>("text");

    QTest::newRow("plaintext/root")
            << bsPlaintext
            << QByteArray("1")
            << "trojita-imap://msg/0"
            << true
            << QByteArray("blesmrt");

    QTest::newRow("plaintext/invalid-index")
            << bsPlaintext
            << QByteArray()
            << "trojita-imap://msg/1"
            << false
            << QByteArray();

    QTest::newRow("plaintext/invalid-child")
            << bsPlaintext
            << QByteArray()
            << "trojita-imap://msg/0/0"
            << false
            << QByteArray();

    QTest::newRow("torture/message-mime")
            << bsTortureTest
            << QByteArray()
            << "trojita-imap://msg/MIME"
            << false
            << QByteArray();

    QTest::newRow("torture/message-text")
            << bsTortureTest
            << QByteArray("TEXT")
            << "trojita-imap://msg/TEXT"
            << true
            << QByteArray("meh");

    QTest::newRow("torture/message-header")
            << bsTortureTest
            << QByteArray("HEADER")
            << "trojita-imap://msg/HEADER"
            << true
            << QByteArray("meh");

    QTest::newRow("torture/plaintext")
            << bsTortureTest
            << QByteArray("1")
            << "trojita-imap://msg/0/0"
            << true
            << QByteArray("plaintext");

    QTest::newRow("torture/plaintext-mime")
            << bsTortureTest
            << QByteArray("1.MIME")
            << "trojita-imap://msg/0/0/MIME"
            << true
            << QByteArray("Content-Type: blabla");

    QTest::newRow("torture/plaintext-text")
            << bsTortureTest
            << QByteArray()
            << "trojita-imap://msg/0/0/TEXT"
            << false
            << QByteArray();

    QTest::newRow("torture/plaintext-header")
            << bsTortureTest
            << QByteArray()
            << "trojita-imap://msg/0/0/HEADER"
            << false
            << QByteArray();

    QTest::newRow("torture/richtext")
            << bsTortureTest
            << QByteArray("2.2.1")
            << "trojita-imap://msg/0/1/0/1/0"
            << true
            << QByteArray("text richtext");

    QTest::newRow("multipartRelated/cid")
            << bsMultipartRelated
            << QByteArray("3")
            << "cid:<image002.jpg@01CEFBE5.40406E50>"
            << true
            << QByteArray("image/jpeg");
}

#define COMMON_METADATA_CHAT_PLAIN_AND_SIGNED \
    cServer("* 1 FETCH (UID 1 BODYSTRUCTURE (" + bsPlaintext + "))\r\n" + \
            "* 2 FETCH (UID 2 BODYSTRUCTURE (" + bsMultipartSignedTextPlain + "))\r\n" \
            + t.last("OK fetched\r\n")); \
    QCOMPARE(model->rowCount(msg1), 1); \
    QCOMPARE(model->rowCount(msg2), 1); \
    QCOMPARE(model->rowCount(msg2.child(0, 0)), 2); \

/** short A fetching operation gets interrupted by switching to the offline mode */
void ImapMsgPartNetAccessManagerTest::testFetchResultOfflineSingle()
{
    COMMON_METADATA_CHAT_PLAIN_AND_SIGNED

    netAccessManager->setModelMessage(msg1);
    QNetworkRequest req;
    req.setUrl(QUrl(QStringLiteral("trojita-imap://msg/0")));
    QNetworkReply *res = netAccessManager->get(req);
    QVERIFY(qobject_cast<Imap::Network::MsgPartNetworkReply*>(res));
    cClient(t.mk("UID FETCH 1 (BODY.PEEK[1])\r\n"));

    QPersistentModelIndex msg1p1 = msgListA.child(0, 0).child(0, 0);
    QVERIFY(msg1p1.isValid());
    QCOMPARE(msg1p1.data(Imap::Mailbox::RoleMessageUid), QVariant(1u));
    QCOMPARE(msg1p1.data(Imap::Mailbox::RoleIsFetched), QVariant(false));
    QCOMPARE(msg1p1.data(Imap::Mailbox::RoleIsUnavailable), QVariant(false));

    networkPolicy->setNetworkOffline();
    cClient(t.mk("LOGOUT\r\n"));
    cServer(t.last("OK logged out\r\n") + "* BYE eh\r\n");
    QCOMPARE(msg1p1.data(Imap::Mailbox::RoleIsFetched), QVariant(false));
    QCOMPARE(msg1p1.data(Imap::Mailbox::RoleIsUnavailable), QVariant(true));

    QCOMPARE(res->isFinished(), true);
    QCOMPARE(res->error(), QNetworkReply::TimeoutError);
}


QTEST_GUILESS_MAIN( ImapMsgPartNetAccessManagerTest )
