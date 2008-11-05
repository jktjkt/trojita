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
#include <QTime>
#include "Parser.h"
#include "rfccodecs.h"
#include "LowLevelParser.h"

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

Parser::Parser( QObject* parent, SocketPtr socket ): QObject(parent), _socket(socket), _lastTagUsed(0), _workerThread( this ), _workerStop( false )
{
    _workerThread.start();
    Q_ASSERT( _socket.get() );
    connect( _socket.get(), SIGNAL( readyRead() ), this, SLOT( socketReadyRead() ) );
}

Parser::~Parser()
{
    disconnect( _socket.get(), SIGNAL( readyRead() ), this, SLOT( socketReadyRead() ) );
    _workerStopMutex.lock();
    _workerStop = true;
    _workerStopMutex.unlock();
    _workerSemaphore.release();
    _workerThread.wait();
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
        throw InvalidArgument("Function called with invalid search criteria");

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
            Commands::PartOfCommand( Commands::ATOM, '(' + items.join(" ") + ')' ) );
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

CommandHandle Parser::namespaceCommand()
{
    return queueCommand( Commands::ATOM, "NAMESPACE" );
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

void Parser::queueResponse( const std::tr1::shared_ptr<Responses::AbstractResponse>& resp )
{
    QMutexLocker locker( &_respMutex );
    _respQueue.push_back( resp );
    emit responseReceived();
}

bool Parser::hasResponse() const
{
    QMutexLocker locker( &_respMutex );
    return ! _respQueue.empty();
}

std::tr1::shared_ptr<Responses::AbstractResponse> Parser::getResponse()
{
    QMutexLocker locker( &_respMutex );
    std::tr1::shared_ptr<Responses::AbstractResponse> ptr;
    if ( _respQueue.empty() )
        return ptr;
    ptr = _respQueue.front();
    _respQueue.pop_front();
    return ptr;
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
                    } else if ( identifier == "IDLE" ) {
                        // FIXME: IDLE
                    } else {
                        throw InvalidArgument( std::string("Dunno how to handle \"special\" command ") +
                                identifier.toStdString() );
                    }
                }
                break;
        }
    }
    buf.append( "\r\n" );
    _socket->write( buf );
    _socket->waitForBytesWritten( -1 );

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

void Parser::processLine( QByteArray line )
{
    if ( line.startsWith( "* " ) ) {
        // check for literals
        int oldSize = 0;

        const int timeout = 5000;
        QTime timer;
        while ( line.endsWith( "}\r\n" ) ) {
            timer.restart();
            // find how many bytes to read
            int offset = line.lastIndexOf( '{' );
            if ( offset < oldSize )
                throw ParseError( "Got unmatched '}'", line, line.size() - 3 );
            bool ok;
            int number = line.mid( offset + 1, line.size() - offset - 4 ).toInt( &ok );
            if ( !ok )
                throw ParseError( "Can't parse numeric literal size", line, offset );
            if ( number < 0 )
                throw ParseError( "Negative literal size", line, offset );

            // now keep pestering our QIODevice until we manage to read as much
            // data as we want
            oldSize = line.size();
            QByteArray buf = _socket->read( number );
            while ( buf.size() < number ) {
                if ( timer.elapsed() > timeout ) {
                    IODeviceSocket* ioSock = qobject_cast<IODeviceSocket*>( _socket.get() );
                    if ( ioSock ) {
                        QProcess* proc = qobject_cast<QProcess*>( ioSock->device() );
                        if ( proc && proc->state() != QProcess::Running ) {
                            // It's dead, Jim. Unfortunately we can't output more debug
                            // info, as errorString() might contain completely useless
                            // stuff from previous failed waitFor*(). Oh noes.
                            throw SocketException( "The QProcess is dead" );
                        }
                    }

                    QByteArray out;
                    QTextStream s( &out );
                    s << "Reading a literal took too long (line " << line.size() <<
                        " bytes so far, buffer " << buf.size() << ", expected literal size " <<
                        number << ")";
                    s.flush();
                    // FIXME: we need something more flexible, including restart
                    // of failed requests...
                    throw SocketTimeout( out.constData() );
                }
                _socket->waitForReadyRead( 500 );
                buf.append( _socket->read( number - buf.size() ) );
            }
            line += buf;
            // as we've had read a literal, we have to read rest of the line as well
            line += _socket->readLine();
        }
        queueResponse( parseUntagged( line ) );
    } else if ( line.startsWith( "+ " ) ) {
        // Command Continuation Request which really shouldn't happen here
        throw ContinuationRequest( line.constData() );
    } else {
        queueResponse( parseTagged( line ) );
    }
}

std::tr1::shared_ptr<Responses::AbstractResponse> Parser::parseUntagged( const QByteArray& line )
{
    int pos = 2;
    uint number;
    try {
        number = LowLevelParser::getUInt( line, pos );
        ++pos;
    } catch ( ParseError& ) {
        return _parseUntaggedText( line, pos );
    }
    return _parseUntaggedNumber( line, pos, number );
}

std::tr1::shared_ptr<Responses::AbstractResponse> Parser::_parseUntaggedNumber(
        const QByteArray& line, int& start, const uint number )
{
    if ( start == line.size() )
        // number and nothing else
        throw NoData( line, start );

    QByteArray kindStr = LowLevelParser::getAtom( line, start );
    Responses::Kind kind;
    try {
        kind = Responses::kindFromString( kindStr );
    } catch ( UnrecognizedResponseKind& e ) {
        throw UnrecognizedResponseKind( e.what(), line, start );
    }

    switch ( kind ) {
        case Responses::EXISTS:
        case Responses::RECENT:
        case Responses::EXPUNGE:
            // no more data should follow
            if ( start >= line.size() )
                throw TooMuchData( line, start );
            else if ( line.mid(start) != QByteArray( "\r\n" ) )
                throw UnexpectedHere( line, start ); // expected CRLF
            else
                try {
                    return std::tr1::shared_ptr<Responses::AbstractResponse>(
                            new Responses::NumberResponse( kind, number ) );
                } catch ( UnexpectedHere& e ) {
                    throw UnexpectedHere( e.what(), line, start );
                }
            break;

        case Responses::FETCH:
            return std::tr1::shared_ptr<Responses::AbstractResponse>(
                    new Responses::Fetch( number, line, start ) );
            break;

        default:
            break;
    }
    throw UnexpectedHere( line, start );
}

std::tr1::shared_ptr<Responses::AbstractResponse> Parser::_parseUntaggedText(
        const QByteArray& line, int& start )
{
    Responses::Kind kind;
    try {
        kind = Responses::kindFromString( LowLevelParser::getAtom( line, start ) );
    } catch ( UnrecognizedResponseKind& e ) {
        throw UnrecognizedResponseKind( e.what(), line, start );
    }
    ++start;
    if ( start == line.size() && kind != Responses::SEARCH )
        throw NoData( line, start );
    switch ( kind ) {
        case Responses::CAPABILITY:
            {
                QStringList capabilities;
                QList<QByteArray> list = line.mid( start ).split( ' ' );
                for ( QList<QByteArray>::const_iterator it = list.begin(); it != list.end(); ++it ) {
                    QByteArray str = *it;
                    if ( str.endsWith( "\r\n" ) )
                        str.chop(2);
                    capabilities << str;
                }
                if ( !capabilities.count() )
                    throw NoData( line, start );
                return std::tr1::shared_ptr<Responses::AbstractResponse>(
                        new Responses::Capability( capabilities ) );
            }
        case Responses::OK:
        case Responses::NO:
        case Responses::BAD:
        case Responses::PREAUTH:
        case Responses::BYE:
            return std::tr1::shared_ptr<Responses::AbstractResponse>(
                    new Responses::State( QString::null, kind, line, start ) );
        case Responses::LIST:
        case Responses::LSUB:
            return std::tr1::shared_ptr<Responses::AbstractResponse>(
                    new Responses::List( kind, line, start ) );
        case Responses::FLAGS:
            return std::tr1::shared_ptr<Responses::AbstractResponse>(
                    new Responses::Flags( line, start ) );
        case Responses::SEARCH:
            {
                QList<uint> numbers;
                while ( start < line.size() - 2 ) {
                    try {
                        uint number = LowLevelParser::getUInt( line, start );
                        numbers << number;
                        ++start;
                    } catch ( ParseError& ) {
                        throw UnexpectedHere( line, start );
                    }
                }
                return std::tr1::shared_ptr<Responses::AbstractResponse>(
                        new Responses::Search( numbers ) );
            }
        case Responses::STATUS:
            return std::tr1::shared_ptr<Responses::AbstractResponse>(
                    new Responses::Status( line, start ) );
        case Responses::NAMESPACE:
            return std::tr1::shared_ptr<Responses::AbstractResponse>(
                    new Responses::Namespace( line, start ) );
        default:
            throw UnexpectedHere( line, start );
    }
}

std::tr1::shared_ptr<Responses::AbstractResponse> Parser::parseTagged( const QByteArray& line )
{
    int pos = 0;
    const QByteArray tag = LowLevelParser::getAtom( line, pos );
    ++pos;
    const Responses::Kind kind = Responses::kindFromString( LowLevelParser::getAtom( line, pos ) );
    ++pos;
    
    return std::tr1::shared_ptr<Responses::AbstractResponse>(
            new Responses::State( tag, kind, line, pos ) );
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


QString Sequence::toString() const
{
    if ( _lo == _hi )
        return QString::number( _lo );
    else
        return QString::number( _lo ) + ':' + QString::number( _hi );
}

QTextStream& operator<<( QTextStream& stream, const Sequence& s )
{
    return stream << s.toString();
}

}

#include "Parser.moc"
