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
#ifndef VIEW_MESSAGEVIEW_H
#define VIEW_MESSAGEVIEW_H

#include <QPersistentModelIndex>
#include <QSet>
#include <QWidget>
#include "Composer/Recipients.h"

class QBoxLayout;
class QLabel;
class QLayout;
class QSettings;
class QTimer;
class QUrl;
class QWebView;

namespace Imap
{
namespace Network
{
class MsgPartNetAccessManager;
}
namespace Message
{
class Envelope;
}
}

namespace Gui
{

class EnvelopeView;
class MainWindow;
class PartWidgetFactory;
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
    MessageView(QWidget *parent, QSettings *settings);
    ~MessageView();

    void reply(MainWindow *mainWindow, Composer::ReplyMode mode);
    QModelIndex currentMessage() const;
public slots:
    void setMessage(const QModelIndex &index);
    void setEmpty();
    void setHomepageUrl(const QUrl &homepage);
    void stopAutoMarkAsRead();
protected:
    void showEvent(QShowEvent *se);
private slots:
    void markAsRead();
    void externalsRequested(const QUrl &url);
    void externalsEnabled();
    void newLabelAction(const QString &tag);
    void deleteLabelAction(const QString &tag);
    void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void headerLinkActivated(QString);
    void partContextMenuRequested(const QPoint &point);
    void partLinkHovered(const QString &link, const QString &title, const QString &textContent);
    void triggerSearchDialog();
    void onWebViewLoadStarted();
    void onWebViewLoadFinished();
signals:
    void messageChanged();
    void linkHovered(const QString &url);
    void searchRequestedBy(QWebView *webView);
    void addressDetailsRequested(const QString &mail, QStringList &addresses);
    void transferError(const QString &errorString);
private:
    bool eventFilter(QObject *object, QEvent *event);
    Imap::Message::Envelope envelope() const;
    QString quoteText() const;

    QWidget *viewer;
    QWidget *headerSection;
    EnvelopeView *m_envelope;
    ExternalElementsWidget *externalElements;
    QBoxLayout *layout;
    TagListWidget *tags;
    QPersistentModelIndex message;
    Imap::Network::MsgPartNetAccessManager *netAccess;
    QTimer *markAsReadTimer;
    QWebView *emptyView;
    PartWidgetFactory *factory;
    Spinner *m_loadingSpinner;
    QSettings *m_settings;
    QSet<QWebView*> m_loadingItems;

    MessageView(const MessageView &); // don't implement
    MessageView &operator=(const MessageView &); // don't implement
};

}

#endif // VIEW_MESSAGEVIEW_H
