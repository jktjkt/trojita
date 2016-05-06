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
#include <QAction>
#include <QTextDocument>
#include <QWebFrame>
#include <QWebView>
#include "test_Composer_responses.h"
#include "Composer/Mailto.h"
#include "Composer/QuoteText.h"
#include "Composer/Recipients.h"
#include "Composer/ReplaceSignature.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Composer/SubjectMangling.h"
#include "Imap/Encoders.h"

typedef QList<QPair<Composer::RecipientKind,QString>> RecipientsType;

Q_DECLARE_METATYPE(Composer::RecipientList)
Q_DECLARE_METATYPE(RecipientsType)
Q_DECLARE_METATYPE(QList<QUrl>)
Q_DECLARE_METATYPE(QList<QByteArray>)

QString recipientKindtoString(const Composer::RecipientKind kind)
{
    switch (kind) {
    case Composer::ADDRESS_BCC:
        return QStringLiteral("Bcc:");
    case Composer::ADDRESS_CC:
        return QStringLiteral("Cc:");
    case Composer::ADDRESS_FROM:
        return QStringLiteral("From:");
    case Composer::ADDRESS_REPLY_TO:
        return QStringLiteral("Reply-To:");
    case Composer::ADDRESS_SENDER:
        return QStringLiteral("Sender:");
    case Composer::ADDRESS_TO:
        return QStringLiteral("To:");
    }
    Q_ASSERT(false);
    return QString();
}

namespace QTest {

    template <>
    char *toString(const Composer::RecipientList &list)
    {
        QString buf;
        QDebug d(&buf);
        Q_FOREACH(const Composer::RecipientList::value_type &item, list) {
            d << recipientKindtoString(item.first).toUtf8().constData() << item.second.asSMTPMailbox().constData() << ",";
        }
        return qstrdup(buf.toUtf8().constData());
    }

    template <>
    char *toString(const Composer::RecipientList::value_type &item)
    {
        return qstrdup(QByteArray(recipientKindtoString(item.first).toUtf8() + ' ' + item.second.asSMTPMailbox()).constData());
    }
}

/** @short Test that subjects remain sane in replied messages */
void ComposerResponsesTest::testReplySubjectMangling()
{
    QFETCH(QString, original);
    QFETCH(QString, replied);

    QCOMPARE(Composer::Util::replySubject(original), replied);
}

/** @short Data for testReplySubjectMangling */
void ComposerResponsesTest::testReplySubjectMangling_data()
{
    QTest::addColumn<QString>("original");
    QTest::addColumn<QString>("replied");

    QTest::newRow("no-subject") << QString() << QStringLiteral("Re: ");
    QTest::newRow("simple") << QStringLiteral("ahoj") << QStringLiteral("Re: ahoj");
    QTest::newRow("already-replied") << QStringLiteral("Re: ahoj") << QStringLiteral("Re: ahoj");
    QTest::newRow("already-replied-no-space") << QStringLiteral("re:ahoj") << QStringLiteral("Re: ahoj");
    QTest::newRow("already-replied-case") << QStringLiteral("RE: ahoj") << QStringLiteral("Re: ahoj");
    QTest::newRow("multiple-re") << QStringLiteral("Re:RE: re: ahoj") << QStringLiteral("Re: ahoj");
    QTest::newRow("trailing-re") << QStringLiteral("ahoj re:") << QStringLiteral("Re: ahoj re:");
    QTest::newRow("leading-trailing-re") << QStringLiteral("re: ahoj re:") << QStringLiteral("Re: ahoj re:");

    // mailing list stuff
    QTest::newRow("ml-empty") << QStringLiteral("[foo]") << QStringLiteral("Re: [foo]");
    QTest::newRow("ml-simple") << QStringLiteral("[foo] bar") << QStringLiteral("Re: [foo] bar");
    QTest::newRow("ml-simple-no-space") << QStringLiteral("[foo]bar") << QStringLiteral("Re: [foo] bar");
    QTest::newRow("ml-broken") << QStringLiteral("[foo bar") << QStringLiteral("Re: [foo bar");
    QTest::newRow("ml-two-words") << QStringLiteral("[foo bar] x") << QStringLiteral("Re: [foo bar] x");
    QTest::newRow("ml-re-empty") << QStringLiteral("[foo] Re:") << QStringLiteral("Re: [foo]");
    QTest::newRow("re-ml-re-empty") << QStringLiteral("Re: [foo] Re:") << QStringLiteral("Re: [foo]");
    QTest::newRow("re-ml-re-empty-no-spaces") << QStringLiteral("Re:[foo]Re:") << QStringLiteral("Re: [foo]");
    QTest::newRow("ml-ml") << QStringLiteral("[foo] [bar] blah") << QStringLiteral("Re: [foo] [bar] blah");
    QTest::newRow("ml-ml-re") << QStringLiteral("[foo] [bar] Re: blah") << QStringLiteral("Re: [foo] [bar] blah");
    QTest::newRow("ml-re-ml") << QStringLiteral("[foo] Re: [bar] blah") << QStringLiteral("Re: [foo] [bar] blah");
    QTest::newRow("ml-re-ml-re") << QStringLiteral("[foo] Re: [bar] Re: blah") << QStringLiteral("Re: [foo] [bar] blah");
    QTest::newRow("re-ml-ml") << QStringLiteral("Re: [foo] [bar] blah") << QStringLiteral("Re: [foo] [bar] blah");
    QTest::newRow("re-ml-ml-re") << QStringLiteral("Re: [foo] [bar] Re: blah") << QStringLiteral("Re: [foo] [bar] blah");
    QTest::newRow("re-ml-re-ml") << QStringLiteral("Re: [foo] Re: [bar] blah") << QStringLiteral("Re: [foo] [bar] blah");
    QTest::newRow("re-ml-re-ml-re") << QStringLiteral("Re: [foo] Re: [bar] Re: blah") << QStringLiteral("Re: [foo] [bar] blah");

    // test removing duplicate items
    QTest::newRow("M-M") << QStringLiteral("[foo] [foo] blah") << QStringLiteral("Re: [foo] blah");
    QTest::newRow("M-M-re") << QStringLiteral("[foo] [foo] Re: blah") << QStringLiteral("Re: [foo] blah");
    QTest::newRow("M-M-re-re") << QStringLiteral("[foo] [foo] Re: Re: blah") << QStringLiteral("Re: [foo] blah");
    QTest::newRow("M-re-M") << QStringLiteral("[foo] Re: [foo] blah") << QStringLiteral("Re: [foo] blah");
    QTest::newRow("re-M-re-M") << QStringLiteral("Re: [foo] Re: [foo] blah") << QStringLiteral("Re: [foo] blah");
    QTest::newRow("re-M-re-M-re") << QStringLiteral("Re: [foo] Re: [foo] Re: blah") << QStringLiteral("Re: [foo] blah");

    // stuff which should not be subject to subject sanitization
    QTest::newRow("brackets-end") << QStringLiteral("blesmrt [test]") << QStringLiteral("Re: blesmrt [test]");
    QTest::newRow("re-brackets-end") << QStringLiteral("Re: blesmrt [test]") << QStringLiteral("Re: blesmrt [test]");
    QTest::newRow("re-brackets-re-end") << QStringLiteral("Re: blesmrt Re: [test]") << QStringLiteral("Re: blesmrt Re: [test]");
    QTest::newRow("brackets-re-end") << QStringLiteral("blesmrt Re: [test]") << QStringLiteral("Re: blesmrt Re: [test]");

    // real-world bugs
    QTest::newRow("extra-space-in-0.3.92")
        << QStringLiteral("[imapext]  Re: Proposal for a new IMAP Working Group to revise CONDSTORE & QRESYNC")
        << QStringLiteral("Re: [imapext] Proposal for a new IMAP Working Group to revise CONDSTORE & QRESYNC");

    // presence of prefixes added with forwarded mails
    QTest::newRow("single-fwd") << QStringLiteral("Fwd: blah") << QStringLiteral("Re: Fwd: blah");
    QTest::newRow("multiple-fwd") << QStringLiteral("FW: Fwd: fwd: blah") << QStringLiteral("Re: FW: Fwd: fwd: blah");
    QTest::newRow("interleaved-re-fwd") << QStringLiteral("Fwd: Re: FW: RE: blah") << QStringLiteral("Re: Fwd: Re: FW: RE: blah");
    QTest::newRow("multiple-re-nospace-fwd") << QStringLiteral("Re:RE:re:Fwd: blah") << QStringLiteral("Re: Fwd: blah");
    QTest::newRow("old-TB-style-fwd") << QStringLiteral("[Fwd: blah]") << QStringLiteral("Re: [Fwd: blah]");
    QTest::newRow("old-TB-style-fwd-ml") << QStringLiteral("[Fwd: [foo] blah]") << QStringLiteral("Re: [Fwd: [foo] blah]");
    QTest::newRow("old-TB-style-fwd-re-ml") << QStringLiteral("[Fwd: Re: [foo] blah]") << QStringLiteral("Re: [Fwd: Re: [foo] blah]");
    QTest::newRow("old-TB-style-fwd-re") << QStringLiteral("Re: [Fwd: Re: blah]") << QStringLiteral("Re: [Fwd: Re: blah]");
    QTest::newRow("fwd-ml") << QStringLiteral("Fwd: [foo]") << QStringLiteral("Re: Fwd: [foo]");
    QTest::newRow("fwd-ml-re") << QStringLiteral("Fwd: [foo] Re: blah") << QStringLiteral("Re: Fwd: [foo] Re: blah");
    QTest::newRow("re-fwd-ml") << QStringLiteral("Re: Fwd: [foo] blah") << QStringLiteral("Re: Fwd: [foo] blah");
    QTest::newRow("re-re-re-fwd-ml") << QStringLiteral("Re: re: RE: Fwd: [foo] blah") << QStringLiteral("Re: Fwd: [foo] blah");
    QTest::newRow("re-ml-re-fwd-ml") << QStringLiteral("Re: [foo] RE: Fwd: [bar] blah") << QStringLiteral("Re: [foo] Fwd: [bar] blah");
}

/** @short Test that subjects remain sane in forwarded messages */
void ComposerResponsesTest::testForwardSubjectMangling()
{
    QFETCH(QString, original);
    QFETCH(QString, forwarded);

    QCOMPARE(Composer::Util::forwardSubject(original), forwarded);
}

/** @short Data for testForwardSubjectMangling */
void ComposerResponsesTest::testForwardSubjectMangling_data()
{
    QTest::addColumn<QString>("original");
    QTest::addColumn<QString>("forwarded");

    QTest::newRow("no-subject") << QString() << QStringLiteral("Fwd: ");
    QTest::newRow("simple") << QStringLiteral("ahoj") << QStringLiteral("Fwd: ahoj");
    QTest::newRow("already-forwarded") << QStringLiteral("Fwd: ahoj") << QStringLiteral("Fwd: Fwd: ahoj");
    QTest::newRow("multiple-fwd") << QStringLiteral("FW: FWD: fw: Fwd: ahoj") << QStringLiteral("Fwd: FW: FWD: fw: Fwd: ahoj");
    QTest::newRow("old-TB-style-fwd") << QStringLiteral("[Fwd: ahoj]") << QStringLiteral("Fwd: [Fwd: ahoj]");
    QTest::newRow("trailing-fwd") << QStringLiteral("ahoj fwd") << QStringLiteral("Fwd: ahoj fwd");
    QTest::newRow("trailing-fwd-parenthesis") << QStringLiteral("ahoj (fwd)") << QStringLiteral("Fwd: ahoj (fwd)");
    QTest::newRow("leading-plus-trailing-fwd") << QStringLiteral("Fwd: ahoj (fwd)") << QStringLiteral("Fwd: Fwd: ahoj (fwd)");
}

/** @short Test different means of responding ("private", "to all", "to list") */
void ComposerResponsesTest::testResponseAddresses()
{
    QFETCH(Composer::RecipientList, original);
    QFETCH(QList<QUrl>, headerListPost);
    QFETCH(bool, headerListPostNo);
    QFETCH(bool, privateOk);
    QFETCH(Composer::RecipientList, privateRecipients);
    QFETCH(bool, allOk);
    QFETCH(Composer::RecipientList, allRecipients);
    QFETCH(bool, allButMeOk);
    QFETCH(Composer::RecipientList, allButMeRecipients);
    QFETCH(bool, listOk);
    QFETCH(Composer::RecipientList, listRecipients);
    QFETCH(bool, senderDetermined);
    QFETCH(QStringList, senderIdentities);
    QFETCH(QString, selectedIdentity);

    Composer::SenderIdentitiesModel model;
    Q_FOREACH(const QString &mail, senderIdentities) {
        Composer::ItemSenderIdentity identity;
        identity.emailAddress = mail;
        model.appendIdentity(identity);
    }

    Composer::RecipientList canary;
    canary << qMakePair(Composer::ADDRESS_REPLY_TO,
                        Imap::Message::MailAddress(QString(), QString(), QString(), QStringLiteral("fail")));
    Composer::RecipientList res;

    res = canary;
    QCOMPARE(Composer::Util::replyRecipientList(Composer::REPLY_PRIVATE, &model, original, headerListPost, headerListPostNo, res),
             privateOk);
    if (!privateOk)
        QVERIFY(res == canary);
    else
        QCOMPARE(res, privateRecipients);

    res = canary;
    QCOMPARE(Composer::Util::replyRecipientList(Composer::REPLY_ALL, &model, original, headerListPost, headerListPostNo, res),
             allOk);
    if (!allOk)
        QVERIFY(res == canary);
    else
        QCOMPARE(res, allRecipients);

    res = canary;
    QCOMPARE(Composer::Util::replyRecipientList(Composer::REPLY_ALL_BUT_ME, &model, original, headerListPost, headerListPostNo, res),
             allButMeOk);
    if (!allButMeOk)
        QVERIFY(res == canary);
    else
        QCOMPARE(res, allButMeRecipients);

    res = canary;
    QCOMPARE(Composer::Util::replyRecipientList(Composer::REPLY_LIST, &model, original, headerListPost, headerListPostNo, res),
             listOk);
    if (!listOk)
        QVERIFY(res == canary);
    else
        QCOMPARE(res, listRecipients);

    int offset = -1;
    bool ok = Composer::Util::chooseSenderIdentity(&model, Composer::Util::extractEmailAddresses(original), offset);
    QCOMPARE(ok, senderDetermined);
    if (senderDetermined) {
        QCOMPARE(model.data(model.index(offset, Composer::SenderIdentitiesModel::COLUMN_EMAIL)).toString(), selectedIdentity);
    } else {
        QCOMPARE(offset, -1);
    }
}

Imap::Message::MailAddress mail(const char * addr)
{
    QStringList list = QString(QLatin1String(addr)).split(QLatin1Char('@'));
    Q_ASSERT(list.size() == 2);
    return Imap::Message::MailAddress(QString(), QString(), list[0], list[1]);
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailFrom(const char * addr)
{
    return qMakePair(Composer::ADDRESS_FROM, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailSender(const char * addr)
{
    return qMakePair(Composer::ADDRESS_SENDER, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailReplyTo(const char * addr)
{
    return qMakePair(Composer::ADDRESS_REPLY_TO, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailTo(const char * addr)
{
    return qMakePair(Composer::ADDRESS_TO, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailCc(const char * addr)
{
    return qMakePair(Composer::ADDRESS_CC, mail(addr));
}

QPair<Composer::RecipientKind, Imap::Message::MailAddress> mailBcc(const char * addr)
{
    return qMakePair(Composer::ADDRESS_BCC, mail(addr));
}

void ComposerResponsesTest::testResponseAddresses_data()
{
    using namespace Composer;

    QTest::addColumn<RecipientList>("original");
    QTest::addColumn<QList<QUrl> >("headerListPost");
    QTest::addColumn<bool>("headerListPostNo");
    QTest::addColumn<bool>("privateOk");
    QTest::addColumn<RecipientList>("privateRecipients");
    QTest::addColumn<bool>("allOk");
    QTest::addColumn<RecipientList>("allRecipients");
    QTest::addColumn<bool>("allButMeOk");
    QTest::addColumn<RecipientList>("allButMeRecipients");
    QTest::addColumn<bool>("listOk");
    QTest::addColumn<RecipientList>("listRecipients");
    QTest::addColumn<bool>("senderDetermined");
    QTest::addColumn<QStringList>("senderIdentities");
    QTest::addColumn<QString>("selectedIdentity");

    RecipientList empty;
    QList<QUrl> listPost;

    QTest::newRow("from")
        << (RecipientList() << mailFrom("a@b"))
        << listPost << false
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b"))
        << false << empty
        << false << QStringList() << QString();

    QTest::newRow("sender")
        << (RecipientList() << mailSender("a@b"))
        << listPost << false
        << false << empty
        << false << empty
        << false << empty
        << false << empty
        << false << QStringList() << QString();

    QTest::newRow("to")
        << (RecipientList() << mailTo("a@b"))
        << listPost << false
        << false << empty
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b"))
        << false << empty
        << false << QStringList() << QString();

    QTest::newRow("cc")
        << (RecipientList() << mailCc("a@b"))
        << listPost << false
        << false << empty
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b"))
        << false << empty
        << false << QStringList() << QString();

    QTest::newRow("bcc")
        << (RecipientList() << mailBcc("a@b"))
        << listPost << false
        << false << empty
        << true << (RecipientList() << mailBcc("a@b"))
        << true << (RecipientList() << mailBcc("a@b"))
        << false << empty
        << false << QStringList() << QString();

    QTest::newRow("from-to")
        << (RecipientList() << mailFrom("a@b") << mailTo("c@d"))
        << listPost << false
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b") << mailCc("c@d"))
        << true << (RecipientList() << mailTo("a@b") << mailCc("c@d"))
        << false << empty
        << false << QStringList() << QString();

    QTest::newRow("from-sender-to")
        << (RecipientList() << mailFrom("a@b") << mailSender("x@y") << mailTo("c@d"))
        << listPost << false
        << true << (RecipientList() << mailTo("a@b"))
        << true << (RecipientList() << mailTo("a@b") << mailCc("c@d"))
        << true << (RecipientList() << mailTo("a@b") << mailCc("c@d"))
        << false << empty
        << false << QStringList() << QString();

    QTest::newRow("list-munged")
        << (RecipientList() << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QStringLiteral("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (QStringList() << QStringLiteral("jkt@flaska.net")) << QStringLiteral("jkt@flaska.net");

    QTest::newRow("list-munged-sender")
        << (RecipientList() << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net") << mailSender("trojita+bounces@lists"))
        << (QList<QUrl>() << QUrl(QStringLiteral("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (QStringList() << QStringLiteral("jkt@flaska.net")) << QStringLiteral("jkt@flaska.net");

    QTest::newRow("list-unmunged")
        << (RecipientList() << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QStringLiteral("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        // The sender identity is matched through the domain
        << true << (QStringList() << QStringLiteral("x@y") << QStringLiteral("z@y") << QStringLiteral("meh@flaSka.net") << QStringLiteral("bar@flaska.net")) << QStringLiteral("meh@flaSka.net");

    QTest::newRow("list-munged-bcc")
        << (RecipientList() << mailBcc("bcc@example.org") << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QStringLiteral("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << false << QStringList() << QString();

    QTest::newRow("list-me-at-the-end")
        << (RecipientList() << mailBcc("bcc@example.org") << mailFrom("someone@else") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net") << mailBcc("jkt@flaska.net"))
        << (QList<QUrl>() << QUrl(QStringLiteral("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("someone@else"))
        << true << (RecipientList() << mailTo("someone@else") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org") << mailBcc("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("someone@else") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (QStringList() << QStringLiteral("jkt@flaska.net")) << QStringLiteral("jkt@flaska.net");

    QTest::newRow("list-unmunged-bcc")
        << (RecipientList() << mailBcc("bcc@example.org") << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QStringLiteral("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        // An exact match for the sender identity
        << true << (QStringList() << QStringLiteral("jkt@gentoo") << QStringLiteral("foo@flaska.net") << QStringLiteral("jKt@flaSka.net")) << QStringLiteral("jKt@flaSka.net");

    QTest::newRow("from-list-sender-to-cc")
        << (RecipientList() << mailFrom("andy@x") << mailSender("list-12345@y") << mailTo("someone@z") << mailCc("list@y"))
        << (QList<QUrl>() << QUrl(QStringLiteral("mailtO:list@Y"))) << false
        << true << (RecipientList() << mailTo("andy@x"))
        << true << (RecipientList() << mailTo("andy@x") << mailCc("someone@z") << mailCc("list@y"))
        << true << (RecipientList() << mailTo("andy@x") << mailCc("someone@z") << mailCc("list@y"))
        << true << (RecipientList() << mailTo("list@Y"))
        << false << (QStringList() << QStringLiteral("foo@bar") << QStringLiteral("baz@bah")) << QString();

    QTest::newRow("from-replyto-to-cc-cc-gerrit")
        << (RecipientList() << mailFrom("gerrit-noreply@qt-project") << mailReplyTo("j.n@digia")
            << mailTo("jkt@flaska") << mailCc("j.n@digia") << mailCc("qt_sanity_bot@ovi") << mailCc("s.k@kdab"))
        << QList<QUrl>() << false
        << true << (RecipientList() << mailTo("j.n@digia"))
        // FIXME: this shall be better!
        << true << (RecipientList() << mailTo("gerrit-noreply@qt-project") << mailCc("j.n@digia") << mailCc("jkt@flaska")
                    << mailCc("qt_sanity_bot@ovi") << mailCc("s.k@kdab"))
        << true << (RecipientList() << mailTo("gerrit-noreply@qt-project") << mailCc("j.n@digia")
                    << mailCc("qt_sanity_bot@ovi") << mailCc("s.k@kdab"))
        << false << empty
        << true << (QStringList() << QStringLiteral("foo@bar") << QStringLiteral("jkt@flaska")) << QStringLiteral("jkt@flaska");

    QTest::newRow("me-to-me")
        << (RecipientList() << mailFrom("j@k") << mailTo("j@k"))
        << QList<QUrl>() << false
        << true << (RecipientList() << mailTo("j@k"))
        << true << (RecipientList() << mailTo("j@k"))
        << false << empty
        << false << empty
        << true << (QStringList() << QStringLiteral("j@k")) << QStringLiteral("j@k");

    QTest::newRow("me-to-someone")
        << (RecipientList() << mailFrom("j@k") << mailTo("a@b"))
        << QList<QUrl>() << false
        << true << (RecipientList() << mailTo("j@k"))
        << true << (RecipientList() << mailTo("j@k") << mailCc("a@b"))
        << true << (RecipientList() << mailTo("a@b"))
        << false << empty
        << true << (QStringList() << QStringLiteral("j@k")) << QStringLiteral("j@k");

    // We have a @gentoo.org identity, mail was sent by someone else, but to something @lists.gentoo.org
    QTest::newRow("lists-subdomain")
        << (RecipientList() << mailFrom("alien@example.org") << mailTo("x@lists.gentoo.org"))
        << (QList<QUrl>() << QUrl(QStringLiteral("mailto:x@lists.gentoo.org"))) << false
        << true << (RecipientList() << mailTo("alien@example.org"))
        << true << (RecipientList() << mailTo("alien@example.org") << mailCc("x@lists.gentoo.org"))
        << true << (RecipientList() << mailTo("alien@example.org") << mailCc("x@lists.gentoo.org"))
        << true << (RecipientList() << mailTo("x@lists.gentoo.org"))
        << true << (QStringList() << QStringLiteral("j@k") << QStringLiteral("test@gentoo.org")) << QStringLiteral("test@gentoo.org");

    // FIXME: more tests!
}

void ComposerResponsesTest::testFormatFlowedComposition()
{
    QFETCH(QString, input);
    QFETCH(QString, split);

    QCOMPARE(Imap::wrapFormatFlowed(input), split);
}

void ComposerResponsesTest::testFormatFlowedComposition_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("split");

    QTest::newRow("empty") << QString() << QString();
    QTest::newRow("one-line") << QStringLiteral("ahoj") << QStringLiteral("ahoj");
    QTest::newRow("one-line-LF") << QStringLiteral("ahoj\n") << QStringLiteral("ahoj\r\n");
    QTest::newRow("one-line-CR") << QStringLiteral("ahoj\r") << QStringLiteral("ahoj");
    QTest::newRow("one-line-CRLF") << QStringLiteral("ahoj\r\n") << QStringLiteral("ahoj\r\n");
    QTest::newRow("two-lines") << QStringLiteral("ahoj\ncau") << QStringLiteral("ahoj\r\ncau");
    QTest::newRow("two-lines-LF") << QStringLiteral("ahoj\ncau\n") << QStringLiteral("ahoj\r\ncau\r\n");

    QTest::newRow("wrapping-real")
            << QString("I'm typing some random stuff right here; I'm courious to see how the word wrapping ends up. "
                       "Perhaps it's going to be relevant for implementation within Trojita: we'll see about that. "
                       "Johoho, let's see how it ends up.\n\n"
                       "And in a signed mail, as a nice bonus. Perhaps try adding some Unicode: ěščřžýáíé for the lulz.\n\n"
                       "-- \nTrojita, a fast e-mail client -- http://trojita.flaska.net/\n")
            << QString("I'm typing some random stuff right here; I'm courious to see how the word \r\nwrapping ends up. "
                       "Perhaps it's going to be relevant for implementation \r\nwithin Trojita: we'll see about that. "
                       "Johoho, let's see how it ends up.\r\n\r\n"
                       "And in a signed mail, as a nice bonus. Perhaps try adding some Unicode: \r\něščřžýáíé for the lulz.\r\n\r\n"
                       "-- \r\nTrojita, a fast e-mail client -- http://trojita.flaska.net/\r\n");

    QTest::newRow("wrapping-longword-1")
            << QStringLiteral("HeresAnOverlyLongWordWhichStartsOnTheStartOfALineAndContinuesWellOverTheLineSize;Let'sSeeHowThisEndsUp. Good.")
            << QStringLiteral("HeresAnOverlyLongWordWhichStartsOnTheStartOfALineAndContinuesWellOverTheLineSize;Let'sSeeHowThisEndsUp. \r\nGood.");

    QTest::newRow("wrapping-longword-2")
            << QStringLiteral("Now this is nicer, suppose that here is AnOverlyLongWordWhichStartsInTheMiddleOfALine. Good.")
            << QStringLiteral("Now this is nicer, suppose that here is \r\nAnOverlyLongWordWhichStartsInTheMiddleOfALine. Good.");

    QTest::newRow("wrapping-longword-3")
            << QStringLiteral("Now this is nicer, suppose that here is AnOverlyLongWordWhichStartsInTheMiddleOfALineAndContinuesWellOverTheLineSize. Good.")
            << QStringLiteral("Now this is nicer, suppose that here is \r\nAnOverlyLongWordWhichStartsInTheMiddleOfALineAndContinuesWellOverTheLineSize. \r\nGood.");

    QTest::newRow("wrapping-longword-4")
            << QStringLiteral("> HeresAnOverlyLongWordWhichStartsOnTheStartOfALineAndContinuesWellOverTheLineSize;Let'sSeeHowThisEndsUp. Good.")
            << QStringLiteral("> HeresAnOverlyLongWordWhichStartsOnTheStartOfALineAndContinuesWellOverTheLineSize;Let'sSeeHowThisEndsUp. Good.");

    QTest::newRow("wrapping-longword-5")
            << QString("> NowDoTheSameWithSomeWordThatIsTooLongToFitInThisParagraphThisWillBeEnoughIHopeBecauseItIsAnnoyingToType, \n"
                       "> isn't it?\n")
            << QString("> NowDoTheSameWithSomeWordThatIsTooLongToFitInThisParagraphThisWillBeEnoughIHopeBecauseItIsAnnoyingToType, \r\n"
                       "> isn't it?\r\n");

    QString longline(QStringLiteral("velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat6 XYZ"));
    Q_ASSERT(longline.length() == 76 + QString("XYZ").length());
    QTest::newRow("wrapping-longline")
            << QString(longline)
            << QStringLiteral("velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat6 \r\nXYZ");

    QString longQuotePrefix = QString(76, QLatin1Char('>')) + QLatin1Char(' ');
    QTest::newRow("long-quote-prefix")
            << longQuotePrefix + QLatin1String("hello world!")
            << longQuotePrefix + QLatin1String("hello world!");

    // In the next test, the best approach would be wrapping the "QtObject" into the next line (and perhaps merging it
    // with the contents in there). However, doing so would require insight into whether that line was originally flowed
    // or not, otherwise we risk creating something which doesn't make sense anymore.
    // For now, simply not wrapping the long quoted lines makes sense (they are still wrapped when the quote is
    // originally created anyway).
    QTest::newRow("quoted-long-line-trailing-word")
            << QString::fromUtf8("> Currently Qml doesn't handle such case. You can wrap your enums in QtObject \n"
                                 "> and expose it as singleton[1] object and use from inside Qml:\n"
                                 ">")
            << QString::fromUtf8("> Currently Qml doesn't handle such case. You can wrap your enums in QtObject \r\n"
                                 "> and expose it as singleton[1] object and use from inside Qml:\r\n"
                                 ">");

    QTest::newRow("some-lines-with-spaces-1")
            << QStringLiteral("foo\n \nbar")
            << QStringLiteral("foo\r\n \r\nbar");
    QTest::newRow("some-lines-with-spaces-2")
            << QStringLiteral("foo\n \nbar\n")
            << QStringLiteral("foo\r\n \r\nbar\r\n");
    QTest::newRow("some-lines-with-spaces-3")
            << QStringLiteral("foo\n \nbar\n\n")
            << QStringLiteral("foo\r\n \r\nbar\r\n\r\n");
    QTest::newRow("some-lines-with-spaces-4")
            << QStringLiteral("foo\n \nbar\n\n ")
            << QStringLiteral("foo\r\n \r\nbar\r\n\r\n ");
}

void ComposerResponsesTest::testRFC6068Mailto()
{
    QFETCH(QUrl, original);
    QFETCH(QString, subject);
    QFETCH(QString, body);
    QFETCH(RecipientsType, recipients);
    QFETCH(QList<QByteArray>, inReplyTo);
    QFETCH(QList<QByteArray>, references);

    QString decodedSubject;
    QString decodedBody;
    RecipientsType decodedRecipients;
    QList<QByteArray> decodedInReplyTo;
    QList<QByteArray> decodedReferences;

    Composer::parseRFC6068Mailto(original, decodedSubject, decodedBody, decodedRecipients, decodedInReplyTo, decodedReferences);

    QCOMPARE(decodedSubject, subject);
    QCOMPARE(decodedBody, body);
    QCOMPARE(decodedRecipients, recipients);
    QCOMPARE(decodedInReplyTo, inReplyTo);
    QCOMPARE(decodedReferences, references);
}

void ComposerResponsesTest::testRFC6068Mailto_data()
{
    QTest::addColumn<QUrl>("original");
    QTest::addColumn<QString>("subject");
    QTest::addColumn<QString>("body");
    QTest::addColumn<RecipientsType>("recipients");
    QTest::addColumn<QList<QByteArray>>("inReplyTo");
    QTest::addColumn<QList<QByteArray>>("references");

    // RFC 6068 2. Syntax of a 'mailto' URI
    QTest::newRow("rfc1") << QUrl::fromEncoded("mailto:addr1@an.example,addr2@an.example") << QString() << QString()
            << ( RecipientsType()
                 << qMakePair(Composer::ADDRESS_TO, QStringLiteral("addr1@an.example"))
                 << qMakePair(Composer::ADDRESS_TO, QStringLiteral("addr2@an.example")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc2") << QUrl::fromEncoded("mailto:?to=addr1@an.example,addr2@an.example") << QString() << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("addr1@an.example"))
                 << qMakePair(Composer::ADDRESS_TO, QStringLiteral("addr2@an.example")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc3") << QUrl::fromEncoded("mailto:addr1@an.example?to=addr2@an.example") << QString() << QString()
            << ( RecipientsType()
                 << qMakePair(Composer::ADDRESS_TO, QStringLiteral("addr1@an.example"))
                 << qMakePair(Composer::ADDRESS_TO, QStringLiteral("addr2@an.example")) )
            << QList<QByteArray>() << QList<QByteArray>();

    // RFC 6068 6.1. Basic Examples
    QTest::newRow("rfc4") << QUrl::fromEncoded("mailto:chris@example.com") << QString() << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("chris@example.com")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc5") << QUrl::fromEncoded("mailto:infobot@example.com?subject=current-issue")
            << QStringLiteral("current-issue") << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("infobot@example.com")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc6") << QUrl::fromEncoded("mailto:infobot@example.com?body=send%20current-issue")
            << QString() << QStringLiteral("send current-issue")
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("infobot@example.com")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc7") << QUrl::fromEncoded("mailto:infobot@example.com?body=send%20current-issue%0D%0Asend%20index")
            << QString() << QStringLiteral("send current-issue\r\nsend index")
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("infobot@example.com")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc8") << QUrl::fromEncoded("mailto:list@example.org?In-Reply-To=%3C3469A91.D10AF4C@example.com%3E")
            << QString() << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("list@example.org")) )
            << ( QList<QByteArray>() << "<3469A91.D10AF4C@example.com>" ) << QList<QByteArray>();
    QTest::newRow("rfc9") << QUrl::fromEncoded("mailto:majordomo@example.com?body=subscribe%20bamboo-l")
            << QString() << QStringLiteral("subscribe bamboo-l")
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("majordomo@example.com")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc10") << QUrl::fromEncoded("mailto:joe@example.com?cc=bob@example.com&body=hello") << QString() << QStringLiteral("hello")
            << ( RecipientsType()
                 << qMakePair(Composer::ADDRESS_TO, QStringLiteral("joe@example.com"))
                 << qMakePair(Composer::ADDRESS_CC, QStringLiteral("bob@example.com")) )
            << QList<QByteArray>() << QList<QByteArray>();

    QTest::newRow("rfc11") << QUrl::fromEncoded("mailto:gorby%25kremvax@example.com") << QString() << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("gorby%kremvax@example.com")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc13") << QUrl::fromEncoded("mailto:Mike%26family@example.org") << QString() << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("Mike&family@example.org")) )
            << QList<QByteArray>() << QList<QByteArray>();

    // RFC 6068 6.2. Examples of Complicated Email Addresses
    QTest::newRow("rfc14") << QUrl::fromEncoded("mailto:%22not%40me%22@example.org") << QString() << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("\"not@me\"@example.org")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc15") << QUrl::fromEncoded("mailto:%22oh%5C%5Cno%22@example.org") << QString() << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("\"oh\\\\no\"@example.org")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc16") << QUrl::fromEncoded("mailto:%22%5C%5C%5C%22it's%5C%20ugly%5C%5C%5C%22%22@example.org") << QString() << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("\"\\\\\\\"it's\\ ugly\\\\\\\"\"@example.org")) )
            << QList<QByteArray>() << QList<QByteArray>();

    // RFC 6068 6.3. Examples Using UTF-8-Based Percent-Encoding
    QTest::newRow("rfc17") << QUrl::fromEncoded("mailto:user@example.org?subject=caf%C3%A9")
            << QStringLiteral("café") << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("user@example.org")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc18") << QUrl::fromEncoded("mailto:user@example.org?subject=%3D%3Futf-8%3FQ%3Fcaf%3DC3%3DA9%3F%3D")
            << QStringLiteral("café") << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("user@example.org")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc19") << QUrl::fromEncoded("mailto:user@example.org?subject=%3D%3Fiso-8859-1%3FQ%3Fcaf%3DE9%3F%3D")
            << QStringLiteral("café") << QString()
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("user@example.org")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc20") << QUrl::fromEncoded("mailto:user@example.org?subject=caf%C3%A9&body=caf%C3%A9")
            << QStringLiteral("café") << QStringLiteral("café")
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("user@example.org")) )
            << QList<QByteArray>() << QList<QByteArray>();
    QTest::newRow("rfc21") << QUrl::fromEncoded("mailto:user@%E7%B4%8D%E8%B1%86.example.org?subject=Test&body=NATTO")
            << QStringLiteral("Test") << QStringLiteral("NATTO")
            << ( RecipientsType() << qMakePair(Composer::ADDRESS_TO, QStringLiteral("user@納豆.example.org")) )
            << QList<QByteArray>() << QList<QByteArray>(); // NOTE: Decoding 納豆.example.org to xn--99zt52a.example.org is done on other layer

    // Another test for comma delimeter
    QTest::newRow("test")
            << QUrl::fromEncoded("mailto:user@example.org,user2@example%2Ctest.org?cc=user%40host.org%2Ctest@example%2Ctest.org")
            << QString() << QString()
            << ( RecipientsType()
                 << qMakePair(Composer::ADDRESS_TO, QStringLiteral("user@example.org"))
                 << qMakePair(Composer::ADDRESS_TO, QStringLiteral("user2@example,test.org"))
                 << qMakePair(Composer::ADDRESS_CC, QStringLiteral("user@host.org,test@example,test.org")) )
            << QList<QByteArray>() << QList<QByteArray>();
}

void ComposerResponsesTest::testReplyQuoting()
{
    QFETCH(QString, source);
    QFETCH(QString, quoted);
    QCOMPARE(Composer::quoteText(source.split(QChar(QLatin1Char('\n')))).join(QChar(QLatin1Char('\n'))), quoted);
}

void ComposerResponsesTest::testReplyQuoting_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("quoted");

    QTest::newRow("empty") << QString() << QStringLiteral(">");
    QTest::newRow("simple") << QStringLiteral("Hello world") << QStringLiteral("> Hello world");
    QTest::newRow("simple-newline") << QStringLiteral("Hello world\n") << QStringLiteral("> Hello world\n>");

    // This following two test cases is how v0.4.1-212-g0523301 behaves.
    // For this case, the ML stripped the format=flowed, and as such we treat it as different pieces of text, not to be wrapped together.
    QTest::newRow("trailing-whitespace")
            << QString::fromUtf8("Currently Qml doesn't handle such case. You can wrap your enums in QtObject \n"
                                 "and expose it as singleton[1] object and use from inside Qml:\n")
            << QString::fromUtf8("> Currently Qml doesn't handle such case. You can wrap your enums in QtObject \n"
                                 "> and expose it as singleton[1] object and use from inside Qml:\n"
                                 ">");
    // This message contained format=flowed, and our code can therefore distinguish that it's a single chunk of the text:
    QTest::newRow("flowed")
            << QString::fromUtf8("Currently Qml doesn't handle such case. You can wrap your enums in QtObject "
                                 "and expose it as singleton[1] object and use from inside Qml:\n")
            << QString::fromUtf8("> Currently Qml doesn't handle such case. You can wrap your enums \n"
                                 "> in QtObject and expose it as singleton[1] object and use from \n"
                                 "> inside Qml:\n"
                                 ">");
}

QTEST_GUILESS_MAIN(ComposerResponsesTest)
