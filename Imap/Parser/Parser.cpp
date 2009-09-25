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
#include <QDebug>
#include <QStringList>
#include <QMutexLocker>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include "Parser.h"
#include "Imap/Encoders.h"
#include "LowLevelParser.h"
#include "../../Streams/IODeviceSocket.h"

//#define PRINT_TRAFFIC 100

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

Parser::Parser( QObject* parent, Imap::SocketPtr socket ):
        QObject(parent), _socket(socket), _lastTagUsed(0), _idling(false), _waitForInitialIdle(false),
        _literalPlus(false), _waitingForContinuation(false), _startTlsInProgress(false),
        _readingMode(ReadingLine), _oldLiteralPosition(0)
{
    connect( _socket.get(), SIGNAL( disconnected( const QString& ) ),
             this, SLOT( handleDisconnected( const QString& ) ) );
    connect( _socket.get(), SIGNAL( readyRead() ), this, SLOT( handleReadyRead() ) );
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
    return queueCommand( Commands::STARTTLS, "STARTTLS" );
}

#if 0
CommandHandle Parser::authenticate( /*Authenticator FIXME*/)
{
    return queueCommand( Commands::ATOM, "AUTHENTICATE" );
}
#endif

CommandHandle Parser::login( const QString& username, const QString& password )
{
    return queueCommand( Commands::Command( "LOGIN" ) <<
            Commands::PartOfCommand( username ) << Commands::PartOfCommand( password ) );
}

CommandHandle Parser::select( const QString& mailbox )
{
    return queueCommand( Commands::Command( "SELECT" ) << encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::examine( const QString& mailbox )
{
    return queueCommand( Commands::Command( "EXAMINE" ) << encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::deleteMailbox( const QString& mailbox )
{
    return queueCommand( Commands::Command( "DELETE" ) << encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::create( const QString& mailbox )
{
    return queueCommand( Commands::Command( "CREATE" ) << encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::rename( const QString& oldName, const QString& newName )
{
    return queueCommand( Commands::Command( "RENAME" ) <<
                         encodeImapFolderName( oldName ) <<
                         encodeImapFolderName( newName ) );
}

CommandHandle Parser::subscribe( const QString& mailbox )
{
    return queueCommand( Commands::Command( "SUBSCRIBE" ) << encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::unSubscribe( const QString& mailbox )
{
    return queueCommand( Commands::Command( "UNSUBSCRIBE" ) << encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::list( const QString& reference, const QString& mailbox )
{
    return queueCommand( Commands::Command( "LIST" ) << reference << encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::lSub( const QString& reference, const QString& mailbox )
{
    return queueCommand( Commands::Command( "LSUB" ) << reference << encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::status( const QString& mailbox, const QStringList& fields )
{
    return queueCommand( Commands::Command( "STATUS" ) << encodeImapFolderName( mailbox ) <<
            Commands::PartOfCommand( Commands::ATOM, "(" + fields.join(" ") +")" )
            );
}

CommandHandle Parser::append( const QString& mailbox, const QString& message, const QStringList& flags, const QDateTime& timestamp )
{
    Commands::Command command( "APPEND" );
    command << encodeImapFolderName( mailbox );
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
            Commands::PartOfCommand( Commands::ATOM, seq.toString() ) <<
            Commands::PartOfCommand( Commands::ATOM, '(' + items.join(" ") + ')' ) );
}

CommandHandle Parser::store( const Sequence& seq, const QString& item, const QString& value )
{
    return queueCommand( Commands::Command( "STORE" ) <<
            Commands::PartOfCommand( Commands::ATOM, seq.toString() ) <<
            Commands::PartOfCommand( Commands::ATOM, item ) <<
            Commands::PartOfCommand( Commands::ATOM, value )
            );
}

CommandHandle Parser::copy( const Sequence& seq, const QString& mailbox )
{
    return queueCommand( Commands::Command("COPY") <<
            Commands::PartOfCommand( Commands::ATOM, seq.toString() ) <<
            encodeImapFolderName( mailbox ) );
}

CommandHandle Parser::uidFetch( const Sequence& seq, const QStringList& items )
{
    return queueCommand( Commands::Command( "UID FETCH" ) <<
            Commands::PartOfCommand( Commands::ATOM, seq.toString() ) <<
            Commands::PartOfCommand( Commands::ATOM, items.join(" ") ) );
}

CommandHandle Parser::uidStore( const Sequence& seq, const QString& item, const QString& value )
{
    return queueCommand( Commands::Command( "UID STORE" ) <<
            Commands::PartOfCommand( Commands::ATOM, seq.toString() ) <<
            Commands::PartOfCommand( Commands::ATOM, item ) <<
            Commands::PartOfCommand( Commands::ATOM, value ) );
}

CommandHandle Parser::uidCopy( const Sequence& seq, const QString& mailbox )
{
    return queueCommand( Commands::Command( "UID COPY" ) <<
            Commands::PartOfCommand( Commands::ATOM, seq.toString() ) <<
            encodeImapFolderName( mailbox ) );
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
    return queueCommand( Commands::IDLE, "IDLE" );
}

CommandHandle Parser::namespaceCommand()
{
    return queueCommand( Commands::ATOM, "NAMESPACE" );
}

CommandHandle Parser::queueCommand( Commands::Command command )
{
    QString tag = generateTag();
    command.addTag( tag );
    _cmdQueue.append( command );
    QTimer::singleShot( 0, this, SLOT(executeCommands()) );
    return tag;
}

void Parser::queueResponse( const QSharedPointer<Responses::AbstractResponse>& resp )
{
    _respQueue.push_back( resp );
    emit responseReceived();
}

bool Parser::hasResponse() const
{
    return ! _respQueue.empty();
}

QSharedPointer<Responses::AbstractResponse> Parser::getResponse()
{
    QSharedPointer<Responses::AbstractResponse> ptr;
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

void Parser::handleReadyRead()
{
    while ( 1 ) {
        switch ( _readingMode ) {
            case ReadingLine:
                if ( _socket->canReadLine() ) {
                    _currentLine += _socket->readLine();
                    if ( _currentLine.endsWith( "}\r\n" ) ) {
                        int offset = _currentLine.lastIndexOf( '{' );
                        if ( offset < _oldLiteralPosition )
                            throw ParseError( "Got unmatched '}'", _currentLine, _currentLine.size() - 3 );
                        bool ok;
                        int number = _currentLine.mid( offset + 1, _currentLine.size() - offset - 4 ).toInt( &ok );
                        if ( !ok )
                            throw ParseError( "Can't parse numeric literal size", _currentLine, offset );
                        if ( number < 0 )
                            throw ParseError( "Negative literal size", _currentLine, offset );
                        _oldLiteralPosition = offset;
                        _readingMode = ReadingNumberOfBytes;
                        _readingBytes = number;
                    } else if ( _currentLine.endsWith( "\r\n" ) ) {
                        // it's complete
                        if ( _startTlsInProgress && _currentLine.startsWith( _startTlsCommand ) ) {
                            _startTlsCommand.clear();
                            _startTlsReply = _currentLine;
                            QTimer::singleShot( 0, this, SLOT(executeACommand()) );
                            return;
                        }
                        processLine( _currentLine );
                        _currentLine.clear();
                        _oldLiteralPosition = 0;
                    } else {
                        throw CantHappen( "canReadLine() returned true, but following readLine() failed" );
                    }
                } else {
                    // Not enough data yet, let's try again later
                    return;
                }
                break;
            case ReadingNumberOfBytes:
                {
                    QByteArray buf = _socket->read( _readingBytes );
                    _readingBytes -= buf.size();
                    _currentLine += buf;
                    if ( _readingBytes == 0 ) {
                        // we've read the literal
                        _readingMode = ReadingLine;
                    } else {
                        return;
                    }
                }
                break;
        }
    }
}

void Parser::executeCommands()
{
    while ( ! _waitingForContinuation && ! _waitForInitialIdle &&
            ! _cmdQueue.isEmpty() && ! _startTlsInProgress )
        executeACommand();
}

void Parser::executeACommand()
{
    Q_ASSERT( ! _cmdQueue.isEmpty() );
    Commands::Command& cmd = _cmdQueue.first();

    QByteArray buf;

    if ( _idling ) {
        buf.append( "DONE\r\n" );
#ifdef PRINT_TRAFFIC
        qDebug() << ">>>" << buf.left( PRINT_TRAFFIC );
#endif
        _socket->write( buf );
        buf.clear();
        _idling = false;
        emit idleTerminated();
    }

    while ( 1 ) {
        Commands::PartOfCommand& part = cmd._cmds[ cmd._currentPart ];
        switch( part._kind ) {
            case Commands::ATOM:
                buf.append( part._text );
                break;
            case Commands::QUOTED_STRING:
                buf.append( '"' );
                buf.append( part._text );
                buf.append( '"' );
                break;
            case Commands::LITERAL:
                if ( _literalPlus ) {
                    buf.append( '{' );
                    buf.append( QByteArray::number( part._text.size() ) );
                    buf.append( "+}\r\n" );
                    buf.append( part._text );
                } else if ( part._numberSent ) {
                    buf.append( part._text );
                } else {
                    buf.append( '{' );
                    buf.append( QByteArray::number( part._text.size() ) );
                    buf.append( "}\r\n" );
#ifdef PRINT_TRAFFIC
                    qDebug() << ">>>" << buf.left( PRINT_TRAFFIC );
#endif
                    _socket->write( buf );
                    part._numberSent = true;
                    _waitingForContinuation = true;
                    return; // and wait for continuation request
                }
                break;
            case Commands::IDLE:
                buf.append( "IDLE\r\n" );
#ifdef PRINT_TRAFFIC
                qDebug() << ">>>" << buf.left( PRINT_TRAFFIC );
#endif
                _socket->write( buf );
                _idling = true;
                _waitForInitialIdle = true;
                _cmdQueue.pop_front();
                return;
                break;
            case Commands::STARTTLS:
                if ( part._numberSent ) {
#ifdef PRINT_TRAFFIC
                    qDebug() << "*** STARTTLS";
#endif
                    _cmdQueue.pop_front();
                    _socket->startTls(); // warn: this might invoke event loop
                    _startTlsInProgress = false;
                    processLine( _startTlsReply );
                    return;
                } else {
                    _startTlsCommand = buf;
                    buf.append( "STARTTLS\r\n" );
#ifdef PRINT_TRAFFIC
                    qDebug() << ">>>" << buf.left( PRINT_TRAFFIC );
#endif
                    _socket->write( buf );
                    part._numberSent = true;
                    _startTlsInProgress = true;
                    return;
                }
                break;
        }
        if ( cmd._currentPart == cmd._cmds.size() - 1 ) {
            // finalize
            buf.append( "\r\n" );
#ifdef PRINT_TRAFFIC
            qDebug() << ">>>" << buf.left( PRINT_TRAFFIC );
#endif
            _socket->write( buf );
            _cmdQueue.pop_front();
            break;
        } else {
            buf.append( ' ' );
            ++cmd._currentPart;
        }
    }
}

/** @short Process a line from IMAP server */
void Parser::processLine( QByteArray line )
{
#ifdef PRINT_TRAFFIC
    qDebug() << "<<<" << line.left( PRINT_TRAFFIC );
#endif
    if ( line.startsWith( "* " ) ) {
        queueResponse( parseUntagged( line ) );
    } else if ( line.startsWith( "+ " ) ) {
        if ( _waitingForContinuation ) {
            _waitingForContinuation = false;
            QTimer::singleShot( 0, this, SLOT(executeCommands()) );
        } else if ( _waitForInitialIdle ) {
            _waitForInitialIdle = false;
            QTimer::singleShot( 0, this, SLOT(executeCommands()) );
        } else {
            throw ContinuationRequest( line.constData() );
        }
    } else {
        queueResponse( parseTagged( line ) );
    }
}

QSharedPointer<Responses::AbstractResponse> Parser::parseUntagged( const QByteArray& line )
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

QSharedPointer<Responses::AbstractResponse> Parser::_parseUntaggedNumber(
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
                    return QSharedPointer<Responses::AbstractResponse>(
                            new Responses::NumberResponse( kind, number ) );
                } catch ( UnexpectedHere& e ) {
                    throw UnexpectedHere( e.what(), line, start );
                }
            break;

        case Responses::FETCH:
            return QSharedPointer<Responses::AbstractResponse>(
                    new Responses::Fetch( number, line, start ) );
            break;

        default:
            break;
    }
    throw UnexpectedHere( line, start );
}

QSharedPointer<Responses::AbstractResponse> Parser::_parseUntaggedText(
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
                return QSharedPointer<Responses::AbstractResponse>(
                        new Responses::Capability( capabilities ) );
            }
        case Responses::OK:
        case Responses::NO:
        case Responses::BAD:
        case Responses::PREAUTH:
        case Responses::BYE:
            return QSharedPointer<Responses::AbstractResponse>(
                    new Responses::State( QString::null, kind, line, start ) );
        case Responses::LIST:
        case Responses::LSUB:
            return QSharedPointer<Responses::AbstractResponse>(
                    new Responses::List( kind, line, start ) );
        case Responses::FLAGS:
            return QSharedPointer<Responses::AbstractResponse>(
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
                return QSharedPointer<Responses::AbstractResponse>(
                        new Responses::Search( numbers ) );
            }
        case Responses::STATUS:
            return QSharedPointer<Responses::AbstractResponse>(
                    new Responses::Status( line, start ) );
        case Responses::NAMESPACE:
            return QSharedPointer<Responses::AbstractResponse>(
                    new Responses::Namespace( line, start ) );
        default:
            throw UnexpectedHere( line, start );
    }
}

QSharedPointer<Responses::AbstractResponse> Parser::parseTagged( const QByteArray& line )
{
    int pos = 0;
    const QByteArray tag = LowLevelParser::getAtom( line, pos );
    ++pos;
    const Responses::Kind kind = Responses::kindFromString( LowLevelParser::getAtom( line, pos ) );
    ++pos;

    return QSharedPointer<Responses::AbstractResponse>(
            new Responses::State( tag, kind, line, pos ) );
}

void Parser::enableLiteralPlus( const bool enabled )
{
    _literalPlus = enabled;
}

void Parser::handleDisconnected( const QString& reason )
{
#ifdef PRINT_TRAFFIC
    qDebug() << "*** Socket disconnected";
#endif
    emit disconnected( reason );
}

Sequence::Sequence( const uint num ): _kind(DISTINCT)
{
    _list << num;
}

Sequence Sequence::startingAt( const uint lo )
{
    Sequence res( lo );
    res._lo = lo;
    res._kind = UNLIMITED;
    return res;
}

QString Sequence::toString() const
{
    switch ( _kind ) {
        case DISTINCT:
        {
            Q_ASSERT( ! _list.isEmpty() );

            QStringList res;
            int i = 0;
            while ( i < _list.size() ) {
                int old = i;
                while ( i < _list.size() - 1 &&
                        _list[i] == _list[ i + 1 ] - 1 )
                    ++i;
                if ( old != i ) {
                    // we've found a sequence
                    res << QString::number( _list[old] ) + QLatin1Char(':') + QString::number( _list[i] );
                } else {
                    res << QString::number( _list[i] );
                }
                ++i;
            }
            return res.join( QLatin1String(",") );
            break;
        }
        case RANGE:
            Q_ASSERT( _lo <= _hi );
            if ( _lo == _hi )
                return QString::number( _lo );
            else
                return QString::number( _lo ) + QLatin1Char(':') + QString::number( _hi );
        case UNLIMITED:
            return QString::number( _lo ) + QLatin1String(":*");
    }
    // fix gcc warning
    Q_ASSERT( false );
    return QString();
}

Sequence& Sequence::add( uint num )
{
    Q_ASSERT( _kind == DISTINCT );
    QList<uint>::iterator it = qLowerBound( _list.begin(), _list.end(), num );
    if ( *it != num )
        _list.insert( it, num );
    return *this;
}

QTextStream& operator<<( QTextStream& stream, const Sequence& s )
{
    return stream << s.toString();
}

}

#include "Parser.moc"
