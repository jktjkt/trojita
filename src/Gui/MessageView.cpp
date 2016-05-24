/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>
   Copyright (C) 2014        Luke Dashjr <luke+trojita@dashjr.org>
   Copyright (C) 2014 - 2015 Stephan Platz <trojita@paalsteek.de>

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

#include <QKeyEvent>
#include <QMenu>
#include <QSettings>
#include <QStackedLayout>

#include "Common/InvokeMethod.h"
#include "Common/SettingsNames.h"
#include "Composer/QuoteText.h"
#include "Composer/SubjectMangling.h"
#include "Cryptography/MessageModel.h"
#include "Gui/MessageView.h"
#include "Gui/ComposeWidget.h"
#include "Gui/EnvelopeView.h"
#include "Gui/ExternalElementsWidget.h"
#include "Gui/OverlayWidget.h"
#include "Gui/PartWidgetFactoryVisitor.h"
#include "Gui/ShortcutHandler/ShortcutHandler.h"
#include "Gui/SimplePartWidget.h"
#include "Gui/Spinner.h"
#include "Gui/TagListWidget.h"
#include "Gui/UserAgentWebPage.h"
#include "Gui/Window.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/MsgListModel.h"
#include "Imap/Model/NetworkWatcher.h"
#include "Imap/Model/Utils.h"
#include "Imap/Network/MsgPartNetAccessManager.h"
#include "Plugins/PluginManager.h"
#include "UiUtils/IconLoader.h"

namespace Gui
{

MessageView::MessageView(QWidget *parent, QSettings *settings, Plugins::PluginManager *pluginManager)
    : QWidget(parent)
    , m_stack(new QStackedLayout(this))
    , messageModel(0)
    , netAccess(new Imap::Network::MsgPartNetAccessManager(this))
    , factory(new PartWidgetFactory(netAccess, this,
                                    std::unique_ptr<PartWidgetFactoryVisitor>(new PartWidgetFactoryVisitor())))
    , m_settings(settings)
    , m_pluginManager(pluginManager)
{
    connect(netAccess, &Imap::Network::MsgPartNetAccessManager::requestingExternal, this, &MessageView::externalsRequested);


    setBackgroundRole(QPalette::Base);
    setForegroundRole(QPalette::Text);
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus); // not by the wheel


    m_zoomIn = ShortcutHandler::instance()->createAction(QStringLiteral("action_zoom_in"), this);
    addAction(m_zoomIn);
    connect(m_zoomIn, &QAction::triggered, this, &MessageView::zoomIn);

    m_zoomOut = ShortcutHandler::instance()->createAction(QStringLiteral("action_zoom_out"), this);
    addAction(m_zoomOut);
    connect(m_zoomOut, &QAction::triggered, this, &MessageView::zoomOut);

    m_zoomOriginal = ShortcutHandler::instance()->createAction(QStringLiteral("action_zoom_original"), this);
    addAction(m_zoomOriginal);
    connect(m_zoomOriginal, &QAction::triggered, this, &MessageView::zoomOriginal);


    // The homepage widget -- our poor man's splashscreen
    m_homePage = new EmbeddedWebView(this, new QNetworkAccessManager(this));
    m_homePage->setFixedSize(450,300);
    CALL_LATER_NOARG(m_homePage, handlePageLoadFinished);
    m_homePage->setPage(new UserAgentWebPage(m_homePage));
    m_homePage->installEventFilter(this);
    m_homePage->setAutoFillBackground(false);
    m_stack->addWidget(m_homePage);


    // The actual widget for the actual message
    m_messageWidget = new QWidget(this);
    auto fullMsgLayout = new QVBoxLayout(m_messageWidget);
    m_stack->addWidget(m_messageWidget);

    m_envelope = new EnvelopeView(m_messageWidget, this);
    fullMsgLayout->addWidget(m_envelope, 1);

    tags = new TagListWidget(m_messageWidget);
    connect(tags, &TagListWidget::tagAdded, this, &MessageView::newLabelAction);
    connect(tags, &TagListWidget::tagRemoved, this, &MessageView::deleteLabelAction);
    fullMsgLayout->addWidget(tags, 3);

    externalElements = new ExternalElementsWidget(this);
    externalElements->hide();
    connect(externalElements, &ExternalElementsWidget::loadingEnabled, this, &MessageView::enableExternalData);
    fullMsgLayout->addWidget(externalElements, 1);

    // put the actual messages into an extra horizontal view
    // this allows us easy usage of the trailing stretch and also to indent the message a bit
    m_msgLayout = new QHBoxLayout;
    m_msgLayout->setContentsMargins(6,6,6,0);
    fullMsgLayout->addLayout(m_msgLayout, 1);
    // add a strong stretch to squeeze header and message to the top
    // possibly passing a large stretch factor to the message could be enough...
    fullMsgLayout->addStretch(1000);


    markAsReadTimer = new QTimer(this);
    markAsReadTimer->setSingleShot(true);
    connect(markAsReadTimer, &QTimer::timeout, this, &MessageView::markAsRead);

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
    unsetPreviousMessage();
}

void MessageView::unsetPreviousMessage()
{
    clearWaitingConns();
    m_loadingItems.clear();
    message = QModelIndex();
    markAsReadTimer->stop();
    if (auto w = bodyWidget()) {
        m_stack->removeWidget(dynamic_cast<QWidget *>(w));
        delete w;
    }
    m_envelope->setMessage(QModelIndex());
    delete messageModel;
    messageModel = nullptr;
}

void MessageView::setEmpty()
{
    unsetPreviousMessage();
    m_loadingSpinner->stop();
    m_stack->setCurrentWidget(m_homePage);
    emit messageChanged();
}

AbstractPartWidget *MessageView::bodyWidget() const
{
    if (m_msgLayout->itemAt(0) && m_msgLayout->itemAt(0)->widget()) {
        return dynamic_cast<AbstractPartWidget *>(m_msgLayout->itemAt(0)->widget());
    } else {
        return nullptr;
    }
}

void MessageView::setMessage(const QModelIndex &index)
{
    Q_ASSERT(index.isValid());
    QModelIndex messageIndex = Imap::deproxifiedIndex(index);
    Q_ASSERT(messageIndex.isValid());

    if (message == messageIndex) {
        // This is a duplicate call, let's do nothing.
        // It might not be our fat-fingered user, but also just a side-effect of our duplicate invocation through
        // QAbstractItemView::clicked() and activated().
        return;
    }

    unsetPreviousMessage();

    message = messageIndex;
    messageModel = new Cryptography::MessageModel(this, message);
    messageModel->setObjectName(QStringLiteral("cryptoMessageModel-%1-%2")
                                .arg(message.data(Imap::Mailbox::RoleMailboxName).toString(),
                                     message.data(Imap::Mailbox::RoleMessageUid).toString()));
    for (const auto &module: m_pluginManager->mimePartReplacers()) {
        messageModel->registerPartHandler(module);
    }
    emit messageModelChanged(messageModel);

    // The data might be available from the local cache, so let's try to save a possible roundtrip here
    // by explicitly requesting the data
    message.data(Imap::Mailbox::RolePartData);

    if (!message.data(Imap::Mailbox::RoleIsFetched).toBool()) {
        // This happens when the message placeholder is already available in the GUI, but the actual message data haven't been
        // loaded yet. This is especially common with the threading model, but also with bigger unsynced mailboxes.
        // Note that the data might be already available in the cache, it's just that it isn't in the mailbox tree yet.
        m_waitingMessageConns.emplace_back(
                    connect(messageModel, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &topLeft){
            if (topLeft.data(Imap::Mailbox::RoleIsFetched).toBool()) {
                // OK, message is fully fetched now
                showMessageNow();
            }
        }));
        m_loadingSpinner->setText(tr("Waiting\nfor\nMessage..."));
        m_loadingSpinner->start();
    } else {
        showMessageNow();
    }
}

/** @short Implementation of the "hey, let's really display the message, its BODYSTRUCTURE is available now" */
void MessageView::showMessageNow()
{
    Q_ASSERT(message.data(Imap::Mailbox::RoleIsFetched).toBool());

    clearWaitingConns();

    QModelIndex rootPartIndex = messageModel->index(0,0);
    Q_ASSERT(rootPartIndex.child(0,0).isValid());

    netAccess->setExternalsEnabled(false);
    externalElements->hide();

    netAccess->setModelMessage(rootPartIndex);

    m_loadingItems.clear();
    m_loadingSpinner->stop();

    m_envelope->setMessage(message);

    auto updateTagList = [this]() {
        tags->setTagList(message.data(Imap::Mailbox::RoleMessageFlags).toStringList());
    };
    connect(messageModel, &QAbstractItemModel::dataChanged, this, updateTagList);
    updateTagList();

    UiUtils::PartLoadingOptions loadingMode;
    if (m_settings->value(Common::SettingsNames::guiPreferPlaintextRendering, QVariant(true)).toBool())
        loadingMode |= UiUtils::PART_PREFER_PLAINTEXT_OVER_HTML;
    auto viewer = factory->walk(rootPartIndex.child(0,0), 0, loadingMode);
    viewer->setParent(this);
    m_msgLayout->addWidget(viewer);
    m_msgLayout->setAlignment(viewer, Qt::AlignTop|Qt::AlignLeft);
    viewer->show();
    // We want to propagate the QWheelEvent to upper layers
    viewer->installEventFilter(this);
    m_stack->setCurrentWidget(m_messageWidget);

    if (m_netWatcher && m_netWatcher->effectiveNetworkPolicy() != Imap::Mailbox::NETWORK_OFFLINE
            && m_settings->value(Common::SettingsNames::autoMarkReadEnabled, QVariant(true)).toBool()) {
        // No additional delay is needed here because the MsgListView won't open a message while the user keeps scrolling,
        // which was AFAIK the original intention
        markAsReadTimer->start(m_settings->value(Common::SettingsNames::autoMarkReadSeconds, QVariant(0)).toUInt() * 1000);
    }

    emit messageChanged();
}

/** @short There's no point in waiting for the message to appear */
void MessageView::clearWaitingConns()
{
    for (auto &conn: m_waitingMessageConns) {
        disconnect(conn);
    }
    m_waitingMessageConns.clear();
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
        if (static_cast<QWheelEvent *>(event)->modifiers() == Qt::ControlModifier) {
            if (static_cast<QWheelEvent *>(event)->delta() > 0) {
                zoomIn();
            } else {
                zoomOut();
            }
        } else {
            // while the containing scrollview has Qt::StrongFocus, the event forwarding breaks that
            // -> completely disable focus for the following wheel event ...
            parentWidget()->setFocusPolicy(Qt::NoFocus);
            MessageView::event(event);
            // ... set reset it
            parentWidget()->setFocusPolicy(Qt::StrongFocus);
        }
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
    if (auto w = bodyWidget()) {
        QStringList quote = Composer::quoteText(w->quoteMe().split(QLatin1Char('\n')));
        const Imap::Message::Envelope &e = message.data(Imap::Mailbox::RoleMessageEnvelope).value<Imap::Message::Envelope>();
        QString sender;
        if (!e.from.isEmpty())
            sender = e.from[0].prettyName(Imap::Message::MailAddress::FORMAT_JUST_NAME);
        if (e.from.isEmpty())
            sender = tr("you");

        // One extra newline at the end of the quoted text to separate the response
        quote << QString();

        return tr("On %1, %2 wrote:\n").arg(e.date.toLocalTime().toString(Qt::SystemLocaleLongDate), sender) + quote.join(QStringLiteral("\n"));
    }
    return QString();
}

#define FORWARD_METHOD(METHOD) \
void MessageView::METHOD() \
{ \
    if (auto w = bodyWidget()) { \
        w->METHOD(); \
    } \
}
FORWARD_METHOD(zoomIn)
FORWARD_METHOD(zoomOut)
FORWARD_METHOD(zoomOriginal)

void MessageView::setNetworkWatcher(Imap::Mailbox::NetworkWatcher *netWatcher)
{
    m_netWatcher = netWatcher;
    factory->setNetworkWatcher(netWatcher);
}

void MessageView::reply(MainWindow *mainWindow, Composer::ReplyMode mode)
{
    if (!message.isValid())
        return;

    // The Message-Id of the original message might have been empty; be sure we can handle that
    QByteArray messageId = message.data(Imap::Mailbox::RoleMessageMessageId).toByteArray();
    QList<QByteArray> messageIdList;
    if (!messageId.isEmpty()) {
        messageIdList.append(messageId);
    }

    ComposeWidget::warnIfMsaNotConfigured(
                ComposeWidget::createReply(mainWindow, mode, message, QList<QPair<Composer::RecipientKind,QString> >(),
                                           Composer::Util::replySubject(message.data(Imap::Mailbox::RoleMessageSubject).toString()),
                                           quoteText(), messageIdList,
                                           message.data(Imap::Mailbox::RoleMessageHeaderReferences).value<QList<QByteArray> >() + messageIdList),
                mainWindow);
}

void MessageView::forward(MainWindow *mainWindow, const Composer::ForwardMode mode)
{
    if (!message.isValid())
        return;

    // The Message-Id of the original message might have been empty; be sure we can handle that
    QByteArray messageId = message.data(Imap::Mailbox::RoleMessageMessageId).toByteArray();
    QList<QByteArray> messageIdList;
    if (!messageId.isEmpty()) {
        messageIdList.append(messageId);
    }

    ComposeWidget::warnIfMsaNotConfigured(
                ComposeWidget::createForward(mainWindow, mode, message, Composer::Util::forwardSubject(message.data(Imap::Mailbox::RoleMessageSubject).toString()),
                                             messageIdList, message.data(Imap::Mailbox::RoleMessageHeaderReferences).value<QList<QByteArray>>() + messageIdList),
                mainWindow);
}

void MessageView::externalsRequested(const QUrl &url)
{
    Q_UNUSED(url);
    externalElements->show();
}

void MessageView::enableExternalData()
{
    netAccess->setExternalsEnabled(true);
    externalElements->hide();
    if (auto w = bodyWidget()) {
        w->reloadContents();
    }
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

void MessageView::setHomepageUrl(const QUrl &homepage)
{
    m_homePage->load(homepage);
}

void MessageView::showEvent(QShowEvent *se)
{
    QWidget::showEvent(se);
    // The Oxygen style reset the attribute - since we're gonna cause an update() here anyway, it's
    // a good moment to stress that "we know better, Hugo ;-)" -- Thomas
    setAutoFillBackground(true);
}

void MessageView::partContextMenuRequested(const QPoint &point)
{
    if (SimplePartWidget *w = qobject_cast<SimplePartWidget *>(sender())) {
        QMenu menu(w);
        w->buildContextMenu(point, menu);
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
    emit searchRequestedBy(qobject_cast<EmbeddedWebView*>(sender()));
}

QModelIndex MessageView::currentMessage() const
{
    return message;
}

void MessageView::onWebViewLoadStarted()
{
    QWebView *wv = qobject_cast<QWebView*>(sender());
    Q_ASSERT(wv);

    if (m_netWatcher && m_netWatcher->effectiveNetworkPolicy() != Imap::Mailbox::NETWORK_OFFLINE) {
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

Plugins::PluginManager *MessageView::pluginManager() const
{
    return m_pluginManager;
}

}
