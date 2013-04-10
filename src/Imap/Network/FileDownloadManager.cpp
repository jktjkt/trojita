/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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
#include "FileDownloadManager.h"
#include "Imap/Model/FullMessageCombiner.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QDesktopServices>
#else
#include <QStandardPaths>
#endif
#include <QDir>

namespace Imap
{

namespace Network
{

FileDownloadManager::FileDownloadManager(QObject *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex):
    QObject(parent), manager(manager), partIndex(partIndex), reply(0), saved(false)
{
}

QString FileDownloadManager::toRealFileName(const QModelIndex &index)
{
    QString fileName = index.data(Imap::Mailbox::RolePartFileName).toString();
    QString uid = index.data(Imap::Mailbox::RoleMessageUid).toString();
    QString partId = index.data(Imap::Mailbox::RolePartId).toString();
    QString name = fileName.isEmpty() ? tr("msg_%1_%2").arg(uid, partId) : fileName;

    return name;
}

QVariant FileDownloadManager::data(int role) const
{
    return partIndex.data(role);
}

void FileDownloadManager::slotDownloadNow()
{
    if (!partIndex.isValid()) {
        emit transferError(tr("FileDownloadManager::slotDownloadNow(): part has disappeared"));
        return;
    }
    QString saveFileName = toRealFileName(partIndex);
    emit fileNameRequested(&saveFileName);
    if (saveFileName.isEmpty())
        return;

    saving.setFileName(saveFileName);
    saved = false;

    QNetworkRequest request;
    QUrl url;
    url.setScheme(QLatin1String("trojita-imap"));
    url.setHost(QLatin1String("msg"));
    url.setPath(partIndex.data(Imap::Mailbox::RolePartPathToPart).toString());
    request.setUrl(url);
    reply = manager->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(slotDataTransfered()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(slotTransferError()));
    connect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(slotDeleteReply(QNetworkReply *)));
}

void FileDownloadManager::slotDataTransfered()
{
    Q_ASSERT(reply);
    if (reply->error() == QNetworkReply::NoError) {
        saving.open(QIODevice::WriteOnly);
        saving.write(reply->readAll());
        saving.close();
        saved = true;
        emit succeeded();
    }
}

void FileDownloadManager::slotDataSave()
{
    Q_ASSERT(m_combiner);
    saving.open(QIODevice::WriteOnly);
    saving.write((m_combiner->data()).data());
    saving.close();
    saved = true;
    emit succeeded();
}

void FileDownloadManager::slotTransferError()
{
    Q_ASSERT(reply);
    emit transferError(reply->errorString());
}

void FileDownloadManager::slotDownloadCompleteMessageNow()
{
    if (!partIndex.isValid()) {
        emit transferError(tr("FileDownloadManager::slotDownloadCompleteMessageNow(): part has disappeared"));
        return;
    }
    QString saveFileName = toRealFileName(partIndex);
    m_combiner = new Imap::Mailbox::FullMessageCombiner(partIndex, this);

    emit fileNameRequested(&saveFileName);
    if (saveFileName.isEmpty())
        return;

    saving.setFileName(saveFileName);
    saved = false;
    connect(m_combiner, SIGNAL(completed()), this, SLOT(slotDataSave()));
    m_combiner->load();//emits completed() upon completion

}

void FileDownloadManager::slotDeleteReply(QNetworkReply *reply)
{
    if (reply == this->reply) {
        if (!saved)
            slotDataTransfered();
        delete reply;
        this->reply = 0;
    }
}

}
}
