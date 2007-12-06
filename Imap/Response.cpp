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
#include "Imap/Response.h"
#include "Imap/LowLevelParser.h"

namespace Imap {
namespace Responses {

QTextStream& operator<<( QTextStream& stream, const Code& r )
{
    switch (r) {
        case ATOM:
            stream << "<<ATOM>>'"; break;
        case ALERT:
            stream << "ALERT"; break;
        case BADCHARSET:
            stream << "BADCHARSET"; break;
        case CAPABILITIES:
            stream << "CAPABILITY"; break;
        case PARSE:
            stream << "PARSE"; break;
        case PERMANENTFLAGS:
            stream << "PERMANENTFLAGS"; break;
        case READ_ONLY:
            stream << "READ-ONLY"; break;
        case READ_WRITE:
            stream << "READ-WRITE"; break;
        case TRYCREATE:
            stream << "TRYCREATE"; break;
        case UIDNEXT:
            stream << "UIDNEXT"; break;
        case UIDVALIDITY:
            stream << "UIDVALIDITY"; break;
        case UNSEEN:
            stream << "UNSEEN"; break;
        case NONE:
            stream << "<<NONE>>"; break;
    }
    return stream;
}

QTextStream& operator<<( QTextStream& stream, const Kind& res )
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
        case BYE:
            stream << "BYE";
            break;
        case PREAUTH:
            stream << "PREAUTH";
            break;
        case EXPUNGE:
            stream << "EXPUNGE";
            break;
        case FETCH:
            stream << "FETCH";
            break;
        case EXISTS:
            stream << "EXISTS";
            break;
        case RECENT:
            stream << "RECENT";
            break;
        case CAPABILITY:
            stream << "CAPABILITY";
            break;
        case LIST:
            stream << "LIST";
            break;
        case LSUB:
            stream << "LSUB";
            break;
        case FLAGS:
            stream << "FLAGS";
            break;
        case SEARCH:
            stream << "SEARCH";
            break;
        case STATUS:
            stream << "STATUS";
            break;
    }
    return stream;
}

Kind kindFromString( QByteArray str ) throw( UnrecognizedResponseKind )
{
    str = str.toUpper();

    if ( str == "OK" )
        return OK;
    if ( str == "NO" )
        return NO;
    if ( str == "BAD" )
        return BAD;
    if ( str == "BYE" )
        return BYE;
    if ( str == "PREAUTH" )
        return PREAUTH;
    if ( str == "FETCH" )
        return FETCH;
    if ( str == "EXPUNGE" )
        return EXPUNGE;
    if ( str == "FETCH" )
        return FETCH;
    if ( str == "EXISTS" )
        return EXISTS;
    if ( str == "RECENT" )
        return RECENT;
    if ( str == "CAPABILITY" )
        return CAPABILITY;
    if ( str == "LIST" )
        return LIST;
    if ( str == "LSUB" )
        return LSUB;
    if ( str == "FLAGS" )
        return FLAGS;
    if ( str == "SEARCH" || str == "SEARCH\r\n" )
        return SEARCH;
    if ( str == "STATUS" )
        return STATUS;
    throw UnrecognizedResponseKind( str.constData() );
}

QTextStream& operator<<( QTextStream& stream, const Status::StateKind& kind )
{
    switch (kind) {
        case Status::MESSAGES:
            stream << "MESSAGES"; break;
        case Status::RECENT:
            stream << "RECENT"; break;
        case Status::UIDNEXT:
            stream << "UIDNEXT"; break;
        case Status::UIDVALIDITY:
            stream << "UIDVALIDITY"; break;
        case Status::UNSEEN:
            stream << "UNSEEN"; break;
    }
    return stream;
}

Status::StateKind Status::stateKindFromStr( QString s )
{
    s = s.toUpper();
    if ( s == "MESSAGES" )
        return MESSAGES;
    if ( s == "RECENT" )
        return RECENT;
    if ( s == "UIDNEXT" )
        return UIDNEXT;
    if ( s == "UIDVALIDITY" )
        return UIDVALIDITY;
    if ( s == "UNSEEN" )
        return UNSEEN;
    throw UnrecognizedResponseKind( s.toStdString() );
}

QTextStream& operator<<( QTextStream& stream, const AbstractResponse& res )
{
    return res.dump( stream );
}

QTextStream& operator<<( QTextStream& stream, const AbstractData& resp )
{
    return resp.dump( stream );
}

State::State( const QString& _tag, const Kind _kind, QList<QByteArray>::const_iterator& it, 
        const QList<QByteArray>::const_iterator& end, const char * const line ):
    tag(_tag), kind(_kind), respCode( NONE )
{
    if ( !tag.isEmpty() ) {
        // tagged
        switch ( kind ) {
            case OK:
            case NO:
            case BAD:
                break;
            default:
                throw UnexpectedHere( line ); // tagged response of weird kind
        }
    }

    QPair<QStringList,QByteArray> _parsedList = ::Imap::LowLevelParser::parseList( '[', ']', it, end, line, true, false );
    QStringList _list = _parsedList.first;
    if ( !_parsedList.second.isEmpty() )
        throw ParseError( line );

    if ( !_list.empty() ) {
        const QString r = (*(_list.begin())).toUpper();
        if ( r == "ALERT" )
            respCode = Responses::ALERT;
        else if ( r == "BADCHARSET" )
            respCode = Responses::BADCHARSET;
        else if ( r == "CAPABILITY" )
            respCode = Responses::CAPABILITIES;
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
            _list.pop_front();

        // now perform validity check and construct & store the storage object
        switch ( respCode ) {
            case Responses::ALERT:
            case Responses::PARSE:
            case Responses::READ_ONLY:
            case Responses::READ_WRITE:
            case Responses::TRYCREATE:
                // check for "no more stuff"
                if ( _list.count() )
                    throw InvalidResponseCode( line );
                respCodeData = std::tr1::shared_ptr<AbstractData>( new RespData<void>() );
                break;
            case Responses::UIDNEXT:
            case Responses::UIDVALIDITY:
            case Responses::UNSEEN:
                // check for "number only"
                {
                if ( ( _list.count() != 1 ) )
                    throw InvalidResponseCode( line );
                bool ok;
                uint number = _list.first().toUInt( &ok );
                if ( !ok )
                    throw InvalidResponseCode( line );
                respCodeData = std::tr1::shared_ptr<AbstractData>( new RespData<uint>( number ) );
                }
                break;
            case Responses::BADCHARSET:
            case Responses::PERMANENTFLAGS:
            case Responses::CAPABILITIES:
                // no check here
                respCodeData = std::tr1::shared_ptr<AbstractData>( new RespData<QStringList>( _list ) );
                break;
            case Responses::ATOM: // no sanity check here, just make a string
            case Responses::NONE: // this won't happen, but if we ommit it, gcc warns us about that
                respCodeData = std::tr1::shared_ptr<AbstractData>( new RespData<QString>( _list.join(" ") ) );
                break;
        }
    }

    QStringList _messageList;
    for ( ; it != end; ++it ) {
        _messageList << *it;
    }
    message = _messageList.join( " " );
    Q_ASSERT( message.endsWith( "\r\n" ) );
    message.chop(2);
}

NumberResponse::NumberResponse( const Kind _kind, const uint _num ) throw(UnexpectedHere):
    AbstractResponse(_kind), number(_num)
{
    if ( kind != EXISTS && kind != EXPUNGE && kind != RECENT )
        throw UnexpectedHere( "Attempted to create NumberResponse of invalid kind" );
}

List::List( const Kind _kind, QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator end,
        const char * const lineData) throw(UnexpectedHere):
    AbstractResponse(LIST), kind(_kind)
{
    if ( kind != LIST && kind != LSUB )
        throw UnexpectedHere( lineData );

    QPair<QStringList,QByteArray> _parsedList = ::Imap::LowLevelParser::parseList( '(', ')', it, end, lineData );
    flags = _parsedList.first;
    if ( !_parsedList.second.isEmpty() )
        throw ParseError( lineData );

    if ( it == end )
        throw NoData( lineData ); // flags and nothing else

    QByteArray str = *it;
    switch ( str.size() ) {
        case 3:
            // ( DQUOTE, char, DQUOTE ) OR "nil"
            if ( str.toLower() == "nil" )
                separator = QString::null;
            else
                separator = str[1];
            break;
        case 4:
            // DQUOTE, backslash, DQUOTE
            separator = str[2];
            break;
        case 2:
            // DQUOTE, backslash, space (!), DQUOTE
            ++it;
            separator = " ";
            break;
        default:
            throw ParseError( lineData ); // weird separator
    }
    ++it;

    if ( it == end )
        throw NoData( lineData ); // no mailbox

    mailbox = ::Imap::LowLevelParser::getMailbox( it, end, lineData );
}

Flags::Flags( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData )
{
    QPair<QStringList,QByteArray> _parsedList = ::Imap::LowLevelParser::parseList( '(', ')', it, end, lineData );
    if ( !_parsedList.second.isEmpty() )
        throw TooMuchData( lineData );
    if ( it != end )
        throw TooMuchData( lineData );
    flags = _parsedList.first;
}

Status::Status( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData )
{
    if ( it == end )
        throw NoData( lineData );

    mailbox = ::Imap::LowLevelParser::getMailbox( it, end, lineData );

    QPair<QStringList,QByteArray> _parsedList = ::Imap::LowLevelParser::parseList( '(', ')', it, end, lineData, false, false );
    if ( it != end || !_parsedList.second.isEmpty() )
        throw TooMuchData( lineData );
    const QStringList& items = _parsedList.first;

    bool gotIdentifier = false;
    QString identifier;
    for ( QStringList::const_iterator it = items.begin(); it != items.end(); ++it ) {
        if ( gotIdentifier ) {
            gotIdentifier = false;
            bool ok;
            uint number = it->toUInt( &ok );
            if (!ok)
                throw ParseError( lineData );
            StateKind kind = stateKindFromStr( identifier );
            states[kind] = number;
        } else {
            identifier = *it;
            gotIdentifier = true;
        }
    }
    if ( gotIdentifier )
        throw ParseError( lineData );
}

Fetch::Fetch( const uint _number, QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData ):
    AbstractResponse(FETCH), number(_number)
{
    if ( it == end )
        throw NoData( lineData );

    bool first = true;
    QByteArray identifier;
    while ( it != end ) {
        if ( identifier.isEmpty() ) {
            if ( first ) {
                // we're reading first identifier
                identifier = it->right( it->size() - 1 ).toUpper();
                first = false;
            } else {
                // no need to strip '('
                identifier = it->toUpper();
            }
            if ( identifier.isEmpty() )
                throw ParseError( lineData );
            ++it;

        } else {
            // we're supposed to read data now
            if ( data.contains( identifier ) )
                throw UnexpectedHere( lineData );

            if ( identifier == "BODY" || identifier == "BODYSTRUCTURE" ) {
                // FIXME
            } else if ( identifier.startsWith( "BODY[" ) ) {
                // FIXME
            } else if ( identifier == "ENVELOPE" ) {
                // FIXME
            } else if ( identifier == "FLAGS" ) {
                QPair<QStringList,QByteArray> _parsedList = ::Imap::LowLevelParser::parseList( '(', ')', it, end, lineData );

                if ( _parsedList.second.size() > 1 )
                    throw ParseError( lineData );
                if ( _parsedList.second.size() == 1 && ( _parsedList.second != ")" || it != end ) )
                    throw ParseError( lineData );

                data[ identifier ] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<QStringList>( _parsedList.first ) );


            } else if ( identifier == "INTERNALDATE" ) {
                QPair<QByteArray,::Imap::LowLevelParser::ParsedAs> item =
                    ::Imap::LowLevelParser::getString( it, end, lineData );
                if ( item.second != ::Imap::LowLevelParser::QUOTED )
                    throw ParseError( lineData );
                if ( item.first.size() != 26 )
                    throw ParseError( lineData );

                QDateTime date = QDateTime::fromString( item.first.left(20), "d-MMM-yyyy HH:mm:ss");

                const char sign = item.first[21];
                bool ok;
                uint hours = item.first.mid(22, 2).toUInt( &ok );
                if (!ok)
                    throw ParseError( lineData );
                uint minutes = item.first.mid(24, 2).toUInt( &ok );
                if (!ok)
                    throw ParseError( lineData );
                switch (sign) {
                    case '+':
                        date.addSecs( 3600*hours + 60*minutes );
                        break;
                    case '-':
                        date.addSecs( -3600*hours - 60*minutes );
                        break;
                    default:
                        throw ParseError( lineData );
                }

                // we can't rely on server being in the same TZ as we are,
                // so we have to convert to UTC here
                date = date.toUTC(); 
                data[ identifier ] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<QDateTime>( date ) );

            } else if ( identifier == "RFC822" ||
                    identifier == "RFC822.HEADER" || identifier == "RFC822.TEXT" ) {
                QPair<QByteArray,::Imap::LowLevelParser::ParsedAs> item =
                    ::Imap::LowLevelParser::getNString( it, end, lineData );
                data[ identifier ] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<QByteArray>( item.first ) );
            } else if ( identifier == "RFC822.SIZE" || identifier == "UID" ) {
                uint number = ::Imap::LowLevelParser::getUInt( it, end, lineData );
                data[ identifier ] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<uint>( number ) );
            } else {
                throw UnexpectedHere( identifier.constData() );
            }
            identifier.clear();
        }
    }
}

Fetch::Fetch( const uint _number, const Fetch::dataType& _data ):
    AbstractResponse(FETCH), number(_number), data(_data)
{
}

QTextStream& State::dump( QTextStream& stream ) const
{
    if ( !tag.isEmpty() )
        stream << tag;
    else
        stream << '*';
    stream << ' ' << kind;
    if ( respCode != NONE )
        stream << " [" << respCode << ' ' << *respCodeData << ']';
    return stream << ' ' << message;
}

QTextStream& Capability::dump( QTextStream& stream ) const
{
    return stream << "* CAPABILITY " << capabilities.join(", "); 
}

QTextStream& NumberResponse::dump( QTextStream& stream ) const
{
    return stream << kind << " " << number; 
}

QTextStream& List::dump( QTextStream& stream ) const
{
    return stream << kind << " '" << mailbox << "' (" << flags.join(", ") << "), sep '" << separator << "'"; 
}

QTextStream& Flags::dump( QTextStream& stream ) const
{
    return stream << "FLAGS " << flags.join(", ");
}

QTextStream& Search::dump( QTextStream& stream ) const
{
    stream << "SEARCH";
    for (QList<uint>::const_iterator it = items.begin(); it != items.end(); ++it )
        stream << " " << *it;
    return stream;
}

QTextStream& Status::dump( QTextStream& stream ) const
{
    stream << "STATUS " << mailbox;
    for (stateDataType::const_iterator it = states.begin(); it != states.end(); ++it ) {
        stream << " " << it.key() << " " << it.value();
    }
    return stream;
}

QTextStream& Fetch::dump( QTextStream& stream ) const // FIXME
{
    stream << "FETCH " << number << " (";
    for ( dataType::const_iterator it = data.begin();
            it != data.end(); ++it )
        stream << ' ' << it.key() << " \"" << *it.value() << '"';
    return stream << ')';
}

template<class T> QTextStream& RespData<T>::dump( QTextStream& stream ) const
{
    return stream << data;
}

template<> QTextStream& RespData<QStringList>::dump( QTextStream& stream ) const
{
    return stream << data.join( " " );
}

template<> QTextStream& RespData<QDateTime>::dump( QTextStream& stream ) const
{
    return stream << data.toString();
}

bool RespData<void>::eq( const AbstractData& other ) const
{
    try {
        dynamic_cast<const RespData<void>&>( other );
        return true;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

template<class T> bool RespData<T>::eq( const AbstractData& other ) const
{
    try {
        const RespData<T>& r = dynamic_cast<const RespData<T>&>( other );
        return data == r.data;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

bool Capability::eq( const AbstractResponse& other ) const
{
    try {
        const Capability& cap = dynamic_cast<const Capability&>( other );
        return capabilities == cap.capabilities;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

bool NumberResponse::eq( const AbstractResponse& other ) const
{
    try {
        const NumberResponse& num = dynamic_cast<const NumberResponse&>( other );
        return kind == num.kind && number == num.number;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

bool State::eq( const AbstractResponse& other ) const
{
    try {
        const State& s = dynamic_cast<const State&>( other );
        if ( kind == s.kind && tag == s.tag && message == s.message && respCode == s.respCode )
            if ( respCode == NONE )
                return true;
            else
                return *respCodeData == *s.respCodeData;
        else
            return false;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

bool List::eq( const AbstractResponse& other ) const
{
    try {
        const List& r = dynamic_cast<const List&>( other );
        return kind == r.kind && mailbox == r.mailbox && flags == r.flags && separator == r.separator;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

bool Flags::eq( const AbstractResponse& other ) const
{
    try {
        const Flags& fl = dynamic_cast<const Flags&>( other );
        return flags == fl.flags;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

bool Search::eq( const AbstractResponse& other ) const
{
    try {
        const Search& s = dynamic_cast<const Search&>( other );
        return items == s.items;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

bool Status::eq( const AbstractResponse& other ) const
{
    try {
        const Status& s = dynamic_cast<const Status&>( other );
        return mailbox == s.mailbox && states == s.states;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

bool Fetch::eq( const AbstractResponse& other ) const
{
    try {
        const Fetch& f = dynamic_cast<const Fetch&>( other );
        if ( number != f.number )
            return false;
        if ( data.keys() != f.data.keys() )
            return false;
        for ( dataType::const_iterator it = data.begin();
                it != data.end(); ++it )
            if ( *it.value() != *f.data[ it.key() ] )
                return false;
        return true;
    } catch ( std::bad_cast& ) {
        return false;
    }
}


}
}

