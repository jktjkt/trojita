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
    static QString intro("(^|[\\s\\(\\[\\{])");
    static QString extro("([\\s\\),;.\\]\\}])");
    static const QRegExp bold(intro + "\\*(\\S*)\\*" + extro);
    static const QRegExp italic(intro + "/(\\S*)/" + extro);
    static const QRegExp underline(intro + "_(\\S*)_" + extro);
    // TODO: include some extern css here?
    QString stylesheet(
        "pre{word-wrap: break-word; white-space: pre-wrap;}"
        ".markup{color:transparent;font-size:0px;}"
        ".quotemarks{color:transparent;font-size:0px;}"
        "blockquote{font-size:90%;padding:0;margin:4pt;margin-left:2em;}"
    );
    static QString htmlHeader("<html><head><style type=\"text/css\"><!--" + stylesheet + "--></style></head><body><pre>");
    static QString htmlFooter("\n</pre></body></html>");

    // Processing:
    // the plain text is split into lines
    // leading quotemarks are counted and stripped
    // next, the line is marked up (*bold*, /italic/, _underline_ and active link support)
    // if the last line ended with a space, the result is appended, otherwise canonical quotemarkes are
    // prepended and the line appended to the markup list (see http://tools.ietf.org/html/rfc3676)
    // whenever the quote level grows, a <blockquote> is opened and closed when it shrinks

    int quoteLevel = 0;
    QStringList plain(page()->mainFrame()->toPlainText().split('\n'));
    QStringList markup(htmlHeader);
    for (int i = 0; i < plain.count(); ++i) {
        QString *line = const_cast<QString*>(&plain.at(i));

        // ignore empty lines
        if (line->isEmpty()) {
            markup << *line;
            continue;
        }
        // determine quotelevel
        int cQuoteLevel = 0;
        if (line->at(0) == '>') {
            int j = 1;
            cQuoteLevel = 1;
            while (j < line->length() && (line->at(j) == '>' || line->at(j) == ' '))
                cQuoteLevel += line->at(j++) == '>';
        }
        // strip quotemarks
        if (cQuoteLevel) {
            static QRegExp quotemarks("^[>\\s]*");
            line->remove(quotemarks);
        }
        // markup *bold*, /italic/, _underline_ and active links
        line->replace(">", "§gt;"); // we cannot escape them after we added actual tags
        line->replace("<", "§lt;"); // and we cannot use the amps "&" since we'll have to escape it to &amp; later on as well
        line->replace(link, "<a href=\"\\1\">\\1</a>");
        line->replace(mail, "<a href=\"mailto:\\1\">\\1</a>");
#define MARKUP(_item_) "<span class=\"markup\">"#_item_"</span>"
        if (line->contains("italic"))
            qDebug() << *line;
        line->replace(bold, "\\1<b>" MARKUP(*) "\\2" MARKUP(*) "</b>\\3");
        line->replace(italic, "\\1<i>" MARKUP(/) "\\2" MARKUP(/) "</i>\\3");
        line->replace(underline, "\\1<u>" MARKUP(_) "\\2" MARKUP(_) "</u>\\3");
#undef MARKUP
        line->replace("&", "&amp;");
        line->replace("§gt;", "&gt;");
        line->replace("§lt;", "&lt;");

        // if this is a non floating new line, prepend canonical quotemarks
        if (cQuoteLevel && !(cQuoteLevel == quoteLevel && markup.last().endsWith(' '))) {
            QString quotemarks("<span class=\"quotemarks\">");
            for (int i = 0; i < cQuoteLevel; ++i)
                quotemarks += "&gt;";
            quotemarks += " </span>";
            line->prepend(quotemarks);
        }

        // handle quotelevel depending blockquotes
        cQuoteLevel -= quoteLevel;
        quoteLevel += cQuoteLevel; // quoteLevel is now what cQuoteLevel was two lines before
        while (cQuoteLevel > 0) {
            line->prepend("<blockquote>");
            --cQuoteLevel;
        }
        while (cQuoteLevel < 0) {
            line->prepend("</blockquote>");
            ++cQuoteLevel;
        }
        // appaned or join the line
        if (markup.last().endsWith(' '))
            markup.last().append(*line);
        else
            markup << *line;
    }
    // close open blockquotes
    // (bottom quoters, we're unfortunately -yet- not permittet to shoot them, so we need to deal with them ;-)
    QString quoteCloser("\n");
    while (quoteLevel > 0) {
        quoteCloser.append("</blockquote>");
        --quoteLevel;
    }
    // and finally set the marked up page.
    page()->mainFrame()->setHtml(markup.join("\n") + quoteCloser + htmlFooter);
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

