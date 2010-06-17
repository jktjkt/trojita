/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#ifndef FORMATTINGREPLY_H
#define FORMATTINGREPLY_H

#include <QBuffer>
#include <QModelIndex>
#include <QNetworkReply>

namespace Imap {
namespace Mailbox {
class Model;
class TreeItemMessage;
class TreeItemPart;
}

namespace Network {

class MsgPartNetworkReply;

class FormattingReply : public QNetworkReply
{
Q_OBJECT
public:
    FormattingReply( QObject* parent, Imap::Mailbox::Model* _model,
        Imap::Mailbox::TreeItemMessage* _msg, Imap::Mailbox::TreeItemPart* _part );
    virtual ~FormattingReply() {}
    virtual void abort();
    virtual void close();
    virtual qint64 bytesAvailable() const;

protected slots:
    /** @short This slot is invoked whenever any sub-request has finished loading */
    void anotherReplyFinished();

    /** @short All of the raw data for the original message part is available now */
    virtual void mainReplyFinished();

protected:
    virtual qint64 readData( char* data, qint64 maxSize );

    /** @short Launch a request for some message part */
    void requestAnotherPart( Imap::Mailbox::TreeItemPart* anotherPart );

    void setData( const QString& mimeType, const QByteArray& data );

    /** @short Tell te world that we're done here */
    virtual void everythingFinished();


    Imap::Mailbox::Model* model;
    Imap::Mailbox::TreeItemMessage* msg;
    Imap::Mailbox::TreeItemPart* part;
    QBuffer buffer;
    QList<MsgPartNetworkReply*> replies;
    QList<bool> pendingBitmap;
    uint pendingCount;
    QList<MsgPartNetworkReply*> reqAlreadyFetched;
};

}
}

#endif // FORMATTINGREPLY_H
