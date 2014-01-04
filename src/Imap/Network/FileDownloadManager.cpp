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
#include "FileDownloadManager.h"
#include "Imap/Model/FullMessageCombiner.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

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
    if (!fileName.isEmpty())
        return fileName;

    QString uid = index.data(Imap::Mailbox::RoleMessageUid).toString();
    QString partId = index.data(Imap::Mailbox::RolePartId).toString();
    if (partId.isEmpty()) {
        // we're saving the complete message
        return tr("msg_%1.eml").arg(uid);
    } else {
        // it's a message part
        return tr("msg_%1_%2").arg(uid, partId);
    }
}

void FileDownloadManager::downloadPart()
{
    if (!partIndex.isValid()) {
        emit transferError(tr("FileDownloadManager::downloadPart(): part has disappeared"));
        return;
    }
    QString saveFileName = toRealFileName(partIndex);
    emit fileNameRequested(&saveFileName);
    if (saveFileName.isEmpty()) {
        emit cancelled();
        return;
    }

    saving.setFileName(saveFileName);
    saved = false;

    QNetworkRequest request;
    QUrl url;
    url.setScheme(QLatin1String("trojita-imap"));
    url.setHost(QLatin1String("msg"));
    url.setPath(partIndex.data(Imap::Mailbox::RolePartPathToPart).toString());
    request.setUrl(url);
    reply = manager->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(onPartDataTransfered()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onReplyTransferError()));
    connect(manager, SIGNAL(finished(QNetworkReply *)), this, SLOT(deleteReply(QNetworkReply *)));
}

void FileDownloadManager::downloadMessage()
{
    if (!partIndex.isValid()) {
        emit transferError(tr("FileDownloadManager::downloadMessage(): message has disappeared"));
        return;
    }
    QString saveFileName = toRealFileName(partIndex);
    Q_ASSERT(!m_combiner);
    m_combiner = new Imap::Mailbox::FullMessageCombiner(partIndex, this);

    emit fileNameRequested(&saveFileName);
    if (saveFileName.isEmpty()) {
        emit cancelled();
        return;
    }

    saving.setFileName(saveFileName);
    saved = false;
    connect(m_combiner, SIGNAL(completed()), this, SLOT(onMessageDataTransferred()));
    connect(m_combiner, SIGNAL(failed(QString)), this, SLOT(onCombinerTransferError(QString)));
    connect(m_combiner, SIGNAL(completed()), m_combiner, SLOT(deleteLater()), Qt::QueuedConnection);
    connect(m_combiner, SIGNAL(failed(QString)), m_combiner, SLOT(deleteLater()), Qt::QueuedConnection);
    m_combiner->load();
}

void FileDownloadManager::onPartDataTransfered()
{
    Q_ASSERT(reply);
    if (reply->error() == QNetworkReply::NoError) {
        if (!saving.open(QIODevice::WriteOnly)) {
            emit transferError(saving.errorString());
            return;
        }
        if (saving.write(reply->readAll()) == -1) {
            emit transferError(saving.errorString());
            return;
        }
        if (!saving.flush()) {
            emit transferError(saving.errorString());
            return;
        }
        saving.close();
        saved = true;
        emit succeeded();
    }
}

void FileDownloadManager::onMessageDataTransferred()
{
    Q_ASSERT(m_combiner);
    if (!saving.open(QIODevice::WriteOnly)) {
        emit transferError(saving.errorString());
        return;
    }
    if (saving.write(m_combiner->data().data()) == -1) {
        emit transferError(saving.errorString());
        return;
    }
    if (!saving.flush()) {
        emit transferError(saving.errorString());
        return;
    }
    saving.close();
    saved = true;
    emit succeeded();
}

void FileDownloadManager::onReplyTransferError()
{
    Q_ASSERT(reply);
    emit transferError(reply->errorString());
}

void FileDownloadManager::onCombinerTransferError(const QString &message)
{
    emit transferError(message);
}

void FileDownloadManager::deleteReply(QNetworkReply *reply)
{
    if (reply == this->reply) {
        if (!saved)
            onPartDataTransfered();
        delete reply;
        this->reply = 0;
    }
}

}
}
