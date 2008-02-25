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

#include <QPair>
#include <QStringList>
#include "Imap/LowLevelParser.h"
#include "Imap/Exceptions.h"
#include "Imap/rfccodecs.h"

namespace Imap {
namespace LowLevelParser {

QPair<QStringList,QByteArray> parseList( const char open, const char close,
        QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData,
        const bool allowNoList, const bool allowEmptyList )
{
    QByteArray item = *it;
    if ( item.endsWith( "\r\n" ) )
        item.chop(2);

    if ( item.startsWith( open ) && item.endsWith( close ) ) {
        ++it;
        item.chop(1);
        QByteArray chopped;
        int pos = item.indexOf( close );
        if ( pos > 0 ) {
            chopped = item.mid( pos );
            item.chop( chopped.size() );
        }
        item = item.right( item.size() - 1 );
        if ( item.isEmpty() )
            if ( allowEmptyList )
                return qMakePair( QStringList(), chopped );
            else
                throw NoData( lineData );
        else
            return qMakePair( QStringList( item ), chopped );
    } else if ( item.startsWith( open ) ) {
        item = item.right( item.size() - 1 );
        ++it;
        QStringList result( item );

        bool foundParenth = false;
        QByteArray chopped;
        while ( it != end && !foundParenth ) {
            QByteArray item = *it;
            if ( item.endsWith( "\r\n" ) )
                item.chop(2);
            if ( item.endsWith( close ) ) {
                foundParenth = true;
                item.chop(1);

                int pos = item.indexOf( close );
                if ( pos > 0 ) {
                    chopped = item.mid( pos );
                    item.chop( chopped.size() );
                }
            }

            result << item;
            ++it;
        }
        if ( !foundParenth )
            throw NoData( lineData ); // unterminated list
        return qMakePair( result, chopped );
    } else if ( !allowNoList )
        throw NoData( lineData ); 
    else
        return qMakePair( QStringList(), QByteArray() );
}

QPair<QByteArray,ParsedAs> getString( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData,
        const char leading, const char trailing )
{
    if ( it == end || it->isEmpty() )
        throw NoData( lineData );

    if ( ( leading == '\0' && it->startsWith( '"' ) ) ||
            ( leading != '\0' && it->startsWith( QByteArray().append( leading ).append( '"' ) ) ) ) {
        // quoted
        bool first = true;
        bool second = false;
        bool escaping = false;
        bool done = false;
        QByteArray data;
        while ( it != end ) {
            QByteArray word = *it;
            if ( word.endsWith( "\r\n" ) )
                word.chop(2);
            QByteArray::const_iterator letter = word.begin();
            for ( /* first two intentionaly left out */;; ++letter ) {
                if ( letter == word.end() ) {
                    data += ' ';
                    break;
                } else {
                    if ( first ) {
                        first = false;
                        if ( leading != '\0' )
                            second = true;
                        continue;
                    } else if ( second ) {
                        second = false;
                        continue;
                    } else if ( !escaping ) {
                        if ( *letter == '"' ) {
                            done = true;
                            ++letter;
                            break;
                        } else if ( *letter == '\\' ) {
                            escaping = true;
                        } else {
                            data += *letter;
                        }
                    } else {
                        // in escape sequence
                        escaping = false;
                        if ( *letter == '"' )
                            data += '"';
                        else if ( *letter == '\\' )
                            data += '\\';
                        else
                            throw ParseError( lineData );
                    }
                }
            }
            if ( letter != word.end() ) {
                // FIXME
                if ( trailing == '\0' || *letter != trailing )
                    throw ParseError( lineData ); // string terminated in the middle of a word
            }
            ++it;
            if (done)
                break;
        }
        if ( !done )
            throw NoData( lineData ); // unterminated quoted string
        return qMakePair( data, QUOTED );
    } else if ( it->startsWith( '{' ) ) {
        // literal

        // find out how many bytes to read...
        int pos = it->indexOf( "}\r\n" );
        if ( pos < 0 )
            throw ParseError( lineData );
        QByteArray numStr = it->mid( 1, pos - 1 );
        bool ok;
        int number = numStr.toInt( &ok );
        if ( !ok )
            throw ParseError( lineData );

        // ...and read it
        QByteArray buf( it->mid( pos + 3 ) );
        ++it;

        while ( buf.size() < number ) {
            if ( it == end )
                throw NoData( lineData );
            buf += ' ';
            buf += *it;
            ++it;
        }

        // check for proper alignment
        if ( buf.size() != number ) {
            QByteArray extraData = buf.mid( number );
            buf.chop( buf.size() - number );
            /* OK, this is bad, we've read more than we wanted despite our
             * efforts for literals to end at item boundaries :(.
             *
             * This shouldn't really happen.
             * */
            throw ParseError( lineData );
        }

        return qMakePair( buf, LITERAL );
    } else
        throw UnexpectedHere( lineData );
}

QPair<QByteArray,ParsedAs> getAString( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData )
{
    if ( it == end || it->isEmpty() )
        throw NoData( lineData );

    QByteArray item = *it;
    if ( item.startsWith( '"' ) || item.startsWith( '{' ) ) {
        return getString( it, end, lineData );
    } else {
        ++it;
        if ( item.endsWith("\r\n") )
            item.chop(2);
        return qMakePair( item, ATOM );
    }
}

QPair<QByteArray,ParsedAs> getNString( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData )
{
    if ( it == end || it->isEmpty() )
        throw NoData( lineData );

    QByteArray item = *it;
    if ( item.startsWith( '"' ) || item.startsWith( '{' ) ) {
        return getString( it, end, lineData );
    } else if ( item.toUpper() == "NIL" ) {
        return qMakePair( QByteArray("NIL"), ATOM );
    } else {
        throw UnexpectedHere( lineData );
    }
}

QString getMailbox( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData )
{
    QPair<QByteArray,ParsedAs> res = getAString( it, end, lineData );
    if ( res.second == ATOM && res.first.toUpper() == "INBOX" )
        return "INBOX";
    else
        return KIMAP::decodeImapFolderName( res.first );
}

uint getUInt( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData )
{
    if ( it == end )
        throw NoData( lineData );

    bool ok;
    QByteArray item = *it;
    if ( item.endsWith( ')' ) )
        item.chop(1);
    else if ( item.endsWith( ")\r\n" ) ) {
        item.chop(3);
    }
    uint number = item.toUInt( &ok );
    if ( !ok )
        throw ParseError( lineData );
    ++it;
    return number;
}

uint getUInt( const QByteArray& line, int& start )
{
    if ( start == line.size() )
        throw NoData( line, start );

    QByteArray item;
    bool breakIt = false;
    while ( !breakIt && start < line.size() ) {
        switch (line[start]) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                item.append( line[start] );
                ++start;
                break;
            default:
                breakIt = true;
                break;
        }
    }

    bool ok;
    uint number = item.toUInt( &ok );
    if (!ok)
        throw ParseError( line, start );
    return number;
}

QByteArray getAtom( const QByteArray& line, int& start )
{
    if ( start == line.size() )
        throw NoData( line, start );

    int old(start);
    bool breakIt = false;
    while (!breakIt && start < line.size() ) {
        if ( line[start] <= '\x1f' ) {
            // CTL characters (excluding 0x7f) as defined in ABNF
            breakIt = true;
            break;
        }
        switch (line[start]) {
            case '(': case ')': case '{': case '\x20': case '\x7f':
            case '%': case '*': case '"': case '\\': case ']':
                breakIt  = true;
                break;
            default:
                ++start;
        }
    }

    if ( old == start )
        throw ParseError( line, start );
    return line.mid( old, start - old );
}

}
}
