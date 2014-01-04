/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
 * @author Jan Kundrát <jkt@flaska.net>
 */

/** @short Namespace for IMAP interaction */
namespace Imap
{

namespace Responses
{
class AbstractResponse;
}

/** @short General exception class */
class ImapException : public std::exception
{
protected:
    /** The error message */
    std::string m_msg;
    /** Line with data that caused this error */
    QByteArray m_line;
    /** Offset in line for error source */
    int m_offset;
    /** Class name of the exception */
    std::string m_exceptionClass;
public:
    ImapException(): m_offset(-1), m_exceptionClass("ImapException") {}
    ImapException(const std::string &msg) : m_msg(msg), m_offset(-1), m_exceptionClass("ImapException") {};
    ImapException(const std::string &msg, const QByteArray &line, const int offset):
        m_msg(msg), m_line(line), m_offset(offset), m_exceptionClass("ImapException") {};
    virtual const char *what() const throw();
    virtual ~ImapException() throw() {};
    std::string msg() const { return m_msg; }
    QByteArray line() const { return m_line; }
    int offset() const { return m_offset; }
    std::string exceptionClass() const { return m_exceptionClass; }
};

#define ECBODY(CLASSNAME, PARENT) class CLASSNAME: public PARENT { \
    public: CLASSNAME(const std::string &msg): PARENT(msg) { m_exceptionClass = #CLASSNAME; }\
    CLASSNAME(const QByteArray &line, const int offset): PARENT(#CLASSNAME, line, offset) { m_exceptionClass = #CLASSNAME; }\
    CLASSNAME(const std::string &msg, const QByteArray &line, const int offset): PARENT(msg, line, offset) { m_exceptionClass = #CLASSNAME; }\
    };

/** @short The STARTTLS command failed */
ECBODY(StartTlsFailed, ImapException)

/** @short A generic parser exception */
ECBODY(ParserException, ImapException)

/** @short Invalid argument was passed to some function */
ECBODY(InvalidArgument, ParserException)

/** @short Socket error */
ECBODY(SocketException, ParserException)

/** @short Waiting for something from the socket took too long */
ECBODY(SocketTimeout, SocketException)

/** @short General parse error */
ECBODY(ParseError, ParserException)

/** @short Parse error: unknown identifier */
ECBODY(UnknownIdentifier, ParseError)

/** @short Parse error: unrecognized kind of response */
ECBODY(UnrecognizedResponseKind, UnknownIdentifier)

/** @short Parse error: this is known, but not expected here */
ECBODY(UnexpectedHere, ParseError)

/** @short Parse error: No usable data */
ECBODY(NoData, ParseError)

/** @short Parse error: Too much data */
ECBODY(TooMuchData, ParseError)

/** @short Command Continuation Request received, but we have no idea how to handle it here */
ECBODY(ContinuationRequest, ParserException)

/** @short Invalid Response Code */
ECBODY(InvalidResponseCode, ParseError)

/** @short This is not an IMAP4 server */
ECBODY(NotAnImapServerError, ParseError)

#undef ECBODY

/** @short Parent for all exceptions thrown by Imap::Mailbox-related classes */
class MailboxException: public ImapException
{
public:
    MailboxException(const char *const msg, const Imap::Responses::AbstractResponse &response);
    explicit MailboxException(const char *const msg);
    virtual const char *what() const throw() { return m_msg.c_str(); };
    virtual ~MailboxException() throw() {};

};

#define ECBODY(CLASSNAME, PARENT) class CLASSNAME: public PARENT { \
    public: CLASSNAME(const char *const msg, const Imap::Responses::AbstractResponse &response): PARENT(msg, response) { m_exceptionClass=#CLASSNAME; } \
    CLASSNAME(const char *const msg): PARENT(msg) { m_exceptionClass=#CLASSNAME; } \
    };

/** @short Server sent us something that isn't expected right now */
ECBODY(UnexpectedResponseReceived, MailboxException)

/** @short Internal error in Imap::Mailbox code -- there must be bug in its code */
ECBODY(CantHappen, MailboxException)

/** @short Server sent us information about message we don't know */
ECBODY(UnknownMessageIndex, MailboxException)

#undef ECBODY

}
#endif /* IMAP_EXCEPTIONS_H */
