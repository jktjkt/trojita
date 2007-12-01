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

#include "Imap/LowLevelParser.h"
#include "Imap/Exceptions.h"

#include <QPair>
#include <QStringList>

namespace Imap {

QStringList LowLevelParser::parseList( const char open, const char close,
        QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData,
        const bool allowNoList, const bool allowEmptyList )
{
    if ( it->startsWith( open ) && it->endsWith( close ) ) {
        QByteArray item = *it;
        ++it;
        item.chop(1);
        item = item.right( item.size() - 1 );
        if ( item.isEmpty() )
            if ( allowEmptyList )
                return QStringList();
            else
                throw NoData( lineData );
        else
            return QStringList( item );
    } else if ( it->startsWith( open ) ) {
        QByteArray item = *it;
        item = item.right( item.size() - 1 );
        ++it;
        QStringList result( item );

        bool foundParenth = false;
        while ( it != end && !foundParenth ) {
            QByteArray item = *it;
            if ( item.endsWith( close ) ) {
                foundParenth = true;
                item.chop(1);
            }
            result << item;
            ++it;
        }
        if ( !foundParenth )
            throw NoData( lineData ); // unterminated list
        return result;
    } else if ( !allowNoList )
        throw NoData( lineData ); 
    else
        return QStringList();
}

QPair<QByteArray,LowLevelParser::ParsedAs> LowLevelParser::getString( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData )
{
    if ( it == end )
        throw NoData( lineData );

    if ( it->startsWith( '"' ) ) {
        // quoted
        bool first = true;
        bool escaping = false;
        bool done = false;
        QByteArray data;
        while ( it != end ) {
            for ( QByteArray::const_iterator letter = it->begin(); /* intentionaly left out */; ++letter ) {
                if ( letter == it->end() ) {
                    data += ' ';
                    break;
                } else {
                    if ( first ) {
                        first = false;
                        continue;
                    } else if ( !escaping ) {
                        if ( *letter == '"' ) {
                            done = true;
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
            ++it;
        }
        if ( !done )
            throw NoData( lineData ); // unterminated quoted string
        return qMakePair( data, QUOTED );
    } else if ( it->startsWith( '{' ) ) {
        // literal
        throw Exception( "not supported" );
    } else
        throw UnexpectedHere( lineData );
}

QPair<QByteArray,LowLevelParser::ParsedAs> LowLevelParser::getAString( QList<QByteArray>::const_iterator& it,
        const QList<QByteArray>::const_iterator& end,
        const char * const lineData )
{
    if ( it == end )
        throw NoData( lineData );

    QByteArray item = *it;
    if ( item.startsWith( '"' ) || item.startsWith( '{' ) ) {
        return getString( it, end, lineData );
    } else {
        ++it;
        return qMakePair( item, ATOM );
    }
}

}
