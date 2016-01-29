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

FileDownloadManager::FileDownloadManager(QObject *parent, Imap::Network::MsgPartNetAccessManager *manager, const QUrl &url, const QModelIndex &relativeRoot):
    QObject(parent), manager(manager), partIndex(QModelIndex()), reply(0), saved(false)
{
    if (url.scheme().toLower() == QLatin1String("cid")) {
        if (!relativeRoot.isValid()) {
            m_errorMessage = tr("Cannot translate CID URL: message is gone");
            return;
        }
        auto cid = url.path().toUtf8();
        if (!cid.startsWith('<')) {
            cid = '<' + cid;
        }
        if (!cid.endsWith('>')) {
            cid += '>';
        }
        partIndex = manager->cidToPart(relativeRoot, cid);
        if (!partIndex.isValid()) {
            m_errorMessage = tr("CID not found (%1)").arg(url.toString());
        }
    } else {
        m_errorMessage = tr("Unsupported URL (%1)").arg(url.toString());
    }
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
    if (!m_errorMessage.isNull()) {
        emit transferError(m_errorMessage);
        return;
    }

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
    url.setScheme(QStringLiteral("trojita-imap"));
    url.setHost(QStringLiteral("msg"));
    url.setPath(partIndex.data(Imap::Mailbox::RolePartPathToPart).toString());
    request.setUrl(url);
    reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, &FileDownloadManager::onPartDataTransfered);
    connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
            this, &FileDownloadManager::onReplyTransferError);
    connect(manager, &QNetworkAccessManager::finished, this, &FileDownloadManager::deleteReply);
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
    connect(m_combiner.data(), &Mailbox::FullMessageCombiner::completed, this, &FileDownloadManager::onMessageDataTransferred);
    connect(m_combiner.data(), &Mailbox::FullMessageCombiner::failed, this, &FileDownloadManager::onCombinerTransferError);
    connect(m_combiner.data(), &Mailbox::FullMessageCombiner::completed, m_combiner.data(), &QObject::deleteLater, Qt::QueuedConnection);
    connect(m_combiner.data(), &Mailbox::FullMessageCombiner::failed, m_combiner.data(), &QObject::deleteLater, Qt::QueuedConnection);
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
