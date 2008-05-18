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

#include "Imap/SocketFactory.h"
#include "Imap/MailboxModel.h"

int main( int argc, char** argv) {
    QCoreApplication app( argc, argv );

    Imap::Mailbox::SocketFactoryPtr factory(
            new Imap::Mailbox::ProcessSocketFactory( "dovecot",
                QStringList() << "--exec-mail" << "imap" ) );

    Imap::ParserPtr parser( new Imap::Parser( 0, factory->create() ) );
    Imap::Mailbox::CachePtr cache;
    Imap::Mailbox::AuthenticatorPtr auth;

    Imap::Mailbox::MailboxModel model( 0, cache, auth, parser, "trms", true, Imap::THREAD_NONE );
    
    //QTimer::singleShot( 1500, &app, SLOT(quit()) );
    app.exec();
}
