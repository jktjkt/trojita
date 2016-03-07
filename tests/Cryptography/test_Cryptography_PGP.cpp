/* Copyright (C) 2014-2015 Stephan Platz <trojita@paalsteek.de>

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

#include <QtTest/QtTest>

#include "test_Cryptography_PGP.h"
#include "configure.cmake.h"
#include "crypto_test_data.h"
#include "Cryptography/MessageModel.h"
#include "Imap/data.h"
#include "Imap/Model/ItemRoles.h"
#include "Streams/FakeSocket.h"

#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
#  ifdef TROJITA_HAVE_GPGMEPP
#    include "Cryptography/GpgMe++.h"
#  endif
#endif

/* TODO: test cases:
 *   * decrypt with not yet valid key
 *   * verify valid signature
 *   * verify corrupted signature
 *   * verify signature of modified text
 *   * verify signature with expired key
 *   * verify signature with not yet valid key
 *   * verify signature with unkown key
 *   * nested scenarios
 */

void CryptographyPGPTest::initTestCase()
{
    LibMailboxSync::initTestCase();
    if (!qputenv("GNUPGHOME", "keys")) {
        QFAIL("Unable to set GNUPGHOME environment variable");
    }
}

void CryptographyPGPTest::testDecryption()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QByteArray, cyphertext);
    QFETCH(QByteArray, plaintext);
    QFETCH(bool, successful);

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
    cEmpty();
    QVERIFY(model->rowCount(msg) > 0);
    Cryptography::MessageModel msgModel(0, msg);
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
#  ifdef TROJITA_HAVE_GPGMEPP
    msgModel.registerPartHandler(std::make_shared<Cryptography::GpgMeReplacer>());
#  endif
#endif
    QModelIndex mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());
    QVERIFY(msgModel.rowCount(mappedMsg) > 0);

    QModelIndex data = mappedMsg.child(0, 0);
    QVERIFY(data.isValid());
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
    QCOMPARE(msgModel.rowCount(data), 0);
    QCOMPARE(data.data(Imap::Mailbox::RoleIsFetched).toBool(), false);

    cClientRegExp(t.mk("UID FETCH 333 \\((BODY\\.PEEK\\[2\\] BODY\\.PEEK\\[1\\]|BODY.PEEK\\[1\\] BODY\\.PEEK\\[2\\])\\)"));
    cServer("* 1 FETCH (UID 333 BODY[2] {" + QByteArray::number(cyphertext.size())
            + "}\r\n" + cyphertext + " BODY[1] {12}\r\nVersion: 1\r\n)\r\n"
            + t.last("OK fetched"));

    QSignalSpy qcaSuccessSpy(&msgModel, SIGNAL(rowsInserted(const QModelIndex &,int,int)));
    QSignalSpy qcaErrorSpy(&msgModel, SIGNAL(error(const QModelIndex &,QString,QString)));

    int i = 0;
    while (data.isValid() && data.data(Imap::Mailbox::RolePartCryptoNotFinishedYet).toBool() && i++ < 1000) {
        QTest::qWait(10);
    }
    // allow for event processing, so that the model can retrieve the results
    QCoreApplication::processEvents();

    if (!qcaErrorSpy.isEmpty() && successful) {
        qDebug() << "Unexpected failure in crypto";
        for (int i = 0; i < qcaErrorSpy.size(); ++i) {
            qDebug() << qcaErrorSpy[i][1].toString();
            qDebug() << qcaErrorSpy[i][2].toString();
        }
    }

    if (successful) {
        QCOMPARE(qcaErrorSpy.empty(), successful);
        QCOMPARE(qcaSuccessSpy.empty(), !successful);
    }

    QVERIFY(data.data(Imap::Mailbox::RoleIsFetched).toBool());

    cEmpty();
    QVERIFY(errorSpy->empty());
#else
    QCOMPARE(msgModel.rowCount(data), 2);
    QCOMPARE(data.data(Imap::Mailbox::RoleIsFetched).toBool(), true);

    QCOMPARE(data.child(0, 0).data(Imap::Mailbox::RolePartMimeType).toString(), QLatin1String("application/pgp-encrypted"));
    QCOMPARE(data.child(1, 0).data(Imap::Mailbox::RolePartMimeType).toString(), QLatin1String("application/octet-stream"));
    cEmpty();

    QSKIP("Some tests were skipped because this build doesn't have GpgME++ support");
#endif
}

void CryptographyPGPTest::testDecryption_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<QByteArray>("cyphertext");
    QTest::addColumn<QByteArray>("plaintext");
    QTest::addColumn<bool>("successful");

    QByteArray bsEncrypted = QByteArrayLiteral(
                "(\"application\" \"pgp-encrypted\" NIL NIL NIL \"7bit\" 12 NIL NIL NIL NIL)"
                "(\"application\" \"octet-stream\" (\"name\" \"encrypted.asc\") NIL \"OpenPGP encrypted message\" \"7bit\" "
                "4127 NIL (\"inline\" (\"filename\" \"encrypted.asc\")) NIL NIL) \"encrypted\" "
                "(\"protocol\" \"application/pgp-encrypted\" \"boundary\" \"trojita=_7cf0b2b6-64c6-41ad-b381-853caf492c54\") NIL NIL NIL");

    // everything is correct
    QTest::newRow("valid")
            << bsEncrypted
            << encValid
            << QByteArray("plaintext")
            << true;

    // corrupted daya
    QTest::newRow("invalid")
            << bsEncrypted
            << encInvalid
            << QByteArray("plaintext")
            << false;

    // the key used for encryption is expired
    QTest::newRow("expiredKey")
            << bsEncrypted
            << encExpired
            << QByteArray("plaintext")
               // NOTE (jkt): This is how my QCA/2.1.0.3, GnuPG/2.0.28 behaves.
            << true;

    // we don't have any key which is needed for encryption
    QTest::newRow("unknownKey")
            << bsEncrypted
            << encUnknown
            << QByteArray("plaintext")
            << false;
}

void CryptographyPGPTest::testVerification()
{
    QFETCH(QByteArray, signature);
    QFETCH(QByteArray, plaintext);
    QFETCH(bool, successful);
    QFETCH(QString, from);
    QFETCH(QString, tldr);
    QFETCH(QString, longDesc);
    QFETCH(bool, validDisregardingTrust);
    QFETCH(bool, validCompletely);

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
    cServer(helperCreateTrivialEnvelope(1, 333, QStringLiteral("subj"), from, QStringLiteral("("
            "(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 423 14 NIL NIL NIL NIL)"
            "(\"application\" \"pgp-signature\" NIL NIL NIL \"7bit\" 851 NIL NIL NIL NIL)"
            " \"signed\" (\"boundary\" \"=-=-=\" \"micalg\" \"pgp-sha256\" \"protocol\" \"application/pgp-signature\")"
            " NIL NIL NIL)"))
            + t.last("OK fetched\r\n"));
    cEmpty();
    QVERIFY(model->rowCount(msg) > 0);
    Cryptography::MessageModel msgModel(0, msg);
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
#  ifdef TROJITA_HAVE_GPGMEPP
    msgModel.registerPartHandler(std::make_shared<Cryptography::GpgMeReplacer>());
#  endif
#endif
    QModelIndex mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());
    QVERIFY(msgModel.rowCount(mappedMsg) > 0);

    QModelIndex data = mappedMsg.child(0, 0);
    QVERIFY(data.isValid());
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
    QCOMPARE(msgModel.rowCount(data), 0);
    QCOMPARE(data.data(Imap::Mailbox::RoleIsFetched).toBool(), false);

    cClientRegExp(t.mk("UID FETCH 333 \\((BODY\\.PEEK\\[(2|1|1\\.MIME)\\] ?){3}\\)"));
    cServer("* 1 FETCH (UID 333 BODY[2] {" + QByteArray::number(signature.size())
            + "}\r\n" + signature + " BODY[1] {" + QByteArray::number(plaintext.size()) + "}\r\n" + plaintext
            + " BODY[1.MIME] {28}\r\nContent-Type: text/plain\r\n\r\n"
            + ")\r\n"
            + t.last("OK fetched"));

    QSignalSpy qcaErrorSpy(&msgModel, SIGNAL(error(const QModelIndex &,QString,QString)));

    int i = 0;
    while (data.isValid() && data.data(Imap::Mailbox::RolePartCryptoNotFinishedYet).toBool() && qcaErrorSpy.empty() && i++ < 1000) {
        QTest::qWait(10);
    }
    // allow for event processing, so that the model can retrieve the results
    QCoreApplication::processEvents();

    if (!qcaErrorSpy.isEmpty() && successful) {
        qDebug() << "Unexpected failure in crypto";
        for (int i = 0; i < qcaErrorSpy.size(); ++i) {
            qDebug() << qcaErrorSpy[i][1].toString();
            qDebug() << qcaErrorSpy[i][2].toString();
        }
    }

    QCOMPARE(qcaErrorSpy.empty(), successful);
    QCOMPARE(data.data(Imap::Mailbox::RolePartCryptoTLDR).toString(), tldr);
    auto actualLongDesc = data.data(Imap::Mailbox::RolePartCryptoDetailedMessage).toString();
    if (!actualLongDesc.startsWith(longDesc)) {
        QCOMPARE(actualLongDesc, longDesc); // let's reuse this for debug output, and don't be scared about the misleading implications
    }
    QCOMPARE(data.data(Imap::Mailbox::RolePartSignatureVerifySupported).toBool(), successful);
    QCOMPARE(data.data(Imap::Mailbox::RolePartSignatureValidDisregardingTrust).toBool(), validDisregardingTrust);
    QCOMPARE(data.data(Imap::Mailbox::RolePartSignatureValidTrusted).toBool(), validCompletely);

    QVERIFY(data.data(Imap::Mailbox::RoleIsFetched).toBool() == successful);

    cEmpty();
    QVERIFY(errorSpy->empty());
#else
    QCOMPARE(msgModel.rowCount(data), 2);
    QCOMPARE(data.data(Imap::Mailbox::RoleIsFetched).toBool(), true);

    QCOMPARE(data.child(0, 0).data(Imap::Mailbox::RolePartMimeType).toString(), QLatin1String("application/pgp-encrypted"));
    QCOMPARE(data.child(1, 0).data(Imap::Mailbox::RolePartMimeType).toString(), QLatin1String("application/octet-stream"));
    cEmpty();

    QSKIP("Some tests were skipped because this build doesn't have GpgME++ support");
#endif
}

void CryptographyPGPTest::testVerification_data()
{
    QTest::addColumn<QByteArray>("signature");
    QTest::addColumn<QByteArray>("plaintext");
    QTest::addColumn<bool>("successful");
    QTest::addColumn<QString>("from");
    QTest::addColumn<QString>("tldr");
    QTest::addColumn<QString>("longDesc");
    QTest::addColumn<bool>("validDisregardingTrust");
    QTest::addColumn<bool>("validCompletely");

    // everything is correct
    QTest::newRow("valid-me")
            << sigFromMe
            << QByteArray("plaintext\r\n")
            << true
            << QStringLiteral("valid@test.trojita.flaska.net")
            << QStringLiteral("Verified signature")
            << QStringLiteral("Verified signature from Valid <valid@test.trojita.flaska.net> (")
            << true
            << true;

    // my signature, but a different identity
    QTest::newRow("valid-me")
            << sigFromMe
            << QByteArray("plaintext\r\n")
            << true
            << QStringLiteral("evil@example.org")
            << QStringLiteral("Signed by stranger")
            << QStringLiteral("Verified signature, but the signer is someone else:\nValid <valid@test.trojita.flaska.net> (")
            << true
            << false;

    // my signature, different data
    QTest::newRow("valid-me")
            << sigFromMe
            << QByteArray("I will pay you right now\r\n")
            << true
            << QStringLiteral("valid@test.trojita.flaska.net")
            << QStringLiteral("Bad signature")
            << QStringLiteral("Bad signature by Valid <valid@test.trojita.flaska.net> (")
            << false
            << false;
}

QTEST_GUILESS_MAIN(CryptographyPGPTest)
