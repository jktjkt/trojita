/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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

#include <algorithm>
#include <QtTest>
#include "test_Imap_BodyParts.h"
#include "Utils/headless_test.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

struct Data {
    QString key;
    QString partId;
    QByteArray text;
    bool isValid;

    Data(const QString &key, const QString &partId, const QByteArray &text):
        key(key), partId(partId), text(text), isValid(true)
    {
    }

    Data(const QString &key): key(key), isValid(false)
    {
    }
};

Q_DECLARE_METATYPE(QList<Data>)

/** @short Check that the part numbering works properly */
void BodyPartsTest::testPartIds()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QList<Data>, mapping);

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

    const QString wherePrefix = QString::number(idxB.row()) + QLatin1Char('.') +
            QString::number(msgListB.row()) + QLatin1Char('.') + QString::number(msg.row()) + QLatin1Char('.');
    QCOMPARE(findIndexByPosition(model, wherePrefix.left(wherePrefix.size() - 1)), msg);

    for (auto it = mapping.constBegin(); it != mapping.constEnd(); ++it) {
        QModelIndex idx = findIndexByPosition(model, wherePrefix + it->key);
        if (!it->isValid) {
            if (idx.isValid()) {
                qDebug() << "Index " << it->key << " is valid";
                QFAIL("Unexpected valid index");
            }
            continue;
        }
        QVERIFY(idx.isValid());
        QString partId = idx.data(Imap::Mailbox::RolePartId).toString();
        QCOMPARE(partId, it->partId);

        QCOMPARE(idx.data(Imap::Mailbox::RolePartData).toString(), QString());
        cClient(t.mk("UID FETCH 333 (BODY.PEEK[") + partId.toUtf8() + "])\r\n");
        cServer("* 1 FETCH (UID 333 BODY[" + partId.toUtf8() + "] {" + QByteArray::number(it->text.size()) + "}\r\n" +
                it->text + ")\r\n" + t.last("OK fetched\r\n"));
        QCOMPARE(idx.data(Imap::Mailbox::RolePartData).toByteArray(), it->text);
    }

    cEmpty();
    QVERIFY(errorSpy->isEmpty());
}

void BodyPartsTest::testPartIds_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<QList<Data>>("mapping");

#define COLUMN_HEADER ("c" + QByteArray::number(Imap::Mailbox::TreeItem::OFFSET_HEADER))
#define COLUMN_TEXT ("c" + QByteArray::number(Imap::Mailbox::TreeItem::OFFSET_TEXT))
#define COLUMN_MIME ("c" + QByteArray::number(Imap::Mailbox::TreeItem::OFFSET_MIME))

    QTest::newRow("plaintext")
        << QByteArray("\"text\" \"plain\" () NIL NIL NIL 19 2 NIL NIL NIL NIL")
        << (QList<Data>()
            // Part 1, a text/plain thing
            << Data("0", "1", "blesmrt")
            // The MIME header of the whole message
            << Data("0" + COLUMN_HEADER, "HEADER", "raw headers")
            // No other items
            << Data("0.1")
            << Data("1")
            );

    QTest::newRow("multipart-signed")
            << QByteArray("(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"quoted-printable\" 539 16)"
                          "(\"application\" \"pgp-signature\" (\"name\" \"signature.asc\") NIL \"Digital signature\" \"7bit\" 205)"
                          " \"signed\"")
            << (QList<Data>()
                //<< Data("0", "1", "blesmrt")
                << Data("0.0", "1", "blesmrt")
                << Data("0.1", "2", "signature")
                // No other parts shall be defined
                << Data("0.2")
                << Data("0.0.0")
                << Data("0.1.0")
                << Data("1")
                << Data("2")
                );
}

TROJITA_HEADLESS_TEST(BodyPartsTest)
