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
#ifndef GUI_PARTWIDGETFACTORY_H
#define GUI_PARTWIDGETFACTORY_H

#include "Imap/Network/MsgPartNetAccessManager.h"

#include <QCoreApplication>

class QModelIndex;

namespace Gui
{

class MessageView;

class PartWidgetFactory
{
    Q_DECLARE_TR_FUNCTIONS(PartWidgetFactory)
    enum { ExpensiveFetchThreshold = 50*1024 };
public:
    /** @short Are the parts supposed to be visible immediately, or only after their respective widget is shown? */
    typedef enum {
        LOAD_IMMEDIATELY, /**< @short Load them immediately */
        LOAD_ON_SHOW /**< @short Load the parts only after they have been shown to the user */
    } PartLoadingMode;

    PartWidgetFactory(Imap::Network::MsgPartNetAccessManager *manager, MessageView *messageView);
    QWidget *create(const QModelIndex &partIndex);
    QWidget *create(const QModelIndex &partIndex, int recursionDepth, const PartLoadingMode loadingMode = LOAD_IMMEDIATELY);
    MessageView *messageView() const;
private:
    Imap::Network::MsgPartNetAccessManager *manager;
    MessageView *m_messageView;

    PartWidgetFactory(const PartWidgetFactory &); // don't implement
    PartWidgetFactory &operator=(const PartWidgetFactory &); // don't implement
};

}

#endif // GUI_PARTWIDGETFACTORY_H
