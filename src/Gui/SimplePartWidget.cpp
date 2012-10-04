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
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkReply>
#include <QWebFrame>

#include "SimplePartWidget.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Network/FileDownloadManager.h"

namespace Gui
{

SimplePartWidget::SimplePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex):
    EmbeddedWebView(parent, manager)
{
    Q_ASSERT(partIndex.isValid());
    QUrl url;
    url.setScheme(QLatin1String("trojita-imap"));
    url.setHost(QLatin1String("msg"));
    url.setPath(partIndex.data(Imap::Mailbox::RolePartPathToPart).toString());
    if (partIndex.data(Imap::Mailbox::RolePartMimeType).toString() == "text/plain")
        connect(this, SIGNAL(loadFinished(bool)), this, SLOT(slotMarkupPlainText()));
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

    static const QRegExp link("(https*://[;/?:@=&$\\-_.+!'(),0-9a-zA-Z%#]*)");
    static const QRegExp mail("([a-zA-Z0-9\\.\\-_]*@[a-zA-Z0-9\\.\\-_]*)");
    static QString intro("([\\s\\(\\[\\{])");
    static QString extro("([\\s\\),;.\\]\\}])");
    static const QRegExp bold(intro + "\\*(\\S*)\\*" + extro);
    static const QRegExp italic(intro + "/(\\S*)/" + extro);
    static const QRegExp underline(intro + "_(\\S*)_" + extro);
    QString content = page()->mainFrame()->toHtml();
    content.replace("&gt;", "§gt;");
    content.replace("&lt;", "§lt;");
    content.replace(link, "<a href=\"\\1\">\\1</a>");
    content.replace(mail, "<a href=\"mailto:\\1\">\\1</a>");
    content.replace("§gt;", "&gt;");
    content.replace("§lt;", "&lt;");
    content.replace(bold, "\\1<b>\\2</b>\\3");
    content.replace(italic, "\\1<i>\\2</i>\\3");
    content.replace(underline, "\\1<u>\\2</u>\\3");
    page()->mainFrame()->setHtml(content);
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

