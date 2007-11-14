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
#include <QMutexLocker>
#include <QProcess>
#include "Parser.h"

/*
 * Parser interface considerations:
 *
 * - Parser receives comments and gives back some kind of ID for tracking the
 *   command state
 * - "High-level stuff" like "has this command already finished" should be
 *   implemented on higher level
 * - Due to command pipelining, there's no way to find out that this untagged
 *   reply we just received was triggered by FOO command
 *
 * Interface:
 *
 * - One function per command
 * - Each received reply emits a signal (Qt-specific stuff now)
 *
 *
 * Usage example FIXME DRAFT:
 *
 *  Imap::Parser parser;
 *  Imap::CommandHandle res = parser.deleteFolder( "foo mailbox/bar/baz" );
 *
 *
 *
 * How it works under the hood:
 *
 * - When there are any data available on the net, process them ASAP
 * - When user queues a command, process it ASAP
 * - You can't block the caller of the queueCommand()
 *
 * So, how to implement this?
 *
 * - Whenever something interesting happens (data/command/exit
 *   requested/available), we ask the worker thread to do something
 *
 * */

namespace Imap {

Parser::Parser( QObject* parent, std::auto_ptr<QIODevice> socket ): QObject(parent), _socket(socket), _lastTagUsed(0), _workerThread( this ), _workerStop( false )
{
    _workerThread.start();
    connect( _socket.get(), SIGNAL( readyRead() ), this, SLOT( socketReadyRead() ) );
}

Parser::~Parser()
{
    _workerStopMutex.lock();
    _workerStop = true;
    _workerStopMutex.unlock();
    _workerSemaphore.release();
    _workerThread.wait();

    if ( QProcess* proc = dynamic_cast<QProcess*>( _socket.get() ) ) {
        // Be nice to it, let it die peacefully before using an axe
        proc->terminate();
        proc->waitForFinished(200);
        proc->kill();
    }
}

CommandHandle Parser::noop()
{
    return queueCommand( Commands::ATOM, "NOOP" );
}

CommandHandle Parser::logout()
{
    return queueCommand( Commands::ATOM, "LOGOUT" );
}

CommandHandle Parser::capability()
{
    return queueCommand( Commands::ATOM, "CAPABILITY" );
}

CommandHandle Parser::startTls()
{
    // FIXME: actual implementation  (which won't be here, but in the event loop
    return queueCommand( Commands::SPECIAL, "STARTTLS" );
}

CommandHandle Parser::authenticate( /*Authenticator FIXME*/)
{
    return queueCommand( Commands::ATOM, "AUTHENTICATE" );
}

CommandHandle Parser::login( const QString& username, const QString& password )
{
    return queueCommand( Commands::Command( "LOGIN" ) <<
            Commands::PartOfCommand( username ) << Commands::PartOfCommand( password ) );
}

CommandHandle Parser::select( const QString& mailbox )
{
    return queueCommand( Commands::Command( "SELECT" ) << mailbox );
}

CommandHandle Parser::examine( const QString& mailbox )
{
    return queueCommand( Commands::Command( "EXAMINE" ) << mailbox );
}

CommandHandle Parser::deleteMailbox( const QString& mailbox )
{
    return queueCommand( Commands::Command( "DELETE" ) << mailbox );
}

CommandHandle Parser::create( const QString& mailbox )
{
    return queueCommand( Commands::Command( "CREATE" ) << mailbox );
}

CommandHandle Parser::rename( const QString& oldName, const QString& newName )
{
    return queueCommand( Commands::Command( "RENAME" ) << oldName << newName );
}

CommandHandle Parser::subscribe( const QString& mailbox )
{
    return queueCommand( Commands::Command( "SUBSCRIBE" ) << mailbox );
}

CommandHandle Parser::unSubscribe( const QString& mailbox )
{
    return queueCommand( Commands::Command( "UNSUBSCRIBE" ) << mailbox );
}

CommandHandle Parser::list( const QString& reference, const QString& mailbox )
{
    return queueCommand( Commands::Command( "LIST" ) << reference << mailbox );
}

CommandHandle Parser::lSub( const QString& reference, const QString& mailbox )
{
    return queueCommand( Commands::Command( "LSUB" ) << reference << mailbox );
}

CommandHandle Parser::status( const QString& mailbox, const QStringList& fields )
{
    return queueCommand( Commands::Command( "STATUS" ) << mailbox <<
            Commands::PartOfCommand( Commands::ATOM, "(" + fields.join(" ") +")" )
            );
}

CommandHandle Parser::append( const QString& mailbox, const QString& message, const QStringList& flags, const QDateTime& timestamp )
{
    Commands::Command command( "APPEND" );
    command << mailbox;
    if ( flags.count() )
        command << Commands::PartOfCommand( Commands::ATOM, "(" + flags.join(" ") + ")" );
    if ( timestamp.isValid() )
        command << Commands::PartOfCommand( timestamp.toString() );
    command << Commands::PartOfCommand( Commands::LITERAL, message );

    return queueCommand( command );
}

CommandHandle Parser::check()
{
    return queueCommand( Commands::ATOM, "CHECK" );
}

CommandHandle Parser::close()
{
    return queueCommand( Commands::ATOM, "CLOSE" );
}

CommandHandle Parser::expunge()
{
    return queueCommand( Commands::ATOM, "EXPUNGE" );
}

CommandHandle Parser::_searchHelper( const QString& command, const QStringList& criteria, const QString& charset )
{
    if ( !criteria.count() || criteria.count() % 2 )
        throw InvalidArgument("Invalid search criteria");

    Commands::Command cmd( command );

    if ( !charset.isEmpty() )
        cmd << "CHARSET" << charset;

    for ( QStringList::const_iterator it = criteria.begin(); it != criteria.end(); ++it )
        cmd << *it;

    return queueCommand( cmd );
}

CommandHandle Parser::fetch( const Sequence& seq, const QStringList& items )
{
    return queueCommand( Commands::Command( "FETCH" ) <<
            seq.toString() <<
            Commands::PartOfCommand( Commands::ATOM, items.join(" ") ) );
}

CommandHandle Parser::store( const Sequence& seq, const QString& item, const QString& value )
{
    return queueCommand( Commands::Command( "STORE" ) << seq.toString() <<
            Commands::PartOfCommand( Commands::ATOM, item ) <<
            Commands::PartOfCommand( Commands::ATOM, value )
            );
}

CommandHandle Parser::copy( const Sequence& seq, const QString& mailbox )
{
    return queueCommand( Commands::Command("COPY") << seq.toString() << mailbox );
}

CommandHandle Parser::uidFetch( const Sequence& seq, const QStringList& items )
{
    return queueCommand( Commands::Command( "UID FETCH" ) <<
            seq.toString() <<
            Commands::PartOfCommand( Commands::ATOM, items.join(" ") ) );
}

CommandHandle Parser::xAtom( const Commands::Command& cmd )
{
    return queueCommand( cmd );
}

CommandHandle Parser::unSelect()
{
    return queueCommand( Commands::ATOM, "UNSELECT" );
}

CommandHandle Parser::idle()
{
    return queueCommand( Commands::SPECIAL, "IDLE" );
}

CommandHandle Parser::queueCommand( Commands::Command command )
{
    QMutexLocker locker( &_cmdMutex );
    QString tag = generateTag();
    command.addTag( tag );
    _cmdQueue.push_back( command );
    _workerSemaphore.release();
    return tag;
}

void Parser::queueResponse( const Responses::Response& resp )
{
    QMutexLocker locker( &_respMutex );
    _respQueue.push_back( resp );
    emit responseReceived();
}

QString Parser::generateTag()
{
    return QString( "y%1" ).arg( _lastTagUsed++ );
}

bool Parser::executeIfPossible()
{
    QMutexLocker locker( &_cmdMutex );
    if ( !_cmdQueue.empty() ) {
        Commands::Command cmd = _cmdQueue.front();
        _cmdQueue.pop_front();
        return executeACommand( cmd );
    } else
        return false;
}

bool Parser::executeACommand( const Commands::Command& cmd )
{
    QByteArray buf;
    buf.append( cmd._tag );
    for ( QList<Commands::PartOfCommand>::const_iterator it = cmd._cmds.begin(); it != cmd._cmds.end(); ++it ) {
        buf.append( ' ' );
        switch( (*it)._kind ) {
            case Commands::ATOM:
                buf.append( (*it)._text );
                break;
            case Commands::QUOTED_STRING:
                buf.append( '"' );
                buf.append( (*it)._text );
                buf.append( '"' );
                break;
            case Commands::LITERAL:
                if ( true ) { // FIXME: only if LITERAL+ is enabled
                    buf.append( '{' );
                    buf.append( QByteArray::number( (*it)._text.size() ) );
                    buf.append( "+}\r\n" );
                    buf.append( (*it)._text );
                }
                break;
            case Commands::SPECIAL:
                {
                    const QString& identifier = (*it)._text;
                    if ( identifier == "STARTTLS" ) {
                        buf.append( "STARTTLS\r\n" );
                        // FIXME: real stuff here
                        _socket->write( buf );
                        return true;
                    }
                    // FIXME: other cases...
                }
                break;
        }
    }
    buf.append( "\r\n" );
    _socket->write( buf );

    return true;
}

void Parser::socketReadyRead()
{
    _workerSemaphore.release();
}

void Parser::socketDisconected()
{
    //FIXME
}

void Parser::processLine( const QByteArray& line )
{
    if ( line.startsWith( "* " ) ) {
        parseUntagged( line );
    } else if ( line.startsWith( "+ " ) ) {
        // Command Continuation Request which really shouldn't happen here
        throw ContinuationRequest( line.constData() );
    } else {
        parseTagged( line );
    }
}

void Parser::parseUntagged( const QByteArray& line )
{
}

void Parser::parseTagged( const QByteArray& line )
{
    QList<QByteArray> splitted = line.split( ' ' );
    if ( splitted.count() < 3 )
        throw NoData( line.constData() );

    /* line is guaranted to have at least three items */
    QList<QByteArray>::const_iterator it = splitted.begin();

    const QString tag( (*it).constData() );
    ++it;

    const QByteArray resultStr( (*it).toUpper() );
    ++it;
    CommandResult result;
    if ( resultStr == "OK" )
        result = OK;
    else if ( resultStr == "NO" )
        result = NO;
    else if ( resultStr == "BAD" )
        result = BAD;
    else
        throw UnknownCommandResult( line.constData() );

    Responses::Code respCode = Responses::NONE;
    QList<QByteArray> respCodeList = _parseResponseCode( it, splitted.end() );
    if ( respCodeList.count() ) {
        const QByteArray r = (*(respCodeList.begin())).toUpper();
        if ( r == "ALERT" )
            respCode = Responses::ALERT;
        else if ( r == "BADCHARSET" )
            respCode = Responses::BADCHARSET;
        else if ( r == "CAPABILITY" )
            respCode = Responses::CAPABILITY;
        else if ( r == "PARSE" )
            respCode = Responses::PARSE;
        else if ( r == "PERMANENTFLAGS" )
            respCode = Responses::PERMANENTFLAGS;
        else if ( r == "READ-ONLY" )
            respCode = Responses::READ_ONLY;
        else if ( r == "READ-WRITE" )
            respCode = Responses::READ_WRITE;
        else if ( r == "TRYCREATE" )
            respCode = Responses::TRYCREATE;
        else if ( r == "UIDNEXT" )
            respCode = Responses::UIDNEXT;
        else if ( r == "UIDVALIDITY" )
            respCode = Responses::UIDVALIDITY;
        else if ( r == "UNSEEN" )
            respCode = Responses::UNSEEN;
        else
            respCode = Responses::ATOM;

        if ( respCode != Responses::ATOM )
            respCodeList.pop_front();

        // now perform validity check
        switch ( respCode ) {
            case Responses::ALERT:
            case Responses::PARSE:
            case Responses::READ_ONLY:
            case Responses::READ_WRITE:
            case Responses::TRYCREATE:
                // check for "no more stuff"
                if ( respCodeList.count() )
                    throw InvalidResponseCode( line.constData() );
                break;
            case Responses::UIDNEXT:
            case Responses::UIDVALIDITY:
            case Responses::UNSEEN:
                // check for "number only"
                {
                if ( ( respCodeList.count() != 1 ) )
                    throw InvalidResponseCode( line.constData() );
                bool ok;
                respCodeList.first().toUInt( &ok );
                if ( !ok )
                    throw InvalidResponseCode( line.constData() );
                }
                break;
            default:
                // no sanity check here
                break;
        }
    }

    QByteArray text;
    bool doSpace = false;
    while ( it != splitted.end() ) {
        if ( doSpace )
            text.append( ' ' );
        doSpace = true;
        text.append( *it );
        ++it;
    }
    Q_ASSERT( text.endsWith( "\r\n" ) );
    text.chop(2);

    Responses::Response resp( tag, result, respCode, respCodeList, text);
    queueResponse( resp );
}

QList<QByteArray> Parser::_parseResponseCode( QList<QByteArray>::const_iterator& begin, const QList<QByteArray>::const_iterator& end ) const
{
    QList<QByteArray> resp;

    if ( (*begin).startsWith( '[' ) && (*begin).endsWith( ']' ) ) {

        // only "[fooobar]"
        QByteArray str = *begin;
        str.chop(1);
        str = str.right( str.size() - 1 );
        resp << str;
        ++begin;

    } else if ( (*begin).startsWith( '[' ) ) {
        QByteArray str = *begin;
        str = str.right( str.size() - 1 );
        resp << str;
        ++begin;

        while ( begin != end ) {
            if ( (*begin).endsWith( ']' ) ) {
                // this is the last item
                str = *begin;
                str.chop( 1 );
                resp << str;
                ++begin;
                break;
            } else {
                resp << *begin;
                ++begin;
            }
        }

    }

    return resp;
}


void WorkerThread::run()
{
    _parser->_workerStopMutex.lock();
    while ( ! _parser->_workerStop ) {
        _parser->_workerStopMutex.unlock();
        _parser->_workerSemaphore.acquire();
        _parser->executeIfPossible();
        while ( _parser->_socket->canReadLine() )
            _parser->processLine( _parser->_socket->readLine() );
        _parser->_workerStopMutex.lock();
    }
}

QTextStream& operator<<( QTextStream& stream, const CommandResult& res )
{
    switch ( res ) {
        case OK:
            stream << "OK";
            break;
        case NO:
            stream << "NO";
            break;
        case BAD:
            stream << "BAD";
            break;
    }
    return stream;
}

}
#include "Parser.moc"
