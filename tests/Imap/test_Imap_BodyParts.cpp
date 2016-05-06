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

#include <algorithm>
#include <QtTest>
#include "data.h"
#include "test_Imap_BodyParts.h"
#include "Utils/FakeCapabilitiesInjector.h"
#include "Streams/FakeSocket.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

struct Data {
    QString key;
    QString partId;
    QByteArray text;
    QString mimeType;

    typedef enum {INVALID_INDEX, NO_FETCHING, REGULAR} ItemType;
    ItemType itemType;

    Data(const QString &key, const QString &partId, const QByteArray &text, const QString &mimeType=QString()):
        key(key), partId(partId), text(text), mimeType(mimeType), itemType(REGULAR)
    {
    }

    Data(const QString &key, const ItemType itemType=INVALID_INDEX, const QString &mimeType=QString()):
        key(key), mimeType(mimeType), itemType(itemType)
    {
    }
};

Q_DECLARE_METATYPE(QList<Data>)

namespace QTest {
template <>
char *toString(const QModelIndex &index)
{
    QString buf;
    QDebug(&buf) << index;
    return qstrdup(buf.toUtf8().constData());
}
}

using namespace Imap::Mailbox;

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
        if (it->itemType == Data::INVALID_INDEX) {
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

        if (it->itemType == Data::REGULAR) {
            cClient(t.mk("UID FETCH 333 (BODY.PEEK[") + partId.toUtf8() + "])\r\n");
            cServer("* 1 FETCH (UID 333 BODY[" + partId.toUtf8() + "] " + asLiteral(it->text) + ")\r\n"
                    + t.last("OK fetched\r\n"));
            QCOMPARE(idx.data(Imap::Mailbox::RolePartData).toByteArray(), it->text);
        } else if (it->itemType == Data::NO_FETCHING) {
            cEmpty();
            QCOMPARE(idx.data(Imap::Mailbox::RolePartData).toByteArray(), QByteArray());
        }

        if (!it->mimeType.isNull()) {
            QCOMPARE(idx.data(Imap::Mailbox::RolePartMimeType).toString(), it->mimeType);
        }
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
#define COLUMN_RAW_CONTENTS ("c" + QByteArray::number(Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS))

    QTest::newRow("plaintext")
        << bsPlaintext
        << (QList<Data>()
            // Part 1, a text/plain thing
            << Data(QStringLiteral("0"), QStringLiteral("1"), "blesmrt")
            // The MIME header of the whole message
            << Data(QString("0" + COLUMN_HEADER), QStringLiteral("HEADER"), "raw headers")
            // No other items
            << Data(QStringLiteral("0.1"))
            << Data(QStringLiteral("1"))
            );

    QTest::newRow("multipart-signed")
            << bsMultipartSignedTextPlain
            << (QList<Data>()
                //<< Data("0", "1", "blesmrt")
                << Data(QStringLiteral("0.0"), QStringLiteral("1"), "blesmrt")
                << Data(QStringLiteral("0.1"), QStringLiteral("2"), "signature")
                // No other parts shall be defined
                << Data(QStringLiteral("0.2"))
                << Data(QStringLiteral("0.0.0"))
                << Data(QStringLiteral("0.1.0"))
                << Data(QStringLiteral("1"))
                << Data(QStringLiteral("2"))
                );

    QTest::newRow("torture-test")
            << bsTortureTest
            << (QList<Data>()
                // Just a top-level child, the multipart/mixed one
                << Data(QStringLiteral("1"))
                // The root is a multipart/mixed item. It's not directly fetchable because it has no "part ID" in IMAP because
                // it's a "top-level multipart", i.e. a multipart which is a child of a message/rfc822.
                << Data(QStringLiteral("0"), Data::NO_FETCHING, QStringLiteral("multipart/mixed"))
                << Data(QString("0" + COLUMN_TEXT), QStringLiteral("TEXT"), "meh")
                << Data(QString("0" + COLUMN_HEADER), QStringLiteral("HEADER"), "meh")
                // There are no MIME or RAW modifier for the root message/rfc822
                << Data(QString("0" + COLUMN_MIME))
                << Data(QString("0" + COLUMN_RAW_CONTENTS))
                // The multipart/mixed is a top-level multipart, and as such it doesn't have the special children
                << Data(QString("0.0" + COLUMN_TEXT))
                << Data(QString("0.0" + COLUMN_HEADER))
                << Data(QString("0.0" + COLUMN_MIME))
                << Data(QString("0.0" + COLUMN_RAW_CONTENTS))
                << Data(QStringLiteral("0.0"), QStringLiteral("1"), "plaintext", QStringLiteral("text/plain"))
                << Data(QString("0.0.0" + COLUMN_MIME), QStringLiteral("1.MIME"), "Content-Type: blabla", QStringLiteral("text/plain"))
                // A text/plain part does not, however, support the TEXT and HEADER modifiers
                << Data(QString("0.0.0" + COLUMN_TEXT))
                << Data(QString("0.0.0" + COLUMN_HEADER))
                << Data(QStringLiteral("0.1.0.0"), QStringLiteral("2.1"), "plaintext another", QStringLiteral("text/plain"))
                << Data(QStringLiteral("0.1.0.1"), QStringLiteral("2.2"), "multipart mixed", QStringLiteral("multipart/mixed"))
                << Data(QStringLiteral("0.1.0.1.0"), QStringLiteral("2.2.1"), "text richtext", QStringLiteral("text/richtext"))
                << Data(QStringLiteral("0.1.0.2"), QStringLiteral("2.3"), "andrew thingy", QStringLiteral("application/andrew-inset"))
                );

    QTest::newRow("message-directly-within-message")
            << bsSignedInsideMessageInsideMessage
            << (QList<Data>()
                << Data(QStringLiteral("1"))
                << Data(QStringLiteral("0"), QStringLiteral("1"), "aaa", QStringLiteral("message/rfc822"))
                << Data(QStringLiteral("0.0"), Data::NO_FETCHING, QStringLiteral("multipart/signed"))
                << Data(QString("0.0" + COLUMN_TEXT), QStringLiteral("1.TEXT"), "meh")
                << Data(QStringLiteral("0.0.0"), QStringLiteral("1.1"), "bbb", QStringLiteral("text/plain"))
                << Data(QStringLiteral("0.0.1"), QStringLiteral("1.2"), "ccc", QStringLiteral("application/pgp-signature"))
                );
}

/** @short Check that we catch responses which refer to invalid data */
void BodyPartsTest::testInvalidPartFetch()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QString, partId);

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

    {
        ExpectSingleErrorHere blocker(this);
        cServer("* 1 FETCH (UID 333 BODY[" + partId.toUtf8() + "] \"pwn\")\r\n");
    }
    QVERIFY(errorSpy->isEmpty());
}

void BodyPartsTest::testInvalidPartFetch_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    // QString allows us to use string literals
    QTest::addColumn<QString>("partId");

    QTest::newRow("extra-part-plaintext") << bsPlaintext << "2";
    QTest::newRow("extra-part-plaintext-child") << bsPlaintext << "1.1";
    QTest::newRow("extra-part-plaintext-zero") << bsPlaintext << "0";
    QTest::newRow("extra-part-signed-zero") << bsMultipartSignedTextPlain << "0";
    QTest::newRow("extra-part-signed-child-1") << bsMultipartSignedTextPlain << "1.0";
    QTest::newRow("extra-part-signed-child-2") << bsMultipartSignedTextPlain << "2.0";
    QTest::newRow("extra-part-signed-extra") << bsMultipartSignedTextPlain << "3";
    QTest::newRow("extra-part-signed-MIME") << bsMultipartSignedTextPlain << "MIME";
    QTest::newRow("extra-part-signed-1-TEXT") << bsMultipartSignedTextPlain << "1.TEXT";
    QTest::newRow("extra-part-signed-1-HEADER") << bsMultipartSignedTextPlain << "1.HEADER";
}

/** @short Check how fetching the raw part data is handled */
void BodyPartsTest::testFetchingRawParts()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QStringLiteral("BINARY"));
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
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bsManyPlaintexts + "))\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(model->rowCount(msg), 1);
    QModelIndex rootMultipart = msg.child(0, 0);
    QVERIFY(rootMultipart.isValid());
    QCOMPARE(model->rowCount(rootMultipart), 5);

    QSignalSpy dataChangedSpy(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)));

#define CHECK_DATACHANGED(ROW, INDEX) \
    QCOMPARE(dataChangedSpy[ROW][0].toModelIndex(), INDEX); \
    QCOMPARE(dataChangedSpy[ROW][1].toModelIndex(), INDEX);

    QModelIndex part, rawPart;
    QByteArray fakePartData = "Canary 1";

    // First make sure that we cam fetch the raw version of a simple part which has not been fetched before.
    part = rootMultipart.child(0, 0);
    QCOMPARE(part.data(RolePartId).toString(), QString("1"));
    rawPart = part.child(0, TreeItem::OFFSET_RAW_CONTENTS);
    QVERIFY(rawPart.isValid());
    QCOMPARE(rawPart.data(RolePartData).toByteArray(), QByteArray());
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[1])\r\n"));
    cServer("* 1 FETCH (UID 333 BODY[1] \"" + fakePartData.toBase64() + "\")\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(dataChangedSpy.size(), 1);
    CHECK_DATACHANGED(0, rawPart);
    QVERIFY(!part.data(RoleIsFetched).toBool());
    QVERIFY(rawPart.data(RoleIsFetched).toBool());
    QCOMPARE(rawPart.data(RolePartData).toByteArray(), fakePartData.toBase64());
    QVERIFY(model->cache()->messagePart("b", 333, "1").isNull());
    QCOMPARE(model->cache()->messagePart("b", 333, "1.X-RAW"), fakePartData.toBase64());
    cEmpty();
    dataChangedSpy.clear();

    // Check that the same fetch is repeated when a request for raw, unprocessed data is made again
    part = rootMultipart.child(1, 0);
    QCOMPARE(part.data(RolePartId).toString(), QString("2"));
    rawPart = part.child(0, TreeItem::OFFSET_RAW_CONTENTS);
    QVERIFY(rawPart.isValid());
    QCOMPARE(part.data(RolePartData).toByteArray(), QByteArray());
    cClient(t.mk("UID FETCH 333 (BINARY.PEEK[2])\r\n"));
    cServer("* 1 FETCH (UID 333 BINARY[2] \"ahoj\")\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(dataChangedSpy.size(), 1);
    CHECK_DATACHANGED(0, part);
    dataChangedSpy.clear();
    QVERIFY(part.data(RoleIsFetched).toBool());
    QVERIFY(!rawPart.data(RoleIsFetched).toBool());
    QCOMPARE(part.data(RolePartData).toString(), QString("ahoj"));
    QCOMPARE(model->cache()->messagePart("b", 333, "2"), QByteArray("ahoj"));
    QCOMPARE(model->cache()->messagePart("b", 333, "2.X-RAW").isNull(), true);
    // Trigger fetching of the raw data.
    // Make sure that we do *not* overwite the already decoded data needlessly.
    // If the server is broken and performs the CTE decoding in a wrong way, let's just silenty ignore this.
    fakePartData = "Canary 2";
    QCOMPARE(rawPart.data(RolePartData).toByteArray(), QByteArray());
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[2])\r\n"));
    cServer("* 1 FETCH (UID 333 BODY[2] \"" + fakePartData.toBase64() + "\")\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(dataChangedSpy.size(), 1);
    CHECK_DATACHANGED(0, rawPart);
    QVERIFY(part.data(RoleIsFetched).toBool());
    QVERIFY(rawPart.data(RoleIsFetched).toBool());
    QCOMPARE(part.data(RolePartData).toString(), QString("ahoj"));
    QCOMPARE(rawPart.data(RolePartData).toByteArray(), fakePartData.toBase64());
    QVERIFY(model->cache()->messagePart("b", 333, "2").isNull());
    QCOMPARE(model->cache()->messagePart("b", 333, "2.X-RAW"), fakePartData.toBase64());
    cEmpty();
    dataChangedSpy.clear();

    // Make sure that requests for part whose raw form was already loaded is accomodated locally
    fakePartData = "Canary 3";
    part = rootMultipart.child(2, 0);
    QCOMPARE(part.data(RolePartId).toString(), QString("3"));
    rawPart = part.child(0, TreeItem::OFFSET_RAW_CONTENTS);
    QVERIFY(rawPart.isValid());
    QCOMPARE(rawPart.data(RolePartData).toByteArray(), QByteArray());
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[3])\r\n"));
    cServer("* 1 FETCH (UID 333 BODY[3] \"" + fakePartData.toBase64() + "\")\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(dataChangedSpy.size(), 1);
    CHECK_DATACHANGED(0, rawPart);
    QVERIFY(!part.data(RoleIsFetched).toBool());
    QVERIFY(rawPart.data(RoleIsFetched).toBool());
    QCOMPARE(rawPart.data(RolePartData).toByteArray(), fakePartData.toBase64());
    QCOMPARE(model->cache()->messagePart("b", 333, "3").isNull(), true);
    QCOMPARE(model->cache()->messagePart("b", 333, "3.X-RAW"), fakePartData.toBase64());
    cEmpty();
    dataChangedSpy.clear();
    // Now the request for actual part data shall be accomodated from the cache.
    // As this is a first request ever, there's no need to emit dataChanged. The on-disk cache is not populated.
    QCOMPARE(part.data(RolePartData).toByteArray(), fakePartData);
    QVERIFY(part.data(RoleIsFetched).toBool());
    QCOMPARE(dataChangedSpy.size(), 0);
    QCOMPARE(model->cache()->messagePart("b", 333, "3").isNull(), true);

    // Make sure that requests for already processed part are accomodated from the cache if possible
    fakePartData = "Canary 4";
    part = rootMultipart.child(3, 0);
    QCOMPARE(part.data(RolePartId).toString(), QString("4"));
    rawPart = part.child(0, TreeItem::OFFSET_RAW_CONTENTS);
    QVERIFY(rawPart.isValid());
    model->cache()->setMsgPart(QStringLiteral("b"), 333, "4.X-RAW", fakePartData.toBase64());
    QVERIFY(!part.data(RoleIsFetched).toBool());
    QVERIFY(!rawPart.data(RoleIsFetched).toBool());
    QCOMPARE(part.data(RolePartData).toByteArray(), fakePartData);
    QCOMPARE(dataChangedSpy.size(), 0);
    QCOMPARE(model->cache()->messagePart("b", 333, "4").isNull(), true);
    cEmpty();
    dataChangedSpy.clear();

    // Make sure that requests for already processed part and the raw data are merged if they happen close enough and in the correct order
    fakePartData = "Canary 5";
    part = rootMultipart.child(4, 0);
    QCOMPARE(part.data(RolePartId).toString(), QString("5"));
    rawPart = part.child(0, TreeItem::OFFSET_RAW_CONTENTS);
    QVERIFY(rawPart.isValid());
    QCOMPARE(rawPart.data(RolePartData).toByteArray(), QByteArray());
    QCOMPARE(part.data(RolePartData).toByteArray(), QByteArray());
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[5])\r\n"));
    cServer("* 1 FETCH (UID 333 BODY[5] \"" + fakePartData.toBase64() + "\")\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(dataChangedSpy.size(), 2);
    CHECK_DATACHANGED(0, rawPart);
    CHECK_DATACHANGED(1, part);
    dataChangedSpy.clear();
    QVERIFY(part.data(RoleIsFetched).toBool());
    QVERIFY(rawPart.data(RoleIsFetched).toBool());
    QCOMPARE(part.data(RolePartData).toString(), QString(fakePartData));
    QCOMPARE(rawPart.data(RolePartData).toByteArray(), fakePartData.toBase64());
    QVERIFY(model->cache()->messagePart("b", 333, "5").isNull());
    QCOMPARE(model->cache()->messagePart("b", 333, "5.X-RAW"), fakePartData.toBase64());
    cEmpty();
}

void BodyPartsTest::testFilenameExtraction()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QString, partId);
    QFETCH(QString, filename);

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

    QModelIndex idx = findIndexByPosition(model, wherePrefix + partId);
    QVERIFY(idx.isValid());
    QCOMPARE(idx.data(Imap::Mailbox::RolePartFileName).toString(), filename);
    QVERIFY(errorSpy->isEmpty());
    cEmpty();
}

void BodyPartsTest::testFilenameExtraction_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<QString>("partId");
    QTest::addColumn<QString>("filename");


    QTest::newRow("evernote-plaintext-0") << bsEvernote << QStringLiteral("0") << QString(); // multipart/mixed
    QTest::newRow("evernote-plaintext-0.0") << bsEvernote << QStringLiteral("0.0") << QString(); // multipart/alternative
    QTest::newRow("evernote-plaintext-0.0.0") << bsEvernote << QStringLiteral("0.0.0") << QString(); // text/plain
    QTest::newRow("evernote-plaintext-0.0.1") << bsEvernote << QStringLiteral("0.0.1") << QString(); // text/html
    QTest::newRow("evernote-plaintext-0.1") << bsEvernote << QStringLiteral("0.1") << QStringLiteral("CAN0000009221(1)"); // application/octet-stream

    QTest::newRow("plaintext-just-filename") << bsPlaintextWithFilenameAsFilename << QStringLiteral("0") << QStringLiteral("pwn.txt");
    QTest::newRow("plaintext-just-obsolete-name") << bsPlaintextWithFilenameAsName << QStringLiteral("0") << QStringLiteral("pwn.txt");
    QTest::newRow("plaintext-filename-preferred-over-name") << bsPlaintextWithFilenameAsBoth << QStringLiteral("0") << QStringLiteral("pwn.txt");
    QTest::newRow("name-overwrites-empty-filename") << bsPlaintextEmptyFilename << QStringLiteral("0") << QStringLiteral("actual");
}

QTEST_GUILESS_MAIN(BodyPartsTest)
