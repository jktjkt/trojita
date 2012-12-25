/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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
#include "AttachmentView.h"
#include "Imap/Network/FileDownloadManager.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"

#include <QAction>
#include <QDesktopServices>
#include <QDrag>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>

namespace Gui
{

AttachmentView::AttachmentView(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex):
    QWidget(parent), m_partIndex(partIndex), m_fileDownloadManager(0), m_downloadButton(0), m_downloadAttachment(0), m_openAttachment(0)
{
    m_fileDownloadManager = new Imap::Network::FileDownloadManager(this, manager, partIndex);
    QHBoxLayout *layout = new QHBoxLayout(this);
    QLabel *lbl = new QLabel(tr("Attachment %1 (%2, %3)").arg(partIndex.data(Imap::Mailbox::RolePartFileName).toString(),
                             partIndex.data(Imap::Mailbox::RolePartMimeType).toString(),
                             Imap::Mailbox::PrettySize::prettySize(partIndex.data(Imap::Mailbox::RolePartOctets).toUInt(),
                                                                   Imap::Mailbox::PrettySize::WITH_BYTES_SUFFIX)));
    layout->addWidget(lbl);
    m_downloadButton = new QToolButton();
    m_downloadButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_downloadButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QMenu *menu = new QMenu(this);
    m_downloadAttachment = menu->addAction(tr("Download"));
    m_openAttachment = menu->addAction(tr("Open Directly"));
    connect(m_downloadAttachment, SIGNAL(triggered()), this, SLOT(slotDownloadAttachment()));
    connect(m_openAttachment, SIGNAL(triggered()), this, SLOT(slotOpenAttachment()));

    m_downloadButton->setMenu(menu);
    m_downloadButton->setDefaultAction(m_downloadAttachment);

    connect(m_downloadButton, SIGNAL(clicked()), m_fileDownloadManager, SLOT(slotDownloadNow()));

    layout->addWidget(m_downloadButton);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void AttachmentView::slotDownloadAttachment()
{
    // We should disconnect the fileDownloadManager from any connected slots
    // and reconnect again to prevent duplicate emitting of signals
    m_fileDownloadManager->disconnect();
    m_downloadButton->setDefaultAction(m_downloadAttachment);
    connect(m_fileDownloadManager, SIGNAL(fileNameRequested(QString *)), this, SLOT(slotFileNameRequested(QString *)));
}

void AttachmentView::slotOpenAttachment()
{
    // We should disconnect the fileDownloadManager from any connected slots
    // and reconnect again to prevent duplicate emitting of signals
    m_fileDownloadManager->disconnect();
    m_downloadButton->setDefaultAction(m_openAttachment);
    connect(m_fileDownloadManager, SIGNAL(fileNameRequested(QString*)), this, SLOT(slotFileNameRequestedOnOpen(QString*)));
    connect(m_fileDownloadManager, SIGNAL(succeeded()), this, SLOT(slotTransferSucceeded()));
}

void AttachmentView::slotFileNameRequestedOnOpen(QString *fileName)
{
    *fileName = QDir(QDesktopServices::storageLocation(QDesktopServices::TempLocation)).filePath(*fileName);
}

void AttachmentView::slotFileNameRequested(QString *fileName)
{
    QString fileLocation;

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    fileLocation =  QDir(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation)).filePath(*fileName);
#else
    fileLocation = QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(*fileName);
#endif

    *fileName = QFileDialog::getSaveFileName(this, tr("Save Attachment"), fileLocation, QString(), 0, QFileDialog::HideNameFilterDetails);
}

void AttachmentView::slotTransferError(const QString &errorString)
{
    QMessageBox::critical(this, tr("Can't save attachment"), tr("Unable to save the attachment. Error:\n%1").arg(errorString));
}

void AttachmentView::slotTransferSucceeded()
{
    QString fileRealPath = QDir(QDesktopServices::storageLocation(QDesktopServices::TempLocation)).
            filePath(m_fileDownloadManager->toRealFileName(m_partIndex));
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileRealPath));
}

void AttachmentView::mousePressEvent(QMouseEvent *event)
{
    QWidget *child = childAt(event->pos());
    if (child == m_downloadButton) {
        // We shouldn't really interfere with its operation
        return;
    }

    if (m_fileDownloadManager->data(Imap::Mailbox::RoleMessageUid) == 0) {
        return;
    }

    QByteArray buf;
    QDataStream stream(&buf, QIODevice::WriteOnly);
    stream << m_fileDownloadManager->data(Imap::Mailbox::RoleMailboxName).toString() <<
              m_fileDownloadManager->data(Imap::Mailbox::RoleMailboxUidValidity).toUInt() <<
              m_fileDownloadManager->data(Imap::Mailbox::RoleMessageUid).toUInt() <<
              m_fileDownloadManager->data(Imap::Mailbox::RolePartId).toString() <<
              m_fileDownloadManager->data(Imap::Mailbox::RolePartPathToPart).toString();

    QMimeData *mimeData = new QMimeData;
    mimeData->setData(QLatin1String("application/x-trojita-imap-part"), buf);
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setHotSpot(event->pos());
    drag->exec(Qt::CopyAction, Qt::CopyAction);
}


}

