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
#include "Parser/Response.h"
#include "Parser/Message.h"

namespace Imap {

namespace Mailbox {

class Model;

class TreeItem {
    void operator=( const TreeItem& ); // don't implement
protected:
    TreeItem* _parent;
    QList<TreeItem*> _children;
    Model* _model;
    bool _fetched, _loading;
public:
    TreeItem( TreeItem* parent );
    TreeItem* parent() const { return _parent; };
    int row() const;

    virtual ~TreeItem();
    virtual unsigned int childrenCount( const Model* const model );
    virtual TreeItem* child( const int offset, const Model* const model );
    virtual void setChildren( const QList<TreeItem*> items );
    virtual void fetch( const Model* const model ) = 0;
    virtual unsigned int columnCount( const Model* const model ) { return 1; };
    virtual unsigned int rowCount( const Model* const model ) = 0;
    virtual QVariant data( const Model* const model, int role ) = 0;
    virtual bool hasChildren( const Model* const model ) = 0;
};

class TreeItemMailbox: public TreeItem {
    void operator=( const TreeItem& ); // don't implement
    QString _mailbox;
    QString _separator;
    QStringList _flags;
public:
    TreeItemMailbox( TreeItem* parent );
    TreeItemMailbox( TreeItem* parent, Responses::List );

    virtual void setChildren( const QList<TreeItem*> items );
    virtual void fetch( const Model* const model );
    virtual unsigned int rowCount( const Model* const model );
    virtual QVariant data( const Model* const model, int role );
    virtual bool hasChildren( const Model* const model );
    virtual TreeItem* child( const int offset, const Model* const model );

    bool hasChildMailboxes( const Model* const model );

    QString mailbox() const { return _mailbox; };
    QString separator() const { return _separator; };
    void handleFetchResponse( const Model* const model, const Responses::Fetch& response );
    void finalizeFetch( const Model* const model, const Responses::Status& response );
};

class TreeItemMsgList: public TreeItem {
    void operator=( const TreeItem& ); // don't implement
    friend class TreeItemMailbox;
public:
    TreeItemMsgList( TreeItem* parent );

    virtual void fetch( const Model* const model );
    virtual unsigned int rowCount( const Model* const model );
    virtual QVariant data( const Model* const model, int role );
    virtual bool hasChildren( const Model* const model );
};

class TreeItemMessage: public TreeItem {
    void operator=( const TreeItem& ); // don't implement
    friend class TreeItemMailbox;
    Message::Envelope _envelope;
public:
    TreeItemMessage( TreeItem* parent );

    virtual void fetch( const Model* const model );
    virtual unsigned int rowCount( const Model* const model );
    virtual QVariant data( const Model* const model, int role );
    virtual bool hasChildren( const Model* const model ) { return true; };
};

class TreeItemPart: public TreeItem {
    void operator=( const TreeItem& ); // don't implement
    QString _mimeType;
    QByteArray _data;
public:
    TreeItemPart( TreeItem* parent, const QString& mimeType );

    virtual unsigned int childrenCount( const Model* const model );
    virtual TreeItem* child( const int offset, const Model* const model );
    virtual void setChildren( const QList<TreeItem*> items );

    virtual void fetch( const Model* const model );
    virtual unsigned int rowCount( const Model* const model );
    virtual QVariant data( const Model* const model, int role );
    virtual bool hasChildren( const Model* const model );

    QString partId() const;
    TreeItemMessage* message();
};

}

}

#endif // IMAP_MAILBOXTREE_H
