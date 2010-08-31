/* Copyright (C) 2007 - 2010 Jan Kundrát <jkt@flaska.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "TaskFactory.h"
#include "OpenConnectionTask.h"
#include "GetAnyConnectionTask.h"
#include "Imap/Model/IdleLauncher.h"
#include "Imap/Parser/Parser.h"

namespace Imap {
namespace Mailbox {

#define CREATE_FAKE_TASK(X) class Fake_##X: public X { \
public: virtual void perform() {} Fake_##X( Imap::Parser* _parser ) { parser = _parser; QTimer::singleShot( 0, this, SLOT(slotSucceed()) ); } \
};

CREATE_FAKE_TASK(OpenConnectionTask)

#undef CREATE_FAKE_TASK

TaskFactory::~TaskFactory()
{
}

OpenConnectionTask* TaskFactory::createOpenConnectionTask( Model* _model )
{
    return new OpenConnectionTask( _model );
}

GetAnyConnectionTask* TaskFactory::createGetAnyConnectionTask( Model* _model )
{
    return new GetAnyConnectionTask( _model );
}



TestingTaskFactory::TestingTaskFactory(): TaskFactory(), fakeOpenConnectionTask(false)
{
}

Parser* TestingTaskFactory::newParser( Model* model )
{
    Parser* parser = new Parser( model, model->_socketFactory->create(), ++model->lastParserId );
    Model::ParserState parserState = Model::ParserState( parser, 0, Model::ReadOnly, CONN_STATE_NONE, 0 );
    parserState.idleLauncher = new IdleLauncher( model, parser );
    QObject::connect( parser, SIGNAL(responseReceived()), model, SLOT(responseReceived()) );
    QObject::connect( parser, SIGNAL(disconnected(const QString)), model, SLOT(slotParserDisconnected(const QString)) );
    QObject::connect( parser, SIGNAL(connectionStateChanged(Imap::ConnectionState)), model, SLOT(handleSocketStateChanged(Imap::ConnectionState)) );
    QObject::connect( parser, SIGNAL(sendingCommand(QString)), model, SLOT(parserIsSendingCommand(QString)) );
    QObject::connect( parser, SIGNAL(parseError(QString,QString,QByteArray,int)), model, SLOT(slotParseError(QString,QString,QByteArray,int)) );
    QObject::connect( parser, SIGNAL(lineReceived(QByteArray)), model, SLOT(slotParserLineReceived(QByteArray)) );
    QObject::connect( parser, SIGNAL(lineSent(QByteArray)), model, SLOT(slotParserLineSent(QByteArray)) );
    QObject::connect( parser, SIGNAL(idleTerminated()), model, SLOT(idleTerminated()) );
    model->_parsers[ parser ] = parserState;
    return parser;
}

OpenConnectionTask* TestingTaskFactory::createOpenConnectionTask( Model *_model )
{
    if ( fakeOpenConnectionTask ) {
        return new Fake_OpenConnectionTask( newParser( _model ) );
    } else {
        return TaskFactory::createOpenConnectionTask( _model );
    }
}

}
}
