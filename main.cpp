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
#include <QTcpSocket>
#include <QStringList>
#include "Imap/Parser.h"
#include "Imap/Command.h"

int main( int argc, char** argv) {
    QTextStream Err( stderr );

    QAbstractSocket* sock = new QTcpSocket();
    Imap::Commands::AbstractCommand* command = 0;

    Err << "*** Begin testing: ***" << endl << endl;

#define DUMP0(X) Err << #X << ":" << endl; command = new Imap::Commands::X(); Err << *command << endl; delete command; command = 0;
#define DUMP1(X,Y) Err << #X << "(" << #Y << "):" << endl; command = new Imap::Commands::X(#Y); Err << *command << endl; delete command; command = 0;
#define DUMP2(X,Y,Z) Err << #X << "(" << #Y << ", " << #Z << "):" << endl; command = new Imap::Commands::X(#Y, #Z); Err << *command << endl; delete command; command = 0;

    DUMP0(Capability);
    DUMP0(Noop);
    DUMP0(Logout);

    Err << endl;

    DUMP0(StartTls);
    DUMP0(Authenticate);
    DUMP2(Login, user, password);

    Err << endl;

    DUMP1(Select, foo mailbox);
    DUMP1(Examine, foo mailbox);
    DUMP1(Create, foo mailbox);
    DUMP1(Delete, foo mailbox);
    DUMP2(Rename, foo mailbox old, bar mailbox new);
    DUMP1(Subscribe, foo mailbox);
    DUMP1(UnSubscribe, foo mailbox);
    DUMP2(List, , );
    DUMP2(List, prefix, );
    DUMP2(LSub, , smrt);
    Err << "Status" << "(some mailbox, (a b c) ):" << endl; command = new Imap::Commands::Status("some mailbox", QStringList("a") << "b" << "c"); Err << *command << endl; delete command; command = 0;
    Err << "Status" << "(some mailbox, () ):" << endl; command = new Imap::Commands::Status("some mailbox", QStringList()); Err << *command << endl; delete command; command = 0;
    Err << "Status" << "(some mailbox, (ahoj) ):" << endl; command = new Imap::Commands::Status("some mailbox", QStringList("ahoj")); Err << *command << endl; delete command; command = 0;

    DUMP2(Append, some mailbox, some extra long message literal that is absolutely uninteresting here);
    Err << "Append" << "(some mailbox, some message, (flagA flagB) ):" << endl; command = new Imap::Commands::Append(QString("some mailbox"), "some message", QStringList("flagA") << "flagB"); Err << *command << endl; delete command; command = 0;
    // FIXME: test date

    DUMP0(UnSelect);
    DUMP0(Check);
    DUMP0(Idle);
#undef DUMP0
#undef DUMP1

    Imap::Parser parser( 0, sock );
}
