/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkReply>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#  include <QStandardPaths>
#endif
#include <QWebFrame>

#include "SimplePartWidget.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Network/FileDownloadManager.h"

namespace Gui
{

SimplePartWidget::SimplePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex):
    EmbeddedWebView(parent, manager), flowedFormat(Composer::Util::FORMAT_PLAIN)
{
    Q_ASSERT(partIndex.isValid());
    QUrl url;
    url.setScheme(QLatin1String("trojita-imap"));
    url.setHost(QLatin1String("msg"));
    url.setPath(partIndex.data(Imap::Mailbox::RolePartPathToPart).toString());
    if (partIndex.data(Imap::Mailbox::RolePartMimeType).toString() == "text/plain")
        connect(this, SIGNAL(loadFinished(bool)), this, SLOT(slotMarkupPlainText()));
    if (partIndex.data(Imap::Mailbox::RolePartContentFormat).toString().toLower() == QLatin1String("flowed"))
        flowedFormat = Composer::Util::FORMAT_FLOWED;
    load(url);

    fileDownloadManager = new Imap::Network::FileDownloadManager(this, manager, partIndex);
    connect(fileDownloadManager, SIGNAL(fileNameRequested(QString *)), this, SLOT(slotFileNameRequested(QString *)));

    saveAction = new QAction(tr("Save..."), this);
    connect(saveAction, SIGNAL(triggered()), fileDownloadManager, SLOT(slotDownloadNow()));
    this->addAction(saveAction);

    setContextMenuPolicy(Qt::ActionsContextMenu);
}

void SimplePartWidget::slotMarkupPlainText() {
    // NOTICE "single shot", we get a recursion otherwise!
    disconnect(this, SIGNAL(loadFinished(bool)), this, SLOT(slotMarkupPlainText()));

    static const QString defaultStyle(
        "pre{word-wrap: break-word; white-space: pre-wrap;}"
        ".quotemarks{color:transparent;font-size:0px;}"
        "blockquote{font-size:90%; margin: 4pt 0 4pt 0; padding: 0 0 0 1em; border-left: 2px solid blue;}"
        // Stop the font size from getting smaller after reaching two levels of quotes
        // (ie. starting on the third level, don't make the size any smaller than what it already is)
        "blockquote blockquote blockquote {font-size: 100%}"
        ".signature{opacity: 0.6;}"
    );

    // build stylesheet and html header
    static QString stylesheet = defaultStyle;
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    static QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1String("message.css"));
#else
    static QFile file(QDesktopServices::storageLocation(QDesktopServices::DataLocation) + QLatin1String("/message.css"));
#endif
    static QDateTime lastVersion;
    QDateTime lastTouched(file.exists() ? QFileInfo(file).lastModified() : QDateTime());
    if (lastVersion < lastTouched) {
        stylesheet = defaultStyle;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString userSheet = QString::fromLocal8Bit(file.readAll().data());
            lastVersion = lastTouched;
            stylesheet += "\n" + userSheet;
            file.close();
        }
    }
    QString htmlHeader("<html><head><style type=\"text/css\"><!--" + stylesheet + "--></style></head><body><pre>");
    static QString htmlFooter("\n</pre></body></html>");

    QStringList markup = Composer::Util::plainTextToHtml(page()->mainFrame()->toPlainText(), flowedFormat);

    // and finally set the marked up page.
    page()->mainFrame()->setHtml(htmlHeader + markup.join("\n") + htmlFooter);
}

void SimplePartWidget::slotFileNameRequested(QString *fileName)
{
    *fileName = QFileDialog::getSaveFileName(this, tr("Save Attachment"),
                *fileName, QString(),
                0, QFileDialog::HideNameFilterDetails
                                            );
}

void SimplePartWidget::slotTransferError(const QString &errorString)
{
    QMessageBox::critical(this, tr("Can't save attachment"),
                          tr("Unable to save the attachment. Error:\n%1").arg(errorString));
}

QString SimplePartWidget::quoteMe() const
{
    QString selection = selectedText();
    if (selection.isEmpty())
        return page()->mainFrame()->toPlainText();
    else
        return selection;
}

void SimplePartWidget::reloadContents()
{
    EmbeddedWebView::reload();
}

}

