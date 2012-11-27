/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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
#ifndef LOADABLEPARTWIDGET_H
#define LOADABLEPARTWIDGET_H

#include <QPersistentModelIndex>
#include <QStackedWidget>

#include "AbstractPartWidget.h"
#include "SimplePartWidget.h"

class QPushButton;

namespace Gui
{

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
    LoadablePartWidget(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &part,
                       QObject *wheelEventFilter, QObject *guiInteractionTarget);
    QString quoteMe() const;
    virtual void reloadContents() {}
private slots:
    void loadClicked();
private:
    Imap::Network::MsgPartNetAccessManager *manager;
    QPersistentModelIndex partIndex;
    SimplePartWidget *realPart;
    QObject *wheelEventFilter;
    QObject *guiInteractionTarget;
    QPushButton *loadButton;

    LoadablePartWidget(const LoadablePartWidget &); // don't implement
    LoadablePartWidget &operator=(const LoadablePartWidget &); // don't implement
};

}

#endif // LOADABLEPARTWIDGET_H
