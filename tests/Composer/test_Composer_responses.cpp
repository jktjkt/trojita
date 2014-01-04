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
#include "Utils/headless_test.h"
#include "Composer/Recipients.h"
#include "Composer/ReplaceSignature.h"
#include "Composer/SenderIdentitiesModel.h"
#include "Composer/SubjectMangling.h"
#include "Imap/Encoders.h"

Q_DECLARE_METATYPE(Composer::RecipientList)
Q_DECLARE_METATYPE(QList<QUrl>)

QString recipientKindtoString(const Composer::RecipientKind kind)
{
    switch (kind) {
    case Composer::ADDRESS_BCC:
        return QLatin1String("Bcc:");
    case Composer::ADDRESS_CC:
        return QLatin1String("Cc:");
    case Composer::ADDRESS_FROM:
        return QLatin1String("From:");
    case Composer::ADDRESS_REPLY_TO:
        return QLatin1String("Reply-To:");
    case Composer::ADDRESS_SENDER:
        return QLatin1String("Sender:");
    case Composer::ADDRESS_TO:
        return QLatin1String("To:");
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
        return qstrdup((recipientKindtoString(item.first).toUtf8() + ' ' + item.second.asSMTPMailbox()).constData());
    }
}

/** @short Test that subjects remain sane in replied/forwarded messages */
void ComposerResponsesTest::testSubjectMangling()
{
    QFETCH(QString, original);
    QFETCH(QString, replied);

    QCOMPARE(Composer::Util::replySubject(original), replied);
}

/** @short Data for testSubjectMangling */
void ComposerResponsesTest::testSubjectMangling_data()
{
    QTest::addColumn<QString>("original");
    QTest::addColumn<QString>("replied");

    QTest::newRow("no-subject") << QString() << QString::fromUtf8("Re: ");
    QTest::newRow("simple") << QString::fromUtf8("ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("already-replied") << QString::fromUtf8("Re: ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("already-replied-no-space") << QString::fromUtf8("re:ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("already-replied-case") << QString::fromUtf8("RE: ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("multiple-re") << QString::fromUtf8("Re:RE: re: ahoj") << QString::fromUtf8("Re: ahoj");
    QTest::newRow("trailing-re") << QString::fromUtf8("ahoj re:") << QString::fromUtf8("Re: ahoj re:");
    QTest::newRow("leading-trailing-re") << QString::fromUtf8("re: ahoj re:") << QString::fromUtf8("Re: ahoj re:");

    // mailing list stuff
    QTest::newRow("ml-empty") << QString::fromUtf8("[foo]") << QString::fromUtf8("Re: [foo]");
    QTest::newRow("ml-simple") << QString::fromUtf8("[foo] bar") << QString::fromUtf8("Re: [foo] bar");
    QTest::newRow("ml-simple-no-space") << QString::fromUtf8("[foo]bar") << QString::fromUtf8("Re: [foo] bar");
    QTest::newRow("ml-broken") << QString::fromUtf8("[foo bar") << QString::fromUtf8("Re: [foo bar");
    QTest::newRow("ml-two-words") << QString::fromUtf8("[foo bar] x") << QString::fromUtf8("Re: [foo bar] x");
    QTest::newRow("ml-re-empty") << QString::fromUtf8("[foo] Re:") << QString::fromUtf8("Re: [foo]");
    QTest::newRow("re-ml-re-empty") << QString::fromUtf8("Re: [foo] Re:") << QString::fromUtf8("Re: [foo]");
    QTest::newRow("re-ml-re-empty-no-spaces") << QString::fromUtf8("Re:[foo]Re:") << QString::fromUtf8("Re: [foo]");
    QTest::newRow("ml-ml") << QString::fromUtf8("[foo] [bar] blah") << QString::fromUtf8("Re: [foo] [bar] blah");
    QTest::newRow("ml-ml-re") << QString::fromUtf8("[foo] [bar] Re: blah") << QString::fromUtf8("Re: [foo] [bar] blah");
    QTest::newRow("ml-re-ml") << QString::fromUtf8("[foo] Re: [bar] blah") << QString::fromUtf8("Re: [foo] [bar] blah");
    QTest::newRow("ml-re-ml-re") << QString::fromUtf8("[foo] Re: [bar] Re: blah") << QString::fromUtf8("Re: [foo] [bar] blah");
    QTest::newRow("re-ml-ml") << QString::fromUtf8("Re: [foo] [bar] blah") << QString::fromUtf8("Re: [foo] [bar] blah");
    QTest::newRow("re-ml-ml-re") << QString::fromUtf8("Re: [foo] [bar] Re: blah") << QString::fromUtf8("Re: [foo] [bar] blah");
    QTest::newRow("re-ml-re-ml") << QString::fromUtf8("Re: [foo] Re: [bar] blah") << QString::fromUtf8("Re: [foo] [bar] blah");
    QTest::newRow("re-ml-re-ml-re") << QString::fromUtf8("Re: [foo] Re: [bar] Re: blah") << QString::fromUtf8("Re: [foo] [bar] blah");

    // test removing duplicate items
    QTest::newRow("M-M") << QString::fromUtf8("[foo] [foo] blah") << QString::fromUtf8("Re: [foo] blah");
    QTest::newRow("M-M-re") << QString::fromUtf8("[foo] [foo] Re: blah") << QString::fromUtf8("Re: [foo] blah");
    QTest::newRow("M-M-re-re") << QString::fromUtf8("[foo] [foo] Re: Re: blah") << QString::fromUtf8("Re: [foo] blah");
    QTest::newRow("M-re-M") << QString::fromUtf8("[foo] Re: [foo] blah") << QString::fromUtf8("Re: [foo] blah");
    QTest::newRow("re-M-re-M") << QString::fromUtf8("Re: [foo] Re: [foo] blah") << QString::fromUtf8("Re: [foo] blah");
    QTest::newRow("re-M-re-M-re") << QString::fromUtf8("Re: [foo] Re: [foo] Re: blah") << QString::fromUtf8("Re: [foo] blah");

    // stuff which should not be subject to subject sanitization
    QTest::newRow("brackets-end") << QString::fromUtf8("blesmrt [test]") << QString::fromUtf8("Re: blesmrt [test]");
    QTest::newRow("re-brackets-end") << QString::fromUtf8("Re: blesmrt [test]") << QString::fromUtf8("Re: blesmrt [test]");
    QTest::newRow("re-brackets-re-end") << QString::fromUtf8("Re: blesmrt Re: [test]") << QString::fromUtf8("Re: blesmrt Re: [test]");
    QTest::newRow("brackets-re-end") << QString::fromUtf8("blesmrt Re: [test]") << QString::fromUtf8("Re: blesmrt Re: [test]");

    // real-world bugs
    QTest::newRow("extra-space-in-0.3.92")
        << QString::fromUtf8("[imapext]  Re: Proposal for a new IMAP Working Group to revise CONDSTORE & QRESYNC")
        << QString::fromUtf8("Re: [imapext] Proposal for a new IMAP Working Group to revise CONDSTORE & QRESYNC");
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
                        Imap::Message::MailAddress(QString(), QString(), QString(), QLatin1String("fail")));
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
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (QStringList() << QString::fromUtf8("jkt@flaska.net")) << QString::fromUtf8("jkt@flaska.net");

    QTest::newRow("list-munged-sender")
        << (RecipientList() << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net") << mailSender("trojita+bounces@lists"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (QStringList() << QString::fromUtf8("jkt@flaska.net")) << QString::fromUtf8("jkt@flaska.net");

    QTest::newRow("list-unmunged")
        << (RecipientList() << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        // The sender identity is matched through the domain
        << true << (QStringList() << "x@y" << "z@y" << "meh@flaSka.net" << "bar@flaska.net") << QString::fromUtf8("meh@flaSka.net");

    QTest::newRow("list-munged-bcc")
        << (RecipientList() << mailBcc("bcc@example.org") << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << false << QStringList() << QString();

    QTest::newRow("list-me-at-the-end")
        << (RecipientList() << mailBcc("bcc@example.org") << mailFrom("someone@else") << mailTo("trojita@lists.flaska.net")
            << mailReplyTo("trojita@lists.flaska.net") << mailBcc("jkt@flaska.net"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("someone@else"))
        << true << (RecipientList() << mailTo("someone@else") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org") << mailBcc("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("someone@else") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        << true << (QStringList() << QString::fromUtf8("jkt@flaska.net")) << QString::fromUtf8("jkt@flaska.net");

    QTest::newRow("list-unmunged-bcc")
        << (RecipientList() << mailBcc("bcc@example.org") << mailFrom("jkt@flaska.net") << mailTo("trojita@lists.flaska.net"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:trojita@lists.flaska.net"))) << false
        << true << (RecipientList() << mailTo("jkt@flaska.net"))
        << true << (RecipientList() << mailTo("jkt@flaska.net") << mailCc("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net") << mailBcc("bcc@example.org"))
        << true << (RecipientList() << mailTo("trojita@lists.flaska.net"))
        // An exact match for the sender identity
        << true << (QStringList() << "jkt@gentoo" << "foo@flaska.net" << "jKt@flaSka.net") << QString::fromUtf8("jKt@flaSka.net");

    QTest::newRow("from-list-sender-to-cc")
        << (RecipientList() << mailFrom("andy@x") << mailSender("list-12345@y") << mailTo("someone@z") << mailCc("list@y"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailtO:list@Y"))) << false
        << true << (RecipientList() << mailTo("andy@x"))
        << true << (RecipientList() << mailTo("andy@x") << mailCc("someone@z") << mailCc("list@y"))
        << true << (RecipientList() << mailTo("andy@x") << mailCc("someone@z") << mailCc("list@y"))
        << true << (RecipientList() << mailTo("list@Y"))
        << false << (QStringList() << "foo@bar" << "baz@bah") << QString();

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
        << true << (QStringList() << QString::fromUtf8("foo@bar") << QString::fromUtf8("jkt@flaska")) << QString::fromUtf8("jkt@flaska");

    QTest::newRow("me-to-me")
        << (RecipientList() << mailFrom("j@k") << mailTo("j@k"))
        << QList<QUrl>() << false
        << true << (RecipientList() << mailTo("j@k"))
        << true << (RecipientList() << mailTo("j@k"))
        << false << empty
        << false << empty
        << true << (QStringList() << QString::fromUtf8("j@k")) << QString::fromUtf8("j@k");

    QTest::newRow("me-to-someone")
        << (RecipientList() << mailFrom("j@k") << mailTo("a@b"))
        << QList<QUrl>() << false
        << true << (RecipientList() << mailTo("j@k"))
        << true << (RecipientList() << mailTo("j@k") << mailCc("a@b"))
        << true << (RecipientList() << mailTo("a@b"))
        << false << empty
        << true << (QStringList() << QString::fromUtf8("j@k")) << QString::fromUtf8("j@k");

    // We have a @gentoo.org identity, mail was sent by someone else, but to something @lists.gentoo.org
    QTest::newRow("lists-subdomain")
        << (RecipientList() << mailFrom("alien@example.org") << mailTo("x@lists.gentoo.org"))
        << (QList<QUrl>() << QUrl(QLatin1String("mailto:x@lists.gentoo.org"))) << false
        << true << (RecipientList() << mailTo("alien@example.org"))
        << true << (RecipientList() << mailTo("alien@example.org") << mailCc("x@lists.gentoo.org"))
        << true << (RecipientList() << mailTo("alien@example.org") << mailCc("x@lists.gentoo.org"))
        << true << (RecipientList() << mailTo("x@lists.gentoo.org"))
        << true << (QStringList() << QString::fromUtf8("j@k") << QString::fromUtf8("test@gentoo.org")) << QString::fromUtf8("test@gentoo.org");

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
    QTest::newRow("one-line") << QString("ahoj") << QString("ahoj");
    QTest::newRow("one-line-LF") << QString("ahoj\n") << QString("ahoj\r\n");
    QTest::newRow("one-line-CR") << QString("ahoj\r") << QString("ahoj");
    QTest::newRow("one-line-CRLF") << QString("ahoj\r\n") << QString("ahoj\r\n");
    QTest::newRow("two-lines") << QString("ahoj\ncau") << QString("ahoj\r\ncau");
    QTest::newRow("two-lines-LF") << QString("ahoj\ncau\n") << QString("ahoj\r\ncau\r\n");

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
            << QString("HeresAnOverlyLongWordWhichStartsOnTheStartOfALineAndContinuesWellOverTheLineSize;Let'sSeeHowThisEndsUp. Good.")
            << QString("HeresAnOverlyLongWordWhichStartsOnTheStartOfALineAndContinuesWellOverTheLineSize;Let'sSeeHowThisEndsUp. \r\nGood.");

    QTest::newRow("wrapping-longword-2")
            << QString("Now this is nicer, suppose that here is AnOverlyLongWordWhichStartsInTheMiddleOfALine. Good.")
            << QString("Now this is nicer, suppose that here is \r\nAnOverlyLongWordWhichStartsInTheMiddleOfALine. Good.");

    QTest::newRow("wrapping-longword-3")
            << QString("Now this is nicer, suppose that here is AnOverlyLongWordWhichStartsInTheMiddleOfALineAndContinuesWellOverTheLineSize. Good.")
            << QString("Now this is nicer, suppose that here is \r\nAnOverlyLongWordWhichStartsInTheMiddleOfALineAndContinuesWellOverTheLineSize. \r\nGood.");

    QTest::newRow("wrapping-longword-4")
            << QString("> HeresAnOverlyLongWordWhichStartsOnTheStartOfALineAndContinuesWellOverTheLineSize;Let'sSeeHowThisEndsUp. Good.")
            << QString("> HeresAnOverlyLongWordWhichStartsOnTheStartOfALineAndContinuesWellOverTheLineSize;Let'sSeeHowThisEndsUp. \r\nGood.");

    QTest::newRow("wrapping-longword-5")
            << QString("> NowDoTheSameWithSomeWordThatIsTooLongToFitInThisParagraphThisWillBeEnoughIHopeBecauseItIsAnnoyingToType, \n"
                       "> isn't it?\n")
            << QString("> NowDoTheSameWithSomeWordThatIsTooLongToFitInThisParagraphThisWillBeEnoughIHopeBecauseItIsAnnoyingToType, \r\n"
                       "> isn't it?\r\n");

    QString longline("velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat6 XYZ");
    Q_ASSERT(longline.length() == 76 + QString("XYZ").length());
    QTest::newRow("wrapping-longline")
            << QString(longline)
            << QString("velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat6 \r\nXYZ");

    QTest::newRow("some-lines-with-spaces-1")
            << QString("foo\n \nbar")
            << QString("foo\r\n \r\nbar");
    QTest::newRow("some-lines-with-spaces-2")
            << QString("foo\n \nbar\n")
            << QString("foo\r\n \r\nbar\r\n");
    QTest::newRow("some-lines-with-spaces-3")
            << QString("foo\n \nbar\n\n")
            << QString("foo\r\n \r\nbar\r\n\r\n");
    QTest::newRow("some-lines-with-spaces-4")
            << QString("foo\n \nbar\n\n ")
            << QString("foo\r\n \r\nbar\r\n\r\n ");
}

TROJITA_HEADLESS_TEST(ComposerResponsesTest)
