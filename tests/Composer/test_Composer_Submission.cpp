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

#include <QtTest>
#include "test_Composer_Submission.h"
#include "Utils/headless_test.h"
#include "Utils/FakeCapabilitiesInjector.h"
#include "Composer/MessageComposer.h"
#include "Imap/Model/ItemRoles.h"
#include "Streams/FakeSocket.h"

ComposerSubmissionTest::ComposerSubmissionTest():
    m_submission(0), sendingSpy(0), sentSpy(0), requestedSendingSpy(0), requestedBurlSendingSpy(0),
    submissionSucceededSpy(0), submissionFailedSpy(0)
{
}

void ComposerSubmissionTest::init()
{
    LibMailboxSync::init();

    // There's a default delay of 50ms which made the cEmpty() and justKeepTask() ignore these pending actions...
    model->setProperty("trojita-imap-delayed-fetch-part", QVariant(0u));

    existsA = 5;
    uidValidityA = 333666;
    uidMapA << 10 << 11 << 12 << 13 << 14;
    uidNextA = 15;
    helperSyncAWithMessagesEmptyState();

    cServer("* 1 FETCH (BODYSTRUCTURE "
            "(\"text\" \"plain\" (\"charset\" \"UTF-8\" \"format\" \"flowed\") NIL NIL \"8bit\" 362 15 NIL NIL NIL)"
            " ENVELOPE (NIL \"subj\" NIL NIL NIL NIL NIL NIL NIL \"<msgid>\")"
            ")\r\n");

    m_msaFactory = new MSA::FakeFactory();
    m_submission = new Composer::Submission(this, model, m_msaFactory);

    sendingSpy = new QSignalSpy(m_msaFactory, SIGNAL(sending()));
    sentSpy = new QSignalSpy(m_msaFactory, SIGNAL(sent()));
    requestedSendingSpy = new QSignalSpy(m_msaFactory, SIGNAL(requestedSending(QByteArray,QList<QByteArray>,QByteArray)));
    requestedBurlSendingSpy = new QSignalSpy(m_msaFactory, SIGNAL(requestedBurlSending(QByteArray,QList<QByteArray>,QByteArray)));

    submissionSucceededSpy = new QSignalSpy(m_submission, SIGNAL(succeeded()));
    submissionFailedSpy = new QSignalSpy(m_submission, SIGNAL(failed(QString)));
}

void ComposerSubmissionTest::cleanup()
{
    LibMailboxSync::cleanup();

    delete m_submission;
    m_submission = 0;
    delete m_msaFactory;
    m_msaFactory = 0;
    delete sendingSpy;
    sendingSpy = 0;
    delete sentSpy;
    sentSpy = 0;
    delete requestedSendingSpy;
    requestedSendingSpy = 0;
    delete requestedBurlSendingSpy;
    requestedBurlSendingSpy = 0;
    delete submissionSucceededSpy;
    submissionSucceededSpy = 0;
    delete submissionFailedSpy;
    submissionFailedSpy = 0;
}

/** @short Test that we can send a very simple mail */
void ComposerSubmissionTest::testEmptySubmission()
{
    // Try sending an empty mail
    m_submission->send();

    QVERIFY(sendingSpy->isEmpty());
    QVERIFY(sentSpy->isEmpty());
    QCOMPARE(requestedSendingSpy->size(), 1);

    // This is a fake MSA implementation, so we have to confirm that sending has begun
    m_msaFactory->doEmitSending();
    QCOMPARE(sendingSpy->size(), 1);
    QCOMPARE(sentSpy->size(), 0);

    m_msaFactory->doEmitSent();

    QCOMPARE(sendingSpy->size(), 1);
    QCOMPARE(sentSpy->size(), 1);

    QCOMPARE(submissionSucceededSpy->size(), 1);
    QCOMPARE(submissionFailedSpy->size(), 0);
    cEmpty();
}

void ComposerSubmissionTest::testSimpleSubmission()
{
    m_submission->composer()->setFrom(
                Imap::Message::MailAddress(QLatin1String("Foo Bar"), QString(),
                                           QLatin1String("foo.bar"), QLatin1String("example.org")));
    m_submission->composer()->setSubject(QLatin1String("testing"));
    m_submission->composer()->setText(QLatin1String("Sample message"));

    m_submission->send();
    QCOMPARE(requestedSendingSpy->size(), 1);
    m_msaFactory->doEmitSending();
    QCOMPARE(sendingSpy->size(), 1);
    m_msaFactory->doEmitSent();
    QCOMPARE(sentSpy->size(), 1);

    QCOMPARE(submissionSucceededSpy->size(), 1);
    QCOMPARE(submissionFailedSpy->size(), 0);

    QVERIFY(requestedSendingSpy->size() == 1 &&
            requestedSendingSpy->at(0).size() == 3 &&
            requestedSendingSpy->at(0)[2].toByteArray().contains("Sample message"));
    cEmpty();

    //qDebug() << requestedSendingSpy->front();
}

void ComposerSubmissionTest::testSimpleSubmissionWithAppendFailed()
{
    helperTestSimpleAppend(false, false, false, false);
}


void ComposerSubmissionTest::testSimpleSubmissionWithAppendNoAppenduid()
{
    helperTestSimpleAppend(true, false, false, false);
}

void ComposerSubmissionTest::testSimpleSubmissionWithAppendAppenduid()
{
    helperTestSimpleAppend(true, true, false, false);
}

void ComposerSubmissionTest::testSimpleSubmissionReplyingToOk()
{
    helperTestSimpleAppend(true, true, true, true);
}

void ComposerSubmissionTest::testSimpleSubmissionReplyingToFailedFlags()
{
    helperTestSimpleAppend(true, true, true, false);
}

void ComposerSubmissionTest::helperSetupProperHeaders()
{
    m_submission->composer()->setFrom(
                Imap::Message::MailAddress(QLatin1String("Foo Bar"), QString(),
                                           QLatin1String("foo.bar"), QLatin1String("example.org")));
    m_submission->composer()->setSubject(QLatin1String("testing"));
    m_submission->composer()->setText(QLatin1String("Sample message"));
    m_submission->setImapOptions(true, QLatin1String("outgoing"), QLatin1String("somehost"), QLatin1String("userfred"), false);
}

void ComposerSubmissionTest::helperTestSimpleAppend(bool appendOk, bool appendUid,
                                                    bool shallUpdateReplyingTo, bool replyingToUpdateOk)
{
    // Don't bother with literal processing
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("LITERAL+"));

    helperSetupProperHeaders();

    if (shallUpdateReplyingTo) {
        QModelIndex msgA10 = model->index(0, 0, msgListA);
        QVERIFY(msgA10.isValid());
        QCOMPARE(msgA10.data(Imap::Mailbox::RoleMessageUid).toUInt(), uidMapA[0]);
        m_submission->composer()->setReplyingToMessage(msgA10);
    }
    m_submission->send();

    // We are waiting for APPEND to finish here
    QCOMPARE(requestedSendingSpy->size(), 0);

    for (int i=0; i<5; ++i)
        QCoreApplication::processEvents();
    QString sentSoFar = QString::fromUtf8(SOCK->writtenStuff());
    QString expected = t.mk("APPEND outgoing (\\Seen) ");
    QCOMPARE(sentSoFar.left(expected.size()), expected);
    cEmpty();
    QCOMPARE(requestedSendingSpy->size(), 0);

    if (!appendOk) {
        cServer(t.last("NO append failed\r\n"));
        cEmpty();

        QCOMPARE(submissionSucceededSpy->size(), 0);
        QCOMPARE(submissionFailedSpy->size(), 1);
        QCOMPARE(requestedSendingSpy->size(), 0);
        justKeepTask();
        return;
    }

    // Assume the APPEND has suceeded
    if (appendUid) {
        cServer(t.last("OK [APPENDUID 666333666 123] append done\r\n"));
    } else {
        cServer(t.last("OK append done\r\n"));
    }
    cEmpty();

    QCOMPARE(requestedSendingSpy->size(), 1);
    m_msaFactory->doEmitSending();
    QCOMPARE(sendingSpy->size(), 1);
    m_msaFactory->doEmitSent();
    QCOMPARE(sentSpy->size(), 1);

    QCOMPARE(submissionFailedSpy->size(), 0);
    if (shallUpdateReplyingTo) {
        QCOMPARE(submissionSucceededSpy->size(), 0);
        cClient(t.mk("UID STORE ") + QByteArray::number(uidMapA[0]) + " +FLAGS (\\Answered)\r\n");
        if (replyingToUpdateOk) {
            cServer(t.last("OK flags updated\r\n"));
        } else {
            cServer(t.last("NO you aren't going to update flags that easily\r\n"));
        }
        for (int i=0; i<5; ++i)
            QCoreApplication::processEvents();
    }
    QCOMPARE(submissionSucceededSpy->size(), 1);
    QCOMPARE(submissionFailedSpy->size(), 0);

    QVERIFY(requestedSendingSpy->size() == 1 &&
            requestedSendingSpy->at(0).size() == 3 &&
            requestedSendingSpy->at(0)[2].toByteArray().contains("Sample message"));
    cEmpty();

    //qDebug() << requestedSendingSpy->front();
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingFileAttachmentSmtpSave()
{
    helperMissingAttachment(true, false, false, true);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingFileAttachmentSmtpNoSave()
{
    helperMissingAttachment(false, false, false, true);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingFileAttachmentBurlSave()
{
    helperMissingAttachment(true, true, false, true);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingFileAttachmentBurlNoSave()
{
    helperMissingAttachment(false, true, false, true);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingFileAttachmentImap()
{
    helperMissingAttachment(true, false, true, true);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingImapAttachmentSmtpSave()
{
    helperMissingAttachment(true, false, false, false);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingImapAttachmentSmtpNoSave()
{
    helperMissingAttachment(false, false, false, false);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingImapAttachmentBurlSave()
{
    helperMissingAttachment(true, true, false, false);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingImapAttachmentBurlNoSave()
{
    helperMissingAttachment(false, true, false, false);
}

/** @short Check that a missing file attachment prevents submission */
void ComposerSubmissionTest::testMissingImapAttachmentImap()
{
    helperMissingAttachment(true, false, true, false);
}

void ComposerSubmissionTest::helperMissingAttachment(bool save, bool burl, bool imap, bool attachingFile)
{
#ifdef Q_OS_OS2
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QSKIP("Looks like QTemporaryFile is broken on OS/2");
#else
    QSKIP("Looks like QTemporaryFile is broken on OS/2", SkipSingle);
#endif
#endif
    helperSetupProperHeaders();

    if (imap) {
        Q_ASSERT(save);
    }

    QPersistentModelIndex msgA10 = model->index(0, 0, msgListA);
    QVERIFY(msgA10.isValid());
    QCOMPARE(msgA10.data(Imap::Mailbox::RoleMessageUid).toUInt(), uidMapA[0]);

    m_submission->setImapOptions(save, QLatin1String("meh"), QLatin1String("pwn"), QLatin1String("bob"), imap);
    m_submission->setSmtpOptions(burl, QLatin1String("pwn"));
    m_submission->composer()->setReplyingToMessage(msgA10);
    m_msaFactory->setBurlSupport(burl);
    m_msaFactory->setImapSupport(imap);

    if (attachingFile) {
        // needs a special block for proper RAII-based removal
        QTemporaryFile tempFile;
        tempFile.open();
        tempFile.write("Sample attachment for Trojita's ComposerSubmissionTest\r\n");
        QCOMPARE(m_submission->composer()->addFileAttachment(tempFile.fileName()), true);
        // The file gets deleted as soon as we leave this scope
    } else {
        // Attaching something which lives on the IMAP server

        // Make sure the IMAP bits are ready
        QCOMPARE(model->rowCount(msgA10), 1);
        QPersistentModelIndex partData = model->index(0, 0, msgA10);
        QVERIFY(partData.isValid());

        helperAttachImapPart(uidMapA[0]);
        cClient(t.mk("UID FETCH ") + QByteArray::number(uidMapA[0]) + " (BODY.PEEK[1])\r\n");
    }

    m_submission->send();

    if (!attachingFile) {
        // Deliver the queued data to make the generic test prologue happy
        cServer("* 1 FETCH (BODY[1] \"contents\")\r\n");
        cServer(t.last("OK fetched\r\n"));
    }
    QCOMPARE(requestedSendingSpy->size(), 0);
    QCOMPARE(submissionSucceededSpy->size(), 0);
    QCOMPARE(submissionFailedSpy->size(), 1);
    cEmpty();
    justKeepTask();
}

void ComposerSubmissionTest::helperAttachImapPart(const uint uid)
{
    QScopedPointer<QMimeData> mimeData(new QMimeData());
    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_6);
    stream << QString::fromUtf8("a") << uidValidityA << uid << QString::fromUtf8("0");
    mimeData->setData(QLatin1String("application/x-trojita-imap-part"), encodedData);
    QCOMPARE(m_submission->composer()->dropMimeData(mimeData.data(), Qt::CopyAction, 0, 0, QModelIndex()), true);
}

#define EXTRACT_TARILING_NUMBER(NUM) \
{ \
    QCOMPARE(sentSoFar.left(expected.size()), expected); \
    bool numberOk = false; \
    QString numericPart = sentSoFar.mid(expected.size()); \
    QCOMPARE(numericPart.right(3), QString::fromUtf8("}\r\n")); \
    int num = numericPart.left(numericPart.size() - 3).toInt(&numberOk); \
    QVERIFY(numberOk); \
    NUM = num; \
}

#define EAT_OCTETS(NUM, OUT) \
{ \
    for (int i=0; i<5; ++i) \
        QCoreApplication::processEvents(); \
    QString buf = QString::fromUtf8(SOCK->writtenStuff()); \
    QVERIFY(buf.size() > NUM); \
    OUT = buf.mid(NUM); \
}



void ComposerSubmissionTest::testBurlSubmission()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("CATENATE"));
    injector.injectCapability(QLatin1String("URLAUTH"));
    helperSetupProperHeaders();
    m_submission->setImapOptions(true, QLatin1String("meh"), QLatin1String("host"), QLatin1String("usr"), false);
    m_submission->setSmtpOptions(true, QLatin1String("smtpUser"));
    m_msaFactory->setBurlSupport(true);

    helperAttachImapPart(uidMapA[0]);
    m_submission->send();

    for (int i=0; i<5; ++i)
        QCoreApplication::processEvents();
    QString sentSoFar = QString::fromUtf8(SOCK->writtenStuff());
    QString expected = t.mk("APPEND meh (\\Seen) CATENATE (TEXT {");
    int octets;
    EXTRACT_TARILING_NUMBER(octets);
    cServer("+ carry on\r\n");
    EAT_OCTETS(octets, sentSoFar);
    expected = QLatin1String(" URL \"/a;UIDVALIDITY=333666/;UID=10/;SECTION=1\" TEXT {");
    EXTRACT_TARILING_NUMBER(octets);
    cServer("+ carry on\r\n");
    EAT_OCTETS(octets, sentSoFar);
    QCOMPARE(sentSoFar, QString::fromUtf8(")\r\n"));
    cServer(t.last("OK [APPENDUID 666333666 123] append done\r\n"));
    cClient(t.mk("GENURLAUTH \"imap://usr@host/meh;UIDVALIDITY=666333666/;UID=123;urlauth=submit+smtpUser\" INTERNAL\r\n"));
    // This is an actual example from RFC 4467
    QString genAuthUrl = "imap://joe@example.com/INBOX/;uid=20/;section=1.2;urlauth=submit+fred:internal:91354a473744909de610943775f92038";
    cServer("* GENURLAUTH \"" + genAuthUrl.toUtf8() + "\"\r\n");
    cServer(t.last("OK genurlauth\r\n"));
    cEmpty();
    QCOMPARE(requestedSendingSpy->size(), 0);
    QCOMPARE(requestedBurlSendingSpy->size(), 1);
    m_msaFactory->doEmitSending();
    QCOMPARE(sendingSpy->size(), 1);
    m_msaFactory->doEmitSent();
    QCOMPARE(sentSpy->size(), 1);

    QCOMPARE(submissionSucceededSpy->size(), 1);
    QCOMPARE(submissionFailedSpy->size(), 0);

    QCOMPARE(requestedBurlSendingSpy->size(), 1);
    QCOMPARE(requestedBurlSendingSpy->at(0).size(), 3);
    QCOMPARE(requestedBurlSendingSpy->at(0)[2].toString(), genAuthUrl);
    cEmpty();
    justKeepTask();
}

/** @short Chech that CATENATE is used and BURL disabled when the IMAP server cannot do URLAUTH */
void ComposerSubmissionTest::testCatenateBurlWithoutUrlauth()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("CATENATE"));
    // The URLAUTH is, however, not advertised by the IMAP server
    helperSetupProperHeaders();
    m_submission->setImapOptions(true, QLatin1String("meh"), QLatin1String("host"), QLatin1String("usr"), false);
    m_submission->setSmtpOptions(true, QLatin1String("smtpUser"));
    m_msaFactory->setBurlSupport(true);

    helperAttachImapPart(uidMapA[0]);
    cClient(t.mk("UID FETCH ") + QByteArray::number(uidMapA[0]) + " (BODY.PEEK[1])\r\n");
    cServer("* 1 FETCH (BODY[1] \"contents fetched over IMAP\")\r\n");
    cServer(t.last("OK fetched\r\n"));

    m_submission->send();

    for (int i=0; i<5; ++i)
        QCoreApplication::processEvents();
    QString sentSoFar = QString::fromUtf8(SOCK->writtenStuff());
    QString expected = t.mk("APPEND meh (\\Seen) CATENATE (TEXT {");
    int octets;
    EXTRACT_TARILING_NUMBER(octets);
    cServer("+ carry on\r\n");
    EAT_OCTETS(octets, sentSoFar);
    expected = QLatin1String(" URL \"/a;UIDVALIDITY=333666/;UID=10/;SECTION=1\" TEXT {");
    EXTRACT_TARILING_NUMBER(octets);
    cServer("+ carry on\r\n");
    EAT_OCTETS(octets, sentSoFar);
    QCOMPARE(sentSoFar, QString::fromUtf8(")\r\n"));
    cServer(t.last("OK [APPENDUID 666333666 123] append done\r\n"));
    cEmpty();
    QCOMPARE(requestedSendingSpy->size(), 1);
    QCOMPARE(requestedBurlSendingSpy->size(), 0);
    m_msaFactory->doEmitSending();
    QCOMPARE(sendingSpy->size(), 1);
    m_msaFactory->doEmitSent();
    QCOMPARE(sentSpy->size(), 1);

    QCOMPARE(submissionSucceededSpy->size(), 1);
    QCOMPARE(submissionFailedSpy->size(), 0);

    QCOMPARE(requestedBurlSendingSpy->size(), 0);
    QCOMPARE(requestedSendingSpy->size(), 1);
    QCOMPARE(requestedSendingSpy->at(0).size(), 3);
    QByteArray outgoingMessage = requestedSendingSpy->at(0)[2].toByteArray();
    QVERIFY(outgoingMessage.contains("Subject: testing\r\n"));
    QVERIFY(outgoingMessage.contains("Sample message\r\n"));
    QVERIFY(outgoingMessage.contains(QByteArray("contents fetched over IMAP").toBase64()));
    cEmpty();
    justKeepTask();
}

/** @short Make sure that failed mail delivery prevents marking the original as answered, etc */
void ComposerSubmissionTest::testFailedMsa()
{
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("LITERAL+"));

    helperSetupProperHeaders();

    QModelIndex msgA10 = model->index(0, 0, msgListA);
    QVERIFY(msgA10.isValid());
    QCOMPARE(msgA10.data(Imap::Mailbox::RoleMessageUid).toUInt(), uidMapA[0]);
    m_submission->composer()->setReplyingToMessage(msgA10);
    m_submission->send();

    // We are waiting for APPEND to finish here
    QCOMPARE(requestedSendingSpy->size(), 0);

    for (int i=0; i<5; ++i)
        QCoreApplication::processEvents();
    QString sentSoFar = QString::fromUtf8(SOCK->writtenStuff());
    QString expected = t.mk("APPEND outgoing (\\Seen) ");
    QCOMPARE(sentSoFar.left(expected.size()), expected);
    cEmpty();
    QCOMPARE(requestedSendingSpy->size(), 0);

    // Assume the APPEND has suceeded
    cServer(t.last("OK [APPENDUID 666333666 123] append done\r\n"));
    cEmpty();

    QCOMPARE(requestedSendingSpy->size(), 1);
    m_msaFactory->doEmitSending();
    QCOMPARE(sendingSpy->size(), 1);
    m_msaFactory->doEmitError(QLatin1String("error"));
    QCOMPARE(sentSpy->size(), 0);

    QCOMPARE(submissionFailedSpy->size(), 1);
    QCOMPARE(submissionSucceededSpy->size(), 0);

    QVERIFY(requestedSendingSpy->size() == 1 &&
            requestedSendingSpy->at(0).size() == 3 &&
            requestedSendingSpy->at(0)[2].toByteArray().contains("Sample message"));
    cEmpty();

}

/** @short Test the Parser that a missing continuation request effectively cancels out the processing of the current command

We're testing it on this level because it is easy to trigger sending a literal this way.
*/
void ComposerSubmissionTest::testNoImapContinuation()
{
    helperSetupProperHeaders();
    m_submission->send();
    QCOMPARE(requestedSendingSpy->size(), 0);

    // Also queue a couple of another IMAP commands
    model->switchToMailbox(idxB);
    model->switchToMailbox(idxC);

    // A command sending the literal string will be rejected.
    // The number of octets is platform-dependent, so we cannot just use cClient() here.
    for (int i=0; i<5; ++i)
        QCoreApplication::processEvents();
    QString sentSoFar = QString::fromUtf8(SOCK->writtenStuff());
    QString expected = t.mk("APPEND outgoing (\\Seen) {");
    int octets;
    EXTRACT_TARILING_NUMBER(octets);
    Q_UNUSED(octets);
    cEmpty();
    cServer(t.last("NO rejected\r\n"));
    // The Parser shall detect this and proceed towards sending other IMAP commands
    cClient(t.mk("SELECT b\r\n"));
    cServer("* 0 exists\r\n" + t.last("OK selected\r\n"));
    cClient(t.mk("SELECT c\r\n"));
    cServer("* 0 exists\r\n" + t.last("OK selected\r\n"));
    cEmpty();
    QCOMPARE(submissionFailedSpy->size(), 1);
    QCOMPARE(submissionSucceededSpy->size(), 0);
    justKeepTask();
}

/** @short Make sure that the original message is marked as replied to when replying */
void ComposerSubmissionTest::testReplyingNormal()
{
    helperSetupProperHeaders();
    m_submission->setImapOptions(false, QString(), QString(), QString(), false);
    QModelIndex origMessage = msgListA.child(0, 0);
    QVERIFY(origMessage.isValid());
    QCOMPARE(origMessage.data(Imap::Mailbox::RoleMessageUid).toInt(), 10);
    m_submission->composer()->setReplyingToMessage(origMessage);

    m_submission->send();
    cEmpty();

    QCOMPARE(requestedSendingSpy->size(), 1);
    m_msaFactory->doEmitSending();
    QCOMPARE(sendingSpy->size(), 1);
    m_msaFactory->doEmitSent();
    QCOMPARE(sentSpy->size(), 1);
    cClient(t.mk("UID STORE 10 +FLAGS (\\Answered)\r\n"));
    cServer("* 1 FETCH (UID 10 FLAGS (\\Answered))\r\n"
            + t.last("OK stored\r\n"));
    cEmpty();

    QCOMPARE(submissionSucceededSpy->size(), 1);
    QCOMPARE(submissionFailedSpy->size(), 0);

    QVERIFY(requestedSendingSpy->size() == 1 &&
            requestedSendingSpy->at(0).size() == 3 &&
            requestedSendingSpy->at(0)[2].toByteArray().contains("Sample message"));
    cEmpty();
}

/** @short Make sure that replying to a removed message works reasonably well */
void ComposerSubmissionTest::testReplyingToRemoved()
{
    helperSetupProperHeaders();
    m_submission->setImapOptions(false, QString(), QString(), QString(), false);
    QModelIndex origMessage = msgListA.child(0, 0);
    QVERIFY(origMessage.isValid());
    QCOMPARE(origMessage.data(Imap::Mailbox::RoleMessageUid).toInt(), 10);
    m_submission->composer()->setReplyingToMessage(origMessage);
    cServer("* 1 EXPUNGE\r\n");
    QVERIFY(!m_submission->composer()->replyingToMessage().isValid());

    m_submission->send();
    cEmpty();

    QCOMPARE(requestedSendingSpy->size(), 1);
    m_msaFactory->doEmitSending();
    QCOMPARE(sendingSpy->size(), 1);
    m_msaFactory->doEmitSent();
    QCOMPARE(sentSpy->size(), 1);
    cEmpty();

    QCOMPARE(submissionSucceededSpy->size(), 1);
    QCOMPARE(submissionFailedSpy->size(), 0);

    QVERIFY(requestedSendingSpy->size() == 1 &&
            requestedSendingSpy->at(0).size() == 3 &&
            requestedSendingSpy->at(0)[2].toByteArray().contains("Sample message"));
    cEmpty();
}

TROJITA_HEADLESS_TEST(ComposerSubmissionTest)
