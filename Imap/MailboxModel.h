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

#ifndef IMAP_MAILBOXMODEL_H
#define IMAP_MAILBOXMODEL_H

#include <tr1/memory>
#include <QAbstractItemModel>
#include "Imap/Cache.h"
#include "Imap/Authenticator.h"
#include "Imap/Parser.h"

/** @short Namespace for IMAP interaction */
namespace Imap {

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox {

/** @short A model component for the model-view architecture */
class MailboxModel: public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY( ThreadAlgorithm threadSorting READ threadSorting WRITE setThreadSorting )

public:
    /** @short IMAP state of a connection */
    enum ImapState {
        IMAP_STATE_CONN_ESTABLISHED /**< Connection established, no details known yet */,
        IMAP_STATE_NOT_AUTH /**< Before we can do anything, we have to authenticate ourselves */,
        IMAP_STATE_AUTH /**< We are authenticated, now we must select a mailbox */,
        IMAP_STATE_SELECTED /**< Some mailbox is selected */,
        IMAP_STATE_LOGOUT /**< We have been logged out */
    };

    MailboxModel( QObject* parent, CachePtr cache,
            AuthenticatorPtr authenticator, ParserPtr parser,
            const QString& mailbox, const bool readWrite = true,
            const ThreadAlgorithm sorting = THREAD_NONE );
    ThreadAlgorithm threadSorting();
    void setThreadSorting( const ThreadAlgorithm value );


    virtual QModelIndex index(int, int, const QModelIndex&) const {return QModelIndex();};
    virtual QModelIndex parent(const QModelIndex&) const {return QModelIndex();};
    virtual int rowCount(const QModelIndex&) const {return 0;};
    virtual int columnCount(const QModelIndex&) const {return 0;};
    virtual QVariant data(const QModelIndex&, int) const { return QVariant();};



private slots:
    void responseReceived();

private:

    void updateState( const ImapState state );
    void responseReceived( std::tr1::shared_ptr<Imap::Responses::AbstractResponse> resp );

    void handleInitial( const Imap::Responses::State* const state );
    void authenticate();
    void select();
    void alert( const Imap::Responses::AbstractResponse* const resp, const QString& message );
    void unknownResponseCode( const Imap::Responses::AbstractResponse* const resp );

    CachePtr _cache;
    AuthenticatorPtr _authenticator;
    ParserPtr _parser;
    QString _mailbox;
    ThreadAlgorithm _threadSorting;
    bool _readWrite;

    ImapState _state;
    QStringList _capabilities;

    bool _capabilitiesFresh;
    bool _waitingForSelect;

    /** @short Tag used for sending the AUTHENTICATE command */
    Imap::CommandHandle _authTag;

    /** @short Tag used for selecting mailbox */
    Imap::CommandHandle _selectTag;
};

QTextStream& operator<<( QTextStream& s, const MailboxModel::ImapState state );

}

}

#endif /* IMAP_MAILBOXMODEL_H */
