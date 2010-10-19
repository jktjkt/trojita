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
#ifndef IMAP_NETWORK_DOWNLOADMANAGER_H
#define IMAP_NETWORK_DOWNLOADMANAGER_H

#include <QFile>
#include <QNetworkReply>
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Imap {
namespace Network {

/** @short A glue code for managing message parts download

This class uses the existing infrastructure provided by the
MsgPartNetAccessmanager to faciliate downloading of individual
message parts into real files.
*/
class DownloadManager : public QObject
{
    Q_OBJECT
public:
    DownloadManager( QObject* parent,
                    Imap::Network::MsgPartNetAccessManager* _manager,
                    Imap::Mailbox::TreeItem* _item );

    static QString toRealFileName( Imap::Mailbox::TreeItem* item );
private slots:
    void slotDataTransfered();
    void slotTransferError();
    void slotDeleteReply(QNetworkReply* reply);
public slots:
    void slotDownloadNow();
signals:
    void transferError( const QString& errorMessage );
    void fileNameRequested( QString* fileName );
    void succeeded();
private:
    Imap::Network::MsgPartNetAccessManager* manager;
    Imap::Mailbox::TreeItem* item;
    QNetworkReply* reply;
    QFile saving;
    bool saved;

    DownloadManager(const DownloadManager&); // don't implement
    DownloadManager& operator=(const DownloadManager&); // don't implement
};

}
}

#endif // IMAP_NETWORK_DOWNLOADMANAGER_H
