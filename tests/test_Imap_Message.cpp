/* Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <qtest_kde.h>

#include "test_Imap_Message.h"
#include "test_Imap_Message.moc"

Q_DECLARE_METATYPE(Imap::Message::MailAddress)
Q_DECLARE_METATYPE(Imap::Message::Envelope)
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
        MailAddress( QByteArray(), QByteArray(), "exmaple.org", "spam" );

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

void ImapMessageTest::testMessage()
{
}

void ImapMessageTest::testMessage_data()
{
}


QTEST_KDEMAIN_CORE( ImapMessageTest )

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
