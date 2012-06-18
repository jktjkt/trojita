/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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
#include <QKeyEvent>
#include <QLabel>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtWebKit/QWebHistory>
#include <QDebug>

#include "MessageView.h"
#include "AbstractPartWidget.h"
#include "EmbeddedWebView.h"
#include "ExternalElementsWidget.h"
#include "PartWidgetFactory.h"
#include "TagListWidget.h"
#include "UserAgentWebPage.h"
#include "Window.h"

#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Network/MsgPartNetAccessManager.h"

namespace Gui
{

MessageView::MessageView(QWidget *parent): QWidget(parent)
{
    netAccess = new Imap::Network::MsgPartNetAccessManager(this);
    connect(netAccess, SIGNAL(requestingExternal(QUrl)), this, SLOT(externalsRequested(QUrl)));
    factory = new PartWidgetFactory(netAccess, this);

    emptyView = new EmbeddedWebView(this, new QNetworkAccessManager(this));
    emptyView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    emptyView->setPage(new UserAgentWebPage(emptyView));
    emptyView->installEventFilter(this);

    layout = new QVBoxLayout(this);
    viewer = emptyView;
    header = new QLabel(this);
    header->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    tags = new TagListWidget(this);
    tags->hide();
    connect(tags, SIGNAL(tagAdded(QString)), this, SLOT(newLabelAction(QString)));
    connect(tags, SIGNAL(tagRemoved(QString)), this, SLOT(deleteLabelAction(QString)));
    connect(header, SIGNAL(linkHovered(QString)), this, SLOT(linkInTitleHovered(QString)));
    externalElements = new ExternalElementsWidget(this);
    externalElements->hide();
    connect(externalElements, SIGNAL(loadingEnabled()), this, SLOT(externalsEnabled()));
    layout->addWidget(externalElements);
    layout->addWidget(header);
    layout->addWidget(tags);
    layout->addWidget(viewer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    markAsReadTimer = new QTimer(this);
    markAsReadTimer->setSingleShot(true);
    connect(markAsReadTimer, SIGNAL(timeout()), this, SLOT(markAsRead()));

    header->setIndent(5);
    header->setWordWrap(true);
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
    header->setText(QString());
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

    if (!messageIndex.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        qDebug() << "Attempted to load a message that hasn't been synced yet";
        setEmpty();
        connect(realModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
        message = messageIndex;
        return;
    }

    QModelIndex rootPartIndex = messageIndex.child(0, 0);

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

        viewer = factory->create(rootPartIndex);
        viewer->setParent(this);
        layout->addWidget(viewer);
        viewer->show();
        header->setText(headerText());

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

bool MessageView::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        MessageView::event(event);
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
        case Qt::Key_Home:
        case Qt::Key_End:
            MessageView::event(event);
            return true;
        default:
            return QObject::eventFilter(object, event);
        }
    } else {
        return QObject::eventFilter(object, event);
    }
}

QString MessageView::headerText()
{
    if (!message.isValid())
        return QString();

    // Accessing the envelope via QVariant is just too much work here; it's way easier to just get the raw pointer
    Imap::Mailbox::Model *model = dynamic_cast<Imap::Mailbox::Model *>(const_cast<QAbstractItemModel *>(message.model()));
    Imap::Mailbox::TreeItemMessage *messagePtr = dynamic_cast<Imap::Mailbox::TreeItemMessage *>(static_cast<Imap::Mailbox::TreeItem *>(message.internalPointer()));
    const Imap::Message::Envelope &envelope = messagePtr->envelope(model);

    QString res;
    if (!envelope.from.isEmpty())
        res += tr("<b>From:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(envelope.from, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    if (!envelope.to.isEmpty())
        res += tr("<b>To:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(envelope.to, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    if (!envelope.cc.isEmpty())
        res += tr("<b>Cc:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(envelope.cc, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    if (!envelope.bcc.isEmpty())
        res += tr("<b>Bcc:</b>&nbsp;%1<br/>").arg(Imap::Message::MailAddress::prettyList(envelope.bcc, Imap::Message::MailAddress::FORMAT_CLICKABLE));
    res += tr("<b>Subject:</b>&nbsp;%1").arg(Qt::escape(envelope.subject));
    if (envelope.date.isValid())
        res += tr("<br/><b>Date:</b>&nbsp;%1").arg(envelope.date.toLocalTime().toString(Qt::SystemLocaleLongDate));
    return res;
}

QString MessageView::quoteText() const
{
    const AbstractPartWidget *w = dynamic_cast<const AbstractPartWidget *>(viewer);
    return w ? w->quoteMe() : QString();
}

void MessageView::reply(MainWindow *mainWindow, ReplyMode mode)
{
    if (!message.isValid())
        return;

    // Accessing the envelope via QVariant is just too much work here; it's way easier to just get the raw pointer
    Imap::Mailbox::Model *model = dynamic_cast<Imap::Mailbox::Model *>(const_cast<QAbstractItemModel *>(message.model()));
    Imap::Mailbox::TreeItemMessage *messagePtr = dynamic_cast<Imap::Mailbox::TreeItemMessage *>(static_cast<Imap::Mailbox::TreeItem *>(message.internalPointer()));
    const Imap::Message::Envelope &envelope = messagePtr->envelope(model);
    // ...now imagine how that would look like on just a single line :)

    QList<QPair<QString,QString> > recipients;
    for (QList<Imap::Message::MailAddress>::const_iterator it = envelope.from.begin(); it != envelope.from.end(); ++it) {
        recipients << qMakePair(tr("To"), QString::fromAscii("%1@%2").arg(it->mailbox, it->host));
    }
    if (mode == REPLY_ALL) {
        for (QList<Imap::Message::MailAddress>::const_iterator it = envelope.to.begin(); it != envelope.to.end(); ++it) {
            recipients << qMakePair(tr("Cc"), QString::fromAscii("%1@%2").arg(it->mailbox, it->host));
        }
        for (QList<Imap::Message::MailAddress>::const_iterator it = envelope.cc.begin(); it != envelope.cc.end(); ++it) {
            recipients << qMakePair(tr("Cc"), QString::fromAscii("%1@%2").arg(it->mailbox, it->host));
        }
    }
    mainWindow->invokeComposeDialog(replySubject(envelope.subject), quoteText(), recipients, envelope.messageId);
}

QString MessageView::replySubject(const QString &subject)
{
    if (!subject.startsWith(tr("Re:")))
        return tr("Re: ") + subject;
    else
        return subject;
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

void MessageView::linkInTitleHovered(const QString &target)
{
    if (target.isEmpty()) {
        header->setToolTip(QString());
        return;
    }

    QUrl url(target);
    QString niceName = url.queryItemValue(QLatin1String("X-Trojita-DisplayName"));
    if (niceName.isEmpty())
        header->setToolTip(QString::fromAscii("%1@%2").arg(
                               Qt::escape(url.userName()), Qt::escape(url.host())));
    else
        header->setToolTip(QString::fromAscii("<p style='white-space:pre'>%1 &lt;%2@%3&gt;</p>").arg(
                               Qt::escape(niceName), Qt::escape(url.userName()), Qt::escape(url.host())));
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
            qDebug() << "got it!";
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

}

