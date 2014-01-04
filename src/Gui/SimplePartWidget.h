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
#ifndef SIMPLEPARTWIDGET_H
#define SIMPLEPARTWIDGET_H

#include "AbstractPartWidget.h"
#include "EmbeddedWebView.h"
#include "Composer/PlainTextFormatter.h"
#include <QAction>
#include <QFile>
#include <QPersistentModelIndex>

class QModelIndex;
class QNetworkReply;

namespace Imap
{
namespace Network
{
class MsgPartNetAccessManager;
}
}

namespace Gui
{

class MessageView;

/** @short Widget that handles display of primitive message parts

More complicated parts are handled by other widgets. Role of this one is to
simply render data that can't be broken down to more trivial pieces.
*/

class SimplePartWidget : public EmbeddedWebView, public AbstractPartWidget
{
    Q_OBJECT
public:
    SimplePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &partIndex,
                     MessageView *messageView);
    virtual QString quoteMe() const;
    virtual void reloadContents();
    QList<QAction *> contextMenuSpecificActions() const;
private slots:
    void slotFileNameRequested(QString *fileName);
    void slotMarkupPlainText();
    void slotDownloadPart();
    void slotDownloadMessage();
signals:
    void linkHovered(const QString &link, const QString &title, const QString &textContent);
    void searchDialogRequested();
private:
    QPersistentModelIndex m_partIndex;
    QAction *m_savePart;
    QAction *m_saveMessage;
    QAction *m_findAction;
    MessageView *m_messageView;
    Imap::Network::MsgPartNetAccessManager *m_netAccessManager;
    Composer::Util::FlowedFormat flowedFormat;

    SimplePartWidget(const SimplePartWidget &); // don't implement
    SimplePartWidget &operator=(const SimplePartWidget &); // don't implement
};

}

#endif // SIMPLEPARTWIDGET_H
