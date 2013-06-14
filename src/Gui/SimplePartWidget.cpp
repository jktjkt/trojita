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
#include <QApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkReply>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#  include <QStandardPaths>
#endif
#include <QWebFrame>

#include "SimplePartWidget.h"
#include "Gui/MessageView.h" // so that the compiler knows that it's a QObject
#include "Gui/Util.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"
#include "Imap/Network/FileDownloadManager.h"

namespace Gui
{

SimplePartWidget::SimplePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager,
                                   const QModelIndex &partIndex, MessageView *messageView):
    EmbeddedWebView(parent, manager), m_partIndex(partIndex), m_netAccessManager(manager),
    flowedFormat(Composer::Util::FORMAT_PLAIN)
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

    m_savePart = new QAction(tr("Save this message part..."), this);
    connect(m_savePart, SIGNAL(triggered()), this, SLOT(slotDownloadPart()));
    this->addAction(m_savePart);

    m_saveMessage = new QAction(tr("Save whole message..."), this);
    connect(m_saveMessage, SIGNAL(triggered()), this, SLOT(slotDownloadMessage()));
    this->addAction(m_saveMessage);

    m_findAction = new QAction(tr("Search..."), this);
    m_findAction->setShortcut(tr("Ctrl+F"));
    connect(m_findAction, SIGNAL(triggered()), this, SIGNAL(searchDialogRequested()));
    addAction(m_findAction);

    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(customContextMenuRequested(QPoint)), messageView, SLOT(partContextMenuRequested(QPoint)));
    connect(this, SIGNAL(searchDialogRequested()), messageView, SLOT(triggerSearchDialog()));
    // The targets expect the sender() of the signal to be a SimplePartWidget, not a QWebPage,
    // which means we have to do this indirection
    connect(page(), SIGNAL(linkHovered(QString,QString,QString)), this, SIGNAL(linkHovered(QString,QString,QString)));
    connect(this, SIGNAL(linkHovered(QString,QString,QString)),
            messageView, SLOT(partLinkHovered(QString,QString,QString)));

    installEventFilter(messageView);
}

void SimplePartWidget::slotMarkupPlainText() {
    // NOTICE "single shot", we get a recursion otherwise!
    disconnect(this, SIGNAL(loadFinished(bool)), this, SLOT(slotMarkupPlainText()));

    // If there's no data, don't try to "fix it up"
    if (!m_partIndex.isValid() || !m_partIndex.data(Imap::Mailbox::RoleIsFetched).toBool())
        return;

    static const QString defaultStyle = QString::fromUtf8(
        "pre{word-wrap: break-word; white-space: pre-wrap;}"
        // The following line, sadly, produces a warning "QFont::setPixelSize: Pixel size <= 0 (0)".
        // However, if it is not in place or if the font size is set higher, even to 0.1px, WebKit reserves space for the
        // quotation characters and therefore a weird white area appears. Even width: 0px doesn't help, so it looks like
        // we will have to live with this warning for the time being.
        ".quotemarks{color:transparent;font-size:0px;}"
        "blockquote{font-size:90%; margin: 4pt 0 4pt 0; padding: 0 0 0 1em; border-left: 2px solid %1;}"
        // Stop the font size from getting smaller after reaching two levels of quotes
        // (ie. starting on the third level, don't make the size any smaller than what it already is)
        "blockquote blockquote blockquote {font-size: 100%}"
        ".signature{opacity: 0.6;}"
        // Dynamic quote collapsing via pure CSS, yay
        "input {display: none}"
        "input ~ span.full {display: block}"
        "input ~ span.short {display: none}"
        "input:checked ~ span.full {display: none}"
        "input:checked ~ span.short {display: block}"
        "label {border: 1px solid %2; border-radius: 5px; padding: 0px 4px 0px 4px; white-space: nowrap}"
        // BLACK UP-POINTING SMALL TRIANGLE (U+25B4)
        // BLACK DOWN-POINTING SMALL TRIANGLE (U+25BE)
        "span.full > blockquote > label:before {content: \"\u25b4\"}"
        "span.short > blockquote > label:after {content: \" \u25be\"}"
        "span.shortquote > blockquote > label {display: none}"
    );

    QFontInfo monospaceInfo(Gui::Util::systemMonospaceFont());
    QString fontSpecification(QLatin1String("pre{"));
    if (monospaceInfo.italic())
        fontSpecification += QLatin1String("font-style: italic; ");
    if (monospaceInfo.bold())
        fontSpecification += QLatin1String("font-weight: bold; ");
    fontSpecification += QString::fromUtf8("font-size: %1px; font-family: \"%2\", monospace }").arg(
                QString::number(monospaceInfo.pixelSize()), monospaceInfo.family());

    QPalette palette = QApplication::palette();
    QString textColors = QString::fromUtf8("body { background-color: %1; color: %2 }"
                                           "a:link { color: %3 } a:visited { color: %4 } a:hover { color: %3 }").arg(
                palette.base().color().name(), palette.text().color().name(),
                palette.link().color().name(), palette.linkVisited().color().name());
    // looks like there's no special color for hovered links in Qt

    // build stylesheet and html header
    QColor tintForQuoteIndicator = palette.base().color();
    tintForQuoteIndicator.setAlpha(0x66);
    static QString stylesheet = defaultStyle.arg(palette.link().color().name(),
                                                 Gui::Util::tintColor(palette.text().color(), tintForQuoteIndicator).name());
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
    QString htmlHeader("<html><head><style type=\"text/css\"><!--" + textColors + fontSpecification + stylesheet + "--></style></head><body><pre>");
    static QString htmlFooter("\n</pre></body></html>");

    // We cannot rely on the QWebFrame's toPlainText because of https://bugs.kde.org/show_bug.cgi?id=321160
    QString markup = Composer::Util::plainTextToHtml(
                Imap::decodeByteArray(m_partIndex.data(Imap::Mailbox::RolePartData).toByteArray(),
                                      m_partIndex.data(Imap::Mailbox::RolePartCharset).toString()),
                flowedFormat);

    // and finally set the marked up page.
    page()->mainFrame()->setHtml(htmlHeader + markup + htmlFooter);
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

QList<QAction *> SimplePartWidget::contextMenuSpecificActions() const
{
    return QList<QAction*>() << m_savePart << m_saveMessage << m_findAction;
}

void SimplePartWidget::slotDownloadPart()
{
    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, m_partIndex);
    connect(manager, SIGNAL(fileNameRequested(QString *)), this, SLOT(slotFileNameRequested(QString *)));
    connect(manager, SIGNAL(transferError(QString)), this, SLOT(slotTransferError(QString)));
    connect(manager, SIGNAL(transferError(QString)), manager, SLOT(deleteLater()));
    connect(manager, SIGNAL(succeeded()), manager, SLOT(deleteLater()));
    manager->downloadPart();
}

void SimplePartWidget::slotDownloadMessage()
{
    QModelIndex index;
    if (m_partIndex.isValid()) {
        const Imap::Mailbox::Model *model = 0;
        Imap::Mailbox::TreeItem *item = Imap::Mailbox::Model::realTreeItem(m_partIndex, &model);
        Q_ASSERT(model);
        Q_ASSERT(item);
        Imap::Mailbox::TreeItemMessage *messagePtr = dynamic_cast<Imap::Mailbox::TreeItemPart*>(item)->message();
        Q_ASSERT(messagePtr);
        index = messagePtr->toIndex(const_cast<Imap::Mailbox::Model*>(model));
    }

    Imap::Network::FileDownloadManager *manager = new Imap::Network::FileDownloadManager(this, m_netAccessManager, index);
    connect(manager, SIGNAL(fileNameRequested(QString *)), this, SLOT(slotFileNameRequested(QString *)));
    connect(manager, SIGNAL(transferError(QString)), this, SLOT(slotTransferError(QString)));
    connect(manager, SIGNAL(transferError(QString)), manager, SLOT(deleteLater()));
    connect(manager, SIGNAL(succeeded()), manager, SLOT(deleteLater()));
    manager->downloadMessage();
}

}

