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
#include <QStringList>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>
#include "Imap/Parser.h"
#include "Imap/Command.h"
#include <unistd.h>

int main( int argc, char** argv) {
    QCoreApplication app( argc, argv );
    QTextStream Err(stderr);
    std::auto_ptr<QIODevice> sock( new QProcess() );
    //static_cast<QProcess*>( sock.get() )->start( "dovecot", QStringList() << "--exec-mail" << "imap" );
    static_cast<QProcess*>( sock.get() )->start( "/home/jkt/work/prog/trojita/trunk/wrapper.sh" );
    static_cast<QProcess*>( sock.get() )->waitForStarted();

    Imap::Parser parser( 0, sock );

    parser.capability();
    parser.noop();

    parser.startTls();
    parser.authenticate();
    parser.login("user", "password with spaces");

    parser.select("foo mailbox");
    parser.examine("foo mailbox");
    parser.create("foo mailbox");
    parser.deleteMailbox("foo mailbox");
    parser.rename("foo mailbox old", "bar mailbox new that is so loooooooooooooooooooooooong"
            "that it doesn't fit on a single line, despite the fact that it is also veeeeery"
            "loooooooooooong");
    parser.subscribe("smrt '");
    parser.unSubscribe("smrt '");
    parser.list("", "");
    parser.list("prefix", "");
    parser.lSub("", "smrt");
    parser.status( "gfooo", QStringList("bar") << "baz" );
    parser.append( "mbox", "message");
    parser.append( "mbox with\nLF", "message\nwhich is a bit longer", QStringList("flagA") << "flagB");

    parser.check();
    parser.close();
    parser.expunge();
    try {
        parser.search( QStringList() );
        Err << "BAD, empty search shouldn't be allowed" << endl;
    } catch ( const Imap::InvalidArgumentException& e ) {
        Err << "[Good, empty searches caught]" << endl;
    }
    try {
        parser.search( QStringList("foo") );
        Err << "BAD, odd search criteria shouldn't be allowed" << endl;
    } catch ( const Imap::InvalidArgumentException& e ) {
        Err << "[Good, even search criteria caught]" << endl;
    }
    parser.search( QStringList("foo") << "bar", "utf-8" );


    parser.xAtom( Imap::Commands::Command() << "XBLURDYBLOOP" << "FOOBAR" << "baz bazbazbaz 333" );

    parser.unSelect();
    parser.idle();

    parser.logout();

    QTimer::singleShot( 1500, &app, SLOT(quit()) );
    app.exec();
}
