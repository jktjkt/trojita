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
#ifndef IMAP_RESPONSE_H
#define IMAP_RESPONSE_H

#include <tr1/memory>
#include <QTextStream>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QStringList>
#include "Imap/Command.h"
#include "Imap/Exceptions.h"

/**
 * @file
 * @short Various data structures related to IMAP responses
 *
 * @author Jan Kundrát <jkt@gentoo.org>
 */

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short IMAP server responses
 *
 * @ref AbstractResponse is an abstarct parent of all classes. Each response
 * that might be received from the server is a child of this one.
 * */
namespace Responses {

    /** @short Result of a command */
    enum Kind {
        OK /**< OK */,
        NO /**< NO */,
        BAD /**< BAD */,
        BYE,
        PREAUTH,
        EXPUNGE,
        FETCH,
        EXISTS,
        RECENT,
        CAPABILITY,
        LIST,
        LSUB,
        FLAGS,
        SEARCH,
        STATUS
    }; // aren't those comments just sexy? :)

    /** @short Response Code */
    enum Code {
        NONE /**< No response code specified */,
        ATOM /**< Not recognized */,
        ALERT /**< ALERT */,
        BADCHARSET /**< BADCHARSET */,
        CAPABILITIES /**< CAPABILITY. Yeah, it's different than the RFC3501 name for it. Responses::Kind already defines a CAPABILITY. */,
        PARSE /**< PARSE */,
        PERMANENTFLAGS /**< PERMANENTFLAGS */,
        READ_ONLY /**< READ-ONLY */, 
        READ_WRITE /**< READ-WRITE */,
        TRYCREATE /**< TRYCREATE */,
        UIDNEXT /**< UIDNEXT */,
        UIDVALIDITY /**< UIDVALIDITY */,
        UNSEEN /**< UNSEEN */
    }; // luvly comments, huh? :)

    /** @short Parent class for all server responses */
    class AbstractResponse {
    public:
        const Kind kind;
        AbstractResponse(): kind(BAD) {};
        AbstractResponse( const Kind _kind ): kind(_kind) {};
        virtual ~AbstractResponse() {};
        virtual QTextStream& dump( QTextStream& ) const = 0;
        virtual bool eq( const AbstractResponse& other ) const = 0;
    };

    /** @short Parent of all "Response Code Data" classes
     *
     * More information available in AbstractData's documentation.
     * */
    class AbstractData {
    public:
        virtual ~AbstractData() {};
        virtual QTextStream& dump( QTextStream& ) const = 0;
        virtual bool eq( const AbstractData& other ) const = 0;
    };

    /** @short Storage for "Response Code Data"
     *
     * In IMAP, each status response might contain some additional information
     * called "Response Code" and associated data. These data come in several
     * shapes and this class servers as a storage for them.
     * */
    template<class T> class RespData : public AbstractData {
    public:
        T data;
        RespData( const T& _data ) : data(_data) {};
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractData& other ) const;
    };

    /** Explicit specialization for void as we can't define a void member of a
     * class */
    template<> class RespData<void> : public AbstractData {
    public:
        virtual QTextStream& dump( QTextStream& s ) const { return s; };
        virtual bool eq( const AbstractData& other ) const;
    };

    /** @short Structure storing OK/NO/BAD/PREAUTH/BYE responses */
    class State : public AbstractResponse {
    public:
        /** @short Tag name or QString::null if untagged */
        QString tag;

        /** @short Kind of response 
         *
         * A tagged status response might be either OK, NO or BAD.
         * Untagged status response might be either te same as tagged or BYE or
         * PREAUTH.
         * */
        Kind kind;

        /** @short Textual information embedded in the response
         *
         * While this information might be handy for correct understanding of
         * what happens at ther server, its value is not standardized so the
         * meaning is usually either duplicate to what's already said elsewhere
         * or only a hint to the user. Nevertheless, we decode and store it.
         * */
        QString message;

        /** @short Kind of optional Response Code
         *
         * For each supported value, type of ResponseCodeData stored in the
         * respCodeData is defined as follows:
         *
         *  ALERT, PARSE, READ_ONLY, READ_WRITE, TRYCREATE:
         *      Nothing else should be included, ie. void
         *
         *  UIDNEXT, UIDVALIDITY, UNSEEN:
         *      Only number, ie. unisgned int
         *
         *  BADCHARSET, PERMANENTFLAGS:
         *      List of strings, ie. QStringList
         *
         *  default:
         *      Any data, ie. QString
         * */
        Code respCode;

        /** @short Response Code Data
         *
         * Format is explained in the respCode documentation.
         * We have to use pointer indirection because virtual methods wouldn't
         * work otherwise.
         * */
        std::tr1::shared_ptr<AbstractData> respCodeData;

        /** @short Default constructor
         *
         * No error checking takes place, we assume _respCodeData's type
         * actually corresponds to all invariants we declare as per respCode's
         * documentation.
         * */
        State( const QString& _tag, const Kind _kind, const QString& _message,
                const Code _respCode,
                const std::tr1::shared_ptr<AbstractData> _respCodeData ):
            tag(_tag), kind(_kind), message(_message), respCode(_respCode),
            respCodeData(_respCodeData) {};

        /** @short "Smart" constructor that parses a response out of a QList<QByteArray> */
        State( const QString& _tag, const Kind _kind, QList<QByteArray>::const_iterator& it,
                const QList<QByteArray>::const_iterator& end, const char * const line );

        /** @short Default destructor that makes containers and QtTest happy */
        State(): respCode(NONE) {};

        /** @short helper for operator<<( QTextStream& ) */
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractResponse& other ) const;
    };

    /** @short Structure storing a CAPABILITY untagged response */
    class Capability : public AbstractResponse {
    public:
        /** @short List of capabilities */
        QStringList capabilities;
        Capability( const QStringList& _caps ) : AbstractResponse(CAPABILITY), capabilities(_caps) {};
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractResponse& other ) const;
    };

    /** @short Structure for EXISTS/EXPUNGE/RECENT responses */
    class NumberResponse : public AbstractResponse {
    public:
        /** @short Number that we're storing */
        uint number;
        NumberResponse( const Kind _kind, const uint _num ) throw( UnexpectedHere );
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractResponse& other ) const;
    };

    /** @short Structure storing a LIST untagged response */
    class List : public AbstractResponse {
    public:
        /** @short LIST or LSUB */
        Kind kind;
        /** @short Flags for this particular mailbox */
        QStringList flags;
        /** @short Hierarchy separator
         *
         * QString::null in case original response containded NIL
         * */
        QString separator;
        /** @short Mailbox name */
        QString mailbox;

        List( const Kind _kind, QList<QByteArray>::const_iterator& it,
                const QList<QByteArray>::const_iterator end,
                const char * const lineData) throw( UnexpectedHere );
        List( const Kind _kind, const QStringList& _flags, const QString& _separator, const QString& _mailbox ):
            AbstractResponse(LIST), kind(_kind), flags(_flags), separator(_separator), mailbox(_mailbox) {};
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractResponse& other ) const;
    };

    /** @short Structure storing a FLAGS untagged response */
    class Flags : public AbstractResponse {
    public:
        /** @short List of flags */
        QStringList flags;
        Flags( const QStringList& _flags ) : AbstractResponse(FLAGS), flags(_flags) {};
        Flags( QList<QByteArray>::const_iterator& it, const QList<QByteArray>::const_iterator& end,
                const char * const lineData );
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractResponse& other ) const;
    };

    /** @short Structure storing a SEARCH untagged response */
    class Search : public AbstractResponse {
    public:
        /** @short List of matching messages */
        QList<uint> items;
        Search( const QList<uint>& _items ) : AbstractResponse(SEARCH), items(_items) {};
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractResponse& other ) const;
    };

    /** @short Structure storing a STATUS untagged response */
    class Status : public AbstractResponse {
    public:
        /** @short Indentifies type of status data */
        enum StateKind {
            MESSAGES,
            RECENT,
            UIDNEXT,
            UIDVALIDITY,
            UNSEEN
        };

        /** @short Mailbox name */
        QString mailbox;
        /** @short Associative array of states */
        QMap<StateKind,uint> states;

        Status( const QString& _mailbox, const QMap<StateKind,uint>& _states ) :
            AbstractResponse(STATUS), mailbox(_mailbox), states(_states) {};
        Status( QList<QByteArray>::const_iterator& it,
                const QList<QByteArray>::const_iterator& end,
                const char * const lineData );
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractResponse& other ) const;
        static StateKind stateKindFromStr( QString s );
    };

    /** @short FETCH response */
    class Fetch : public AbstractResponse {
    public:
        /** @short Sequence number of message that we're working with */
        uint number;

        /** @short Fetched items */
        QMap<QString,std::tr1::shared_ptr<AbstractData> > data;

        Fetch( const uint _number, QList<QByteArray>::const_iterator& it,
                const QList<QByteArray>::const_iterator& end,
                const char * const lineData );
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractResponse& other ) const;
    };

    QTextStream& operator<<( QTextStream& stream, const Code& r );
    QTextStream& operator<<( QTextStream& stream, const Kind& res );
    QTextStream& operator<<( QTextStream& stream, const Status::StateKind& kind );
    QTextStream& operator<<( QTextStream& stream, const AbstractResponse& res );
    QTextStream& operator<<( QTextStream& stream, const AbstractData& resp );

    inline bool operator==( const AbstractResponse& first, const AbstractResponse& other ) {
        return first.eq( other );
    }

    inline bool operator!=( const AbstractResponse& first, const AbstractResponse& other ) {
        return !first.eq( other );
    }

    inline bool operator==( const AbstractData& first, const AbstractData& other ) {
        return first.eq( other );
    }

    inline bool operator!=( const AbstractData& first, const AbstractData& other ) {
        return !first.eq( other );
    }

    /** @short Build Responses::Kind from textual value */
    Kind kindFromString( QByteArray str ) throw( UnrecognizedResponseKind );

}

}

#endif // IMAP_RESPONSE_H
