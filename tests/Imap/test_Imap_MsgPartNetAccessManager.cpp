/* Copyright (C) 2014 Stephan Platz <trojita@paalsteek.de>

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
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "Imap/Network/ForbiddenReply.h"
#include "Imap/Network/MsgPartNetworkReply.h"
#include "Streams/FakeSocket.h"
#include "Utils/headless_test.h"

void ImapMsgPartNetAccessManagerTest::testMessageParts()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QByteArray, partId);
    QFETCH(QString, url);
    QFETCH(bool, validity);
    QFETCH(QByteArray, text);

    // By default, there's a 50ms delay between the time we request a part download and the time it actually happens.
    // That's too long for a unit test.
    model->setProperty("trojita-imap-delayed-fetch-part", 0);

    helperSyncBNoMessages();
    cServer("* 1 EXISTS\r\n");
    cClient(t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    cServer("* 1 FETCH (UID 333 FLAGS ())\r\n" + t.last("OK fetched\r\n"));

    QCOMPARE(model->rowCount(msgListB), 1);
    QModelIndex msg = msgListB.child(0, 0);
    QVERIFY(msg.isValid());
    QCOMPARE(model->rowCount(msg), 0);
    cClient(t.mk("UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bodystructure + "))\r\n" + t.last("OK fetched\r\n"));
    QVERIFY(model->rowCount(msg) > 0);

    Imap::Network::MsgPartNetAccessManager nam(this);
    nam.setModelMessage(msg);
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    QNetworkReply *res = nam.get(req);
    if (validity) {
        QVERIFY(qobject_cast<Imap::Network::MsgPartNetworkReply*>(res));
        cClient(t.mk("UID FETCH 333 (BODY.PEEK[") + partId + "])\r\n");
        cServer("* 1 FETCH (UID 333 BODY[" + partId + "] {" + QByteArray::number(text.size()) + "}\r\n" +
                                                                               text + ")\r\n" + t.last("OK fetched\r\n"));
        cEmpty();
        QCOMPARE(text, res->readAll());
    } else {
        QVERIFY(qobject_cast<Imap::Network::ForbiddenReply*>(res));
    }
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

TROJITA_HEADLESS_TEST( ImapMsgPartNetAccessManagerTest )
