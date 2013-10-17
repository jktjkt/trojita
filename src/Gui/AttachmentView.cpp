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
#include "AttachmentView.h"
#include "IconLoader.h"
#include "MessageView.h" // so that the compiler knows it's a QObject
#include "Common/DeleteAfter.h"
#include "Common/Paths.h"
#include "Imap/Network/FileDownloadManager.h"
#include "Imap/Model/DragAndDrop.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Utils.h"

#include <QAction>
#include <QDesktopServices>
#include <QDrag>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QLabel>
#include <QTemporaryFile>
#include <QToolButton>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#  include <QMimeDatabase>
#else
#  include "mimetypes-qt4/include/QMimeDatabase"
#endif

namespace Gui
{

AttachmentView::AttachmentView(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager,
                               const QModelIndex &partIndex, MessageView *messageView, QWidget *contentWidget):
    QFrame(parent), m_partIndex(partIndex), m_downloadButton(0), m_messageView(messageView), m_downloadAttachment(0),
    m_openAttachment(0), m_showHideAttachment(0), m_netAccess(manager), m_tmpFile(0),
    m_contentWidget(contentWidget)
{
    QVBoxLayout *contentLayout = new QVBoxLayout(this);
    QWidget *attachmentControls = new QWidget();
    contentLayout->addWidget(attachmentControls);
    if (m_contentWidget) {
        contentLayout->addWidget(m_contentWidget);
    }

    QHBoxLayout *layout = new QHBoxLayout(attachmentControls);

    // Icon on the left
    QLabel *lbl = new QLabel();
    static const QSize iconSize(22, 22);

    QString mimeDescription = partIndex.data(Imap::Mailbox::RolePartMimeType).toString();
    QString rawMime = mimeDescription;
    QMimeType mimeType = QMimeDatabase().mimeTypeForName(mimeDescription);
    if (mimeType.isValid() && !mimeType.isDefault()) {
        mimeDescription = mimeType.comment();
        QIcon icon = QIcon::fromTheme(mimeType.iconName(),
                                      QIcon::fromTheme(mimeType.genericIconName(), loadIcon(QLatin1String("mail-attachment")))
                                      );
        lbl->setPixmap(icon.pixmap(iconSize));
    } else {
        lbl->setPixmap(loadIcon(QLatin1String("mail-attachment")).pixmap(iconSize));
    }
    layout->addWidget(lbl);

    QWidget *labelArea = new QWidget();
    QVBoxLayout *subLayout = new QVBoxLayout(labelArea);
    // The file name shall be mouse-selectable
    lbl = new QLabel();
    lbl->setTextFormat(Qt::PlainText);
    lbl->setText(partIndex.data(Imap::Mailbox::RolePartFileName).toString());
    lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    subLayout->addWidget(lbl);
    // Some metainformation -- the MIME type and the file size
    lbl = new QLabel(tr("%2, %3").arg(mimeDescription,
                                      Imap::Mailbox::PrettySize::prettySize(partIndex.data(Imap::Mailbox::RolePartOctets).toUInt(),
                                                                            Imap::Mailbox::PrettySize::WITH_BYTES_SUFFIX)));
    if (rawMime != mimeDescription) {
        lbl->setToolTip(rawMime);
    }
    QFont f = lbl->font();
    f.setItalic(true);
    f.setPointSizeF(f.pointSizeF() * 0.8);
    lbl->setFont(f);
    subLayout->addWidget(lbl);
    layout->addWidget(labelArea);
    layout->addStretch();

    // Download/Open buttons
    m_downloadButton = new QToolButton();
    m_downloadButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_downloadButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_downloadButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QMenu *menu = new QMenu(this);
    m_downloadAttachment = menu->addAction(loadIcon(QLatin1String("document-save-as")), tr("Download"));
    m_openAttachment = menu->addAction(tr("Open Directly"));
    connect(m_downloadAttachment, SIGNAL(triggered()), this, SLOT(slotDownloadAttachment()));
    connect(m_openAttachment, SIGNAL(triggered()), this, SLOT(slotOpenAttachment()));
    if (m_contentWidget) {
        m_showHideAttachment = menu->addAction(loadIcon(QLatin1String("view-preview")), tr("Show Preview"));
        m_showHideAttachment->setCheckable(true);
        m_showHideAttachment->setChecked(!m_contentWidget->isHidden());
        connect(m_showHideAttachment, SIGNAL(triggered(bool)), m_contentWidget, SLOT(setVisible(bool)));
    }

    m_downloadButton->setMenu(menu);
    m_downloadButton->setDefaultAction(m_downloadAttachment);

    layout->addWidget(m_downloadButton);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
}

void AttachmentView::slotDownloadAttachment()
{
    m_downloadButton->setEnabled(false);

    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccess, m_partIndex);
    connect(manager, SIGNAL(fileNameRequested(QString*)), this, SLOT(slotFileNameRequested(QString*)));
    connect(manager, SIGNAL(transferError(QString)), m_messageView, SIGNAL(transferError(QString)));
    connect(manager, SIGNAL(transferError(QString)), this, SLOT(enableDownloadAgain()));
    connect(manager, SIGNAL(transferError(QString)), manager, SLOT(deleteLater()));
    connect(manager, SIGNAL(cancelled()), this, SLOT(enableDownloadAgain()));
    connect(manager, SIGNAL(cancelled()), manager, SLOT(deleteLater()));
    connect(manager, SIGNAL(succeeded()), this, SLOT(enableDownloadAgain()));
    connect(manager, SIGNAL(succeeded()), manager, SLOT(deleteLater()));
    manager->downloadPart();
}

void AttachmentView::slotOpenAttachment()
{
    m_openAttachment->setEnabled(false);

    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccess, m_partIndex);
    connect(manager, SIGNAL(fileNameRequested(QString*)), this, SLOT(slotFileNameRequestedOnOpen(QString*)));
    connect(manager, SIGNAL(transferError(QString)), m_messageView, SIGNAL(transferError(QString)));
    connect(manager, SIGNAL(transferError(QString)), this, SLOT(onOpenFailed()));
    connect(manager, SIGNAL(transferError(QString)), manager, SLOT(deleteLater()));
    // we aren't connecting to cancelled() as it cannot really happen -- the filename is never empty
    connect(manager, SIGNAL(succeeded()), this, SLOT(openDownloadedAttachment()));
    connect(manager, SIGNAL(succeeded()), manager, SLOT(deleteLater()));
    manager->downloadPart();
}

void AttachmentView::slotFileNameRequestedOnOpen(QString *fileName)
{
    Q_ASSERT(!m_tmpFile);
    m_tmpFile = new QTemporaryFile(QDir::tempPath() + QLatin1String("/trojita-attachment-XXXXXX-") +
                                   fileName->replace(QLatin1Char('/'), QLatin1Char('_')));
    m_tmpFile->open();
    *fileName = m_tmpFile->fileName();
}

void AttachmentView::slotFileNameRequested(QString *fileName)
{
    QString fileLocation = QDir(Common::writablePath(Common::LOCATION_DOWNLOAD)).filePath(*fileName);
    *fileName = QFileDialog::getSaveFileName(this, tr("Save Attachment"), fileLocation, QString(), 0, QFileDialog::HideNameFilterDetails);
}

void AttachmentView::enableDownloadAgain()
{
    m_downloadButton->setEnabled(true);
}

void AttachmentView::onOpenFailed()
{
    delete m_tmpFile;
    m_tmpFile = 0;
    m_openAttachment->setEnabled(true);
}

void AttachmentView::openDownloadedAttachment()
{
    Q_ASSERT(m_tmpFile);

    // Make sure that the file is read-only so that the launched application does not attempt to modify it
    m_tmpFile->setPermissions(QFile::ReadOwner);

    QDesktopServices::openUrl(QUrl::fromLocalFile(m_tmpFile->fileName()));

    // This will delete the temporary file in ten seconds. It should give the application plenty of time to start and also prevent
    // leaving cruft behind.
    new Common::DeleteAfter(m_tmpFile, 10000);
    m_tmpFile = 0;
    m_openAttachment->setEnabled(true);
}

void AttachmentView::mousePressEvent(QMouseEvent *event)
{
    QWidget *child = childAt(event->pos());
    if (child == m_downloadButton) {
        // We shouldn't really interfere with its operation
        return;
    }

    QMimeData *mimeData = Imap::Mailbox::mimeDataForDragAndDrop(m_partIndex);
    if (!mimeData)
        return;
    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setHotSpot(event->pos());
    drag->exec(Qt::CopyAction, Qt::CopyAction);
}

QString AttachmentView::quoteMe() const
{
    const AbstractPartWidget *widget = dynamic_cast<const AbstractPartWidget *>(m_contentWidget);
    return widget && !m_contentWidget->isHidden() ? widget->quoteMe() : QString();
}

void AttachmentView::reloadContents()
{
    if (AbstractPartWidget *w = dynamic_cast<AbstractPartWidget*>(m_contentWidget))
        w->reloadContents();
}

}

