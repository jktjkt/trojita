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
#include <QDebug>
#include <QDesktopServices>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebFrame>
#include <QWebHistory>
#include <QWebHitTestResult>
#include <QWebPage>

#include "MessageView.h"
#include "AbstractPartWidget.h"
#include "ComposeWidget.h"
#include "EmbeddedWebView.h"
#include "EnvelopeView.h"
#include "ExternalElementsWidget.h"
#include "OverlayWidget.h"
#include "PartWidgetFactory.h"
#include "SimplePartWidget.h"
#include "Spinner.h"
#include "TagListWidget.h"
#include "UserAgentWebPage.h"
#include "Window.h"
#include "Common/InvokeMethod.h"
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Composer/SubjectMangling.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Gui
{

MessageView::MessageView(QWidget *parent, QSettings *settings): QWidget(parent), m_settings(settings)
{
    QPalette pal = palette();
    pal.setColor(backgroundRole(), palette().color(QPalette::Active, QPalette::Base));
    pal.setColor(foregroundRole(), palette().color(QPalette::Active, QPalette::Text));
    setPalette(pal);
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus); // not by the wheel
    netAccess = new Imap::Network::MsgPartNetAccessManager(this);
    connect(netAccess, SIGNAL(requestingExternal(QUrl)), this, SLOT(externalsRequested(QUrl)));
    factory = new PartWidgetFactory(netAccess, this);

    emptyView = new EmbeddedWebView(this, new QNetworkAccessManager(this));
    emptyView->setFixedSize(450,300);
    CALL_LATER_NOARG(emptyView, handlePageLoadFinished);
    emptyView->setPage(new UserAgentWebPage(emptyView));
    emptyView->installEventFilter(this);
    emptyView->setAutoFillBackground(false);

    viewer = emptyView;

    //BEGIN create header section

    headerSection = new QWidget(this);

    // we create a dummy header, pass it through the style and the use it's color roles so we
    // know what headers in general look like in the system
    QHeaderView helpingHeader(Qt::Horizontal);
    helpingHeader.ensurePolished();
    pal = headerSection->palette();
    pal.setColor(headerSection->backgroundRole(), palette().color(QPalette::Active, helpingHeader.backgroundRole()));
    pal.setColor(headerSection->foregroundRole(), palette().color(QPalette::Active, helpingHeader.foregroundRole()));
    headerSection->setPalette(pal);
    headerSection->setAutoFillBackground(true);

    // the actual mail header
    m_envelope = new EnvelopeView(headerSection, this);

    // the tag bar
    tags = new TagListWidget(headerSection);
    tags->setBackgroundRole(helpingHeader.backgroundRole());
    tags->setForegroundRole(helpingHeader.foregroundRole());
    tags->hide();
    connect(tags, SIGNAL(tagAdded(QString)), this, SLOT(newLabelAction(QString)));
    connect(tags, SIGNAL(tagRemoved(QString)), this, SLOT(deleteLabelAction(QString)));

    // whether we allow to load external elements
    externalElements = new ExternalElementsWidget(this);
    externalElements->hide();
    connect(externalElements, SIGNAL(loadingEnabled()), this, SLOT(externalsEnabled()));

    // layout the header
    layout = new QVBoxLayout(headerSection);
    layout->addWidget(m_envelope, 1);
    layout->addWidget(tags, 3);
    layout->addWidget(externalElements, 1);

    //END create header section

    //BEGIN layout the message

    layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(headerSection, 1);

    headerSection->hide();

    // put the actual messages into an extra horizontal view
    // this allows us easy usage of the trailing stretch and also to indent the message a bit
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setContentsMargins(6,6,6,0);
    hLayout->addWidget(viewer);
    static_cast<QVBoxLayout*>(layout)->addLayout(hLayout, 1);
    // add a strong stretch to squeeze header and message to the top
    // possibly passing a large stretch factor to the message could be enough...
    layout->addStretch(1000);

    //END layout the message

    // make the layout used to add messages our new horizontal layout
    layout = hLayout;

    markAsReadTimer = new QTimer(this);
    markAsReadTimer->setSingleShot(true);
    connect(markAsReadTimer, SIGNAL(timeout()), this, SLOT(markAsRead()));

    m_loadingSpinner = new Spinner(this);
    m_loadingSpinner->setText(tr("Fetching\nMessage"));
    m_loadingSpinner->setType(Spinner::Sun);
}

MessageView::~MessageView()
{
    // Redmine #496 -- the default order of destruction starts with our QNAM subclass which in turn takes care of all pending
    // QNetworkReply instances created by that manager. When the destruction goes to the WebKit objects, they try to disconnect
    // from the network replies which are however gone already. We can mitigate that by simply making sure that the destruction
    // starts with the QWebView subclasses and only after that proceeds to the QNAM. Qt's default order leads to segfaults here.
    if (viewer != emptyView) {
        delete viewer;
    }
    delete emptyView;

    delete factory;
}

void MessageView::setEmpty()
{
    markAsReadTimer->stop();
    m_envelope->setMessage(QModelIndex());
    headerSection->hide();
    message = QModelIndex();
    disconnect(this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
    tags->hide();
    if (viewer != emptyView) {
        layout->removeWidget(viewer);
        viewer->deleteLater();
        viewer = emptyView;
        viewer->show();
        layout->addWidget(viewer);
        emit messageChanged();
        m_loadingItems.clear();
        m_loadingSpinner->stop();
    }
}

void MessageView::setMessage(const QModelIndex &index)
{
    // first, let's get a real model
    QModelIndex messageIndex;
    const Imap::Mailbox::Model *constModel = 0;
    Imap::Mailbox::TreeItem *item = Imap::Mailbox::Model::realTreeItem(index, &constModel, &messageIndex);
    Q_ASSERT(item); // Make sure it's a message
    Q_ASSERT(messageIndex.isValid());
    Imap::Mailbox::Model *realModel = const_cast<Imap::Mailbox::Model *>(constModel);
    Q_ASSERT(realModel);

    // The data might be available from the local cache, so let's try to save a possible roundtrip here
    item->fetch(realModel);

    if (!messageIndex.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        // This happens when the message placeholder is already available in the GUI, but the actual message data haven't been
        // loaded yet. This is especially common with the threading model.
        // Note that the data might be already available in the cache, it's just that it isn't in the mailbox tree yet.
        setEmpty();
        connect(realModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
        message = messageIndex;
        return;
    }

    QModelIndex rootPartIndex = messageIndex.child(0, 0);

    headerSection->show();
    if (message != messageIndex) {
        emptyView->hide();
        layout->removeWidget(viewer);
        if (viewer != emptyView) {
            viewer->setParent(0);
            viewer->deleteLater();
        }
        message = messageIndex;
        netAccess->setExternalsEnabled(false);
        externalElements->hide();

        netAccess->setModelMessage(message);

        m_loadingItems.clear();
        m_loadingSpinner->stop();

        PartWidgetFactory::PartLoadingOptions loadingMode;
        if (m_settings->value(Common::SettingsNames::guiPreferPlaintextRendering, QVariant(true)).toBool())
            loadingMode |= PartWidgetFactory::PART_PREFER_PLAINTEXT_OVER_HTML;
        viewer = factory->create(rootPartIndex, 0, loadingMode);
        viewer->setParent(this);
        layout->addWidget(viewer);
        viewer->show();
        m_envelope->setMessage(message);

        tags->show();
        tags->setTagList(messageIndex.data(Imap::Mailbox::RoleMessageFlags).toStringList());
        disconnect(this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
        connect(realModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));

        emit messageChanged();

        // We want to propagate the QWheelEvent to upper layers
        viewer->installEventFilter(this);
    }

    if (realModel->isNetworkAvailable())
        markAsReadTimer->start(200); // FIXME: make this configurable
}

void MessageView::markAsRead()
{
    if (!message.isValid())
        return;
    Imap::Mailbox::Model *model = const_cast<Imap::Mailbox::Model *>(dynamic_cast<const Imap::Mailbox::Model *>(message.model()));
    Q_ASSERT(model);
    if (!model->isNetworkAvailable())
        return;
    if (!message.data(Imap::Mailbox::RoleMessageIsMarkedRead).toBool())
        model->markMessagesRead(QModelIndexList() << message, Imap::Mailbox::FLAG_ADD);
}

/** @short Inhibit the automatic marking of the current message as already read

The user might have e.g. explicitly marked a previously read message as unread again immediately after navigating back to it
in the message listing. In that situation, the message viewer shall respect this decision and inhibit the helper which would
otherwise mark the current message as read after a short timeout.
*/
void MessageView::stopAutoMarkAsRead()
{
    markAsReadTimer->stop();
}

bool MessageView::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        // while the containing scrollview has Qt::StrongFocus, the event forwarding breaks that
        // -> completely disable focus for the following wheel event ...
        parentWidget()->setFocusPolicy(Qt::NoFocus);
        MessageView::event(event);
        // ... set reset it
        parentWidget()->setFocusPolicy(Qt::StrongFocus);
        return true;
    } else if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            MessageView::event(event);
            return true;
        case Qt::Key_Home:
        case Qt::Key_End:
            return false;
        default:
            return QObject::eventFilter(object, event);
        }
    } else {
        return QObject::eventFilter(object, event);
    }
}

QString MessageView::quoteText() const
{
    if (const AbstractPartWidget *w = dynamic_cast<const AbstractPartWidget *>(viewer)) {
        QStringList quote;
        QStringList lines = w->quoteMe().split('\n');
        for (QStringList::iterator line = lines.begin(); line != lines.end(); ++line) {
            if (Composer::Util::signatureSeparator().exactMatch(*line)) {
                // This is the signature separator, we should not include anything below that in the quote
                break;
            }
            // rewrap - we need to keep the quotes at < 79 chars, yet the grow with every level
            if (line->length() < 79-2) {
                // this line is short enough, prepend quote mark and continue
                if (line->isEmpty() || line->at(0) == '>')
                    line->prepend(">");
                else
                    line->prepend("> ");
                quote << *line;
                continue;
            }
            // long line -> needs to be wrapped
            // 1st, detect the quote depth and eventually stript the quotes from the line
            int quoteLevel = 0;
            int contentStart = 0;
            if (line->at(0) == '>') {
                quoteLevel = 1;
                while (quoteLevel < line->length() && line->at(quoteLevel) == '>')
                    ++quoteLevel;
                contentStart = quoteLevel;
                if (quoteLevel < line->length() && line->at(quoteLevel) == ' ')
                    ++contentStart;
            }

            // 2nd, build a qute string
            QString quotemarks;
            for (int i = 0; i < quoteLevel; ++i)
                quotemarks += ">";
            quotemarks += "> ";

            // 3rd, wrap the line, prepend the quotemarks to each line and add it to the quote text
            int space(contentStart), lastSpace(contentStart), pos(contentStart), length(0);
            while (pos < line->length()) {
                if (line->at(pos) == ' ')
                    space = pos+1;
                ++length;
                if (length > 65-quotemarks.length() && space != lastSpace) {
                    // wrap
                    quote << quotemarks + line->mid(lastSpace, space - lastSpace);
                    lastSpace = space;
                    length = pos - space;
                }
                ++pos;
            }
            quote << quotemarks + line->mid(lastSpace);
        }
        const Imap::Message::Envelope &e = message.data(Imap::Mailbox::RoleMessageEnvelope).value<Imap::Message::Envelope>();
        QString sender;
        if (!e.from.isEmpty())
            sender = e.from[0].prettyName(Imap::Message::MailAddress::FORMAT_JUST_NAME);
        if (e.from.isEmpty())
            sender = tr("you");

        // One extra newline at the end of the quoted text to separate the response
        quote << QString();

        return tr("On %1, %2 wrote:\n").arg(e.date.toLocalTime().toString(Qt::SystemLocaleLongDate)).arg(sender) + quote.join("\n");
    }
    return QString();
}

void MessageView::reply(MainWindow *mainWindow, Composer::ReplyMode mode)
{
    if (!message.isValid())
        return;

    QByteArray messageId = message.data(Imap::Mailbox::RoleMessageMessageId).toByteArray();

    ComposeWidget *w = mainWindow->invokeComposeDialog(
                Composer::Util::replySubject(message.data(Imap::Mailbox::RoleMessageSubject).toString()), quoteText(),
                QList<QPair<Composer::RecipientKind,QString> >(),
                QList<QByteArray>() << messageId,
                message.data(Imap::Mailbox::RoleMessageHeaderReferences).value<QList<QByteArray> >() << messageId,
                message
                );
    if (!w)
        return;

    bool ok = w->setReplyMode(mode);
    if (!ok) {
        QString err;
        switch (mode) {
        case Composer::REPLY_ALL:
        case Composer::REPLY_ALL_BUT_ME:
            // do nothing
            break;
        case Composer::REPLY_LIST:
            err = tr("It doesn't look like this is a message to the mailing list. Please fill in the recipients manually.");
            break;
        case Composer::REPLY_PRIVATE:
            err = trUtf8("Trojitá was unable to safely determine the real e-mail address of the author of the message. "
                         "You might want to use the \"Reply All\" function and trim the list of addresses manually.");
            break;
        }
        if (!err.isEmpty())
            QMessageBox::warning(w, tr("Cannot Determine Recipients"), err);
    }
}

void MessageView::externalsRequested(const QUrl &url)
{
    Q_UNUSED(url);
    externalElements->show();
}

void MessageView::externalsEnabled()
{
    netAccess->setExternalsEnabled(true);
    externalElements->hide();
    AbstractPartWidget *w = dynamic_cast<AbstractPartWidget *>(viewer);
    if (w)
        w->reloadContents();
}

void MessageView::newLabelAction(const QString &tag)
{
    if (!message.isValid())
        return;

    Imap::Mailbox::Model *model = dynamic_cast<Imap::Mailbox::Model *>(const_cast<QAbstractItemModel *>(message.model()));
    model->setMessageFlags(QModelIndexList() << message, tag, Imap::Mailbox::FLAG_ADD);
}

void MessageView::deleteLabelAction(const QString &tag)
{
    if (!message.isValid())
        return;

    Imap::Mailbox::Model *model = dynamic_cast<Imap::Mailbox::Model *>(const_cast<QAbstractItemModel *>(message.model()));
    model->setMessageFlags(QModelIndexList() << message, tag, Imap::Mailbox::FLAG_REMOVE);
}

void MessageView::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_ASSERT(topLeft.row() == bottomRight.row() && topLeft.parent() == bottomRight.parent());
    if (topLeft == message) {
        if (viewer == emptyView && message.data(Imap::Mailbox::RoleIsFetched).toBool()) {
            qDebug() << "MessageView: message which was previously not loaded has just became available";
            setEmpty();
            setMessage(topLeft);
        }
        tags->setTagList(message.data(Imap::Mailbox::RoleMessageFlags).toStringList());
    }
}

void MessageView::setHomepageUrl(const QUrl &homepage)
{
    emptyView->load(homepage);
}

void MessageView::showEvent(QShowEvent *se)
{
    QWidget::showEvent(se);
    // The Oxygen style reset the attribute - since we're gonna cause an update() here anyway, it's
    // a good moment to stress that "we know better, Hugo ;-)" -- Thomas
    setAutoFillBackground(true);
}

void MessageView::headerLinkActivated(QString s)
{
    // Trojita is registered to handle any mailto: URL
    QDesktopServices::openUrl(QUrl(s));
}

void MessageView::partContextMenuRequested(const QPoint &point)
{
    if (SimplePartWidget *w = qobject_cast<SimplePartWidget *>(sender())) {
        QMenu menu(w);
        Q_FOREACH(QAction *action, w->contextMenuSpecificActions())
            menu.addAction(action);
        menu.addAction(w->pageAction(QWebPage::SelectAll));
        if (!w->page()->mainFrame()->hitTestContent(point).linkUrl().isEmpty()) {
            menu.addSeparator();
            menu.addAction(w->pageAction(QWebPage::CopyLinkToClipboard));
        }
        menu.exec(w->mapToGlobal(point));
    }
}

void MessageView::partLinkHovered(const QString &link, const QString &title, const QString &textContent)
{
    Q_UNUSED(title);
    Q_UNUSED(textContent);
    emit linkHovered(link);
}

void MessageView::triggerSearchDialog()
{
    emit searchRequestedBy(qobject_cast<QWebView*>(sender()));
}

QModelIndex MessageView::currentMessage() const
{
    return message;
}

void MessageView::onWebViewLoadStarted()
{
    QWebView *wv = qobject_cast<QWebView*>(sender());
    Q_ASSERT(wv);
    QModelIndex messageIndex;
    const Imap::Mailbox::Model *constModel = 0;
    Imap::Mailbox::Model::realTreeItem(message, &constModel, &messageIndex);
    if (constModel && constModel->isNetworkAvailable()) {
        m_loadingItems << wv;
        m_loadingSpinner->start(250);
    }
}

void MessageView::onWebViewLoadFinished()
{
    QWebView *wv = qobject_cast<QWebView*>(sender());
    Q_ASSERT(wv);
    m_loadingItems.remove(wv);
    if (m_loadingItems.isEmpty())
        m_loadingSpinner->stop();
}

}
