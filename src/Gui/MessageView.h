/* Copyright (C) 2006 - 2016 Jan Kundrát <jkt@kde.org>
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
#ifndef VIEW_MESSAGEVIEW_H
#define VIEW_MESSAGEVIEW_H

#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <QWidget>
#include "Composer/Recipients.h"
#include "Gui/PartWalker.h"

class QBoxLayout;
class QLabel;
class QLayout;
class QSettings;
class QStackedLayout;
class QTimer;
class QUrl;
class QWebView;

namespace Cryptography {
class MessageModel;
}

namespace Imap {
namespace Network {
class MsgPartNetAccessManager;
}
namespace Message {
class Envelope;
}
namespace Mailbox {
class NetworkWatcher;
}
}

namespace Plugins {
class PluginManager;
}

namespace Gui {

class AbstractPartWidget;
class EmbeddedWebView;
class EnvelopeView;
class MainWindow;
class ExternalElementsWidget;
class Spinner;
class TagListWidget;


/** @short Widget for displaying complete e-mail messages as available from the IMAP server

  Widget which can render a regular message as exported by the Imap::Mailbox::Model model.
  Notably, this class will not render message/rfc822 MIME parts.
*/
class MessageView : public QWidget
{
    Q_OBJECT
public:
    MessageView(QWidget *parent, QSettings *settings, Plugins::PluginManager *pluginManager);
    ~MessageView();

    void setNetworkWatcher(Imap::Mailbox::NetworkWatcher *netWatcher);
    void reply(MainWindow *mainWindow, Composer::ReplyMode mode);
    void forward(MainWindow *mainWindow, const Composer::ForwardMode mode);
    QModelIndex currentMessage() const;
    Plugins::PluginManager *pluginManager() const;
public slots:
    void setMessage(const QModelIndex &index);
    void setEmpty();
    void setHomepageUrl(const QUrl &homepage);
    void stopAutoMarkAsRead();
    void zoomIn();
    void zoomOut();
    void zoomOriginal();
protected:
    void showEvent(QShowEvent *se) override;
private slots:
    void markAsRead();
    void externalsRequested(const QUrl &url);
    void enableExternalData();
    void newLabelAction(const QString &tag);
    void deleteLabelAction(const QString &tag);
    void partContextMenuRequested(const QPoint &point);
    void partLinkHovered(const QString &link, const QString &title, const QString &textContent);
    void triggerSearchDialog();
    void onWebViewLoadStarted();
    void onWebViewLoadFinished();
signals:
    void messageChanged();
    void messageModelChanged(QAbstractItemModel *model);
    void linkHovered(const QString &url);
    void searchRequestedBy(EmbeddedWebView *webView);
    void transferError(const QString &errorString);
private:
    bool eventFilter(QObject *object, QEvent *event) override;
    Imap::Message::Envelope envelope() const;
    QString quoteText() const;
    void showMessageNow();
    AbstractPartWidget *bodyWidget() const;
    void unsetPreviousMessage();
    void clearWaitingConns();

    QStackedLayout *m_stack;
    QWebView *m_homePage;

    QWidget *m_messageWidget;
    QBoxLayout *m_msgLayout;
    EnvelopeView *m_envelope;
    ExternalElementsWidget *externalElements;
    TagListWidget *tags;
    QPersistentModelIndex message;
    Cryptography::MessageModel *messageModel;
    Imap::Network::MsgPartNetAccessManager *netAccess;
    QPointer<Imap::Mailbox::NetworkWatcher> m_netWatcher;
    QTimer *markAsReadTimer;
    QWidget *m_bodyWidget;
    QAction *m_zoomIn, *m_zoomOut, *m_zoomOriginal;

    std::unique_ptr<PartWidgetFactory> factory;
    Spinner *m_loadingSpinner;
    QSettings *m_settings;
    Plugins::PluginManager *m_pluginManager;
    QSet<QWebView*> m_loadingItems;

    std::vector<QMetaObject::Connection> m_waitingMessageConns;

    MessageView(const MessageView &); // don't implement
    MessageView &operator=(const MessageView &); // don't implement

    friend class SimplePartWidget; // needs access to onWebViewLoadStarted/onWebViewLoadFinished
};

}

#endif // VIEW_MESSAGEVIEW_H
