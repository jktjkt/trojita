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

#include <QTest>

#include "test_Imap_Message.h"
#include "Utils/headless_test.h"
#include "Imap/Encoders.h"

Q_DECLARE_METATYPE(Imap::Message::MailAddress)
Q_DECLARE_METATYPE(QVariantList)

    /*
void ImapMessageTest::testEnvelope()
{
    QFETCH( Imap::Message::Envelope, expected );
    QFETCH( QVariantList, data );
    Imap::Message::Envelope envelope;
    try {
        envelope = Imap::Message::Envelope::fromList( data, QByteArray(), 0 );
        QCOMPARE( expected, Imap::Message::Envelope::fromList( data, QByteArray(), 0 ) );
    } catch ( Imap::Exception& e ) {
        QFAIL( e.what() );
    }
}

void ImapMessageTest::testEnvelope_data()
{
    using namespace Imap::Message;

    QTest::addColumn<Envelope>("expected");
    QTest::addColumn<QVariantList>("data");

    QList<MailAddress> from, sender, to, cc, bcc, replyTo;

    from << MailAddress( "Jan Kundrat", QByteArray(), "flaska.net", "jkt" ) <<
        MailAddress( "Another Developer", QByteArray(), "example.org", "another.guy" );
    sender << MailAddress( "Jan Kundrat", QByteArray(), "flaska.net", "jkt" );
    to << MailAddress( QByteArray(), QByteArray(), "lists.flaska.net", "trojita-announce" );
    bcc << MailAddress( QByteArray(), QByteArray(), "example.org", "trash" ) <<
        MailAddress( QByteArray(), QByteArray(), "example.org", "spam" );

    QTest::newRow("foo") <<
        Envelope( QDateTime( QDate( 2008, 4, 27 ), QTime( 13, 41, 37 ), Qt::UTC ),
                "A random test subject", from, sender, replyTo, to, cc, bcc,
                QByteArray(), QByteArray() ) <<
        QVariantList() <<
            QByteArray("Sun, 27 Apr 2008 15:41:37 +0200 (CEST)") << 
            QByteArray("A random test subject") <<
            ( // from
             QVariantList() <<
             ( QVariantList() << QByteArray("Jan Kundrat") << QByteArray() << QByteArray("flaska.net") << QByteArray("jkt") ) <<
             ( QVariantList() << QByteArray("Another Developer") ) ) <<
            ( QVariantList() << ( QVariantList() << QByteArray("Jan Kundrat") << QByteArray() << QByteArray("flaska.net") << QByteArray("jkt") ) ) <<
            ( QVariantList() << ( QVariantList() << QByteArray("Jan Kundrat") << QByteArray() << QByteArray("flaska.net") << QByteArray("jkt") ) ) <<
            ( QVariantList() << ( QVariantList() << QByteArray("Jan Kundrat") << QByteArray() << QByteArray("flaska.net") << QByteArray("jkt") ) ) <<
            ( QVariantList() << ( QVariantList() << QByteArray("Jan Kundrat") << QByteArray() << QByteArray("flaska.net") << QByteArray("jkt") ) );
}*/

void ImapMessageTest::testMailAddresEq()
{
    using namespace Imap::Message;

    QCOMPARE( MailAddress(), MailAddress() );
    QCOMPARE( MailAddress( "name", "adl", "mailbox", "host" ),
              MailAddress( QVariantList() <<
                  QByteArray("name") << QByteArray("adl") << QByteArray("mailbox") << QByteArray("host"),
                  QByteArray(), 0 ) );
}

void ImapMessageTest::testMailAddresNe()
{
    QFETCH( Imap::Message::MailAddress, expected );
    QFETCH( QVariantList, data );
    QVERIFY2( expected != Imap::Message::MailAddress( data, QByteArray(), 0 ), "addresses equal" );
}

void ImapMessageTest::testMailAddresNe_data()
{
    using namespace Imap::Message;

    QTest::addColumn<MailAddress>("expected");
    QTest::addColumn<QVariantList>("data");

    QTest::newRow( "difference-name" ) <<
        MailAddress( "name", "adl", "mailbox", "host" ) <<
        ( QVariantList() << QByteArray("nAme") << QByteArray("adl") << QByteArray("mailbox") << QByteArray("host") );

    QTest::newRow( "difference-adl" ) <<
        MailAddress( "name", "adl", "mailbox", "host" ) <<
        ( QVariantList() << QByteArray("name") << QByteArray("aDl") << QByteArray("mailbox") << QByteArray("host") );

    QTest::newRow( "difference-mailbox" ) <<
        MailAddress( "name", "adl", "mailbox", "host" ) <<
        ( QVariantList() << QByteArray("name") << QByteArray("adl") << QByteArray("maIlbox") << QByteArray("host") );

    QTest::newRow( "difference-host" ) <<
        MailAddress( "name", "adl", "mailbox", "host" ) <<
        ( QVariantList() << QByteArray("name") << QByteArray("adl") << QByteArray("mailbox") << QByteArray("h0st") );

}

void ImapMessageTest::testMailAddressFormat()
{
    QFETCH( Imap::Message::MailAddress, addr );
    QFETCH( QString, pretty );
    QFETCH( QByteArray, addrspec );
    QFETCH( bool, should2047 );

    QCOMPARE( addr.prettyName(Imap::Message::MailAddress::FORMAT_READABLE), pretty );
    QCOMPARE( addr.asSMTPMailbox(), addrspec );
    
    QByteArray full = addr.asMailHeader();
    QByteArray bracketed;
    bracketed.append(" <").append(addrspec).append(">");
    QVERIFY( full.endsWith(bracketed) );
    full.remove(full.size() - bracketed.size(), bracketed.size());
    
    if (should2047) {
        QVERIFY( full.startsWith("=?") );
        QVERIFY( full.endsWith("?=") );
        QCOMPARE( addr.name, Imap::decodeRFC2047String(full) );
    } else {
        QVERIFY( !full.contains("=?") );
        QVERIFY( !full.contains("?=") );
    }
}

void ImapMessageTest::testMailAddressFormat_data()
{
    using namespace Imap::Message;

    QTest::addColumn<MailAddress>("addr");
    QTest::addColumn<QString>("pretty");
    QTest::addColumn<QByteArray>("addrspec");
    QTest::addColumn<bool>("should2047");

    QTest::newRow("simple") <<
        MailAddress( "name", "adl", "mailbox", "host" ) <<
        QString("name <mailbox@host>") <<
        QByteArray("mailbox@host") << false;

    QTest::newRow("domain-literal") <<
        MailAddress( "words name", "adl", "us.er", "[127.0.0.1]" ) <<
        QString("words name <us.er@[127.0.0.1]>") <<
        QByteArray("us.er@[127.0.0.1]") << false;

    QTest::newRow("idn") <<
        MailAddress( "words j. name", "adl", "us.er",
                     QString::fromUtf8("trojit\xC3\xA1.example.com") ) <<
        QString::fromUtf8("words j. name <us.er@trojit\xC3\xA1.example.com>") <<
        QByteArray("us.er@xn--trojit-uta.example.com") << false;

    /* overspecific test: we want to test here that the combining mark
       is normalized before being converted to punycode for the
       IDN. We don't actually care whether it's normalized for
       PRETTYNAME, but we have to give a value for the test. */
#if QT_VERSION >= QT_VERSION_CHECK(4, 8, 0)
    // For some reason, this is broken on OpenSuSE's Qt 4.6. I have no data for
    // Qt 4.7.
    QTest::newRow("idn+normalize") <<
        MailAddress( "words j. name", "adl", "us.er",
                     QString::fromUtf8("trojita\xCC\x81.example.com") ) <<
        QString::fromUtf8("words j. name <us.er@trojita\xCC\x81.example.com>") <<
        QByteArray("us.er@xn--trojit-uta.example.com") << false;
#endif

    QTest::newRow("odd-mailbox") <<
        MailAddress( "words (q) name", "adl", "us er", "example.com" ) <<
        QString("words (q) name <us er@example.com>") <<
        QByteArray("\"us er\"@example.com") << false;

    QTest::newRow("intl-realname") <<
        MailAddress( QString::fromUtf8("words \xE2\x98\xBA name"),
                     "adl", "*", "example.com" ) <<
        QString::fromUtf8("words \xE2\x98\xBA name <*@example.com>") <<
        QByteArray("*@example.com") << true;

    QTest::newRow("intl-with-composed-mailbox") <<
        MailAddress(QString::fromUtf8("Jan Kundrát"), "", "jan.kundrat", "demo.isode.com") <<
        QString::fromUtf8("Jan Kundrát <jan.kundrat@demo.isode.com>") <<
        QByteArray("jan.kundrat@demo.isode.com") << true;
}

void ImapMessageTest::testMailAddressParsing()
{
    QFETCH(QString, textInput);
    QFETCH(Imap::Message::MailAddress, expected);

    Imap::Message::MailAddress actual;
    QVERIFY(Imap::Message::MailAddress::fromPrettyString(actual, textInput));
    QCOMPARE(actual, expected);

    Imap::Message::MailAddress afterUrlTransformation;
    QVERIFY(Imap::Message::MailAddress::fromUrl(afterUrlTransformation, actual.asUrl(), QLatin1String("mailto")));
    QCOMPARE(afterUrlTransformation, expected);
}

void ImapMessageTest::testMailAddressParsing_data()
{
    using namespace Imap::Message;

    QTest::addColumn<QString>("textInput");
    QTest::addColumn<MailAddress>("expected");

    QTest::newRow("trojita-ml") <<
        QString::fromUtf8("trojita@lists.flaska.net") <<
        MailAddress(QString(), QString(), "trojita", "lists.flaska.net");

    QTest::newRow("trojita-ml-with-short-ascii-name") <<
        QString::fromUtf8("Trojita <trojita@lists.flaska.net>") <<
        MailAddress(QString::fromUtf8("Trojita"), QString(), "trojita", "lists.flaska.net");

    /*QTest::newRow("trojita-ml-with-short-ascii-name-quoted") <<
        QString::fromUtf8("\"Trojita\" <trojita@lists.flaska.net>") <<
        MailAddress(QString::fromUtf8("Trojita"), QString(), "trojita", "lists.flaska.net");*/

    QTest::newRow("trojita-ml-with-ascii-name") <<
        QString::fromUtf8("Trojita ML <trojita@lists.flaska.net>") <<
        MailAddress(QString::fromUtf8("Trojita ML"), QString(), "trojita", "lists.flaska.net");

    /*QTest::newRow("trojita-ml-with-ascii-name-quoted") <<
        QString::fromUtf8("\"Trojita ML\" <trojita@lists.flaska.net>") <<
        MailAddress(QString::fromUtf8("Trojita ML"), QString(), "trojita", "lists.flaska.net");*/

    QTest::newRow("trojita-ml-with-short-unicode-name") <<
        QString::fromUtf8("Trojitá <trojita@lists.flaska.net>") <<
        MailAddress(QString::fromUtf8("Trojitá"), QString(), "trojita", "lists.flaska.net");

    /*QTest::newRow("trojita-ml-with-short-unicode-name-quoted") <<
        QString::fromUtf8("\"Trojitá\" <trojita@lists.flaska.net>") <<
        MailAddress(QString::fromUtf8("Trojitá"), QString(), "trojita", "lists.flaska.net");*/

    QTest::newRow("trojita-ml-with-unicode-name") <<
        QString::fromUtf8("Trojitá ML <trojita@lists.flaska.net>") <<
        MailAddress(QString::fromUtf8("Trojitá ML"), QString(), "trojita", "lists.flaska.net");

    /*QTest::newRow("trojita-ml-with-unicode-name-quoted") <<
        QString::fromUtf8("\"Trojitá ML\" <trojita@lists.flaska.net>") <<
        MailAddress(QString::fromUtf8("Trojitá ML"), QString(), "trojita", "lists.flaska.net");*/

    QTest::newRow("jkt-isode-ascii") <<
        QString::fromUtf8("Jan Kundrat <jan.kundrat@demo.isode.com>") <<
        MailAddress(QString::fromUtf8("Jan Kundrat"), QString(), "jan.kundrat", "demo.isode.com");

    /*QTest::newRow("jkt-isode-ascii-quoted") <<
        QString::fromUtf8("\"Jan Kundrat\" <jan.kundrat@demo.isode.com>") <<
        MailAddress(QString::fromUtf8("Jan Kundrat"), QString(), "jan.kundrat", "demo.isode.com");*/

    QTest::newRow("jkt-isode-unicode") <<
        QString::fromUtf8("Jan Kundrát <jan.kundrat@demo.isode.com>") <<
        MailAddress(QString::fromUtf8("Jan Kundrát"), QString(), "jan.kundrat", "demo.isode.com");

    /*QTest::newRow("jkt-isode-unicode-quoted") <<
        QString::fromUtf8("\"Jan Kundrát\" <jan.kundrat@demo.isode.com>") <<
        MailAddress(QString::fromUtf8("Jan Kundrát"), QString(), "jan.kundrat", "demo.isode.com");*/

    QTest::newRow("long-address-with-fancy-symbols") <<
        QString::fromUtf8("Some Fünny Äddre¶ <this-is_a.test+some-thin_g.yay@foo-blah.d_o-t.example.org>") <<
        MailAddress(QString::fromUtf8("Some Fünny Äddre¶"), QString(), "this-is_a.test+some-thin_g.yay", "foo-blah.d_o-t.example.org");

    QTest::newRow("long-address-with-fancy-symbols-no-human-name") <<
        QString::fromUtf8("this-is_a.test+some-thin_g.yay@foo-blah.d_o-t.example.org") <<
        MailAddress(QString(), QString(), "this-is_a.test+some-thin_g.yay", "foo-blah.d_o-t.example.org");
}

void ImapMessageTest::testMessage()
{
}

void ImapMessageTest::testMessage_data()
{
}


TROJITA_HEADLESS_TEST( ImapMessageTest )

namespace QTest {

template<> char * toString( const Imap::Message::Envelope& resp )
{
    QByteArray buf;
    QTextStream stream( &buf );
    stream << resp;
    stream.flush();
    return qstrdup( buf.data() );
}

template<> char * toString( const Imap::Message::MailAddress& resp )
{
    QByteArray buf;
    QTextStream stream( &buf );
    stream << resp;
    stream.flush();
    return qstrdup( buf.data() );
}

}
