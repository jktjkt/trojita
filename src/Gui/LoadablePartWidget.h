/* Copyright (C) 2006 - 2010 Jan Kundrát <jkt@gentoo.org>

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
#ifndef LOADABLEPARTWIDGET_H
#define LOADABLEPARTWIDGET_H

#include <QStackedWidget>

#include "AbstractPartWidget.h"
#include "SimplePartWidget.h"

class QPushButton;

namespace Gui {

/** @short Widget which implements "click-through" for loading message parts on demand

  When a policy dictates that certain body parts should not be shown unless
  really required, this widget comes to action.  It provides a click-wrapped
  meaning of showing huge body parts.  No data are transfered unless the user
  clicks a button.
*/
class LoadablePartWidget : public QStackedWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    LoadablePartWidget( QWidget* parent,
                        Imap::Network::MsgPartNetAccessManager* _manager,
                        Imap::Mailbox::TreeItemPart* _part,
                        QObject* _wheelEventFilter );
    QString quoteMe() const;
    virtual void reloadContents() {}
private slots:
    void loadClicked();
private:
    Imap::Network::MsgPartNetAccessManager* manager;
    Imap::Mailbox::TreeItemPart* part;
    SimplePartWidget* realPart;
    QObject* wheelEventFilter;
    QPushButton* loadButton;

    LoadablePartWidget(const LoadablePartWidget&); // don't implement
    LoadablePartWidget& operator=(const LoadablePartWidget&); // don't implement
};

}

#endif // LOADABLEPARTWIDGET_H
