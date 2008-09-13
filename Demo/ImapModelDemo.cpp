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
#include <QApplication>
#include <QTimer>
#include <QTreeView>

#include "Imap/SocketFactory.h"
#include "Imap/Model.h"

#include "Imap/ModelTest/modeltest.h"

int main( int argc, char** argv) {
    QApplication app( argc, argv );

    Imap::Mailbox::SocketFactoryPtr factory(
            new Imap::Mailbox::ProcessSocketFactory( "dovecot",
                QStringList() << "--exec-mail" << "imap" )
            /*new Imap::Mailbox::ProcessSocketFactory( "ssh",
                QStringList() << "sosna.fzu.cz" << "/usr/sbin/imapd" )*/
            );

    Imap::ParserPtr parser( new Imap::Parser( 0, factory->create() ) );
    Imap::Mailbox::CachePtr cache( new Imap::Mailbox::NoCache() );
    Imap::Mailbox::AuthenticatorPtr auth;

    Imap::Mailbox::Model model( 0, cache, auth, parser );

    new ModelTest( &model );
    
    QTreeView tree;
    tree.setModel( &model );
    tree.setWindowTitle( "IMAP mailbox" );
    tree.setUniformRowHeights( true );
    tree.show();

    //QTimer::singleShot( 15000, &app, SLOT(quit()) );
    return app.exec();
}
