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
#ifndef VIEW_MESSAGEVIEW_H
#define VIEW_MESSAGEVIEW_H

#include <QPersistentModelIndex>
#include <QWidget>

class QBoxLayout;
class QLabel;
class QLayout;
class QTimer;
class QUrl;
class QWebView;

namespace Imap
{
namespace Network
{
class MsgPartNetAccessManager;
}
}

namespace Gui
{

class MainWindow;
class PartWidgetFactory;
class ExternalElementsWidget;
class TagListWidget;


/** @short Widget for displaying complete e-mail messages as available from the IMAP server

  Widget which can render a regular message as exported by the Imap::Mailbox::Model model.
  Notably, this class will not render message/rfc822 MIME parts.
*/
class MessageView : public QWidget
{
    Q_OBJECT
public:
    MessageView(QWidget *parent=0);
    ~MessageView();
    enum ReplyMode { REPLY_SENDER_ONLY, /**< @short Reply to sender(s) only */
                     REPLY_ALL /**< @short Reply to all recipients */
                   };
    void reply(MainWindow *mainWindow, ReplyMode mode);
public slots:
    void setMessage(const QModelIndex &index);
    void setEmpty();
    void setHomepageUrl(const QUrl &homepage);
protected:
    void showEvent(QShowEvent *se);
private slots:
    void markAsRead();
    void externalsRequested(const QUrl &url);
    void externalsEnabled();
    void linkInTitleHovered(const QString &target);
    void newLabelAction(const QString &tag);
    void deleteLabelAction(const QString &tag);
    void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void headerLinkActivated(QString);
signals:
    void messageChanged();
private:
    bool eventFilter(QObject *object, QEvent *event);
    QString headerText();
    QString quoteText() const;
    static QString replySubject(const QString &subject);

    QWidget *viewer;
    QWidget *headerSection;
    QLabel *header;
    ExternalElementsWidget *externalElements;
    QBoxLayout *layout;
    TagListWidget *tags;
    QPersistentModelIndex message;
    Imap::Network::MsgPartNetAccessManager *netAccess;
    QTimer *markAsReadTimer;
    QWebView *emptyView;
    PartWidgetFactory *factory;

    MessageView(const MessageView &); // don't implement
    MessageView &operator=(const MessageView &); // don't implement
};

}

#endif // VIEW_MESSAGEVIEW_H
