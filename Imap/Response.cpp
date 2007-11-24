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
    throw UnrecognizedResponseKind( str.constData() );
}

QTextStream& operator<<( QTextStream& stream, const AbstractResponse& res )
{
    return res.dump( stream );
}

QTextStream& operator<<( QTextStream& stream, const AbstractRespCodeData& resp )
{
    return resp.dump( stream );
}

Status::Status( const QString& _tag, const Kind _kind, QList<QByteArray>::const_iterator& it, 
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

    QStringList _list;
    // FIXME: do we have to use KIMAP::decodeIMAPFolderName() here?

    if ( (*it).startsWith( '[' ) && (*it).endsWith( ']' ) ) {

        // only "[fooobar]"
        QByteArray str = *it;
        str.chop(1);
        str = str.right( str.size() - 1 );
        _list << str;
        ++it;

    } else if ( (*it).startsWith( '[' ) ) {

        QByteArray str = *it;
        str = str.right( str.size() - 1 );
        _list << str;
        ++it;

        while ( it != end ) {
            if ( (*it).endsWith( ']' ) ) {
                // this is the last item
                str = *it;
                str.chop( 1 );
                _list << str;
                ++it;
                break;
            } else {
                _list << *it;
                ++it;
            }
        }

    }

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
                respCodeData = std::tr1::shared_ptr<AbstractRespCodeData>( new RespCodeData<void>() );
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
                respCodeData = std::tr1::shared_ptr<AbstractRespCodeData>( new RespCodeData<uint>( number ) );
                }
                break;
            case Responses::BADCHARSET:
            case Responses::PERMANENTFLAGS:
            case Responses::CAPABILITIES:
                // no check here
                respCodeData = std::tr1::shared_ptr<AbstractRespCodeData>( new RespCodeData<QStringList>( _list ) );
                break;
            case Responses::ATOM: // no sanity check here, just make a string
            case Responses::NONE: // this won't happen, but if we ommit it, gcc warns us about that
                respCodeData = std::tr1::shared_ptr<AbstractRespCodeData>( new RespCodeData<QString>( _list.join(" ") ) );
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

NumberResponse::NumberResponse( const Kind _kind, const uint _num ) throw(InvalidArgument):
    AbstractResponse(_kind), number(_num)
{
    if ( kind != EXISTS || kind != EXPUNGE || kind != RECENT )
        throw InvalidArgument( "Attempted to create NumberResponse of invalid kind" );
}

QTextStream& Status::dump( QTextStream& stream ) const
{
    if ( !tag.isEmpty() )
        stream << "tag " << tag;
    else
        stream << "untagged";

    stream << " " << kind << ", respCode " << respCode;
    if ( respCode != NONE ) {
        stream << ", respCodeData " << *respCodeData;
    }
    return stream << ", message: " << message;
}

QTextStream& Capability::dump( QTextStream& stream ) const
{
    return stream << "Capabilities: " << capabilities.join(", "); 
}

QTextStream& NumberResponse::dump( QTextStream& stream ) const
{
    return stream << kind << ": " << number; 
}

template<class T> QTextStream& RespCodeData<T>::dump( QTextStream& stream ) const
{
    return stream << data;
}

template<> QTextStream& RespCodeData<QStringList>::dump( QTextStream& stream ) const
{
    return stream << data.join( "  " );
}

bool RespCodeData<void>::eq( const AbstractRespCodeData& other ) const
{
    try {
        dynamic_cast<const RespCodeData<void>&>( other );
        return true;
    } catch ( std::bad_cast& ) {
        return false;
    }
}

template<class T> bool RespCodeData<T>::eq( const AbstractRespCodeData& other ) const
{
    try {
        const RespCodeData<T>& r = dynamic_cast<const RespCodeData<T>&>( other );
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
        return number == num.number;
    } catch ( std::bad_cast& ) {
        return false;
    }
}


bool Status::eq( const AbstractResponse& other ) const
{
    try {
        const Status& s = dynamic_cast<const Status&>( other );
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


}
}

