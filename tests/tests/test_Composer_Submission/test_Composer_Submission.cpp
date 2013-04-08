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

#include <QtTest>
#include "test_Composer_Submission.h"
#include "../headless_test.h"
#include "test_LibMailboxSync/FakeCapabilitiesInjector.h"
#include "Composer/MessageComposer.h"
#include "Imap/Model/ItemRoles.h"
#include "Streams/FakeSocket.h"

// Already defined in ItemRoles.h!
//Q_DECLARE_METATYPE(QList<QByteArray>)

ComposerSubmissionTest::ComposerSubmissionTest():
    m_submission(0), sendingSpy(0), sentSpy(0), requestedSendingSpy(0)
{
    qRegisterMetaType<QList<QByteArray> >();
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
    helperTestSimpleAppend(false, false);
}


void ComposerSubmissionTest::testSimpleSubmissionWithAppendNoAppenduid()
{
    helperTestSimpleAppend(true, false);
}

void ComposerSubmissionTest::testSimpleSubmissionWithAppendAppenduid()
{
    helperTestSimpleAppend(true, true);
}

void ComposerSubmissionTest::helperSetupProperHeaders()
{
    m_submission->composer()->setFrom(
                Imap::Message::MailAddress(QLatin1String("Foo Bar"), QString(),
                                           QLatin1String("foo.bar"), QLatin1String("example.org")));
    m_submission->composer()->setSubject(QLatin1String("testing"));
    m_submission->composer()->setText(QLatin1String("Sample message"));
    m_submission->setImapOptions(true, QLatin1String("outgoing"), QLatin1String("somehost"), false);
}

void ComposerSubmissionTest::helperTestSimpleAppend(bool appendOk, bool appendUid)
{
    // Don't bother with literal processing
    FakeCapabilitiesInjector injector(model);
    injector.injectCapability(QLatin1String("LITERAL+"));

    helperSetupProperHeaders();
    m_submission->send();

    // We are waiting for APPEND to finish here
    QCOMPARE(requestedSendingSpy->size(), 0);

    for (int i=0; i<5; ++i)
        QCoreApplication::processEvents();
    QString sentSoFar = QString::fromUtf8(SOCK->writtenStuff());
    QString expected = t.mk("APPEND outgoing ($SubmitPending \\Seen) ");
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
    helperSetupProperHeaders();

    if (imap) {
        Q_ASSERT(save);
    }

    m_submission->setImapOptions(save, QLatin1String("meh"), QLatin1String("pwn"), imap);
    m_submission->setSmtpOptions(burl, QLatin1String("pwn"));
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
        QPersistentModelIndex msgA10 = model->index(0, 0, msgListA);
        QVERIFY(msgA10.isValid());
        QCOMPARE(msgA10.data(Imap::Mailbox::RoleMessageUid).toUInt(), uidMapA[0]);
        QCOMPARE(model->rowCount(msgA10), 1);
        QPersistentModelIndex partData = model->index(0, 0, msgA10);
        QVERIFY(partData.isValid());

        QScopedPointer<QMimeData> mimeData(new QMimeData());
        QByteArray encodedData;
        QDataStream stream(&encodedData, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_4_6);
        stream << QString::fromUtf8("a") << uidValidityA << uidMapA[0] << QString::fromUtf8("1") << QString::fromUtf8("0");
        mimeData->setData(QLatin1String("application/x-trojita-imap-part"), encodedData);
        QCOMPARE(m_submission->composer()->dropMimeData(mimeData.data(), Qt::CopyAction, 0, 0, QModelIndex()), true);
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

TROJITA_HEADLESS_TEST(ComposerSubmissionTest)
