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

State::State( const QString& _tag, const Kind _kind, const QByteArray& line, int& start ):
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
                throw UnexpectedHere( line, start ); // tagged response of weird kind
        }
    }

    QStringList _list;
    try {
        _list = QVariant( LowLevelParser::parseList( '[', ']', line, start ) ).toStringList();
        ++start;
    } catch ( UnexpectedHere& ) {
        // this is perfectly possible
    }
    if ( start >= line.size() - 2 )
        throw NoData( line, start );

    if ( !_list.empty() ) {
        const QString r = _list.first().toUpper();
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
                    throw InvalidResponseCode( line, start );
                respCodeData = std::tr1::shared_ptr<AbstractData>( new RespData<void>() );
                break;
            case Responses::UIDNEXT:
            case Responses::UIDVALIDITY:
            case Responses::UNSEEN:
                // check for "number only"
                {
                if ( _list.count() != 1 )
                    throw InvalidResponseCode( line, start );
                bool ok;
                uint number = _list.first().toUInt( &ok );
                if ( !ok )
                    throw InvalidResponseCode( line, start );
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

    message = line.mid( start );
    Q_ASSERT( message.endsWith( "\r\n" ) );
    message.chop(2);
}

NumberResponse::NumberResponse( const Kind _kind, const uint _num ) throw(UnexpectedHere):
    AbstractResponse(_kind), number(_num)
{
    if ( kind != EXISTS && kind != EXPUNGE && kind != RECENT )
        throw UnexpectedHere( "Attempted to create NumberResponse of invalid kind" );
}

List::List( const Kind _kind, const QByteArray& line, int& start ):
    AbstractResponse(LIST), kind(_kind)
{
    if ( kind != LIST && kind != LSUB )
        throw UnexpectedHere( line, start ); // FIXME: well, "start" is too late here...

    flags = QVariant( LowLevelParser::parseList( '(', ')', line, start ) ).toStringList();
    ++start;

    if ( start >= line.size() - 5 )
        throw NoData( line, start ); // flags and nothing else

    if ( line.at(start) == '"' ) {
        ++start;
        if ( line.at(start) == '\\' )
            ++start;
        separator = line.at(start);
        ++start;
        if ( line.at(start) != '"' )
            throw ParseError( line, start );
        ++start;
    } else if ( line.mid( start, 3 ).toLower() == "nil" ) {
        separator = QString::null;
        start += 3;
    } else
        throw ParseError( line, start );
        
    ++start;

    if ( start >= line.size() )
        throw NoData( line, start ); // no mailbox

    mailbox = LowLevelParser::getMailbox( line, start );
}

Flags::Flags( const QByteArray& line, int& start )
{
    flags = QVariant( LowLevelParser::parseList( '(', ')', line, start ) ).toStringList();
    if ( start >= line.size() )
        throw TooMuchData( line, start );
}

Status::Status( const QByteArray& line, int& start )
{
    mailbox = LowLevelParser::getMailbox( line, start );
    ++start;
    if ( start >= line.size() )
        throw NoData( line, start );
    QStringList items = QVariant( LowLevelParser::parseList( '(', ')', line, start  ) ).toStringList();
    if ( start != line.size() - 2 )
        throw TooMuchData( line, start );

    bool gotIdentifier = false;
    QString identifier;
    for ( QStringList::const_iterator it = items.begin(); it != items.end(); ++it ) {
        if ( gotIdentifier ) {
            gotIdentifier = false;
            bool ok;
            uint number = it->toUInt( &ok );
            if (!ok)
                throw ParseError( line, start );
            StateKind kind = stateKindFromStr( identifier );
            states[kind] = number;
        } else {
            identifier = *it;
            gotIdentifier = true;
        }
    }
    if ( gotIdentifier )
        throw ParseError( line, start );
}

QDateTime Fetch::dateify( QByteArray str, const QByteArray& line, const int start )
{
    // FIXME: all offsets in exceptions are broken here.
    if ( str.size() == 25 )
        str = QByteArray::number( 0 ) + str;
    if ( str.size() != 26 )
        throw ParseError( line, start );

    QDateTime date = QDateTime::fromString( str.left(20), "d-MMM-yyyy HH:mm:ss");
    const char sign = str[21];
    bool ok;
    uint hours = str.mid(22, 2).toUInt( &ok );
    if (!ok)
        throw ParseError( line, start );
    uint minutes = str.mid(24, 2).toUInt( &ok );
    if (!ok)
        throw ParseError( line, start );
    switch (sign) {
        case '+':
            date = date.addSecs( -3600*hours - 60*minutes );
            break;
        case '-':
            date = date.addSecs( +3600*hours + 60*minutes );
            break;
        default:
            throw ParseError( line, start );
    }
    date.setTimeSpec( Qt::UTC );
    return date;
}

QList<MailAddress> Envelope::getListOfAddresses( const QVariant& in, const QByteArray& line, const int start )
{
    if ( in.type() == QVariant::ByteArray ) {
        if ( ! in.toByteArray().isNull() )
            throw UnexpectedHere( line, start );
    } else if ( in.type() != QVariant::List )
        throw ParseError( line, start );

    QVariantList list = in.toList();
    QList<MailAddress> res;
    for ( QVariantList::const_iterator it = list.begin(); it != list.end(); ++it ) {
        if ( it->type() != QVariant::List )
            throw UnexpectedHere( line, start ); // FIXME: wrong offset
        res.append( MailAddress( it->toList(), line, start ) );
    }
    return res;
}

MailAddress::MailAddress( const QVariantList& input, const QByteArray& line, const int start )
{
    // FIXME: all offsets are wrong here
    // FIXME: decode strings from various RFC2822 encodings
    if ( input.size() != 4 )
        throw ParseError( line, start );

    if ( input[0].type() != QVariant::ByteArray )
        throw UnexpectedHere( line, start );
    if ( input[1].type() != QVariant::ByteArray )
        throw UnexpectedHere( line, start );
    if ( input[2].type() != QVariant::ByteArray )
        throw UnexpectedHere( line, start );
    if ( input[3].type() != QVariant::ByteArray )
        throw UnexpectedHere( line, start );

    name = input[0].toByteArray();
    adl = input[1].toByteArray();
    mailbox = input[2].toByteArray();
    host = input[3].toByteArray();
}

Envelope Envelope::fromList( const QVariantList& items, const QByteArray& line, const int start )
{
    if ( items.size() != 10 )
        throw ParseError( line, start ); // FIXME: wrong offset

    // date
    QDateTime date;
    if ( items[0].type() == QVariant::ByteArray ) {
        QByteArray dateStr = items[0].toByteArray();
        date = LowLevelParser::parseRFC2822DateTime( dateStr );
    }
    // Otherwise it's "invalid", null.

    QByteArray subject = items[1].toByteArray(); // FIXME: decode

    QList<MailAddress> from, sender, replyTo, to, cc, bcc;
    from = Envelope::getListOfAddresses( items[2], line, start );
    sender = Envelope::getListOfAddresses( items[3], line, start );
    replyTo = Envelope::getListOfAddresses( items[4], line, start );
    to = Envelope::getListOfAddresses( items[5], line, start );
    cc = Envelope::getListOfAddresses( items[6], line, start );
    bcc = Envelope::getListOfAddresses( items[7], line, start );

    if ( items[8].type() != QVariant::ByteArray )
        throw UnexpectedHere( line, start );
    QByteArray inReplyTo = items[8].toByteArray();

    if ( items[9].type() != QVariant::ByteArray )
        throw UnexpectedHere( line, start );
    QByteArray messageId = items[9].toByteArray();

    return Envelope( date, subject, from, sender, replyTo, to, cc, bcc, inReplyTo, messageId );
}

bool TextMessage::eq( const AbstractData& other ) const
{
    // FIXME
    return false;
}

QTextStream& TextMessage::dump( QTextStream& s ) const
{
    return s;
}

std::tr1::shared_ptr<AbstractMessage> AbstractMessage::fromList( const QVariantList& items, const QByteArray& line, const int start )
{
    if ( items.size() < 3 )
        throw NoData( line, start );

    if ( items[0].type() == QVariant::ByteArray ) {
        // it's a single-part message, hurray
        if ( items.size() < 7 )
            throw NoData( line, start );

        int i = 0;
        QString mediaType = items[i].toString().toLower();
        ++i;
        QString mediaSubType = items[i].toString().toLower();
        ++i;

        if ( items[i].type() != QVariant::List )
            throw UnexpectedHere( line, start ); // body-fld-param not recognized
        QVariantList bodyFldParamRaw = items[i].toList();
        QList<QByteArray> bodyFldParam;
        for ( QVariantList::const_iterator it = bodyFldParamRaw.begin(); it != bodyFldParamRaw.end(); ++it )
            if ( it->type() != QVariant::ByteArray )
                throw UnexpectedHere( line, start ); // body-fld-param contains strange data
            else
                bodyFldParam.append( it->toByteArray() );
        if ( bodyFldParam.size() % 2 == 1 || bodyFldParam.size() < 2 )
            throw ParseError( line, start ); // body-fld-param of odd size
        ++i;

        if ( items[i].type() != QVariant::ByteArray )
            throw UnexpectedHere( line, start ); // body-fld-id not recognized
        QByteArray bodyFldId = items[i].toByteArray();
        ++i;

        if ( items[i].type() != QVariant::ByteArray )
            throw UnexpectedHere( line, start ); // body-fld-desc not recognized
        QByteArray bodyFldDesc = items[i].toByteArray();
        ++i;

        if ( items[i].type() != QVariant::ByteArray )
            throw UnexpectedHere( line, start ); // body-fld-enc not recognized
        QByteArray bodyFldEnc = items[i].toByteArray();
        ++i;

        if ( items[i].type() != QVariant::UInt )
            throw UnexpectedHere( line, start ); // body-fld-octets not recognized
        uint bodyFldOctets = items[i].toUInt();
        ++i;

        uint bodyFldLines = 0;
        Envelope envelope;
        std::tr1::shared_ptr<AbstractMessage> body;

        enum { MESSAGE, TEXT, BASIC} kind;

        if ( mediaType == "message" && mediaSubType == "rfc822" ) {
            // extract envelope, body, body-fld-lines

            if ( items.size() < 10 )
                throw NoData( line, start ); // too few fields for a Message-message

            kind = MESSAGE;
            if ( items[i].type() != QVariant::List )
                throw UnexpectedHere( line, start ); // envelope not recognized
            envelope = Envelope::fromList( items[i].toList(), line, start );
            ++i;

            if ( items[i].type() != QVariant::List )
                throw UnexpectedHere( line, start ); // body not recognized
            body = AbstractMessage::fromList( items[i].toList(), line, start );
            ++i;

            if ( items[i].type() != QVariant::UInt )
                throw UnexpectedHere( line, start ); // body-fld-lines not found
            bodyFldLines = items[i].toUInt();
            ++i;

        } else if ( mediaType == "text" ) {
            // extract body-fld-lines

            kind = TEXT;
            if ( items[i].type() != QVariant::UInt )
                throw UnexpectedHere( line, start ); // body-fld-lines not found
            bodyFldLines = items[i].toUInt();
            ++i;

        } else {
            // don't extract anything as we're done here
            kind = BASIC;
        }

        // extract body-ext-1part
        // body-fld-md5
        QByteArray bodyFldMd5;
        if ( i < items.size() ) {
            if ( items[i].type() != QVariant::ByteArray )
                throw UnexpectedHere( line, start ); // body-fld-md5 not found
            bodyFldMd5 = items[i].toByteArray();
            ++i;
        }

        // body-fld-dsp
        QPair<QByteArray, QMap<QByteArray,QByteArray> > bodyFldDsp;
        if ( i < items.size() ) {
            if ( items[i].type() != QVariant::List )
                throw UnexpectedHere( line, start ); // body-fld-dsp not found
            QVariantList list = items[i].toList();
            if ( list.size() != 2 )
                throw ParseError( line, start ); // body-fld-dsp: wrong number of entries in the list
            if ( list[0].type() != QVariant::ByteArray )
                throw UnexpectedHere( line, start ); // body-fld-dsp: first item is not a string
            bodyFldDsp.first = list[0].toByteArray();
            if ( list[1].type() != QVariant::List )
                throw UnexpectedHere( line, start ); // body-fld-dsp: body-fld-param not recognized
            list = list[1].toList();
            if ( list.size() % 2 )
                throw UnexpectedHere( line, start ); // body-fld-param: wrong number of entries
            for ( int j = 0; j < list.size(); j += 2 )
                if ( list[j].type() != QVariant::ByteArray || list[j+1].type() != QVariant::ByteArray )
                    throw UnexpectedHere( line, start ); // body-fld-param: string not found
                else
                    bodyFldDsp.second[ list[j].toByteArray() ] = list[j+1].toByteArray();
            ++i;
        }

        // body-fld-lang
        QList<QByteArray> bodyFldLang;
        if ( i < items.size() ) {
            if ( items[i].type() == QVariant::ByteArray ) {
                bodyFldLang << items[i].toByteArray();
            } else if ( items[i].type() == QVariant::List ) {
                QVariantList list = items[i].toList();
                for ( QVariantList::const_iterator it = list.begin(); it != list.end(); ++it )
                    if ( it->type() != QVariant::ByteArray )
                        throw UnexpectedHere( line, start ); // body-fld-lang has wrong structure
                    else
                        bodyFldLang << it->toByteArray();
            } else
                throw UnexpectedHere( line, start ); // body-fld-lang not found
            ++i;
        }

        // body-fld-loc
        QByteArray bodyFldLoc;
        if ( i < items.size() ) {
            if ( items[i].type() != QVariant::ByteArray )
                throw UnexpectedHere( line, start ); // body-fld-loc not found
            bodyFldLoc = items[i].toByteArray();
            ++i;
        }

        // body-extension
        QVariant bodyExtension;
        if ( i < items.size() ) {
            if ( i == items.size() - 1 ) {
                bodyExtension = items[i];
                ++i;
            } else {
                QVariantList list;
                for ( ; i < items.size(); ++i )
                    list << items[i];
                bodyExtension = list;
            }
        }

        switch ( kind ) {
            case MESSAGE:
                // FIXME
                throw Exception( "Body MESSAGE not implemented yet" );
            case TEXT:
                return std::tr1::shared_ptr<AbstractMessage>(
                    new TextMessage( mediaType, mediaSubType, bodyFldParam,
                        bodyFldId, bodyFldDesc, bodyFldEnc, bodyFldOctets,
                        bodyFldMd5, bodyFldDsp, bodyFldLang, bodyFldLoc,
                        bodyExtension, bodyFldLines )
                    );
            case BASIC:
                // FIXME
                throw Exception( "Body BASIC not implemented yet" );
        }

    } else if ( items[0].type() == QVariant::List ) {
        // FIXME multipart parsing...
        throw Exception( "MULTIPART message body parsing not done yet" );
    } else {
        throw UnexpectedHere( line, start );
    }
}

Fetch::Fetch( const uint _number, const QByteArray& line, int& start ):
    AbstractResponse(FETCH), number(_number)
{
    ++start;

    if ( start >= line.size() )
        throw NoData( line, number );

    QVariantList list = LowLevelParser::parseList( '(', ')', line, start );

    bool isIdentifier = true;
    QString identifier;
    for (QVariantList::const_iterator it = list.begin(); it != list.end();
            ++it, isIdentifier = !isIdentifier ) {
        if ( isIdentifier ) {
            identifier = it->toString().toUpper();
            if ( identifier.isEmpty() )
                throw UnexpectedHere( line, start ); // FIXME: wrong offset
            if ( data.contains( identifier ) )
                throw UnexpectedHere( line, start ); // FIXME: wrong offset
        } else {
            if ( identifier == "BODY" || identifier == "BODYSTRUCTURE" ) {
                if ( it->type() != QVariant::List )
                    throw UnexpectedHere( line, start );
                data[identifier] = AbstractMessage::fromList( it->toList(), line, start );

            } else if ( identifier.startsWith( "BODY[" ) ) {
                // FIXME: split into more identifiers?
                if ( it->type() != QVariant::ByteArray )
                    throw UnexpectedHere( line, start );
                data[identifier] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<QByteArray>( it->toByteArray() ) );

            } else if ( identifier == "ENVELOPE" ) {
                if ( it->type() != QVariant::List )
                    throw UnexpectedHere( line, start );
                QVariantList items = it->toList();
                data[identifier] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<Envelope>( Envelope::fromList( items, line, start ) ) );

            } else if ( identifier == "FLAGS" ) {
                if ( ! it->canConvert( QVariant::StringList ) )
                    throw UnexpectedHere( line, start ); // FIXME: wrong offset
            
                data[identifier] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<QStringList>( it->toStringList() ) );

            } else if ( identifier == "INTERNALDATE" ) {
                if ( it->type() != QVariant::ByteArray )
                        throw UnexpectedHere( line, start ); // FIXME: wrong offset
                QByteArray _str = it->toByteArray();
                data[ identifier ] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<QDateTime>( dateify( _str, line, start ) ) );

            } else if ( identifier == "RFC822" ||
                    identifier == "RFC822.HEADER" || identifier == "RFC822.TEXT" ) {
                if ( it->type() != QVariant::ByteArray )
                    throw UnexpectedHere( line, start ); // FIXME: wrong offset
                data[ identifier ] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<QByteArray>( it->toByteArray() ) );
            } else if ( identifier == "RFC822.SIZE" || identifier == "UID" ) {
                if ( it->type() != QVariant::UInt )
                    throw ParseError( line, start ); // FIXME: wrong offset
                data[ identifier ] = std::tr1::shared_ptr<AbstractData>(
                        new RespData<uint>( it->toUInt() ) );
            } else {
                throw UnexpectedHere( line, start ); // FIXME: wrong offset
            }

        }
    }

    if ( start != line.size() - 2 )
        throw TooMuchData( line, start );

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

QTextStream& Fetch::dump( QTextStream& stream ) const
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

QTextStream& operator<<( QTextStream& stream, const MailAddress& address )
{
    stream << '"' << address.name << "\" <";
    if ( !address.host.isNull() )
        stream << address.mailbox << '@' << address.host;
    else
        stream << address.mailbox;
    stream << '>';
    return stream;
}

QTextStream& operator<<( QTextStream& stream, const QList<MailAddress>& address )
{
    stream << "[ ";
    for ( QList<MailAddress>::const_iterator it = address.begin(); it != address.end(); ++it )
        stream << *it << ", ";
    return stream << " ]";
}

QTextStream& operator<<( QTextStream& stream, const Envelope& e )
{
    return stream << "Date: " << e.date.toString() << "\nSubject: " << e.subject << "\nFrom: " <<
        e.from << "\nSender: " << e.sender << "\nReply-To: " << 
        e.replyTo << "\nTo: " << e.to << "\nCc: " << e.cc << "\nBcc: " <<
        e.bcc << "\nIn-Reply-To: " << e.inReplyTo << "\nMessage-Id: " << e.messageId <<
        "\n";
}

bool operator==( const Envelope& a, const Envelope& b )
{
    return a.date == b.date && a.date == b.date && a.subject == b.subject &&
        a.from == b.from && a.sender == b.sender && a.replyTo == b.replyTo &&
        a.to == b.to && a.cc == b.cc && a.bcc == b.bcc &&
        a.inReplyTo == b.inReplyTo && a.messageId == b.messageId;
}

bool operator==( const MailAddress& a, const MailAddress& b )
{
    return a.name == b.name && a.adl == b.adl && a.mailbox == b.mailbox && a.host == b.host;
}

}
}

