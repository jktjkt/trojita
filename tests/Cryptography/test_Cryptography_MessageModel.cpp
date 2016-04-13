/* Copyright (C) 2014 - 2015 Stephan Platz <trojita@paalsteek.de>

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
#include <QStandardItemModel>

#include "test_Cryptography_MessageModel.h"
#include "configure.cmake.h"
#include "Cryptography/MessageModel.h"
#include "Cryptography/MessagePart.h"
#ifdef TROJITA_HAVE_MIMETIC
#include "Cryptography/LocalMimeParser.h"
#endif
#include "Imap/data.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Streams/FakeSocket.h"

/** @short Verify passthrough of existing MIME parts without modifications */
void CryptographyMessageModelTest::testImapMessageParts()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QByteArray, partId);
    QFETCH(pathList, path);
    QFETCH(QByteArray, pathToPart);
    QFETCH(QByteArray, data);

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
    Cryptography::MessageModel msgModel(0, msg);
    cClient(t.mk("UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bodystructure + "))\r\n" + t.last("OK fetched\r\n"));
    cEmpty();
    QVERIFY(model->rowCount(msg) > 0);
    QModelIndex mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());
    QVERIFY(msgModel.rowCount(mappedMsg) > 0);

    QModelIndex mappedPart = mappedMsg;
    for (QList<QPair<int,int> >::const_iterator it = path.constBegin(), end = path.constEnd(); it != end; ++it) {
        mappedPart = mappedPart.child(it->first, it->second);
        QVERIFY(mappedPart.isValid());
    }
    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartData).toString(), QString());
    cClient(t.mk(QByteArray("UID FETCH 333 (BODY.PEEK[" + partId + "])\r\n")));
    cServer("* 1 FETCH (UID 333 BODY[" + partId + "] " + asLiteral(data) + ")\r\n" + t.last("OK fetched\r\n"));

    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartData).toByteArray(), data);
    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartPathToPart).toByteArray(), pathToPart);

    cEmpty();
    QVERIFY(errorSpy->isEmpty());
}

void CryptographyMessageModelTest::testImapMessageParts_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<QByteArray>("partId");
    QTest::addColumn<QList<QPair<int,int> > >("path");
    QTest::addColumn<QByteArray>("pathToPart");
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("plaintext-root")
            << bsPlaintext
            << QByteArray("1")
            << (pathList() << qMakePair(0,0))
            << QByteArray("/0")
            << QByteArray("blesmrt");

    QTest::newRow("torture-plaintext")
            << bsTortureTest
            << QByteArray("1")
            << (pathList() << qMakePair(0,0) << qMakePair(0,0))
            << QByteArray("/0/0")
            << QByteArray("plaintext");

    QTest::newRow("torture-richtext")
            << bsTortureTest
            << QByteArray("2.2.1")
            << (pathList() << qMakePair(0,0) << qMakePair(1,0)
                << qMakePair(0,0) << qMakePair(1,0) << qMakePair(0,0))
            << QByteArray("/0/1/0/1/0")
            << QByteArray("text richtext");
}

/** @short Verify building and retrieving of a custom MIME tree structure */
void CryptographyMessageModelTest::testCustomMessageParts()
{
    // Initialize model with a root item
    QStandardItemModel *minimal = new QStandardItemModel();
    QStandardItem *dummy_root = new QStandardItem();
    QStandardItem *root_mime = new QStandardItem(QStringLiteral("multipart/mixed"));
    dummy_root->appendRow(root_mime);
    minimal->invisibleRootItem()->appendRow(dummy_root);

    // Make sure we didn't mess up until here
    QVERIFY(minimal->index(0,0).child(0,0).isValid());
    QCOMPARE(minimal->index(0,0).child(0,0).data(), root_mime->data(Qt::DisplayRole));

    Cryptography::MessageModel msgModel(0, minimal->index(0,0));

    QModelIndex rootPartIndex = msgModel.index(0,0).child(0,0);

    QCOMPARE(rootPartIndex.data(), root_mime->data(Qt::DisplayRole));

    Cryptography::LocalMessagePart *localRoot = new Cryptography::LocalMessagePart(nullptr, 0, QByteArrayLiteral("multipart/mixed"));
    Cryptography::LocalMessagePart *localText = new Cryptography::LocalMessagePart(localRoot, 0, QByteArrayLiteral("text/plain"));
    localText->setData(QByteArrayLiteral("foobar"));
    localRoot->setChild(0, Cryptography::MessagePart::Ptr(localText));
    Cryptography::LocalMessagePart *localHtml = new Cryptography::LocalMessagePart(localRoot, 1, QByteArrayLiteral("text/html"));
    localHtml->setData(QByteArrayLiteral("<html>foobar</html>"));
    localRoot->setChild(1, Cryptography::MessagePart::Ptr(localHtml));

    msgModel.insertSubtree(rootPartIndex, Cryptography::MessagePart::Ptr(localRoot));

    QVERIFY(msgModel.rowCount(rootPartIndex) > 0);

    QModelIndex localRootIndex = rootPartIndex.child(0, 0);
    QVERIFY(localRootIndex.isValid());
    QCOMPARE(localRootIndex.data(Imap::Mailbox::RolePartMimeType), localRoot->data(Imap::Mailbox::RolePartMimeType));
    QModelIndex localTextIndex = localRootIndex.child(0, 0);
    QVERIFY(localTextIndex.isValid());
    QCOMPARE(localTextIndex.data(Imap::Mailbox::RolePartMimeType), localText->data(Imap::Mailbox::RolePartMimeType));
    QModelIndex localHtmlIndex = localRootIndex.child(1, 0);
    QVERIFY(localHtmlIndex.isValid());
    QCOMPARE(localHtmlIndex.data(Imap::Mailbox::RolePartMimeType), localHtml->data(Imap::Mailbox::RolePartMimeType));
}

/** @short Check adding a custom MIME tree structure to an existing message
 *
 *  This test fetches the structure of an IMAP message and adds some custom
 *  structure to that message. Adding random data as child of a text/plain
 *  MIME part does not make sense semantically but that's not what we want to
 *  test here.
 */
void CryptographyMessageModelTest::testMixedMessageParts()
{

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
    const QByteArray bsEncrypted("\"encrypted\" (\"protocol\" \"application/pgp-encrypted\" \"boundary\" \"trojita=_7cf0b2b6-64c6-41ad-b381-853caf492c54\") NIL NIL NIL");
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bsEncrypted + "))\r\n" + t.last("OK fetched\r\n"));
    cEmpty();
    QVERIFY(model->rowCount(msg) > 0);
    Cryptography::MessageModel msgModel(0, msg);
    QModelIndex mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());
    QVERIFY(msgModel.rowCount(mappedMsg) > 0);
    QCOMPARE(msgModel.parent(mappedMsg), QModelIndex());

    QModelIndex mappedPart = mappedMsg.child(0, 0);
    QVERIFY(mappedPart.isValid());
    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartPathToPart).toByteArray(), QByteArrayLiteral("/0"));

    cEmpty();
    QVERIFY(errorSpy->isEmpty());

    // Add some custom structure to the given IMAP message
    Cryptography::LocalMessagePart *localRoot = new Cryptography::LocalMessagePart(nullptr, 0, QByteArrayLiteral("multipart/mixed"));
    Cryptography::LocalMessagePart *localText = new Cryptography::LocalMessagePart(localRoot, 0, QByteArrayLiteral("text/plain"));
    localText->setData(QByteArrayLiteral("foobar"));
    localRoot->setChild(0, Cryptography::MessagePart::Ptr(localText));
    Cryptography::LocalMessagePart *localHtml = new Cryptography::LocalMessagePart(localRoot, 1, QByteArrayLiteral("text/html"));
    localHtml->setData(QByteArrayLiteral("<html>foobar</html>"));
    localRoot->setChild(1, Cryptography::MessagePart::Ptr(localHtml));

    msgModel.insertSubtree(mappedPart, Cryptography::MessagePart::Ptr(localRoot));

    QVERIFY(msgModel.rowCount(mappedPart) > 0);

    QModelIndex localRootIndex = mappedPart.child(0, 0);
    QVERIFY(localRootIndex.isValid());
    QCOMPARE(localRootIndex.data(Imap::Mailbox::RolePartMimeType), localRoot->data(Imap::Mailbox::RolePartMimeType));
    QModelIndex localTextIndex = localRootIndex.child(0, 0);
    QVERIFY(localTextIndex.isValid());
    QCOMPARE(localTextIndex.data(Imap::Mailbox::RolePartMimeType), localText->data(Imap::Mailbox::RolePartMimeType));
    QModelIndex localHtmlIndex = localRootIndex.child(1, 0);
    QVERIFY(localHtmlIndex.isValid());
    QCOMPARE(localHtmlIndex.data(Imap::Mailbox::RolePartMimeType), localHtml->data(Imap::Mailbox::RolePartMimeType));
}

/** @short Verify that we can handle data from Mimetic and use them locally */
void CryptographyMessageModelTest::testLocalMimeParsing()
{
#ifdef TROJITA_HAVE_MIMETIC
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

    Cryptography::MessageModel msgModel(0, msg);
    msgModel.registerPartHandler(std::make_shared<Cryptography::LocalMimeMessageParser>());

    const QByteArray bsTopLevelRfc822Message = QByteArrayLiteral(
                "\"messaGe\" \"rFc822\" NIL NIL NIL \"7bit\" 1511 (\"Thu, 8 Aug 2013 09:02:50 +0200\" "
                "\"Re: Your GSoC status\" ((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) "
                "((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) "
                "((\"Pali\" NIL \"pali.rohar\" \"gmail.com\")) ((\"Jan\" NIL \"jkt\" \"flaska.net\")) "
                "NIL NIL NIL \"<201308080902.51071@pali>\") "
                "((\"Text\" \"Plain\" (\"ChaRset\" \"uTf-8\") NIL NIL \"qUoted-printable\" 632 20 NIL NIL NIL NIL)"
                "(\"applicatioN\" \"pGp-signature\" (\"Name\" \"signature.asc\") NIL "
                "\"This is a digitally signed message part.\" \"7bit\" 205 NIL NIL NIL NIL) \"signed\" "
                "(\"boundary\" \"nextPart2106994.VznBGuL01i\" \"protocol\" \"application/pgp-signature\" \"micalg\" \"pgp-sha1\") "
                "NIL NIL NIL) 51 NIL NIL NIL NIL");

    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bsTopLevelRfc822Message + "))\r\n" + t.last("OK fetched\r\n"));
    cEmpty();
    QVERIFY(model->rowCount(msg) > 0);
    auto mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());
    QVERIFY(msgModel.rowCount(mappedMsg) > 0);

    QPersistentModelIndex msgRoot = mappedMsg.child(0, 0);
    QModelIndex formerMsgRoot = msgRoot;
    QVERIFY(msgRoot.isValid());
    QCOMPARE(msgRoot.data(Imap::Mailbox::RolePartPathToPart).toByteArray(),
             QByteArrayLiteral("/0"));

    QCOMPARE(msgRoot.data(Imap::Mailbox::RolePartMimeType).toByteArray(), QByteArrayLiteral("message/rfc822"));
    QCOMPARE(msgRoot.internalPointer(), formerMsgRoot.internalPointer());
    QCOMPARE(msgModel.rowCount(msgRoot), 0);
    cClientRegExp(t.mk("UID FETCH 333 \\(BODY\\.PEEK\\[1\\.(TEXT|HEADER)\\] BODY\\.PEEK\\[1\\.(TEXT|HEADER)\\]\\)"));
    QByteArray myHeader = QByteArrayLiteral("Content-Type: mULTIpart/miXed; boundary=sep\r\n"
                                            "MIME-Version: 1.0\r\n"
                                            "Subject: =?ISO-8859-2?B?7Lno+L794e3p?=\r\n"
                                            "From: =?utf-8?B?xJs=?= <1@example.org>\r\n"
                                            "Sender: =?utf-8?B?xJq=?= <0@example.org>\r\n"
                                            "To: =?utf-8?B?xJs=?= <2@example.org>, =?iso-8859-1?Q?=E1?= <3@example.org>\r\n"
                                            "Cc: =?utf-8?B?xJz=?= <4@example.org>\r\n"
                                            "reply-to: =?utf-8?B?xJm=?= <r@example.org>\r\n"
                                            "BCC: =?utf-8?B?xJr=?= <5@example.org>\r\n\r\n");
    QByteArray myBinaryBody = QByteArrayLiteral("This is the actual message body.\r\n");
    QString myUnicode = QStringLiteral("Λέσβος");
    QByteArray myBody = QByteArrayLiteral("preamble of a MIME message\r\n--sep\r\n"
                                          "Content-Type: text/plain; charset=\"utf-8\"\r\nContent-Transfer-Encoding: base64\r\n\r\n")
            + myUnicode.toUtf8().toBase64()
            + QByteArrayLiteral("\r\n--sep\r\nContent-Type: pWned/NOw\r\n\r\n")
            + myBinaryBody
            + QByteArrayLiteral("\r\n--sep--\r\n");
    cServer("* 1 FETCH (UID 333 BODY[1.TEXT] " + asLiteral(myBody) + " BODY[1.HEADER] " + asLiteral(myHeader) + ")\r\n"
            + t.last("OK fetched\r\n"));

    // the part got replaced, so our QModelIndex should be invalid now
    QVERIFY(msgRoot.internalPointer() != formerMsgRoot.internalPointer());

    QCOMPARE(msgModel.rowCount(msgRoot), 1);
    QCOMPARE(msgRoot.data(Imap::Mailbox::RolePartMimeType).toByteArray(), QByteArrayLiteral("message/rfc822"));
    QCOMPARE(msgRoot.data(Imap::Mailbox::RoleMessageSubject).toString(), QStringLiteral("ěščřžýáíé"));
    QCOMPARE(msgRoot.data(Imap::Mailbox::RoleMessageSender),
             QVariant(QVariantList() <<
                      (QStringList() << QStringLiteral("Ě") << QString() << QStringLiteral("0") << QStringLiteral("example.org"))));
    QCOMPARE(msgRoot.data(Imap::Mailbox::RoleMessageFrom),
             QVariant(QVariantList() <<
                      (QStringList() << QStringLiteral("ě") << QString() << QStringLiteral("1") << QStringLiteral("example.org"))));
    QCOMPARE(msgRoot.data(Imap::Mailbox::RoleMessageTo),
             QVariant(QVariantList() <<
                      (QStringList() << QStringLiteral("ě") << QString() << QStringLiteral("2") << QStringLiteral("example.org")) <<
                      (QStringList() << QStringLiteral("á") << QString() << QStringLiteral("3") << QStringLiteral("example.org"))));
    QCOMPARE(msgRoot.data(Imap::Mailbox::RoleMessageCc),
             QVariant(QVariantList() <<
                      (QStringList() << QStringLiteral("Ĝ") << QString() << QStringLiteral("4") << QStringLiteral("example.org"))));
    QCOMPARE(msgRoot.data(Imap::Mailbox::RoleMessageBcc),
             QVariant(QVariantList() <<
                      (QStringList() << QStringLiteral("Ě") << QString() << QStringLiteral("5") << QStringLiteral("example.org"))));
    QCOMPARE(msgRoot.data(Imap::Mailbox::RoleMessageReplyTo),
             QVariant(QVariantList() <<
                      (QStringList() << QStringLiteral("ę") << QString() << QStringLiteral("r") << QStringLiteral("example.org"))));

    // NOTE: the OFFSET_MIME parts are not implemented; that's more or less on purpose because they aren't being used through
    // the rest of the code so far.

    auto mHeader = msgRoot.child(0, Imap::Mailbox::TreeItem::OFFSET_HEADER);
    QVERIFY(mHeader.isValid());
    auto mText = msgRoot.child(0, Imap::Mailbox::TreeItem::OFFSET_TEXT);
    QVERIFY(mText.isValid());
    auto mMime = msgRoot.child(0, Imap::Mailbox::TreeItem::OFFSET_MIME);
    QVERIFY(!mMime.isValid());
    auto mRaw = msgRoot.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
    QVERIFY(mRaw.isValid());
    // We cannot compare them for an exact byte-equality because Mimetic apparently mangles the data a bit,
    // for example there's an extra space after the comma in the To field in this case :(
    QCOMPARE(mHeader.data(Imap::Mailbox::RolePartData).toByteArray().left(30), myHeader.left(30));
    QCOMPARE(mHeader.data(Imap::Mailbox::RolePartData).toByteArray().right(4), QByteArrayLiteral("\r\n\r\n"));
    QCOMPARE(mText.data(Imap::Mailbox::RolePartData).toByteArray(), myBody);

    // still that new C++11 toy, oh yeah :)
    using bodyFldParam_t = std::result_of<decltype(&Imap::Mailbox::TreeItemPart::bodyFldParam)(Imap::Mailbox::TreeItemPart)>::type;
    bodyFldParam_t expectedBodyFldParam;
    QCOMPARE(msgRoot.data(Imap::Mailbox::RolePartBodyFldParam).value<bodyFldParam_t>(), expectedBodyFldParam);

    auto multipartIdx = msgRoot.child(0, 0);
    QVERIFY(multipartIdx.isValid());
    QCOMPARE(multipartIdx.data(Imap::Mailbox::RolePartMimeType).toByteArray(), QByteArrayLiteral("multipart/mixed"));
    QCOMPARE(msgModel.rowCount(multipartIdx), 2);
    expectedBodyFldParam.clear();
    expectedBodyFldParam["BOUNDARY"] = "sep";
    QCOMPARE(multipartIdx.data(Imap::Mailbox::RolePartBodyFldParam).value<bodyFldParam_t>(), expectedBodyFldParam);

    auto c1 = multipartIdx.child(0, 0);
    QVERIFY(c1.isValid());
    QCOMPARE(msgModel.rowCount(c1), 0);
    QCOMPARE(c1.data(Imap::Mailbox::RolePartMimeType).toByteArray(), QByteArrayLiteral("text/plain"));
    QCOMPARE(c1.data(Imap::Mailbox::RolePartData).toString(), myUnicode);
    expectedBodyFldParam.clear();
    expectedBodyFldParam["CHARSET"] = "utf-8";
    QCOMPARE(c1.data(Imap::Mailbox::RolePartBodyFldParam).value<bodyFldParam_t>(), expectedBodyFldParam);

    auto c1raw = c1.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
    QVERIFY(c1raw.isValid());
    QCOMPARE(c1raw.data(Imap::Mailbox::RolePartData).toByteArray(), myUnicode.toUtf8().toBase64());
    QVERIFY(!c1.child(0, Imap::Mailbox::TreeItem::OFFSET_HEADER).isValid());
    QVERIFY(!c1.child(0, Imap::Mailbox::TreeItem::OFFSET_TEXT).isValid());
    QVERIFY(!c1.child(0, Imap::Mailbox::TreeItem::OFFSET_MIME).isValid());

    auto c2 = multipartIdx.child(1, 0);
    QVERIFY(c2.isValid());
    QCOMPARE(msgModel.rowCount(c2), 0);
    QCOMPARE(c2.data(Imap::Mailbox::RolePartMimeType).toByteArray(), QByteArrayLiteral("pwned/now"));
    QCOMPARE(c2.data(Imap::Mailbox::RolePartData).toByteArray(), myBinaryBody);

    auto c2raw = c2.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
    QVERIFY(c2raw.isValid());
    QCOMPARE(c2raw.data(Imap::Mailbox::RolePartData).toByteArray(), myBinaryBody);
    QVERIFY(!c2.child(0, Imap::Mailbox::TreeItem::OFFSET_HEADER).isValid());
    QVERIFY(!c2.child(0, Imap::Mailbox::TreeItem::OFFSET_TEXT).isValid());
    QVERIFY(!c2.child(0, Imap::Mailbox::TreeItem::OFFSET_MIME).isValid());

    cEmpty();
    QVERIFY(errorSpy->isEmpty());
#else
    QSKIP("Mimetic not available, cannot test LocalMimeMessageParser");
#endif
}

void CryptographyMessageModelTest::testDelayedLoading()
{
    model->setProperty("trojita-imap-delayed-fetch-part", 0);
    helperSyncBNoMessages();
    cServer("* 1 EXISTS\r\n");
    cClient(t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    cServer("* 1 FETCH (UID 333 FLAGS ())\r\n" + t.last("OK fetched\r\n"));
    QCOMPARE(model->rowCount(msgListB), 1);
    QModelIndex msg = msgListB.child(0, 0);
    QVERIFY(msg.isValid());

    Cryptography::MessageModel msgModel(0, msg);
    msgModel.setObjectName("msgModel");

    cEmpty();
    QCOMPARE(msgModel.rowCount(QModelIndex()), 0);
    cClient(t.mk("UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bsPlaintext + "))\r\n" + t.last("OK fetched\r\n"));
    cEmpty();
    QCOMPARE(model->rowCount(msg), 1);
    QCOMPARE(msgModel.rowCount(QModelIndex()), 1);
    auto mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());

    QPersistentModelIndex msgRoot = mappedMsg.child(0, 0);
    QVERIFY(msgRoot.isValid());
    QCOMPARE(msgRoot.data(Imap::Mailbox::RolePartPathToPart).toByteArray(),
             QByteArrayLiteral("/0"));
}

QTEST_GUILESS_MAIN(CryptographyMessageModelTest)
