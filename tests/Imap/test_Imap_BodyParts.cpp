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
#include "test_Imap_BodyParts.h"
#include "Utils/headless_test.h"
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
            cServer("* 1 FETCH (UID 333 BODY[" + partId.toUtf8() + "] {" + QByteArray::number(it->text.size()) + "}\r\n" +
                    it->text + ")\r\n" + t.last("OK fetched\r\n"));
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

const QByteArray bsPlaintext("\"text\" \"plain\" () NIL NIL NIL 19 2 NIL NIL NIL NIL");
const QByteArray bsMultipartSignedTextPlain(
        "(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"quoted-printable\" 539 16)"
        "(\"application\" \"pgp-signature\" (\"name\" \"signature.asc\") NIL \"Digital signature\" \"7bit\" 205)"
        " \"signed\"");
const QByteArray bsTortureTest("(\"TEXT\" \"PLAIN\" (\"charset\" \"us-ascii\") NIL \"Explanation\" \"7bit\" 190 3 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Rich Text demo\" \"7bit\" 4906 "
        "(\"Tue, 24 Dec 1991 08:14:27 -0500 (EST)\" \"Re: a MIME-Version misfeature\" "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((NIL NIL \"ietf-822\" \"dimacs.rutgers.edu\")) NIL NIL NIL \"<sdJn_nq0M2YtNKaFtO@thumper.bellcore.com>\") "
        "((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 767 16 NIL NIL NIL NIL)"
        "((\"text\" \"richtext\" (\"charset\" \"us-ascii\") NIL NIL \"quoted-printable\" 887 13 NIL NIL NIL NIL) "
        "\"mixed\" (\"boundary\" \"Alternative_Boundary_8dJn:mu0M2Yt5KaFZ8AdJn:mu0M2Yt1KaFdA\") NIL NIL NIL)"
        "(\"application\" \"andrew-inset\" NIL NIL NIL \"7bit\" 917 NIL NIL NIL NIL) \"alternative\" "
        "(\"boundary\" \"Interpart_Boundary_AdJn:mu0M2YtJKaFh9AdJn:mu0M2YtJKaFk=\") NIL NIL NIL) 106 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Voice Mail demo\" \"7bit\" 562270 (\"Tue, 8 Oct 91 10:25:36 EDT\" \"Re: multipart mail\" "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((NIL NIL \"mrc\" \"panda.com\")) "
        "NIL NIL NIL \"<9110081425.AA00616@greenbush.bellcore.com>\") (\"audio\" \"basic\" NIL NIL \"Hi Mark\" \"base64\" 561308 NIL NIL NIL NIL) 7605 NIL NIL NIL NIL)"
        "(\"audio\" \"basic\" NIL NIL \"Flint phone\" \"base64\" 36234 NIL NIL NIL NIL)(\"image\" \"pbm\" NIL NIL \"MTR's photo\" \"base64\" 1814 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Star Trek Party\" \"7bit\" 182880 (\"Thu, 19 Sep 91 12:41:43 EDT\" NIL "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((NIL NIL \"abel\" \"thumper.bellcore.com\")(NIL NIL \"bianchi\" \"thumper.bellcore.com\")(NIL NIL \"braun\" \"thumper.bellcore.com\")(NIL NIL \"cameron\" \"thumper.bellcore.com\")"
        "(NIL NIL \"carmen\" \"thumper.bellcore.com\")(NIL NIL \"jfp\" \"thumper.bellcore.com\")(NIL NIL \"jxr\" \"thumper.bellcore.com\")(NIL NIL \"kraut\" \"thumper.bellcore.com\")"
        "(NIL NIL \"lamb\" \"thumper.bellcore.com\")(NIL NIL \"lowery\" \"thumper.bellcore.com\")(NIL NIL \"lynn\" \"thumper.bellcore.com\")(NIL NIL \"mlittman\" \"thumper.bellcore.com\")"
        "(NIL NIL \"nancyg\" \"thumper.bellcore.com\")(NIL NIL \"sau\" \"thumper.bellcore.com\")(NIL NIL \"shoshi\" \"thumper.bellcore.com\")(NIL NIL \"slr\" \"thumper.bellcore.com\")"
        "(NIL NIL \"stornett\" \"flash.bellcore.com\")(NIL NIL \"tkl\" \"thumper.bellcore.com\")) ((NIL NIL \"nsb\" \"thumper.bellcore.com\")(NIL NIL \"trina\" \"flash.bellcore.com\")) "
        "NIL NIL \"<9109191641.AA12840@greenbush.bellcore.com>\") (((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 731 16 NIL NIL NIL NIL)"
        "(\"audio\" \"x-sun\" NIL NIL \"He's dead, Jim\" \"base64\" 31472 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"Where_No_One_Has_Gone_Before\") NIL NIL NIL)"
        "((\"image\" \"gif\" NIL NIL \"Kirk/Spock/McCoy\" \"base64\" 26000 NIL NIL NIL NIL)(\"image\" \"gif\" NIL NIL \"Star Trek Next Generation\" \"base64\" 18666 NIL NIL NIL NIL)"
        "(\"APPLICATION\" \"X-BE2\" (\"version\" \"12\") NIL NIL \"7bit\" 46125 NIL NIL NIL NIL)"
        "(\"application\" \"atomicmail\" (\"version\" \"1.12\") NIL NIL \"7bit\" 9203 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"Where_No_Man_Has_Gone_Before\") NIL NIL NIL)"
        "(\"audio\" \"x-sun\" NIL NIL \"Distress calls\" \"base64\" 47822 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"Outermost_Trek\") NIL NIL NIL) 4565 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Digitizer test\" \"7bit\" 86113 (\"Fri, 24 May 91 10:40:25 EDT\" \"A cheap digitizer test\" "
        "((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) ((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) "
        "((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) ((NIL NIL \"nsb\" \"sleepy.bellcore.com\")) ((NIL NIL \"sau\" \"sleepy.bellcore.com\")) "
        "NIL NIL \"<9105241440.AA08935@sleepy.bellcore.com>\") ((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 21 0 NIL NIL NIL NIL)"
        "(\"image\" \"pgm\" NIL NIL \"Bellcore mug\" \"base64\" 84174 NIL NIL NIL NIL)(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 267 8 NIL NIL NIL NIL) \"MIXED\" "
        "(\"boundary\" \"mail.sleepy.sau.144.8891\") NIL NIL NIL) 483 NIL NIL NIL NIL)(\"message\" \"rfc822\" NIL NIL \"More Imagery\" \"7bit\" 74470 "
        "(\"Fri, 7 Jun 91 09:09:05 EDT\" \"meta-mail\" ((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) ((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) "
        "((\"Stephen A Uhler\" NIL \"sau\" \"sleepy.bellcore.com\")) ((NIL NIL \"nsb\" \"sleepy.bellcore.com\")) NIL NIL NIL \"<9106071309.AA00574@sleepy.bellcore.com>\") "
        "((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 1246 26 NIL NIL NIL NIL)(\"image\" \"pbm\" NIL NIL \"Mail architecture slide\" \"base64\" 71686 NIL NIL NIL NIL) "
        "\"MIXED\" (\"boundary\" \"mail.sleepy.sau.158.532\") NIL NIL NIL) 431 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"PostScript demo\" \"7bit\" 398008 (\"Mon, 7 Oct 91 12:13:55 EDT\" \"An image that went gif->ppm->ps\" "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((NIL NIL \"mrc\" \"cac.washington.edu\")) NIL NIL NIL \"<9110071613.AA10867@greenbush.bellcore.com>\") "
        "(\"application\" \"postscript\" NIL NIL \"Captain Picard\" \"7bit\" 397154 NIL NIL NIL NIL) 6438 NIL NIL NIL NIL)"
        "(\"image\" \"gif\" NIL NIL \"Quoted-Printable test\" \"quoted-printable\" 78302 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"q-p vs. base64 test\" \"7bit\" 103685 (\"Sat, 26 Oct 91 09:35:10 EDT\" \"Audio in q-p\" "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((NIL NIL \"mrc\" \"akbar.cac.washington.edu\")) "
        "NIL NIL NIL \"<9110261335.AA01130@greenbush.bellcore.com>\") ((\"AUDIO\" \"BASIC\" NIL NIL \"I'm sorry, Dave (q-p)\" \"QUOTED-PRINTABLE\" 62094 NIL NIL NIL NIL)"
        "(\"AUDIO\" \"BASIC\" NIL NIL \"I'm sorry, Dave (BASE64)\" \"BASE64\" 40634 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"hal_9000\") NIL NIL NIL) 1382 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Multiple encapsulation\" \"7bit\" 314835 (\"Thu, 24 Oct 1991 17:32:56 -0700 (PDT)\" \"Here's some more\" "
        "((\"Mark Crispin\" NIL \"MRC\" \"CAC.Washington.EDU\")) ((\"Mark Crispin\" NIL \"mrc\" \"Tomobiki-Cho.CAC.Washington.EDU\")) "
        "((\"Mark Crispin\" NIL \"MRC\" \"CAC.Washington.EDU\")) ((\"Mark Crispin\" NIL \"MRC\" \"CAC.Washington.EDU\")) "
        "NIL NIL NIL \"<MailManager.688350776.11603.mrc@Tomobiki-Cho.CAC.Washington.EDU>\") "
        "((\"APPLICATION\" \"POSTSCRIPT\" NIL NIL \"The Simpsons!!\" \"BASE64\" 53346 NIL NIL NIL NIL)"
        "(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL \"Alice's PDP-10 w/ TECO & DDT\" \"BASE64\" 18530 299 NIL NIL NIL NIL)"
        "(\"message\" \"rfc822\" NIL NIL \"Going deeper\" \"7bit\" 241934 (\"Thu, 24 Oct 1991 17:08:20 -0700 (PDT)\" \"A Multipart message\" "
        "((\"Nathaniel S. Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel S. Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel S. Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((NIL NIL \"nsb\" \"thumper.bellcore.com\")) NIL NIL NIL NIL) "
        "((\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 319 7 NIL NIL NIL NIL)((\"image\" \"gif\" NIL NIL \"Bunny\" \"base64\" 3276 NIL NIL NIL NIL)"
        "(\"audio\" \"basic\" NIL NIL \"TV Theme songs\" \"base64\" 156706 NIL NIL NIL NIL) \"parallel\" (\"boundary\" \"seconddivider\") NIL NIL NIL)"
        "(\"application\" \"atomicmail\" NIL NIL NIL \"7bit\" 4924 NIL NIL NIL NIL)(\"message\" \"rfc822\" NIL NIL \"Yet another level deeper...\" \"7bit\" 75920 "
        "(\"Thu, 24 Oct 1991 17:09:10 -0700 (PDT)\" \"Monster!\" ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) "
        "((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) ((\"Nathaniel Borenstein\" NIL \"nsb\" \"thumper.bellcore.com\")) NIL NIL NIL NIL NIL) "
        "(\"AUDIO\" \"X-SUN\" NIL NIL \"I'm Twying...\" \"base64\" 75682 NIL NIL NIL NIL) 1031 NIL NIL NIL NIL) \"mixed\" (\"boundary\" \"foobarbazola\") "
        "NIL NIL NIL) 2094 NIL NIL NIL NIL) \"MIXED\" (\"BOUNDARY\" \"16819560-2078917053-688350843:#11603\") NIL NIL NIL) 3282 NIL NIL NIL NIL) \"MIXED\" (\"boundary\" \"owatagusiam\") NIL NIL NIL");
QByteArray bsSignedInsideMessageInsideMessage("\"message\" \"rfc822\" NIL NIL NIL \"7bit\" 1511 (\"Thu, 8 Aug 2013 09:02:50 +0200\" \"Re: Your GSoC status\" "
        "((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) ((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) ((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) "
        "((\"Jan\" NIL \"jkt\" \"flaska.net\")) NIL NIL NIL \"<201308080902.51071@pali>\") "
        "((\"Text\" \"Plain\" (\"charset\" \"utf-8\") NIL NIL \"quoted-printable\" 632 20)"
        "(\"application\" \"pgp-signature\" (\"name\" \"signature.asc\") NIL \"This is a digitally signed message part.\" \"7bit\" 205) \"signed\") 51");
QByteArray bsManyPlaintexts("(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
                            "(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
                            "(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
                            "(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
                            "(\"plain\" \"plain\" () NIL NIL \"base64\" 0)"
                            " \"mixed\"");

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
            << Data("0", "1", "blesmrt")
            // The MIME header of the whole message
            << Data("0" + COLUMN_HEADER, "HEADER", "raw headers")
            // No other items
            << Data("0.1")
            << Data("1")
            );

    QTest::newRow("multipart-signed")
            << bsMultipartSignedTextPlain
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

    QTest::newRow("torture-test")
            << bsTortureTest
            << (QList<Data>()
                // Just a top-level child, the multipart/mixed one
                << Data("1")
                // The root is a multipart/mixed item. It's not directly fetchable because it has no "part ID" in IMAP because
                // it's a "top-level multipart", i.e. a multipart which is a child of a message/rfc822.
                << Data("0", Data::NO_FETCHING, "multipart/mixed")
                << Data("0" + COLUMN_TEXT, "TEXT", "meh")
                << Data("0" + COLUMN_HEADER, "HEADER", "meh")
                // There are no MIME or RAW modifier for the root message/rfc822
                << Data("0" + COLUMN_MIME)
                << Data("0" + COLUMN_RAW_CONTENTS)
                // The multipart/mixed is a top-level multipart, and as such it doesn't have the special children
                << Data("0.0" + COLUMN_TEXT)
                << Data("0.0" + COLUMN_HEADER)
                << Data("0.0" + COLUMN_MIME)
                << Data("0.0" + COLUMN_RAW_CONTENTS)
                << Data("0.0", "1", "plaintext", "text/plain")
                << Data("0.0.0" + COLUMN_MIME, "1.MIME", "Content-Type: blabla", "text/plain")
                // A text/plain part does not, however, support the TEXT and HEADER modifiers
                << Data("0.0.0" + COLUMN_TEXT)
                << Data("0.0.0" + COLUMN_HEADER)
                << Data("0.1.0.0", "2.1", "plaintext another", "text/plain")
                << Data("0.1.0.1", "2.2", "multipart mixed", "multipart/mixed")
                << Data("0.1.0.1.0", "2.2.1", "text richtext", "text/richtext")
                << Data("0.1.0.2", "2.3", "andrew thingy", "application/andrew-inset")
                );

    QTest::newRow("message-directly-within-message")
            << bsSignedInsideMessageInsideMessage
            << (QList<Data>()
                << Data("1")
                << Data("0", "1", "aaa", "message/rfc822")
                << Data("0.0", Data::NO_FETCHING, "multipart/signed")
                << Data("0.0" + COLUMN_TEXT, "1.TEXT", "meh")
                << Data("0.0.0", "1.1", "bbb", "text/plain")
                << Data("0.0.1", "1.2", "ccc", "application/pgp-signature")
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
    injector.injectCapability("BINARY");
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
    QCOMPARE(dataChangedSpy[ROW][0].value<QModelIndex>(), INDEX); \
    QCOMPARE(dataChangedSpy[ROW][1].value<QModelIndex>(), INDEX);

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
    model->cache()->setMsgPart("b", 333, "4.X-RAW", fakePartData.toBase64());
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

TROJITA_HEADLESS_TEST(BodyPartsTest)
