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
#ifndef IMAP_NETWORK_DOWNLOADMANAGER_H
#define IMAP_NETWORK_DOWNLOADMANAGER_H

#include <QFile>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QNetworkReply>
#include "Imap/Model/FullMessageCombiner.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Imap
{
namespace Network
{

/** @short A glue code for managing message parts download

This class uses the existing infrastructure provided by the
MsgPartNetAccessmanager to faciliate downloading of individual
message parts into real files.
*/
class FileDownloadManager : public QObject
{
    Q_OBJECT
public:
    FileDownloadManager(QObject *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex);
    static QString toRealFileName(const QModelIndex &index);
private slots:
    void onPartDataTransfered();
    void onReplyTransferError();
    void onCombinerTransferError(const QString &message);
    void deleteReply(QNetworkReply *reply);
public slots:
    void downloadPart();
    void downloadMessage();
    void onMessageDataTransferred();
signals:
    void transferError(const QString &errorMessage);
    void fileNameRequested(QString *fileName);
    void succeeded();
    void cancelled();
private:
    Imap::Network::MsgPartNetAccessManager *manager;
    QPersistentModelIndex partIndex;
    QNetworkReply *reply;
    QFile saving;
    bool saved;
    QPointer<Imap::Mailbox::FullMessageCombiner> m_combiner;

    FileDownloadManager(const FileDownloadManager &); // don't implement
    FileDownloadManager &operator=(const FileDownloadManager &); // don't implement
};

}
}

#endif // IMAP_NETWORK_DOWNLOADMANAGER_H
