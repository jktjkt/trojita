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

#ifndef IMAP_MESSAGE_H
#define IMAP_MESSAGE_H

#include <tr1/memory>
#include <QByteArray>
#include <QVariant>
#include <QDateTime>
#include <QString>
#include <QPair>
#include "Imap/Data.h"
#include "Imap/Exceptions.h"
#include "Imap/LowLevelParser.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling e-mail messages */
namespace Message {

    /** @short Storage container for one address from an envelope */
    struct MailAddress {
        /** @short Phrase from RFC2822 mailbox */
        QByteArray name;

        /** @hosrt Route information */
        QByteArray adl;

        /** @short RFC2822 Group Name or Local Part */
        QByteArray mailbox;

        /** @short RFC2822 Domain Name */
        QByteArray host;

        MailAddress( const QByteArray& _name, const QByteArray& _adl, 
                const QByteArray& _mailbox, const QByteArray& _host ):
            name(_name), adl(_adl), mailbox(_mailbox), host(_host) {};
        MailAddress( const QVariantList& input, const QByteArray& line, const int start );
    };

    /** @short Storage for envelope */
    struct Envelope {
        QDateTime date;
        QString subject;
        QList<MailAddress> from;
        QList<MailAddress> sender;
        QList<MailAddress> replyTo;
        QList<MailAddress> to;
        QList<MailAddress> cc;
        QList<MailAddress> bcc;
        QByteArray inReplyTo;
        QByteArray messageId;

        Envelope() {};
        Envelope( const QDateTime& _date, const QString& _subject, const QList<MailAddress>& _from, 
                const QList<MailAddress>& _sender, const QList<MailAddress>& _replyTo,
                const QList<MailAddress>& _to, const QList<MailAddress>& _cc,
                const QList<MailAddress>& _bcc, const QByteArray& _inReplyTo,
                const QByteArray& _messageId ):
            date(_date), subject(_subject), from(_from), sender(_sender), replyTo(_replyTo),
            to(_to), cc(_cc), bcc(_bcc), inReplyTo(_inReplyTo), messageId(_messageId) {};
        static Envelope fromList( const QVariantList& items, const QByteArray& line, const int start );

    private:
        static QList<MailAddress> getListOfAddresses( const QVariant& in,
                const QByteArray& line, const int start );
        friend class Fetch;
    };


    /** @short Abstract parent of all Message classes
     *
     * A message can be either one-part (OneMessage) or multipart (MultiMessage)
     * */
    struct AbstractMessage: public Responses::AbstractData {
        virtual ~AbstractMessage() {};
        static std::tr1::shared_ptr<AbstractMessage> fromList( const QVariantList& items, const QByteArray& line, const int start );

        typedef QMap<QByteArray,QByteArray> bodyFldParam_t;
        typedef QPair<QByteArray, bodyFldParam_t> bodyFldDsp_t;

        static bodyFldParam_t makeBodyFldParam( const QVariant& list, const QByteArray& line, const int start );
        static bodyFldDsp_t makeBodyFldDsp( const QVariant& list, const QByteArray& line, const int start );
        static QList<QByteArray> makeBodyFldLang( const QVariant& input, const QByteArray& line, const int start );
    };

    /** @short Abstract parent class for all non-multipart messages */
    struct OneMessage: public AbstractMessage {
        QString mediaType;
        QString mediaSubType;
        bodyFldParam_t bodyFldParam;
        QByteArray bodyFldId;
        QByteArray bodyFldDesc;
        QByteArray bodyFldEnc;
        uint bodyFldOctets;
        // optional fields:
        QByteArray bodyFldMd5;
        bodyFldDsp_t bodyFldDsp;
        QList<QByteArray> bodyFldLang;
        QByteArray bodyFldLoc;
        QVariant bodyExtension;
        OneMessage( const QString& _mediaType, const QString& _mediaSubType,
                const bodyFldParam_t& _bodyFldParam, const QByteArray& _bodyFldId,
                const QByteArray& _bodyFldDesc, const QByteArray& _bodyFldEnc,
                const uint _bodyFldOctets, const QByteArray& _bodyFldMd5,
                const bodyFldDsp_t& _bodyFldDsp,
                const QList<QByteArray>& _bodyFldLang, const QByteArray& _bodyFldLoc,
                const QVariant& _bodyExtension ):
            mediaType(_mediaType), mediaSubType(_mediaSubType), bodyFldParam(_bodyFldParam),
            bodyFldId(_bodyFldId), bodyFldDesc(_bodyFldDesc), bodyFldEnc(_bodyFldEnc),
            bodyFldOctets(_bodyFldOctets), bodyFldMd5(_bodyFldMd5), bodyFldDsp(_bodyFldDsp),
            bodyFldLang(_bodyFldLang), bodyFldLoc(_bodyFldLoc), bodyExtension(_bodyExtension) {};
    };

    /** @short Ordinary Message (body-type-basic in RFC3501) */
    struct BasicMessage: public OneMessage {
        // nothing new, just stuff from OneMessage
        BasicMessage( const QString& _mediaType, const QString& _mediaSubType,
                const bodyFldParam_t& _bodyFldParam, const QByteArray& _bodyFldId,
                const QByteArray& _bodyFldDesc, const QByteArray& _bodyFldEnc,
                const uint _bodyFldOctets, const QByteArray& _bodyFldMd5,
                const bodyFldDsp_t& _bodyFldDsp,
                const QList<QByteArray>& _bodyFldLang, const QByteArray& _bodyFldLoc,
                const QVariant& _bodyExtension ):
            OneMessage( _mediaType, _mediaSubType, _bodyFldParam, _bodyFldId,
                    _bodyFldDesc, _bodyFldEnc, _bodyFldOctets, _bodyFldMd5,
                    _bodyFldDsp, _bodyFldLang, _bodyFldLoc, _bodyExtension) {};
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractData& other ) const;
    };

    /** @short A message holding another RFC822 message (body-type-msg) */
    struct MsgMessage: public OneMessage {
        Envelope envelope;
        std::tr1::shared_ptr<AbstractMessage> body;
        uint bodyFldLines;
        MsgMessage( const QString& _mediaType, const QString& _mediaSubType,
                const bodyFldParam_t& _bodyFldParam, const QByteArray& _bodyFldId,
                const QByteArray& _bodyFldDesc, const QByteArray& _bodyFldEnc,
                const uint _bodyFldOctets, const QByteArray& _bodyFldMd5,
                const bodyFldDsp_t& _bodyFldDsp,
                const QList<QByteArray>& _bodyFldLang, const QByteArray& _bodyFldLoc,
                const QVariant& _bodyExtension,
                const Envelope& _envelope, const std::tr1::shared_ptr<AbstractMessage>& _body,
                const uint _bodyFldLines ):
            OneMessage( _mediaType, _mediaSubType, _bodyFldParam, _bodyFldId,
                    _bodyFldDesc, _bodyFldEnc, _bodyFldOctets, _bodyFldMd5,
                    _bodyFldDsp, _bodyFldLang, _bodyFldLoc, _bodyExtension),
            envelope(_envelope), body(_body), bodyFldLines(_bodyFldLines) {};
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractData& other ) const;
    };

    /** @short A text message (body-type-text) */
    struct TextMessage: public OneMessage {
        uint bodyFldLines;
        TextMessage( const QString& _mediaType, const QString& _mediaSubType,
                const bodyFldParam_t& _bodyFldParam, const QByteArray& _bodyFldId,
                const QByteArray& _bodyFldDesc, const QByteArray& _bodyFldEnc,
                const uint _bodyFldOctets, const QByteArray& _bodyFldMd5,
                const bodyFldDsp_t& _bodyFldDsp,
                const QList<QByteArray>& _bodyFldLang, const QByteArray& _bodyFldLoc,
                const QVariant& _bodyExtension,
                const uint _bodyFldLines ):
            OneMessage( _mediaType, _mediaSubType, _bodyFldParam, _bodyFldId,
                    _bodyFldDesc, _bodyFldEnc, _bodyFldOctets, _bodyFldMd5,
                    _bodyFldDsp, _bodyFldLang, _bodyFldLoc, _bodyExtension),
            bodyFldLines(_bodyFldLines) {};
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractData& other ) const;
    };

    /** @short Multipart message (body-type-mpart) */
    struct MultiMessage: public AbstractMessage {
        QList<std::tr1::shared_ptr<AbstractMessage> > bodies;
        QString mediaSubType;
        // optional fields
        bodyFldParam_t bodyFldParam;
        bodyFldDsp_t bodyFldDsp;
        QList<QByteArray> bodyFldLang;
        QByteArray bodyFldLoc;
        QVariant bodyExtension;

        MultiMessage( const QList<std::tr1::shared_ptr<AbstractMessage> >& _bodies,
                const QString& _mediaSubType, const bodyFldParam_t& _bodyFldParam,
                const bodyFldDsp_t& _bodyFldDsp,
                const QList<QByteArray>& _bodyFldLang, const QByteArray& _bodyFldLoc,
                const QVariant& _bodyExtension ):
            bodies(_bodies), mediaSubType(_mediaSubType), bodyFldParam(_bodyFldParam),
            bodyFldDsp(_bodyFldDsp), bodyFldLang(_bodyFldLang), bodyFldLoc(_bodyFldLoc),
            bodyExtension(_bodyExtension) {};
        virtual QTextStream& dump( QTextStream& s ) const;
        virtual bool eq( const AbstractData& other ) const;
    };

    QTextStream& operator<<( QTextStream& stream, const MailAddress& address );
    QTextStream& operator<<( QTextStream& stream, const QList<MailAddress>& address );
    QTextStream& operator<<( QTextStream& stream, const Envelope& e );

    bool operator==( const Envelope& a, const Envelope& b );
    inline bool operator!=( const Envelope& a, const Envelope& b ) { return !(a == b); };
    bool operator==( const MailAddress& a, const MailAddress& b );
    inline bool operator!=( const MailAddress& a, const MailAddress& b ) { return !(a == b); };

}

}

#endif /* IMAP_MESSAGE_H */
