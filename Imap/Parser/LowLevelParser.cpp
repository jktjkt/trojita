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
#include <QVariant>
#include <QDateTime>
#include "LowLevelParser.h"
#include "../Exceptions.h"
#include "Imap/Encoders.h"

namespace Imap {
namespace LowLevelParser {

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

QPair<QByteArray,ParsedAs> getString( const QByteArray& line, int& start )
{
    if ( start == line.size() )
        throw NoData( line, start );

    if ( line[start] == '"' ) {
        // quoted string
        ++start;
        bool escaping = false;
        QByteArray res;
        bool terminated = false;
        while ( start != line.size() && !terminated ) {
            if (escaping) {
                escaping = false;
                if ( line[start] == '"' || line[start] == '\\' )
                    res.append( line[start] );
                else
                    throw UnexpectedHere( line, start );
            } else {
                switch (line[start]) {
                    case '"':
                        terminated = true;
                        break;
                    case '\\':
                        escaping = true;
                        break;
                    case '\r': case '\n':
                        throw ParseError( line, start );
                    default:
                        res.append( line[start] );
                }
            }
            ++start;
        }
        if (!terminated)
            throw NoData( line, start );
        return qMakePair( res, QUOTED );
    } else if ( line[start] == '{' ) {
        // literal
        ++start;
        int size = getUInt( line, start );
        if ( line.mid( start, 3 ) != "}\r\n" )
            throw ParseError( line, start );
        start += 3;
        if ( start + size > line.size() )
            throw NoData( line, start );
        int old(start);
        start += size;
        return qMakePair( line.mid(old, size), LITERAL );
    } else
        throw UnexpectedHere( line, start );
}

QPair<QByteArray,ParsedAs> getAString( const QByteArray& line, int& start )
{
    if ( start == line.size() )
        throw NoData( line, start );

    if ( line[start] == '{' || line[start] == '"' )
        return getString( line, start );
    else
        return qMakePair( getAtom( line, start ), ATOM );
}

QPair<QByteArray,ParsedAs> getNString( const QByteArray& line, int& start )
{
    QPair<QByteArray,ParsedAs> r = getAString( line, start );
    if ( r.second == ATOM && r.first.toUpper() == "NIL" ) {
        r.first.clear();
        r.second = NIL;
    }
    return r;
}

QString getMailbox( const QByteArray& line, int& start )
{
    QPair<QByteArray,ParsedAs> r = getAString( line, start );
    if ( r.second == ATOM && r.first.toUpper() == "INBOX" )
        return "INBOX";
    else
        return decodeImapFolderName( r.first );

}

QVariantList parseList( const char open, const char close,
        const QByteArray& line, int& start )
{
    if ( start >= line.size() )
        throw NoData( line, start );

    if ( line[start] == open ) {
        // found the opening parenthesis
        ++start;
        if ( start >= line.size() )
            throw ParseError( line, start );

        QVariantList res;
        if ( line[start] == close ) {
            ++start;
            return res;
        }
        while ( line[start] != close ) {
            res.append( getAnything( line, start ) );
            if ( start >= line.size() )
                throw NoData( line, start ); // truncated list
            if ( line[start] == close ) {
                ++start;
                return res;
            }
            eatSpaces( line, start );
        }
        return res;
    } else
        throw UnexpectedHere( line, start );
}

QVariant getAnything( const QByteArray& line, int& start )
{
    if ( start >= line.size() )
        throw NoData( line, start );

    if ( line[start] == '[' ) {
        QVariant res = parseList( '[', ']', line, start );
        return res;
    } else if ( line[start] == '(' ) {
        QVariant res = parseList( '(', ')', line, start );
        return res;
    } else if ( line[start] == '"' || line[start] == '{' ) {
        QPair<QByteArray,ParsedAs> res = getString( line, start );
        return res.first;
    } else if ( line.mid( start, 3 ).toUpper() == "NIL" ) {
        start += 3;
        return QByteArray();
    } else if ( line[start] == '\\' ) {
        // valid for "flag"
        ++start;
        if ( start >= line.size() )
            throw NoData( line, start );
        if ( line[start] == '*' ) {
            ++start;
            return QByteArray( "\\*" );
        }
        return QByteArray( 1, '\\' ) + getAtom( line, start );
    } else {
        switch ( line.at( start ) ) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return getUInt( line, start );
                break;
            default:
                {
                QByteArray atom = getAtom( line, start );
                if ( atom.indexOf( '[', 0 ) != -1 ) {
                    // "BODY[something]" -- there's no whitespace between "[" and
                    // next atom...
                    int pos = line.indexOf( ']', start );
                    if ( pos == -1 )
                        throw ParseError( line, start );
                    ++pos;
                    atom += line.mid( start, pos - start );
                    start = pos;
                    if ( start < line.size() && line[start] == '<' ) {
                        // Let's check if it continues with "<range>"
                        pos = line.indexOf( '>', start );
                        if ( pos == -1 )
                            throw ParseError( line, start );
                        ++pos;
                        atom += line.mid( start, pos - start );
                        start = pos;
                    }
                }
                return atom;
                }
        }
    }
}

QDateTime parseRFC2822DateTime( const QString& string )
{
    QStringList monthNames = QStringList() << "jan" << "feb" << "mar" << "apr"
        << "may" << "jun" << "jul" << "aug" << "sep" << "oct" << "nov" << "dec";
        
    QRegExp rx( QString( "^(?:\\s*([A-Z][a-z]+)\\s*,\\s*)?" // date-of-week
                "(\\d{1,2})\\s+(%1)\\s+(\\d{2,4})" // date
                "\\s+(\\d{2})\\s*:(\\d{2})\\s*(?::\\s*(\\d{2})\\s*)" // time
                "(\\s+(?:(?:([+-]?)(\\d{2})(\\d{2}))|(UT|GMT|EST|EDT|CST|CDT|MST|MDT|PST|PDT|[A-IK-Za-ik-z])))?" // timezone
                ).arg( monthNames.join( "|" ) ), Qt::CaseInsensitive );
    int pos = rx.indexIn( string );

    if ( pos == -1 )
        throw ParseError( "Date format not recognized" );

    QStringList list = rx.capturedTexts();

    if ( list.size() != 13 )
        throw ParseError( "Date regular expression returned weird data (internal error?)" );

    int year = list[4].toInt();
    int month = monthNames.indexOf( list[3].toLower() ) + 1;
    if ( month == 0 )
        throw ParseError( "Invalid month name" );
    int day = list[2].toInt();
    int hours = list[5].toInt();
    int minutes = list[6].toInt();
    int seconds = list[7].toInt();
    int shift = list[10].toInt() * 60 + list[11].toInt();
    if ( list[9] == "-" )
        shift *= 60;
    else
        shift *= -60;
    if ( ! list[12].isEmpty() ) {
        const QString tz = list[12].toUpper();
        if ( tz == "UT" || tz == "GMT" )
            shift = 0;
        else if ( tz == "EST" )
            shift = 5 * 3600;
        else if ( tz == "EDT" )
            shift = 4 * 3600;
        else if ( tz == "CST" )
            shift = 6 * 3600;
        else if ( tz == "CDT" )
            shift = 5 * 3600;
        else if ( tz == "MST" )
            shift = 7 * 3600;
        else if ( tz == "MDT" )
            shift = 6 * 3600;
        else if ( tz == "PST" )
            shift = 8 * 3600;
        else if ( tz == "PDT" )
            shift = 7 * 3600;
        else if ( tz.size() == 1 )
            shift = 0;
        else
            throw ParseError( "Invalid TZ specification" );
    }

    QDateTime date( QDate( year, month, day ), QTime( hours, minutes, seconds ), Qt::UTC );
    date = date.addSecs( shift );

    return date;
}

void eatSpaces( const QByteArray& line, int& start )
{
    while ( line.size() > start && line[start] == ' ' )
        ++start;
}

}
}
