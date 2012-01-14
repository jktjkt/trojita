/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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
#include <QDebug>
#include <QStringList>
#include <QTimer>

#include "MsgPartNetworkReply.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Imap {

namespace Network {

MsgPartNetworkReply::MsgPartNetworkReply(QObject* parent, Imap::Mailbox::Model* _model, Imap::Mailbox::TreeItemMessage* _msg,
        Imap::Mailbox::TreeItemPart* _part):
    QNetworkReply(parent), model(_model), msg(_msg), part(_part)
{
    setOpenMode(QIODevice::ReadOnly | QIODevice::Unbuffered);
    Q_ASSERT(model);
    Q_ASSERT(msg);
    Q_ASSERT(part);

    connect(_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(slotModelDataChanged(QModelIndex,QModelIndex)));

    // We have to ask for contents before we check whether it's already fetched
    part->fetch(model);
    if (part->fetched()) {
        QTimer::singleShot(0, this, SLOT(slotMyDataChanged()));
    }

    buffer.setBuffer(part->dataPtr());
    buffer.open(QIODevice::ReadOnly);
}

/** @short Check to see whether the data which concern this object has arrived already */
void MsgPartNetworkReply::slotModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    Q_UNUSED(bottomRight);
    // FIXME: use bottomRight as well!
    if (topLeft.model() != model) {
        return;
    }
    Imap::Mailbox::TreeItemPart* receivedPart = dynamic_cast<Imap::Mailbox::TreeItemPart*>(Imap::Mailbox::Model::realTreeItem(topLeft));
    if (receivedPart == part) {
        slotMyDataChanged();
    }
}

/** @short Data for the current message part are available now */
void MsgPartNetworkReply::slotMyDataChanged()
{
    if (part->mimeType().startsWith(QLatin1String("text/"))) {
        setHeader(QNetworkRequest::ContentTypeHeader,
                  part->charset().isEmpty() ?
                    part->mimeType() :
                    QString::fromAscii("%1; charset=%2").arg(part->mimeType(), part->charset())
                 );
    } else if (part->mimeType() == QLatin1String("image/pjpeg")) {
        // The "image/pjpeg" nonsense is non-standard kludge produced by Micorosft Internet Explorer
        // (http://msdn.microsoft.com/en-us/library/ms775147(VS.85).aspx#_replace). As of May 2011, it is not listed in
        // the official list of assigned MIME types (http://www.iana.org/assignments/media-types/image/index.html), but generated
        // by MSIE nonetheless. Users of e-mail can see it for example in messages produced by webmails which do not check the
        // client-provided MIME types. QWebView would (arguably correctly) refuse to display such a blob, but the damned users
        // typically want to see their images (I certainly do), even though they are not standards-compliant. Hence we fix the
        // header here.
        setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("image/jpeg"));
    } else {
        setHeader(QNetworkRequest::ContentTypeHeader, part->mimeType());
    }
    emit readyRead();
    emit finished();
}

/** @short QIODevice compatibility */
void MsgPartNetworkReply::abort()
{
    close();
}

/** @short QIODevice compatibility */
void MsgPartNetworkReply::close()
{
    buffer.close();
}

/** @short QIODevice compatibility */
qint64 MsgPartNetworkReply::bytesAvailable() const
{
    return buffer.bytesAvailable() + QNetworkReply::bytesAvailable();
}

/** @short QIODevice compatibility */
qint64 MsgPartNetworkReply::readData(char* data, qint64 maxSize)
{
    return buffer.read(data, maxSize);
}

}
}
