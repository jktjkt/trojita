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

#include "Imap/Parser/Parser.h"
#include "Imap/Streams/SocketFactory.h"
#include "Demo/ParserMonitor.h"

int main( int argc, char** argv) {
    QCoreApplication app( argc, argv );

    Imap::Mailbox::SocketFactoryPtr factory(
            new Imap::Mailbox::UnixProcessSocketFactory( "/usr/sbin/dovecot",
                QStringList() << "--exec-mail" << "imap" ) );
    Imap::ParserPtr parser( new Imap::Parser( 0, factory->create() ) );
    Demo::ParserMonitor monitor( 0, parser.get() );

    parser->capability();
    parser->namespaceCommand();
    parser->noop();
    parser->list( "", "%" );
    parser->select( "MBOX-torture-test" );
    //parser->fetch( Imap::Sequence(1), QStringList() << "UID" << "BODY[HEADER.FIELDS (Subject Received)]<100.10>" << "UID" );
    //parser->fetch( Imap::Sequence(1), QStringList() << "RFC822.HEADER" );
    //parser->fetch( Imap::Sequence(1, 7), QStringList() << "bodyStructure" );
    //parser->fetch( Imap::Sequence(1, 6), QStringList() << "FLAGS" << "BODY[HEADER.FIELDS (DATE FROM)]");
    //parser->fetch( Imap::Sequence(1, 6), QStringList() << "UID" << /*"RFC822.HEADER" <<*/ "INTERNALDATE" << "RFC822.SIZE" << "FLAGS" );
    parser->fetch( Imap::Sequence(1), QStringList() << "body[]" );
    parser->logout();

    QTimer::singleShot( 1500, &app, SLOT(quit()) );
    app.exec();
}
