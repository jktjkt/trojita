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
#ifndef IMAP_LOWLEVELPARSER_H
#define IMAP_LOWLEVELPARSER_H

#include <QList>
#include <QPair>
#include <QVariant>

namespace Imap {
namespace LowLevelParser {

    /*
     * FIXME: this is ugly. We should have used another scheme for iterators as
     * we generally want to iterate over bytes, not over words separated by
     * spaces. This should be fixed later, for now, let's stick with ugly hacks
     * :-(.
     * */

    enum ParsedAs {
        ATOM,
        QUOTED,
        LITERAL,
        NIL
    };


    /** @short Parse parenthesized list 
     *
     * Parenthesized lists is defined as a sequence of space-separated strings
     * enclosed between "open" and "close" characters.
     *
     * it, end -- iterators determining where to start and end
     * lineData -- used when throwing exception
     * allowNoList -- if false and there's no opening parenthesis, exception is
     *                thrown
     * allowEmptyList -- if false and the list is empty (ie. nothing between opening
     *                   and closing bracket), exception is thrown
     * */
    QPair<QStringList,QByteArray> parseList( const char open, const char close,
            QList<QByteArray>::const_iterator& it,
            const QList<QByteArray>::const_iterator& end,
            const char * const lineData,
            const bool allowNoList = false, const bool allowEmptyList = true );

    QPair<QByteArray,ParsedAs> getString( QList<QByteArray>::const_iterator& it,
            const QList<QByteArray>::const_iterator& end,
            const char * const lineData,
            const char leading ='\0', const char trailing = '\0' );

    QPair<QByteArray,ParsedAs> getAString( QList<QByteArray>::const_iterator& it,
            const QList<QByteArray>::const_iterator& end,
            const char * const lineData );

    QPair<QByteArray,ParsedAs> getNString( QList<QByteArray>::const_iterator& it,
            const QList<QByteArray>::const_iterator& end,
            const char * const lineData );

    QString getMailbox( QList<QByteArray>::const_iterator& it,
            const QList<QByteArray>::const_iterator& end,
            const char * const lineData );

    uint getUInt( QList<QByteArray>::const_iterator& it,
            const QList<QByteArray>::const_iterator& end,
            const char * const lineData );

    uint getUInt( const QByteArray& line, int& start );

    QByteArray getAtom( const QByteArray& line, int& start );

    QPair<QByteArray,ParsedAs> getString( const QByteArray& line, int& start );

    QPair<QByteArray,ParsedAs> getAString( const QByteArray& line, int& start );

    QPair<QByteArray,ParsedAs> getNString( const QByteArray& line, int& start );

    QString getMailbox( const QByteArray& line, int& start );

    /** @short Parse parenthesized list 
     *
     * Parenthesized lists is defined as a sequence of space-separated strings
     * enclosed between "open" and "close" characters.
     *
     * open, close -- enclosing parentheses
     * line, start -- full line data and starting offset
     * allowNoList -- if false and there's no opening parenthesis, exception is
     *                thrown
     * allowEmptyList -- if false and the list is empty (ie. nothing between opening
     *                   and closing bracket), exception is thrown
     *
     * We need to support parsing of nested lists (as found in the envelope data
     * structure), that's why we deal with QVariant here.
     * */
    QVariantList parseList( const char open, const char close,
            const QByteArray& line, int& start,
            const bool allowNoList = false, const bool allowEmptyList = true );

}
}

#endif /* IMAP_LOWLEVELPARSER_H */
