/* Copyright (C) 2007 - 2008 Jan Kundrát <jkt@gentoo.org>

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

#ifndef IMAP_MAILBOXTREE_H
#define IMAP_MAILBOXTREE_H

#include <QList>
#include <QString>
#include "../Parser/Response.h"
#include "../Parser/Message.h"
#include "MailboxMetadata.h"

namespace Imap {

namespace Mailbox {

class Model;
class MailboxModel;

class TreeItem {
    friend class Model; // for _loading and _fetched
    void operator=( const TreeItem& ); // don't implement

protected:
    /** @short Availability of an item */
    enum FetchingState {
        NONE, /**< @short No attempt to download an item has been made yet */
        UNAVAILABLE, /**< @short Item isn't cached and remote requests are disabled */
        LOADING, /**< @short Download of an item is already scheduled */
        DONE /**< @short Item is available right now */
    };

    TreeItem* _parent;
    QList<TreeItem*> _children;
    FetchingState _fetchStatus;
public:
    TreeItem( TreeItem* parent );
    TreeItem* parent() const { return _parent; }
    int row() const;

    virtual ~TreeItem();
    virtual unsigned int childrenCount( Model* const model );
    virtual TreeItem* child( const int offset, Model* const model );
    virtual QList<TreeItem*> setChildren( const QList<TreeItem*> items );
    virtual void fetch( Model* const model ) = 0;
    virtual unsigned int rowCount( Model* const model ) = 0;
    virtual QVariant data( Model* const model, int role ) = 0;
    virtual bool hasChildren( Model* const model ) = 0;
    virtual bool fetched() const { return _fetchStatus == DONE; }
    virtual bool loading() const { return _fetchStatus == LOADING; }
    virtual bool isUnavailable( Model* const model ) const;
};

class TreeItemPart;
class TreeItemMessage;

class TreeItemMailbox: public TreeItem {
    void operator=( const TreeItem& ); // don't implement
    MailboxMetadata _metadata;
    friend class MailboxModel;
public:
    TreeItemMailbox( TreeItem* parent );
    TreeItemMailbox( TreeItem* parent, Responses::List );
    static TreeItemMailbox* fromMetadata( TreeItem* parent, const MailboxMetadata& metadata );

    virtual QList<TreeItem*> setChildren( const QList<TreeItem*> items );
    virtual void fetch( Model* const model );
    virtual unsigned int rowCount( Model* const model );
    virtual QVariant data( Model* const model, int role );
    virtual bool hasChildren( Model* const model );
    virtual TreeItem* child( const int offset, Model* const model );

    bool hasChildMailboxes( Model* const model );

    QString mailbox() const { return _metadata.mailbox; }
    QString separator() const { return _metadata.separator; }
    const MailboxMetadata& mailboxMetadata() const { return _metadata; }
    /** @short Update internal tree with the results of a FETCH response

      If \a changedPart is not null, it will be updated to point to the message
      part whose content got fetched.
    */
    void handleFetchResponse( Model* const model,
                              const Responses::Fetch& response,
                              TreeItemPart** changedPart=0,
                              TreeItemMessage** changedMessage=0 );
    void handleFetchWhileSyncing( Model* const model, Parser* ptr, const Responses::Fetch& response );
    void finalizeFetch( Model* const model, const Responses::Status& response );
    void rescanForChildMailboxes( Model* const model );
    void handleExpunge( Model* const model, const Responses::NumberResponse& resp );
    void handleExistsSynced( Model* const model, Parser* ptr, const Responses::NumberResponse& resp );
    bool isSelectable() const;
private:
    TreeItemPart* partIdToPtr( Model* model, const int msgNumber, const QString& msgId );
};

class TreeItemMsgList: public TreeItem {
    void operator=( const TreeItem& ); // don't implement
    friend class TreeItemMailbox;
    friend class Model;
    FetchingState _numberFetchingStatus;
    int _totalMessageCount;
    int _unreadMessageCount;
public:
    TreeItemMsgList( TreeItem* parent );

    virtual void fetch( Model* const model );
    virtual unsigned int rowCount( Model* const model );
    virtual QVariant data( Model* const model, int role );
    virtual bool hasChildren( Model* const model );

    int totalMessageCount( Model* const model );
    int unreadMessageCount( Model* const model );
    void fetchNumbers( Model* const model );
    void recalcUnreadMessageCount();
    bool numbersFetched() const;
};

class TreeItemMessage: public TreeItem {
    void operator=( const TreeItem& ); // don't implement
    friend class TreeItemMailbox;
    friend class TreeItemMsgList;
    friend class Model;
    Message::Envelope _envelope;
    uint _size;
    uint _uid;
    QStringList _flags;
    bool _flagsHandled;
public:
    TreeItemMessage( TreeItem* parent );

    virtual void fetch( Model* const model );
    virtual unsigned int rowCount( Model* const model );
    virtual QVariant data( Model* const model, int role );
    virtual bool hasChildren( Model* const model ) { Q_UNUSED( model ); return true; }
    Message::Envelope envelope( Model* const model );
    uint size( Model* const model );
    bool isMarkedAsDeleted() const;
    bool isMarkedAsRead() const;
    bool isMarkedAsReplied() const;
    bool isMarkedAsForwarded() const;
    bool isMarkedAsRecent() const;
    uint uid() const;
};

class TreeItemPart: public TreeItem {
    void operator=( const TreeItem& ); // don't implement
    friend class TreeItemMailbox; // needs access to _data
    friend class Model; // dtto
    QString _mimeType;
    QString _charset;
    QByteArray _encoding;
    QByteArray _data;
    QByteArray _bodyFldId;
    QByteArray _bodyDisposition;
    QString _fileName;
    uint _octets;
public:
    TreeItemPart( TreeItem* parent, const QString& mimeType );

    virtual unsigned int childrenCount( Model* const model );
    virtual TreeItem* child( const int offset, Model* const model );
    virtual QList<TreeItem*> setChildren( const QList<TreeItem*> items );

    void fetchFromCache( Model* const model );
    virtual void fetch( Model* const model );
    virtual unsigned int rowCount( Model* const model );
    virtual QVariant data( Model* const model, int role );
    virtual bool hasChildren( Model* const model );

    QString partId() const;
    QString pathToPart() const;
    TreeItemMessage* message() const;

    /** @short Provide access to the internal buffer holding data

        It is safe to access the obtained pointer as long as this object is not
        deleted. This function violates the classic concept of object
        encapsulation, but is really useful for the implementation of
        Imap::Network::MsgPartNetworkReply.
     */
    QByteArray* dataPtr();
    QString mimeType() const { return _mimeType; }
    QString charset() const { return _charset; }
    void setCharset( const QString& ch ) { _charset = ch; }
    void setEncoding( const QByteArray& encoding ) { _encoding = encoding; }
    QByteArray encoding() const { return _encoding; }
    void setBodyFldId( const QByteArray& id ) { _bodyFldId = id; }
    QByteArray bodyFldId() const { return _bodyFldId; }
    void setBodyDisposition( const QByteArray& disposition ) { _bodyDisposition = disposition; }
    QByteArray bodyDisposition() const { return _bodyDisposition; }
    void setFileName( const QString& name ) { _fileName = name; }
    QString fileName() const { return _fileName; }
    void setOctets( const uint size ) { _octets = size; }
    /** @short Return the downloadable size of the message part */
    uint octets() const { return _octets; }
private:
    bool isTopLevelMultiPart() const;
};

}

}

#endif // IMAP_MAILBOXTREE_H
