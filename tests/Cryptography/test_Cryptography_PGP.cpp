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
    QFETCH(QString, from);
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
    cServer(helperCreateTrivialEnvelope(1, 333, QStringLiteral("subj"), from, bodystructure)
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

    cClientRegExp(t.mk("UID FETCH 333 \\((BODY\\.PEEK\\[2\\] BODY\\.PEEK\\[1\\]|BODY.PEEK\\[1\\] BODY\\.PEEK\\[2\\])\\)"));
    cServer("* 1 FETCH (UID 333 BODY[2] " + asLiteral(cyphertext) + " BODY[1] " + asLiteral("Version: 1\r\n") + ")\r\n"
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
    QTest::addColumn<QString>("from");
    QTest::addColumn<bool>("successful");

    // everything is correct
    QTest::newRow("valid")
            << bsEncrypted
            << encValid
            << QByteArray("plaintext")
            << QStringLiteral("valid@test.trojita.flaska.net")
            << true;

    // corrupted daya
    QTest::newRow("invalid")
            << bsEncrypted
            << encInvalid
            << QByteArray("plaintext")
            << QStringLiteral("valid@test.trojita.flaska.net")
            << false;

    // the key used for encryption is expired
    QTest::newRow("expiredKey")
            << bsEncrypted
            << encExpired
            << QByteArray("plaintext")
               // NOTE (jkt): This is how my QCA/2.1.0.3, GnuPG/2.0.28 behaves.
            << QStringLiteral("valid@test.trojita.flaska.net")
            << true;

    // we don't have any key which is needed for encryption
    QTest::newRow("unknownKey")
            << bsEncrypted
            << encUnknown
            << QByteArray("plaintext")
            << QStringLiteral("valid@test.trojita.flaska.net")
            << false;
}

/** @short What happens when ENVELOPE doesn't arrive at the time that parts are already there? */
void CryptographyPGPTest::testDecryptWithoutEnvelope()
{
#ifdef TROJITA_HAVE_CRYPTO_MESSAGES
    model->setProperty("trojita-imap-delayed-fetch-part", 0);

    helperSyncBNoMessages();
    cServer("* 1 EXISTS\r\n");
    cClient(t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    cServer("* 1 FETCH (UID 333 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(model->rowCount(msgListB), 1);
    QModelIndex msg = msgListB.child(0, 0);
    QVERIFY(msg.isValid());
    Cryptography::MessageModel msgModel(0, msg);
    msgModel.registerPartHandler(std::make_shared<Cryptography::GpgMeReplacer>());

    QCOMPARE(model->rowCount(msg), 0);
    cClient(t.mk("UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bsEncrypted + "))\r\n" + t.last("OK fetched\r\n"));
    // notice that the ENVELOPE never arrived
    cEmpty();
    QVERIFY(model->rowCount(msg) > 0);
    QModelIndex mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());
    QVERIFY(msgModel.rowCount(mappedMsg) > 0);

    QModelIndex data = mappedMsg.child(0, 0);
    QVERIFY(data.isValid());
    QCOMPARE(msgModel.rowCount(data), 0);
    QCOMPARE(data.data(Imap::Mailbox::RoleIsFetched).toBool(), false);

    cClientRegExp(t.mk("UID FETCH 333 \\((BODY\\.PEEK\\[2\\] BODY\\.PEEK\\[1\\]|BODY.PEEK\\[1\\] BODY\\.PEEK\\[2\\])\\)"));
    cServer("* 1 FETCH (UID 333 BODY[2] " + asLiteral(encValid) + " BODY[1] " + asLiteral("Version: 1\r\n") + ")\r\n"
            + t.last("OK fetched"));

    QSignalSpy qcaSuccessSpy(&msgModel, SIGNAL(rowsInserted(const QModelIndex &,int,int)));
    QSignalSpy qcaErrorSpy(&msgModel, SIGNAL(error(const QModelIndex &,QString,QString)));

    int i = 0;
    while (data.isValid() && data.data(Imap::Mailbox::RolePartCryptoNotFinishedYet).toBool() && i++ < 1000) {
        QTest::qWait(10);
    }
    // allow for event processing, so that the model can retrieve the results
    QCoreApplication::processEvents();

    QCOMPARE(data.data(Imap::Mailbox::RolePartCryptoNotFinishedYet).toBool(), false);
    QVERIFY(qcaSuccessSpy.isEmpty());
    QVERIFY(qcaErrorSpy.isEmpty());

    QVERIFY(!data.data(Imap::Mailbox::RoleIsFetched).toBool()); // because the ENVELOPE hasn't arrived yet

    cEmpty();
    QVERIFY(errorSpy->empty());
#else
    QSKIP("Cannot test without GpgME++ support");
#endif
}

void CryptographyPGPTest::testVerification()
{
    QFETCH(QByteArray, signature);
    QFETCH(QByteArray, ptMimeHdr);
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
    cServer(helperCreateTrivialEnvelope(1, 333, QStringLiteral("subj"), from, QStringLiteral(
            "(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 423 14 NIL NIL NIL NIL)"
            "(\"application\" \"pgp-signature\" NIL NIL NIL \"7bit\" 851 NIL NIL NIL NIL)"
            " \"signed\" (\"boundary\" \"=-=-=\" \"micalg\" \"pgp-sha256\" \"protocol\" \"application/pgp-signature\")"
            " NIL NIL NIL"))
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
    cServer("* 1 FETCH (UID 333 BODY[2] " + asLiteral(signature) + " BODY[1] " + asLiteral(plaintext)
            + " BODY[1.MIME] " + asLiteral(ptMimeHdr) + ")\r\n"
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
    QCOMPARE(data.data(Imap::Mailbox::RolePartMimeType).toByteArray(), QByteArray("multipart/signed"));
    QVERIFY(data.data(Imap::Mailbox::RoleIsFetched).toBool() == successful);

    auto partIdx = data.child(0, 0);
    QCOMPARE(partIdx.data(Imap::Mailbox::RolePartMimeType).toByteArray(), QByteArray("text/plain"));
    QCOMPARE(partIdx.data(Imap::Mailbox::RolePartUnicodeText).toString(), QString::fromUtf8(plaintext));

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
    QTest::addColumn<QByteArray>("ptMimeHdr");
    QTest::addColumn<QByteArray>("plaintext");
    QTest::addColumn<bool>("successful");
    QTest::addColumn<QString>("from");
    QTest::addColumn<QString>("tldr");
    QTest::addColumn<QString>("longDesc");
    QTest::addColumn<bool>("validDisregardingTrust");
    QTest::addColumn<bool>("validCompletely");

    QByteArray ptMimeHdr = QByteArrayLiteral("Content-Type: text/plain\r\n\r\n");

    // everything is correct
    QTest::newRow("valid-me")
            << sigFromMe
            << ptMimeHdr
            << QByteArray("plaintext\r\n")
            << true
            << QStringLiteral("valid@test.trojita.flaska.net")
            << QStringLiteral("Verified signature")
            << QStringLiteral("Verified signature from Valid <valid@test.trojita.flaska.net> (")
            << true
            << true;

    // my signature, but a different identity
    QTest::newRow("my-signature-different-identity")
            << sigFromMe
            << ptMimeHdr
            << QByteArray("plaintext\r\n")
            << true
            << QStringLiteral("evil@example.org")
            << QStringLiteral("Signed by stranger")
            << QStringLiteral("Verified signature, but the signer is someone else:\nValid <valid@test.trojita.flaska.net> (")
            << true
            << false;

    // my signature, different data
    QTest::newRow("my-signature-different-data")
            << sigFromMe
            << ptMimeHdr
            << QByteArray("I will pay you right now\r\n")
            << true
            << QStringLiteral("valid@test.trojita.flaska.net")
            << QStringLiteral("Bad signature")
            << QStringLiteral("Bad signature by Valid <valid@test.trojita.flaska.net> (")
            << false
            << false;

    // A missing Content-Type header (and also an invalid signature)
    QTest::newRow("invalid-implicit-content-type")
            << sigFromMe
            << QByteArrayLiteral("Content-Transfer-Encoding: 8bit\r\n\r\n")
            << QByteArray("plaintext\r\n")
            << true
            << QStringLiteral("valid@test.trojita.flaska.net")
            << QStringLiteral("Bad signature")
            << QStringLiteral("Bad signature by Valid <valid@test.trojita.flaska.net> (")
            << false
            << false;
}

void CryptographyPGPTest::testMalformed()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(int, rowCount);
    QFETCH(QString, tldr);
    QFETCH(QString, detail);

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
    cServer(helperCreateTrivialEnvelope(1, 333, QStringLiteral("subj"), QStringLiteral("foo@example.org"), QString::fromUtf8(bodystructure))
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
    QCoreApplication::processEvents();
    QCOMPARE(msgModel.rowCount(data), rowCount);
    QCOMPARE(data.data(Imap::Mailbox::RolePartCryptoTLDR).toString(), tldr);
    QCOMPARE(data.data(Imap::Mailbox::RolePartCryptoDetailedMessage).toString(), detail);

    cEmpty();
    QVERIFY(errorSpy->empty());
#else
    QCOMPARE(msgModel.rowCount(data), rowCount);
    QCOMPARE(data.data(Imap::Mailbox::RoleIsFetched).toBool(), true);

    cEmpty();

    QSKIP("Some tests were skipped because this build doesn't have GpgME++ support");
#endif
}

void CryptographyPGPTest::testMalformed_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<int>("rowCount");
    QTest::addColumn<QString>("tldr");
    QTest::addColumn<QString>("detail");

    // Due to the missing "protocol", the part replacer won't even react
    QTest::newRow("signed-missing-protocol-micalg")
            << bsMultipartSignedTextPlain
            << 2
            << QString()
            << QString();

    // A mailing list has stripped the signature
    QTest::newRow("signed-ml-stripped-gpg-signature")
            << QByteArray("(\"text\" \"plain\" (\"charset\" \"us-ascii\") NIL NIL \"7bit\" 423 14 NIL NIL NIL NIL)"
                          " \"signed\" (\"boundary\" \"=-=-=\" \"micalg\" \"pgp-sha256\" \"protocol\" \"application/pgp-signature\")"
                          " NIL NIL NIL")
            << 1
            << QStringLiteral("Malformed Signed Message")
            << QStringLiteral("Expected 2 parts, but found 1.");

}

QTEST_GUILESS_MAIN(CryptographyPGPTest)
