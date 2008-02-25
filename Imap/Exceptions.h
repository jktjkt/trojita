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
#ifndef IMAP_EXCEPTIONS_H
#define IMAP_EXCEPTIONS_H
#include <exception>
#include <string>
#include <QByteArray>

/**
 * @file
 * @short Common IMAP-related exceptions
 *
 * All IMAP-related exceptions inherit from Imap::Exception which inherits from
 * std::exception.
 *
 * @author Jan Kundrát <jkt@gentoo.org>
 */

/** @short Namespace for IMAP interaction */
namespace Imap {

    /** @short General exception class */
    class Exception : public std::exception {
        /** The error message */
        std::string _msg;
        /** Line with data that caused this error */
        QByteArray _line;
        /** Offset in line for error source */
        int _offset;
    public:
        Exception( const std::string& msg ) : _msg(msg), _offset(-1) {};
        Exception( const std::string& msg, const QByteArray& line, const int offset ):
            _msg(msg), _line(line), _offset(offset) {};
        virtual const char* what( const int context ) const throw();
        virtual const char* what() const throw() { return what( 10 ); };
        virtual ~Exception() throw() {};
    };

#define ECBODY(CLASSNAME, PARENT) public: CLASSNAME( const std::string& msg ): PARENT(msg ) {};\
    CLASSNAME( const QByteArray& line, const int offset ): PARENT( #CLASSNAME, line, offset ) {};\
    CLASSNAME( const std::string& msg, const QByteArray& line, const int offset ): PARENT( msg, line, offset ) {};

    /** @short Invalid argument was passed to some function */
    class InvalidArgument: public Exception {
    public:
        ECBODY(InvalidArgument, Exception);
    };

    /** @short Socket error */
    class SocketException : public Exception {
    public:
        ECBODY(SocketException, Exception);
    };

    /** @short General parse error */
    class ParseError : public Exception {
    public:
        ECBODY(ParseError, Exception);
    };

    /** @short Parse error: unknown identifier */
    class UnknownIdentifier : public ParseError {
    public:
        ECBODY(UnknownIdentifier, ParseError);
    };

    /** @short Parse error: unrecognized kind of response */
    class UnrecognizedResponseKind : public UnknownIdentifier {
    public:
        ECBODY(UnrecognizedResponseKind, UnknownIdentifier);
    };

    /** @short Parse error: this is known, but not expected here */
    class UnexpectedHere : public ParseError {
    public:
        ECBODY(UnexpectedHere, ParseError);
    };

    /** @short Parse error: No usable data */
    class NoData : public ParseError {
    public:
        ECBODY(NoData, ParseError);
    };

    /** @short Parse error: Too much data */
    class TooMuchData : public ParseError {
    public:
        ECBODY(TooMuchData, ParseError);
    };

    /** @short Command Continuation Request received, but we have no idea how to handle it here */
    class ContinuationRequest : public Exception {
    public:
        ECBODY(ContinuationRequest, Exception);
    };

    /** @short Unknown command result (ie. anything else than OK, NO or BAD */
    class UnknownCommandResult : public ParseError {
    public:
        ECBODY(UnknownCommandResult, ParseError);
    };

    /** @short Invalid Response Code */
    class InvalidResponseCode : public ParseError {
    public:
        ECBODY(InvalidResponseCode, ParseError);
    };

#undef ECBODY

}
#endif /* IMAP_EXCEPTIONS_H */
